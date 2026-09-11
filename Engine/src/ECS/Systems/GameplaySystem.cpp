#include "Enjin/ECS/Systems/GameplaySystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace ECS {

namespace {

Math::Vector3 LerpVec(const Math::Vector3& a, const Math::Vector3& b, f32 t) {
    return Math::Vector3(a.x + (b.x - a.x) * t,
                         a.y + (b.y - a.y) * t,
                         a.z + (b.z - a.z) * t);
}

f32 Length(const Math::Vector3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Math::Vector3 Normalized(const Math::Vector3& v) {
    const f32 len = Length(v);
    if (len < 1e-6f) return Math::Vector3(0, 0, 0);
    return Math::Vector3(v.x / len, v.y / len, v.z / len);
}

// Is this entity something a player is driving? Used by "affectsPlayer" and by
// the switches and goals that ask for a player specifically.
bool IsPlayerControlled(World* world, Entity e) {
    return world->HasComponent<FirstPersonController>(e) ||
           world->HasComponent<ThirdPersonController>(e) ||
           world->HasComponent<TopDown3DController>(e) ||
           world->HasComponent<TopDown2DController>(e) ||
           world->HasComponent<Platformer2DController>(e);
}

void MoveEntity(World* world, Entity e, const Math::Vector3& delta) {
    if (auto* t = world->GetComponent<TransformComponent>(e)) {
        t->position.x += delta.x;
        t->position.y += delta.y;
        t->position.z += delta.z;
    }
}

} // namespace

WorldBounds ComputeWorldBounds(World* world, Entity entity) {
    WorldBounds bounds;
    Math::Vector3 origin(0, 0, 0);
    if (auto* t = world->GetComponent<TransformComponent>(entity)) {
        origin = t->position;
    }
    bounds.min = origin;
    bounds.max = origin;

    // Collider extents are WORLD SPACE and are not multiplied by the transform
    // scale -- this is the engine's convention (CLAUDE.md), and applying scale
    // here would make every overlap in this system disagree with every physics
    // query in the rest of the engine.
    if (auto* box = world->GetComponent<BoxColliderComponent>(entity)) {
        const Math::Vector3 c(origin.x + box->center.x,
                              origin.y + box->center.y,
                              origin.z + box->center.z);
        const Math::Vector3 h(box->size.x * 0.5f, box->size.y * 0.5f, box->size.z * 0.5f);
        bounds.min = Math::Vector3(c.x - h.x, c.y - h.y, c.z - h.z);
        bounds.max = Math::Vector3(c.x + h.x, c.y + h.y, c.z + h.z);
        return bounds;
    }
    if (auto* sphere = world->GetComponent<SphereColliderComponent>(entity)) {
        const Math::Vector3 c(origin.x + sphere->center.x,
                              origin.y + sphere->center.y,
                              origin.z + sphere->center.z);
        const f32 r = sphere->radius;
        bounds.min = Math::Vector3(c.x - r, c.y - r, c.z - r);
        bounds.max = Math::Vector3(c.x + r, c.y + r, c.z + r);
        return bounds;
    }
    if (auto* cap = world->GetComponent<CapsuleColliderComponent>(entity)) {
        // Capsule height is the CYLINDER only; total is height + 2 * radius.
        const Math::Vector3 c(origin.x + cap->center.x,
                              origin.y + cap->center.y,
                              origin.z + cap->center.z);
        const f32 r = cap->radius;
        const f32 halfH = cap->height * 0.5f + r;
        bounds.min = Math::Vector3(c.x - r, c.y - halfH, c.z - r);
        bounds.max = Math::Vector3(c.x + r, c.y + halfH, c.z + r);
        return bounds;
    }
    return bounds;   // a point, not a miss
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void GameplaySystem::OnPlayStart(World* world) {
    if (!world) return;
    m_RestPositions.clear();
    m_RestRotations.clear();
    m_Spawned.clear();
    m_GaveUp.clear();
    m_Random.Reset();
    m_Started = true;
    m_WarnedNoInputMap = false;
    m_WarnedNoSceneManager = false;

    // Capture where the author put everything this system animates. Lock's
    // closedPosition / openPosition and Switch's offPosition / onPosition are
    // OFFSETS from that rest pose; read as world positions they would slam every
    // door and plate to the origin on the first frame.
    for (Entity e : world->GetEntitiesWithComponent<LockComponent>()) {
        if (auto* t = world->GetComponent<TransformComponent>(e)) {
            m_RestPositions[e] = t->position;
            m_RestRotations[e] = t->rotation.ToEulerDegrees();
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<SwitchComponent>()) {
        if (auto* t = world->GetComponent<TransformComponent>(e)) {
            m_RestPositions[e] = t->position;
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<MovingPlatformComponent>()) {
        if (auto* t = world->GetComponent<TransformComponent>(e)) {
            m_RestPositions[e] = t->position;
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<PushableComponent>()) {
        if (auto* t = world->GetComponent<TransformComponent>(e)) {
            m_RestPositions[e] = t->position;
        }
    }

    // spawnOnStart, before the first Update, so a spawned entity gets a full
    // first frame like anything placed by hand.
    for (Entity e : world->GetEntitiesWithComponent<SpawnPointComponent>()) {
        auto* spawn = world->GetComponent<SpawnPointComponent>(e);
        if (!spawn) continue;
        spawn->currentSpawns = 0;
        spawn->spawnedEntities.clear();
        spawn->spawnTimer = spawn->spawnOnStart ? spawn->spawnDelay : -1.0f;
    }
}

void GameplaySystem::Reset(World* world) {
    if (!world) return;

    // Destroy exactly what this system created, by handle. Entity ids are
    // generational, so a recycled slot cannot be mistaken for one of ours.
    for (Entity e : m_Spawned) {
        if (world->IsValid(e)) world->DestroyEntity(e);
    }
    m_Spawned.clear();
    m_GaveUp.clear();

    // Put every animated entity back where the author left it.
    for (const auto& entry : m_RestPositions) {
        const Entity e = static_cast<Entity>(entry.first);
        if (!world->IsValid(e)) continue;
        if (auto* t = world->GetComponent<TransformComponent>(e)) {
            t->position = entry.second;
            auto rot = m_RestRotations.find(entry.first);
            if (rot != m_RestRotations.end()) {
                t->rotation = Math::Quaternion::FromEulerDegrees(rot->second);
            }
        }
    }
    m_RestPositions.clear();
    m_RestRotations.clear();

    // And clear the runtime state on the components themselves, so a second Play
    // does not begin with every door already open.
    for (Entity e : world->GetEntitiesWithComponent<LockComponent>()) {
        if (auto* lock = world->GetComponent<LockComponent>(e)) {
            lock->isOpen = false;
            lock->isAnimating = false;
            lock->openProgress = 0.0f;
            lock->openTimer = 0.0f;
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<SwitchComponent>()) {
        if (auto* sw = world->GetComponent<SwitchComponent>(e)) {
            sw->isActive = false;
            sw->wasActive = false;
            sw->activeTimer = 0.0f;
            sw->transitionProgress = 0.0f;
            sw->activatedBy = 0;
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<GoalZoneComponent>()) {
        if (auto* goal = world->GetComponent<GoalZoneComponent>(e)) {
            goal->isSatisfied = false;
            goal->satisfiedBy = 0;
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<TeleporterComponent>()) {
        if (auto* tp = world->GetComponent<TeleporterComponent>(e)) {
            tp->cooldownTimer = 0.0f;
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<MovingPlatformComponent>()) {
        if (auto* mp = world->GetComponent<MovingPlatformComponent>(e)) {
            mp->currentWaypoint = 0;
            mp->direction = 1;
            mp->moveProgress = 0.0f;
            mp->waitTimer = 0.0f;
            mp->isWaiting = false;
        }
    }
    for (Entity e : world->GetEntitiesWithComponent<PushableComponent>()) {
        if (auto* p = world->GetComponent<PushableComponent>(e)) {
            p->velocity = Math::Vector3(0, 0, 0);
            p->isBeingPushed = false;
            p->pushedBy = 0;
            p->gridMoving = false;
            p->gridMoveProgress = 0.0f;
        }
    }
    m_Started = false;
}

void GameplaySystem::Update(World* world, f32 dt) {
    if (!world || dt <= 0.0f) return;
    if (!m_Started) {
        // Someone ticked without calling OnPlayStart. Rather than run with no rest
        // positions -- which would read every offset as a world position and fling
        // the scene apart -- capture them now and say so once.
        ENJIN_LOG_WARN(Editor, "GameplaySystem::Update before OnPlayStart; "
                               "capturing rest positions now");
        OnPlayStart(world);
    }

    // Order matters. Platforms and conveyors move things, switches and goals read
    // where things ended up, and locks animate from what the switches decided. A
    // goal evaluated before the platform moved would be a frame stale, which on a
    // single-frame win condition is the difference between winning and not.
    UpdateMovingPlatforms(world, dt);
    UpdateConveyors(world, dt);
    UpdatePushables(world, dt);
    UpdateSwitches(world, dt);
    UpdateLocks(world, dt);
    UpdateTeleporters(world, dt);
    UpdateGoalZones(world, dt);
    UpdateSpawnPoints(world, dt);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

bool GameplaySystem::InteractPressed() const {
    if (!m_InputMap) return false;
    return m_InputMap->IsActionPressed(InputSystem::GameAction::Interact);
}

bool GameplaySystem::EntityMatchesTag(World* world, Entity entity,
                                      const std::string& tag) const {
    if (tag.empty()) return true;      // no requirement = anything qualifies
    if (auto* tags = world->GetComponent<TagComponent>(entity)) {
        if (tags->HasTag(tag)) return true;
    }
    // A name match is accepted too, because the inspector's tag field and the
    // entity name are the two places an author writes "crate", and requiring the
    // TagComponent would make the field silently never match for most scenes.
    if (auto* name = world->GetComponent<NameComponent>(entity)) {
        if (name->name == tag) return true;
    }
    return false;
}

bool GameplaySystem::EntityHasKey(World* world, Entity entity,
                                  const std::string& key, bool consume) const {
    if (key.empty()) return true;
    auto* inv = world->GetComponent<InventoryComponent>(entity);
    if (!inv) return false;
    auto it = std::find(inv->keys.begin(), inv->keys.end(), key);
    if (it == inv->keys.end()) return false;
    if (consume) inv->keys.erase(it);
    return true;
}

void GameplaySystem::ApplyLinkedActivation(World* world, Entity source, bool active) {
    auto* sw = world->GetComponent<SwitchComponent>(source);
    if (!sw) return;

    for (Entity target : sw->linkedEntities) {
        if (!world->IsValid(target)) continue;

        // A switch opens a lock without needing its key: the switch IS the key.
        if (auto* lock = world->GetComponent<LockComponent>(target)) {
            if (lock->openMode == LockComponent::OpenMode::OpenOnly) {
                if (active) lock->isOpen = true;
            } else {
                lock->isOpen = active;
            }
            lock->isAnimating = true;
        }
        // A Triggered platform runs only while the switch is on.
        if (auto* platform = world->GetComponent<MovingPlatformComponent>(target)) {
            if (platform->mode == MovingPlatformComponent::PlatformMode::Triggered) {
                platform->isMoving = active;
            }
        }
        // A conveyor linked to a switch runs with it.
        if (auto* conveyor = world->GetComponent<ConveyorComponent>(target)) {
            conveyor->isActive = active;
        }
    }
}

// ---------------------------------------------------------------------------
// Locks and doors
// ---------------------------------------------------------------------------

void GameplaySystem::UpdateLocks(World* world, f32 dt) {
    const bool interact = InteractPressed();

    for (Entity e : world->GetEntitiesWithComponent<LockComponent>()) {
        auto* lock = world->GetComponent<LockComponent>(e);
        auto* transform = world->GetComponent<TransformComponent>(e);
        if (!lock || !transform) continue;

        const auto restIt = m_RestPositions.find(e);
        const Math::Vector3 rest = (restIt != m_RestPositions.end()) ? restIt->second
                                                                    : transform->position;
        const auto restRotIt = m_RestRotations.find(e);
        const Math::Vector3 restRot = (restRotIt != m_RestRotations.end())
                                    ? restRotIt->second
                                    : transform->rotation.ToEulerDegrees();

        // Who is close enough to use it.
        Entity user = 0;
        if (lock->interactRange > 0.0f) {
            for (Entity other : world->GetEntitiesWithComponent<TransformComponent>()) {
                if (other == e || !IsPlayerControlled(world, other)) continue;
                auto* ot = world->GetComponent<TransformComponent>(other);
                if (!ot) continue;
                const Math::Vector3 d(ot->position.x - transform->position.x,
                                      ot->position.y - transform->position.y,
                                      ot->position.z - transform->position.z);
                if (Length(d) <= lock->interactRange) { user = other; break; }
            }
        }

        const bool wantsToOpen = user != 0 && (lock->autoOpen || interact);
        if (wantsToOpen && !lock->isOpen) {
            if (!lock->isLocked ||
                EntityHasKey(world, user, lock->requiredKey, lock->consumeKey)) {
                lock->isLocked = false;
                lock->isOpen = true;
                lock->isAnimating = true;
                if (lock->openMode == LockComponent::OpenMode::Timed) {
                    lock->openTimer = lock->openDuration;
                }
            }
        } else if (wantsToOpen && lock->isOpen && interact &&
                   lock->openMode == LockComponent::OpenMode::Toggle) {
            lock->isOpen = false;
            lock->isAnimating = true;
        }

        if (lock->openMode == LockComponent::OpenMode::Timed && lock->isOpen) {
            lock->openTimer -= dt;
            if (lock->openTimer <= 0.0f) {
                lock->isOpen = false;
                lock->isAnimating = true;
            }
        }

        // Animate toward the target. openSpeed is a lerp rate, so progress moves
        // openSpeed * dt per second and the whole travel takes 1/openSpeed seconds.
        const f32 target = lock->isOpen ? 1.0f : 0.0f;
        if (std::fabs(lock->openProgress - target) > 1e-4f) {
            const f32 step = lock->openSpeed * dt;
            lock->openProgress += (target > lock->openProgress) ? step : -step;
            lock->openProgress = std::max(0.0f, std::min(1.0f, lock->openProgress));
            lock->isAnimating = true;
        } else {
            lock->openProgress = target;
            lock->isAnimating = false;
        }

        // closedPosition / openPosition are offsets from where the door was placed.
        const Math::Vector3 offset = LerpVec(lock->closedPosition, lock->openPosition,
                                          lock->openProgress);
        transform->position = Math::Vector3(rest.x + offset.x, rest.y + offset.y,
                                            rest.z + offset.z);
        const Math::Vector3 rotOffset = LerpVec(lock->closedRotation, lock->openRotation,
                                             lock->openProgress);
        transform->rotation = Math::Quaternion::FromEulerDegrees(
            Math::Vector3(restRot.x + rotOffset.x, restRot.y + rotOffset.y,
                          restRot.z + rotOffset.z));
    }
}

// ---------------------------------------------------------------------------
// Switches and pressure plates
// ---------------------------------------------------------------------------

void GameplaySystem::UpdateSwitches(World* world, f32 dt) {
    const bool interact = InteractPressed();

    for (Entity e : world->GetEntitiesWithComponent<SwitchComponent>()) {
        auto* sw = world->GetComponent<SwitchComponent>(e);
        auto* transform = world->GetComponent<TransformComponent>(e);
        if (!sw || !transform) continue;

        sw->wasActive = sw->isActive;
        const WorldBounds plate = ComputeWorldBounds(world, e);

        switch (sw->type) {
            case SwitchComponent::SwitchType::PressurePlate: {
                bool held = false;
                Entity by = 0;
                for (Entity other : world->GetEntitiesWithComponent<TransformComponent>()) {
                    if (other == e) continue;
                    if (!EntityMatchesTag(world, other,
                                          sw->requireSpecificTag ? sw->requiredTag
                                                                 : std::string())) {
                        continue;
                    }
                    if (!plate.Overlaps(ComputeWorldBounds(world, other))) continue;

                    // Weight, when the author asked for one. A pushable's mass and a
                    // rigidbody's mass are both real answers; an entity with neither
                    // weighs nothing, which is why a 0 threshold means "anything".
                    if (sw->activationWeight > 0.0f) {
                        f32 mass = 0.0f;
                        if (auto* push = world->GetComponent<PushableComponent>(other)) {
                            mass = push->mass;
                        } else if (auto* rb = world->GetComponent<RigidbodyComponent>(other)) {
                            mass = rb->mass;
                        }
                        if (mass < sw->activationWeight) continue;
                    }
                    held = true;
                    by = other;
                    break;
                }
                sw->isActive = held;
                sw->activatedBy = by;
                break;
            }
            case SwitchComponent::SwitchType::Toggle:
            case SwitchComponent::SwitchType::OneShot: {
                if (!interact) break;
                bool inRange = false;
                for (Entity other : world->GetEntitiesWithComponent<TransformComponent>()) {
                    if (other == e || !IsPlayerControlled(world, other)) continue;
                    if (plate.Overlaps(ComputeWorldBounds(world, other))) {
                        inRange = true;
                        sw->activatedBy = other;
                        break;
                    }
                }
                if (!inRange) break;
                if (sw->type == SwitchComponent::SwitchType::OneShot) {
                    sw->isActive = true;
                } else {
                    sw->isActive = !sw->isActive;
                }
                break;
            }
            case SwitchComponent::SwitchType::Timed: {
                if (interact) {
                    bool inRange = false;
                    for (Entity other : world->GetEntitiesWithComponent<TransformComponent>()) {
                        if (other == e || !IsPlayerControlled(world, other)) continue;
                        if (plate.Overlaps(ComputeWorldBounds(world, other))) {
                            inRange = true;
                            break;
                        }
                    }
                    if (inRange) {
                        sw->isActive = true;
                        sw->activeTimer = sw->activeDuration;
                    }
                }
                if (sw->isActive) {
                    sw->activeTimer -= dt;
                    if (sw->activeTimer <= 0.0f) sw->isActive = false;
                }
                break;
            }
            case SwitchComponent::SwitchType::Sequence: {
                // A sequence switch activates only if every LOWER index in its group
                // is already on. Activating out of order resets the whole group,
                // which is the behaviour the field names describe.
                if (!interact) break;
                bool inRange = false;
                for (Entity other : world->GetEntitiesWithComponent<TransformComponent>()) {
                    if (other == e || !IsPlayerControlled(world, other)) continue;
                    if (plate.Overlaps(ComputeWorldBounds(world, other))) {
                        inRange = true;
                        break;
                    }
                }
                if (!inRange) break;

                bool predecessorsOn = true;
                for (Entity other : world->GetEntitiesWithComponent<SwitchComponent>()) {
                    auto* os = world->GetComponent<SwitchComponent>(other);
                    if (!os || other == e) continue;
                    if (os->type != SwitchComponent::SwitchType::Sequence) continue;
                    if (os->sequenceGroup != sw->sequenceGroup) continue;
                    if (os->sequenceIndex < sw->sequenceIndex && !os->isActive) {
                        predecessorsOn = false;
                        break;
                    }
                }
                if (predecessorsOn) {
                    sw->isActive = true;
                } else {
                    for (Entity other : world->GetEntitiesWithComponent<SwitchComponent>()) {
                        auto* os = world->GetComponent<SwitchComponent>(other);
                        if (!os) continue;
                        if (os->type != SwitchComponent::SwitchType::Sequence) continue;
                        if (os->sequenceGroup == sw->sequenceGroup) os->isActive = false;
                    }
                }
                break;
            }
        }

        // Drive what it controls, on the EDGE only, so a held plate does not fight
        // a lock that something else is also driving every frame.
        if (sw->isActive != sw->wasActive) {
            ApplyLinkedActivation(world, e, sw->isActive);
        }

        // The plate's own travel. offPosition / onPosition are offsets from where
        // the plate was placed, same rule as a door.
        const f32 target = sw->isActive ? 1.0f : 0.0f;
        const f32 step = sw->transitionSpeed * dt;
        if (std::fabs(sw->transitionProgress - target) > 1e-4f) {
            sw->transitionProgress += (target > sw->transitionProgress) ? step : -step;
            sw->transitionProgress = std::max(0.0f, std::min(1.0f, sw->transitionProgress));
        } else {
            sw->transitionProgress = target;
        }
        const auto restIt = m_RestPositions.find(e);
        if (restIt != m_RestPositions.end()) {
            const Math::Vector3 offset =
                LerpVec(sw->offPosition, sw->onPosition, sw->transitionProgress);
            transform->position = Math::Vector3(restIt->second.x + offset.x,
                                                restIt->second.y + offset.y,
                                                restIt->second.z + offset.z);
        }
    }
}

// ---------------------------------------------------------------------------
// Goal zones
// ---------------------------------------------------------------------------

void GameplaySystem::UpdateGoalZones(World* world, f32 dt) {
    (void)dt;
    for (Entity e : world->GetEntitiesWithComponent<GoalZoneComponent>()) {
        auto* goal = world->GetComponent<GoalZoneComponent>(e);
        if (!goal) continue;

        const WorldBounds zone = ComputeWorldBounds(world, e);
        bool satisfied = false;
        Entity by = 0;

        for (Entity other : world->GetEntitiesWithComponent<TransformComponent>()) {
            if (other == e) continue;
            if (!zone.Overlaps(ComputeWorldBounds(world, other))) continue;

            switch (goal->type) {
                case GoalZoneComponent::GoalType::PushTarget:
                    if (!world->HasComponent<PushableComponent>(other)) continue;
                    if (!EntityMatchesTag(world, other, goal->requiredTag)) continue;
                    break;
                case GoalZoneComponent::GoalType::StandOn:
                case GoalZoneComponent::GoalType::Checkpoint:
                case GoalZoneComponent::GoalType::LevelExit:
                    if (!IsPlayerControlled(world, other)) continue;
                    break;
                case GoalZoneComponent::GoalType::ItemDeposit: {
                    auto* inv = world->GetComponent<InventoryComponent>(other);
                    if (!inv) continue;
                    bool holds = goal->requiredItem.empty();
                    for (const auto& slot : inv->slots) {
                        if (slot.itemId == goal->requiredItem && slot.quantity > 0) {
                            holds = true;
                            break;
                        }
                    }
                    if (!holds) continue;
                    break;
                }
            }
            satisfied = true;
            by = other;
            break;
        }

        const bool wasSatisfied = goal->isSatisfied;
        goal->isSatisfied = satisfied;
        goal->satisfiedBy = by;

        // A checkpoint fires once, on the edge. Whether it actually saves is the
        // save system's decision: "On Checkpoint" in the Save Debug panel had no
        // consumer anywhere, and nothing automatic had ever called Checkpoint().
        if (satisfied && !wasSatisfied &&
            goal->type == GoalZoneComponent::GoalType::Checkpoint && m_Saves) {
            m_Saves->OnCheckpointReached(world, m_Scenes ? m_Scenes->GetCurrentSceneName()
                                                         : std::string());
        }

        // The level exit fires once, on the edge, and only when every goal in its
        // group is satisfied -- a multi-goal puzzle whose exit triggered on the
        // first goal would end the level early.
        if (satisfied && !wasSatisfied &&
            goal->type == GoalZoneComponent::GoalType::LevelExit) {
            bool groupComplete = true;
            for (Entity other : world->GetEntitiesWithComponent<GoalZoneComponent>()) {
                auto* og = world->GetComponent<GoalZoneComponent>(other);
                if (!og || og->goalGroup != goal->goalGroup) continue;
                if (!og->isSatisfied) { groupComplete = false; break; }
            }
            if (groupComplete && !goal->nextScene.empty()) {
                if (m_Scenes) {
                    m_Scenes->RequestSceneChange(goal->nextScene);
                } else if (!m_WarnedNoSceneManager) {
                    m_WarnedNoSceneManager = true;
                    ENJIN_LOG_WARN(Editor,
                        "GoalZone LevelExit wants scene '%s' but no SceneManager is "
                        "attached to GameplaySystem; the level cannot be completed",
                        goal->nextScene.c_str());
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Conveyors
// ---------------------------------------------------------------------------

void GameplaySystem::UpdateConveyors(World* world, f32 dt) {
    for (Entity e : world->GetEntitiesWithComponent<ConveyorComponent>()) {
        auto* belt = world->GetComponent<ConveyorComponent>(e);
        if (!belt || !belt->isActive) continue;

        const Math::Vector3 dir = Normalized(belt->direction);
        if (Length(dir) < 1e-6f) continue;   // a belt going nowhere moves nothing

        const WorldBounds surface = ComputeWorldBounds(world, e);
        const Math::Vector3 delta(dir.x * belt->speed * dt,
                                  dir.y * belt->speed * dt,
                                  dir.z * belt->speed * dt);

        for (Entity rider : world->GetEntitiesWithComponent<TransformComponent>()) {
            if (rider == e) continue;
            const bool isPlayer = IsPlayerControlled(world, rider);
            const bool isPushable = world->HasComponent<PushableComponent>(rider);
            if (isPlayer && !belt->affectsPlayer) continue;
            if (isPushable && !belt->affectsPushables) continue;
            if (!isPlayer && !isPushable) continue;

            if (surface.Overlaps(ComputeWorldBounds(world, rider))) {
                MoveEntity(world, rider, delta);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Teleporters
// ---------------------------------------------------------------------------

void GameplaySystem::UpdateTeleporters(World* world, f32 dt) {
    for (Entity e : world->GetEntitiesWithComponent<TeleporterComponent>()) {
        auto* tp = world->GetComponent<TeleporterComponent>(e);
        if (!tp) continue;

        if (tp->cooldownTimer > 0.0f) {
            tp->cooldownTimer -= dt;
            continue;
        }

        const WorldBounds pad = ComputeWorldBounds(world, e);
        for (Entity traveller : world->GetEntitiesWithComponent<TransformComponent>()) {
            if (traveller == e) continue;
            if (!EntityMatchesTag(world, traveller, tp->requiredTag)) continue;
            if (!pad.Overlaps(ComputeWorldBounds(world, traveller))) continue;

            Math::Vector3 destination = tp->targetPosition;
            Math::Vector3 destRotation = tp->targetRotation;
            if (tp->linkedTeleporter != 0 && world->IsValid(tp->linkedTeleporter)) {
                if (auto* lt = world->GetComponent<TransformComponent>(tp->linkedTeleporter)) {
                    destination = lt->position;
                    destRotation = lt->rotation.ToEulerDegrees();
                }
                // Put the far end on cooldown too, or the traveller arrives inside
                // it and is sent straight back -- the classic two-teleporter loop.
                if (auto* linked = world->GetComponent<TeleporterComponent>(tp->linkedTeleporter)) {
                    linked->cooldownTimer = std::max(linked->cooldown, tp->cooldown);
                }
            }

            if (auto* t = world->GetComponent<TransformComponent>(traveller)) {
                t->position = destination;
                t->rotation = Math::Quaternion::FromEulerDegrees(destRotation);
            }
            if (!tp->preserveVelocity) {
                if (auto* rb = world->GetComponent<RigidbodyComponent>(traveller)) {
                    rb->velocity = Math::Vector3(0, 0, 0);
                    rb->angularVelocity = Math::Vector3(0, 0, 0);
                }
                if (auto* push = world->GetComponent<PushableComponent>(traveller)) {
                    push->velocity = Math::Vector3(0, 0, 0);
                }
            }
            tp->cooldownTimer = tp->cooldown;
            break;   // one traveller per activation
        }
    }
}

// ---------------------------------------------------------------------------
// Moving platforms
// ---------------------------------------------------------------------------

void GameplaySystem::UpdateMovingPlatforms(World* world, f32 dt) {
    for (Entity e : world->GetEntitiesWithComponent<MovingPlatformComponent>()) {
        auto* platform = world->GetComponent<MovingPlatformComponent>(e);
        auto* transform = world->GetComponent<TransformComponent>(e);
        if (!platform || !transform) continue;
        if (platform->waypoints.size() < 2 || !platform->isMoving) continue;

        if (platform->isWaiting) {
            platform->waitTimer -= dt;
            if (platform->waitTimer > 0.0f) continue;
            platform->isWaiting = false;
        }

        const auto restIt = m_RestPositions.find(e);
        const Math::Vector3 rest = (restIt != m_RestPositions.end()) ? restIt->second
                                                                    : Math::Vector3(0, 0, 0);

        const int count = static_cast<int>(platform->waypoints.size());
        int from = platform->currentWaypoint;
        int to = from + platform->direction;

        if (to >= count || to < 0) {
            switch (platform->mode) {
                case MovingPlatformComponent::PlatformMode::Loop:
                    to = (to >= count) ? 0 : count - 1;
                    break;
                case MovingPlatformComponent::PlatformMode::PingPong:
                    platform->direction = -platform->direction;
                    to = from + platform->direction;
                    if (to >= count || to < 0) continue;   // single waypoint
                    break;
                case MovingPlatformComponent::PlatformMode::OneWay:
                case MovingPlatformComponent::PlatformMode::Triggered:
                    platform->isMoving = false;
                    continue;
            }
        }

        // Waypoints are offsets from where the platform was placed, so a platform
        // authored anywhere in the scene follows the shape the author drew rather
        // than jumping to the origin.
        const Math::Vector3 a = platform->waypoints[static_cast<usize>(from)];
        const Math::Vector3 b = platform->waypoints[static_cast<usize>(to)];
        const f32 legLength = Length(Math::Vector3(b.x - a.x, b.y - a.y, b.z - a.z));

        const Math::Vector3 before = transform->position;
        if (legLength < 1e-5f) {
            platform->moveProgress = 1.0f;
        } else {
            platform->moveProgress += (platform->speed * dt) / legLength;
        }

        // Place it from the CLAMPED progress before the arrival bookkeeping resets
        // it. Resetting first and then lerping ran the leg at t=0, so the platform
        // jumped back to the waypoint it had just left on every single arrival.
        const f32 placeAt = std::min(1.0f, platform->moveProgress);
        if (platform->moveProgress >= 1.0f) {
            platform->moveProgress = 0.0f;
            platform->currentWaypoint = to;
            if (platform->waitTime > 0.0f) {
                platform->isWaiting = true;
                platform->waitTimer = platform->waitTime;
            }
        }

        const Math::Vector3 offset = LerpVec(a, b, placeAt);
        transform->position = Math::Vector3(rest.x + offset.x, rest.y + offset.y,
                                            rest.z + offset.z);

        // Carry whatever is standing on it, by the same delta. Without this a
        // player rides for one frame and is left behind, which looks like the
        // platform sliding out from under them.
        if (platform->carryEntities) {
            const Math::Vector3 delta(transform->position.x - before.x,
                                      transform->position.y - before.y,
                                      transform->position.z - before.z);
            if (Length(delta) > 1e-6f) {
                WorldBounds top = ComputeWorldBounds(world, e);
                // Only the top face carries: extend upward a little, and do not
                // reach down through the platform and drag things under it.
                top.min.y = top.max.y - 0.05f;
                top.max.y += 1.0f;
                for (Entity rider : world->GetEntitiesWithComponent<TransformComponent>()) {
                    if (rider == e) continue;
                    if (!IsPlayerControlled(world, rider) &&
                        !world->HasComponent<PushableComponent>(rider)) {
                        continue;
                    }
                    if (top.Overlaps(ComputeWorldBounds(world, rider))) {
                        MoveEntity(world, rider, delta);
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Pushables
// ---------------------------------------------------------------------------

void GameplaySystem::UpdatePushables(World* world, f32 dt) {
    for (Entity e : world->GetEntitiesWithComponent<PushableComponent>()) {
        auto* push = world->GetComponent<PushableComponent>(e);
        auto* transform = world->GetComponent<TransformComponent>(e);
        if (!push || !transform) continue;

        // Grid mode: a move in progress finishes before anything else can start
        // one, which is what makes a Sokoban push feel like one square.
        if (push->gridSnap && push->gridMoving) {
            push->gridMoveProgress += push->gridMoveSpeed * dt;
            if (push->gridMoveProgress >= 1.0f) {
                push->gridMoveProgress = 1.0f;
                push->gridMoving = false;
                push->isBeingPushed = false;
                push->pushedBy = 0;
            }
            transform->position = LerpVec(push->gridMoveStart, push->gridMoveTarget,
                                       push->gridMoveProgress);
            continue;
        }

        // Who is pushing. A player overlapping the crate and moving toward it is
        // pushing it; the direction comes from where they are relative to it,
        // which needs no contact normal from the physics backend.
        Entity pusher = 0;
        Math::Vector3 pushDir(0, 0, 0);
        const WorldBounds crate = ComputeWorldBounds(world, e);
        for (Entity other : world->GetEntitiesWithComponent<TransformComponent>()) {
            if (other == e || !IsPlayerControlled(world, other)) continue;
            if (!crate.Overlaps(ComputeWorldBounds(world, other))) continue;
            auto* ot = world->GetComponent<TransformComponent>(other);
            if (!ot) continue;
            pushDir = Normalized(Math::Vector3(transform->position.x - ot->position.x,
                                               0.0f,
                                               transform->position.z - ot->position.z));
            if (Length(pushDir) > 1e-5f) { pusher = other; }
            break;
        }

        push->isBeingPushed = (pusher != 0);
        push->pushedBy = pusher;

        if (pusher != 0) {
            // Constraints are per axis, and "pushable off a ledge" is not handled
            // here: gravity belongs to the physics backend, and faking it would
            // fight it.
            Math::Vector3 allowed(push->pushableX ? pushDir.x : 0.0f,
                                  push->pushableY ? pushDir.y : 0.0f,
                                  push->pushableZ ? pushDir.z : 0.0f);

            if (push->gridSnap) {
                // Snap to the dominant axis: a Sokoban crate moves one square in one
                // direction, never diagonally.
                if (std::fabs(allowed.x) >= std::fabs(allowed.z)) {
                    allowed = Math::Vector3(allowed.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
                } else {
                    allowed = Math::Vector3(0.0f, 0.0f, allowed.z > 0.0f ? 1.0f : -1.0f);
                }
                push->gridMoveStart = transform->position;
                push->gridMoveTarget =
                    Math::Vector3(transform->position.x + allowed.x * push->gridCellSize,
                                  transform->position.y,
                                  transform->position.z + allowed.z * push->gridCellSize);
                push->gridMoveProgress = 0.0f;
                push->gridMoving = true;
                continue;
            }

            // Heavier is slower, which is what `mass` says it does. A zero or
            // negative mass would divide by nothing, so it is floored.
            const f32 mass = std::max(0.01f, push->mass);
            const f32 speed = push->pushSpeed / mass;
            push->velocity = Math::Vector3(allowed.x * speed, allowed.y * speed,
                                           allowed.z * speed);
        }

        // Friction is a per-frame damping factor in the component, so it is applied
        // per frame. At 60fps a friction of 0.9 keeps 0.9 of the speed each frame.
        if (Length(push->velocity) > 1e-4f) {
            MoveEntity(world, e, Math::Vector3(push->velocity.x * dt,
                                               push->velocity.y * dt,
                                               push->velocity.z * dt));
            push->velocity = Math::Vector3(push->velocity.x * push->friction,
                                           push->velocity.y * push->friction,
                                           push->velocity.z * push->friction);
        } else {
            push->velocity = Math::Vector3(0, 0, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Spawn points
// ---------------------------------------------------------------------------

void GameplaySystem::UpdateSpawnPoints(World* world, f32 dt) {
    for (Entity e : world->GetEntitiesWithComponent<SpawnPointComponent>()) {
        auto* spawn = world->GetComponent<SpawnPointComponent>(e);
        if (!spawn) continue;
        if (m_GaveUp.count(e)) continue;   // already reported; do not repeat it

        // Forget anything that has since been destroyed, so respawnTime measures
        // from the death rather than from the last spawn.
        const usize before = spawn->spawnedEntities.size();
        spawn->spawnedEntities.erase(
            std::remove_if(spawn->spawnedEntities.begin(), spawn->spawnedEntities.end(),
                           [world](Entity s) { return !world->IsValid(s); }),
            spawn->spawnedEntities.end());
        const bool lostOne = spawn->spawnedEntities.size() < before;

        if (lostOne && spawn->respawnTime > 0.0f && spawn->spawnTimer < 0.0f) {
            spawn->spawnTimer = spawn->respawnTime;
        }
        if (spawn->spawnTimer < 0.0f) continue;

        spawn->spawnTimer -= dt;
        if (spawn->spawnTimer > 0.0f) continue;
        spawn->spawnTimer = -1.0f;

        if (spawn->maxSpawns >= 0 && spawn->currentSpawns >= spawn->maxSpawns) continue;
        if (spawn->prefabToSpawn.empty()) {
            // Nothing to spawn is not a crash, but it IS the author having left the
            // field blank, and a spawn point that produces nothing forever with no
            // word is exactly the silence this whole sweep is about.
            ENJIN_LOG_WARN(Editor, "SpawnPoint '%s' has no prefab set; nothing spawned",
                           spawn->spawnId.c_str());
            m_GaveUp.insert(e);
            continue;
        }

        Math::Vector3 at(0, 0, 0);
        if (auto* t = world->GetComponent<TransformComponent>(e)) at = t->position;
        if (spawn->spawnRadius > 0.0f) {
            // Uniform over the DISC, not over (angle, radius) -- the naive version
            // clusters everything at the centre, which reads as a broken radius.
            const f32 angle = m_Random.Next01() * 6.2831853f;
            const f32 radius = spawn->spawnRadius * std::sqrt(m_Random.Next01());
            at.x += std::cos(angle) * radius;
            at.z += std::sin(angle) * radius;
        }
        const Math::Vector3 rotation(
            0.0f, spawn->randomRotation ? m_Random.Next01() * 360.0f : 0.0f, 0.0f);

        // The prefab, actually instantiated. An empty entity named after the prefab
        // would be the very failure this file exists to remove: the spawn count goes
        // up, the list fills, and there is nothing in the scene.
        auto prefab = Assets::PrefabManager::Get().LoadPrefab(spawn->prefabToSpawn);
        if (!prefab) {
            ENJIN_LOG_WARN(Editor,
                "SpawnPoint '%s': cannot load prefab '%s'; nothing spawned",
                spawn->spawnId.c_str(), spawn->prefabToSpawn.c_str());
            m_GaveUp.insert(e);
            continue;
        }

        // Structural mutation, main thread only (adr-0004).
        Entity created = Assets::PrefabManager::Get().Instantiate(world, *prefab, at,
                                                                  rotation);
        if (created == INVALID_ENTITY) {
            ENJIN_LOG_WARN(Editor, "SpawnPoint '%s': prefab '%s' instantiated nothing",
                           spawn->spawnId.c_str(), spawn->prefabToSpawn.c_str());
            continue;
        }

        // Tagged transient so a save taken mid-play cannot bake spawned entities
        // into the authored scene file.
        world->AddComponent<TransientComponent>(created, TransientComponent{});

        spawn->spawnedEntities.push_back(created);
        spawn->currentSpawns++;
        m_Spawned.push_back(created);
    }
}

} // namespace ECS
} // namespace Enjin
