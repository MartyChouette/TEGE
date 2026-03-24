#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Math/Math.h"
#include <algorithm>

namespace Enjin {
namespace Gameplay {
namespace GameplayLoop {

void ProcessContactDamage(ECS::World* world, ECS::Entity entityA,
                          ECS::Entity entityB,
                          std::vector<ECS::Entity>& deferredDestroys) {
    if (!world) return;

    auto tryDamage = [&](ECS::Entity damager, ECS::Entity target) {
        auto* dmg = world->GetComponent<ECS::DamageComponent>(damager);
        auto* hp = world->GetComponent<ECS::HealthComponent>(target);
        if (!dmg || !hp) {
            return;  // No damage or health component — skip
        }
        if (hp->isDead) return;

        // Mario-style stomp check: if the TARGET (player) is above the DAMAGER
        // (enemy) and falling, it's a stomp — kill the enemy instead of damaging
        // the player. Only stompable if the damager HAS a HealthComponent (enemies).
        // Hazards (spikes, lava) have DamageComponent but no HealthComponent —
        // they always damage the player and cannot be stomped.
        auto* targetCtrl = world->GetComponent<ECS::Platformer2DController>(target);
        auto* enemyHp = world->GetComponent<ECS::HealthComponent>(damager);
        if (targetCtrl && enemyHp && targetCtrl->velocity.y < -1.0f) {
            auto* targetT = world->GetComponent<ECS::TransformComponent>(target);
            auto* damagerT = world->GetComponent<ECS::TransformComponent>(damager);
            if (targetT && damagerT && targetT->position.y > damagerT->position.y + 0.3f) {
                // Stomp! Kill the enemy, bounce the player
                enemyHp->currentHealth = 0.0f;
                enemyHp->isDead = true;
                deferredDestroys.push_back(damager);
                ENJIN_LOG_INFO(Game, "STOMP: entity %llu stomped entity %llu",
                    (unsigned long long)target, (unsigned long long)damager);
                // Bounce the player upward (small hop after stomp)
                targetCtrl->velocity.y = targetCtrl->jumpForce * 0.6f;
                targetCtrl->isGrounded = false;
                return;  // Stomp replaces damage — player is not hurt
            }
        }

        // Check damageOnce -- skip if already damaged this entity
        if (dmg->damageOnce) {
            for (auto e : dmg->damagedEntities) {
                if (e == target) return;
            }
            dmg->damagedEntities.push_back(target);
        }

        // Check invulnerability
        if (hp->isInvulnerable || hp->invulnerabilityTimer > 0.0f) return;

        // Apply damage (shield absorbs first)
        f32 remaining = dmg->damage;
        if (hp->currentShield > 0.0f) {
            f32 absorbed = Math::Min(remaining, hp->currentShield);
            hp->currentShield -= absorbed;
            remaining -= absorbed;
        }
        hp->currentHealth -= remaining;
        hp->timeSinceLastDamage = 0.0f;
        // Diagnostic: log damage application for debugging sensor interactions
        ENJIN_LOG_INFO(Game, "DAMAGE: entity %llu dealt %.1f to entity %llu (hp: %.1f/%.1f)",
            (unsigned long long)damager, dmg->damage,
            (unsigned long long)target, hp->currentHealth, hp->maxHealth);

        // Start invulnerability window
        if (hp->invulnerabilityTime > 0.0f) {
            hp->invulnerabilityTimer = hp->invulnerabilityTime;
        }

        // Knockback (apply to character controller velocity if present)
        if (dmg->knockbackForce > 0.0f) {
            auto* ctrl = world->GetComponent<ECS::Platformer2DController>(target);
            if (ctrl) {
                auto* dmgT = world->GetComponent<ECS::TransformComponent>(damager);
                auto* tgtT = world->GetComponent<ECS::TransformComponent>(target);
                if (dmgT && tgtT) {
                    f32 dir = (tgtT->position.x > dmgT->position.x) ? 1.0f : -1.0f;
                    ctrl->velocity.x = dir * dmg->knockbackForce;
                    ctrl->velocity.y = dmg->knockbackForce * 0.5f;
                    ctrl->isGrounded = false;
                }
            }
        }

        // Check death
        if (hp->currentHealth <= 0.0f) {
            hp->currentHealth = 0.0f;
            hp->isDead = true;
        }

        // Destroy damager if configured
        if (dmg->destroyOnHit) {
            deferredDestroys.push_back(damager);
        }
    };

    tryDamage(entityA, entityB);
    tryDamage(entityB, entityA);
}

void ProcessPickup(ECS::World* world, ECS::Entity entityA,
                   ECS::Entity entityB,
                   std::vector<ECS::Entity>& deferredDestroys) {
    if (!world) return;

    auto tryPickup = [&](ECS::Entity pickupEntity, ECS::Entity collector) {
        auto* pickup = world->GetComponent<ECS::PickupComponent>(pickupEntity);
        if (!pickup || pickup->isCollected) return;

        // Only characters with a controller or health can collect pickups
        bool isPlayer = world->GetComponent<ECS::Platformer2DController>(collector) != nullptr ||
                        world->GetComponent<ECS::TopDown2DController>(collector) != nullptr ||
                        world->GetComponent<ECS::FirstPersonController>(collector) != nullptr ||
                        world->GetComponent<ECS::ThirdPersonController>(collector) != nullptr;
        if (!isPlayer) return;

        // Apply pickup effect
        switch (pickup->type) {
            case ECS::PickupComponent::PickupType::Health: {
                auto* hp = world->GetComponent<ECS::HealthComponent>(collector);
                if (hp) {
                    hp->currentHealth = Math::Min(hp->currentHealth + pickup->value, hp->maxHealth);
                }
                break;
            }
            case ECS::PickupComponent::PickupType::Coin:
            case ECS::PickupComponent::PickupType::Ammo:
            case ECS::PickupComponent::PickupType::Key:
            case ECS::PickupComponent::PickupType::Powerup:
            case ECS::PickupComponent::PickupType::Custom:
                // These are handled by scripts/visual scripts via OnTriggerEnter
                // For now, mark as collected so it disappears
                break;
        }

        pickup->isCollected = true;

        if (pickup->destroyOnPickup && !pickup->canRespawn) {
            deferredDestroys.push_back(pickupEntity);
        } else if (pickup->destroyOnPickup) {
            // Hide but keep for respawn -- make invisible
            auto* transform = world->GetComponent<ECS::TransformComponent>(pickupEntity);
            if (transform) transform->visible = false;
            pickup->respawnTimer = pickup->respawnTime;
        }
    };

    tryPickup(entityA, entityB);
    tryPickup(entityB, entityA);
}

// Per-frame AABB overlap check for damage hazards (spikes, lava).
// Box2D v3's sensor system doesn't reliably detect overlaps between
// stationary kinematic sensors and moving kinematic visitors. This
// manual check runs every frame and catches what sensors miss.
void CheckHazardOverlaps(ECS::World* world, f32 deltaTime,
                          std::vector<ECS::Entity>& deferredDestroys) {
    if (!world) return;

    // Find all player entities (entities with a controller + health)
    for (auto player : world->GetEntitiesWithComponent<ECS::Platformer2DController>()) {
        auto* playerT = world->GetComponent<ECS::TransformComponent>(player);
        auto* playerHp = world->GetComponent<ECS::HealthComponent>(player);
        if (!playerT || !playerHp || playerHp->isDead) continue;
        if (playerHp->isInvulnerable || playerHp->invulnerabilityTimer > 0.0f) continue;

        auto* playerCtrl = world->GetComponent<ECS::Platformer2DController>(player);
        f32 pr = playerCtrl ? playerCtrl->collisionRadius : 0.4f;
        f32 ph = playerCtrl ? playerCtrl->collisionHeight * 0.5f : 0.8f;

        // Check against all entities with DamageComponent but no HealthComponent (hazards)
        for (auto hazard : world->GetEntitiesWithComponent<ECS::DamageComponent>()) {
            if (world->GetComponent<ECS::HealthComponent>(hazard)) continue;  // Skip enemies (handled by stomp/sensor)

            auto* hazardT = world->GetComponent<ECS::TransformComponent>(hazard);
            auto* hazardDmg = world->GetComponent<ECS::DamageComponent>(hazard);
            auto* hazardBody = world->GetComponent<Physics::Body2DComponent>(hazard);
            if (!hazardT || !hazardDmg || !hazardBody) continue;

            // Get hazard AABB from Body2DComponent
            f32 hx = 0.5f, hy = 0.5f;
            if (hazardBody->shapeType == Physics::Shape2DType::Box) {
                hx = hazardBody->box.halfExtents.x;
                hy = hazardBody->box.halfExtents.y;
            } else if (hazardBody->shapeType == Physics::Shape2DType::Polygon && !hazardBody->polygon.vertices.empty()) {
                // Use bounding box of polygon vertices
                hx = hy = 0;
                for (const auto& v : hazardBody->polygon.vertices) {
                    hx = Math::Max(hx, Math::Abs(v.x));
                    hy = Math::Max(hy, Math::Abs(v.y));
                }
            }

            // AABB overlap test (player capsule vs hazard box)
            f32 dx = Math::Abs(playerT->position.x - hazardT->position.x);
            f32 dy = Math::Abs(playerT->position.y - hazardT->position.y);
            if (dx < pr + hx && dy < ph + hy) {
                // Overlap — apply damage
                if (hazardDmg->damageOnce) {
                    bool alreadyHit = false;
                    for (auto e : hazardDmg->damagedEntities) {
                        if (e == player) { alreadyHit = true; break; }
                    }
                    if (alreadyHit) continue;
                    hazardDmg->damagedEntities.push_back(player);
                }

                f32 remaining = hazardDmg->damage;
                if (playerHp->currentShield > 0.0f) {
                    f32 absorbed = Math::Min(remaining, playerHp->currentShield);
                    playerHp->currentShield -= absorbed;
                    remaining -= absorbed;
                }
                playerHp->currentHealth -= remaining;
                playerHp->timeSinceLastDamage = 0.0f;
                ENJIN_LOG_INFO(Game, "HAZARD: entity %llu dealt %.1f to player %llu (hp: %.1f/%.1f)",
                    (unsigned long long)hazard, hazardDmg->damage,
                    (unsigned long long)player, playerHp->currentHealth, playerHp->maxHealth);

                if (playerHp->invulnerabilityTime > 0.0f) {
                    playerHp->invulnerabilityTimer = playerHp->invulnerabilityTime;
                }

                // Knockback: push player away from hazard
                if (hazardDmg->knockbackForce > 0.0f && playerCtrl) {
                    f32 dir = (playerT->position.x > hazardT->position.x) ? 1.0f : -1.0f;
                    playerCtrl->velocity.x = dir * hazardDmg->knockbackForce;
                    playerCtrl->velocity.y = hazardDmg->knockbackForce * 0.5f;
                    playerCtrl->isGrounded = false;
                }

                if (playerHp->currentHealth <= 0.0f) {
                    playerHp->currentHealth = 0.0f;
                    playerHp->isDead = true;
                }
            }
        }
    }
}

// 3D pickup AABB overlap — CharacterVirtual doesn't fire collision events
// with static bodies, so we check manually each frame.
void CheckPickupOverlaps3D(ECS::World* world,
                            std::vector<ECS::Entity>& deferredDestroys) {
    if (!world) return;

    // Collect all player entities (any 3D controller type)
    auto checkPlayer = [&](ECS::Entity player) {
        auto* playerT = world->GetComponent<ECS::TransformComponent>(player);
        if (!playerT) return;

        // Player AABB half-extents (capsule approximation)
        f32 pr = 0.5f, ph = 1.0f;
        auto* cap = world->GetComponent<ECS::CapsuleColliderComponent>(player);
        if (cap) { pr = cap->radius; ph = cap->height * 0.5f + cap->radius; }

        for (auto pickup : world->GetEntitiesWithComponent<ECS::PickupComponent>()) {
            auto* pk = world->GetComponent<ECS::PickupComponent>(pickup);
            if (!pk || pk->isCollected) continue;

            auto* pickupT = world->GetComponent<ECS::TransformComponent>(pickup);
            if (!pickupT) continue;

            // Use the PickupComponent's pickupRange (default 1.0) for detection distance.
            // This is more intuitive than relying on collider radius.
            f32 pickupR = pk->pickupRange;

            // 3D distance check
            Math::Vector3 diff = playerT->position - pickupT->position;
            f32 dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            if (dist < pr + pickupR) {
                // Overlap — process pickup
                ProcessPickup(world, pickup, player, deferredDestroys);
            }
        }
    };

    for (auto e : world->GetEntitiesWithComponent<ECS::ThirdPersonController>()) checkPlayer(e);
    for (auto e : world->GetEntitiesWithComponent<ECS::FirstPersonController>()) checkPlayer(e);
    for (auto e : world->GetEntitiesWithComponent<ECS::TopDown3DController>()) checkPlayer(e);
}

void UpdateHealthSystems(ECS::World* world, f32 deltaTime,
                         std::vector<ECS::Entity>& deferredDestroys) {
    if (!world) return;

    for (auto entity : world->GetEntitiesWithComponent<ECS::HealthComponent>()) {
        auto* hp = world->GetComponent<ECS::HealthComponent>(entity);
        if (!hp) continue;

        // Update invulnerability timer
        if (hp->invulnerabilityTimer > 0.0f) {
            hp->invulnerabilityTimer -= deltaTime;
        }

        // Track time since last damage (for regen delay)
        hp->timeSinceLastDamage += deltaTime;

        // Health regeneration
        if (!hp->isDead && hp->regenRate > 0.0f && hp->timeSinceLastDamage >= hp->regenDelay) {
            hp->currentHealth = Math::Min(hp->currentHealth + hp->regenRate * deltaTime, hp->maxHealth);
        }

        // Shield regeneration
        if (!hp->isDead && hp->shieldRegenRate > 0.0f && hp->timeSinceLastDamage >= hp->shieldRegenDelay) {
            hp->currentShield = Math::Min(hp->currentShield + hp->shieldRegenRate * deltaTime, hp->maxShield);
        }

        // Death handling -- destroy non-player entities, respawn players
        if (hp->isDead) {
            // Ragdoll activation on death: if entity has a RagdollComponent with
            // autoActivateOnDeath, activate it instead of immediately destroying.
            auto* ragdoll = world->GetComponent<ECS::RagdollComponent>(entity);
            if (ragdoll && ragdoll->autoActivateOnDeath && !ragdoll->enabled) {
                ragdoll->enabled = true;
                ragdoll->blendProgress = 0.0f;
                ragdoll->blendWeight = 0.0f;
                ragdoll->settleTimer = 0.0f;

                // Stop animator
                auto* animComp = world->GetComponent<ECS::AnimatorComponent>(entity);
                if (animComp) {
                    ragdoll->wasAnimating = animComp->animator.IsPlaying();
                    animComp->animator.Stop();
                }
            }

            // Check if any player controller is present
            bool isPlayer = world->GetComponent<ECS::Platformer2DController>(entity) ||
                            world->GetComponent<ECS::TopDown2DController>(entity) ||
                            world->GetComponent<ECS::FirstPersonController>(entity) ||
                            world->GetComponent<ECS::ThirdPersonController>(entity);

            if (isPlayer) {
                // If a GameOverComponent exists, skip auto-respawn — let the
                // game over system handle the defeat state instead.
                bool hasGameOver = false;
                for (auto goEntity : world->GetEntitiesWithComponent<ECS::GameOverComponent>()) {
                    (void)goEntity;
                    hasGameOver = true;
                    break;
                }

                if (!hasGameOver) {
                    // Legacy behaviour: respawn at Y=2 above origin
                    hp->isDead = false;
                    hp->currentHealth = hp->maxHealth;
                    hp->currentShield = hp->maxShield;
                    hp->invulnerabilityTimer = 1.0f;

                    for (auto dmgEntity : world->GetEntitiesWithComponent<ECS::DamageComponent>()) {
                        auto* dmg = world->GetComponent<ECS::DamageComponent>(dmgEntity);
                        if (dmg) dmg->damagedEntities.clear();
                    }
                    auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
                    if (transform) {
                        transform->position = Math::Vector3(0.0f, 2.0f, 0.0f);
                    }
                    auto* ctrl = world->GetComponent<ECS::Platformer2DController>(entity);
                    if (ctrl) {
                        ctrl->velocity = Math::Vector3(0.0f);
                        ctrl->isGrounded = false;
                    }
                }
            } else {
                // Non-player death: destroy entity (unless ragdoll is active)
                if (!ragdoll || !ragdoll->enabled) {
                    deferredDestroys.push_back(entity);
                }
            }
        }
    }

    // Pickup respawn
    for (auto entity : world->GetEntitiesWithComponent<ECS::PickupComponent>()) {
        auto* pickup = world->GetComponent<ECS::PickupComponent>(entity);
        if (!pickup || !pickup->isCollected || !pickup->canRespawn) continue;

        pickup->respawnTimer -= deltaTime;
        if (pickup->respawnTimer <= 0.0f) {
            pickup->isCollected = false;
            auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
            if (transform) transform->visible = true;
        }
    }
}

void FlushDeferredDestroys(ECS::World* world,
                           std::vector<ECS::Entity>& deferredDestroys) {
    if (!world || deferredDestroys.empty()) return;

    for (auto entity : deferredDestroys) {
        // Verify entity still exists before destroying
        if (world->GetComponent<ECS::TransformComponent>(entity)) {
            world->DestroyEntity(entity);
        }
    }
    deferredDestroys.clear();
}

void DispatchCollisionEvents3D(ECS::World* world,
                               Physics::IPhysicsBackend* physics,
                               ECS::VisualScriptSystem* vsSystem,
                               f32 deltaTime,
                               std::vector<ECS::Entity>& deferredDestroys) {
    if (!physics) return;

    const auto& collisionEvents = physics->GetPendingCollisionEvents();
    for (const auto& evt : collisionEvents) {
        if (evt.isTrigger) {
            if (evt.type == Physics::CollisionEvent::Type::Enter) {
                if (vsSystem) {
                    vsSystem->OnTriggerEnter(evt.entityA, evt.entityB, deltaTime);
                    vsSystem->OnTriggerEnter(evt.entityB, evt.entityA, deltaTime);
                }
                ProcessContactDamage(world, evt.entityA, evt.entityB, deferredDestroys);
                ProcessPickup(world, evt.entityA, evt.entityB, deferredDestroys);
            } else {
                if (vsSystem) {
                    vsSystem->OnTriggerExit(evt.entityA, evt.entityB, deltaTime);
                    vsSystem->OnTriggerExit(evt.entityB, evt.entityA, deltaTime);
                }
            }
        } else {
            if (evt.type == Physics::CollisionEvent::Type::Enter) {
                if (vsSystem) {
                    vsSystem->OnCollisionEnter(evt.entityA, evt.entityB, deltaTime);
                    vsSystem->OnCollisionEnter(evt.entityB, evt.entityA, deltaTime);
                }
                ProcessContactDamage(world, evt.entityA, evt.entityB, deferredDestroys);
                ProcessPickup(world, evt.entityA, evt.entityB, deferredDestroys);
            } else {
                if (vsSystem) {
                    vsSystem->OnCollisionExit(evt.entityA, evt.entityB, deltaTime);
                    vsSystem->OnCollisionExit(evt.entityB, evt.entityA, deltaTime);
                }
            }
        }
    }
    physics->ClearPendingCollisionEvents();
}

void Wire2DCollisionCallbacks(Physics::IPhysicsBackend2D* physics2D,
                              ECS::World* world,
                              ECS::VisualScriptSystem* vsSystem,
                              std::vector<ECS::Entity>& deferredDestroys) {
    if (!physics2D) return;

    physics2D->SetOnCollisionEnter([world, vsSystem, &deferredDestroys](const Physics::Contact2D& c) {
        if (vsSystem) {
            vsSystem->OnCollisionEnter(c.entityA, c.entityB, 0.0f);
            vsSystem->OnCollisionEnter(c.entityB, c.entityA, 0.0f);
        }
        ProcessContactDamage(world, c.entityA, c.entityB, deferredDestroys);
        ProcessPickup(world, c.entityA, c.entityB, deferredDestroys);
    });
    physics2D->SetOnCollisionExit([vsSystem](const Physics::Contact2D& c) {
        if (vsSystem) {
            vsSystem->OnCollisionExit(c.entityA, c.entityB, 0.0f);
            vsSystem->OnCollisionExit(c.entityB, c.entityA, 0.0f);
        }
    });
    physics2D->SetOnSensorEnter([world, vsSystem, &deferredDestroys](const Physics::Contact2D& c) {
        if (vsSystem) {
            vsSystem->OnTriggerEnter(c.entityA, c.entityB, 0.0f);
            vsSystem->OnTriggerEnter(c.entityB, c.entityA, 0.0f);
        }
        ProcessContactDamage(world, c.entityA, c.entityB, deferredDestroys);
        ProcessPickup(world, c.entityA, c.entityB, deferredDestroys);
    });
    physics2D->SetOnSensorExit([vsSystem](const Physics::Contact2D& c) {
        if (vsSystem) {
            vsSystem->OnTriggerExit(c.entityA, c.entityB, 0.0f);
            vsSystem->OnTriggerExit(c.entityB, c.entityA, 0.0f);
        }
    });
}

bool UpdateGameOverState(ECS::World* world, f32 deltaTime) {
    if (!world) return false;

    // Find all GameOverComponent entities
    for (auto entity : world->GetEntitiesWithComponent<ECS::GameOverComponent>()) {
        auto* go = world->GetComponent<ECS::GameOverComponent>(entity);
        if (!go) continue;

        // Already triggered — tick the delay timer
        if (go->triggered) {
            if (!go->screenVisible) {
                go->delayTimer += deltaTime;
                if (go->delayTimer >= go->delay) {
                    go->screenVisible = true;
                    return true;  // Signal caller to show game over UI
                }
            }
            // Screen already visible — keep returning true
            return go->screenVisible;
        }

        // --- Defeat check: any player entity is dead ---
        bool playerDead = false;
        auto check = [&](auto entities) {
            for (auto e : entities) {
                auto* hp = world->GetComponent<ECS::HealthComponent>(e);
                if (hp && hp->isDead) {
                    playerDead = true;
                    return;
                }
            }
        };
        check(world->GetEntitiesWithComponent<ECS::Platformer2DController>());
        if (!playerDead) check(world->GetEntitiesWithComponent<ECS::TopDown2DController>());
        if (!playerDead) check(world->GetEntitiesWithComponent<ECS::FirstPersonController>());
        if (!playerDead) check(world->GetEntitiesWithComponent<ECS::ThirdPersonController>());

        if (playerDead) {
            go->triggered = true;
            go->won = false;
            go->delayTimer = 0.0f;
            go->screenVisible = false;
            ENJIN_LOG_INFO(Game, "Game Over triggered: DEFEAT");
            continue;  // Let the delay tick next frame
        }

        // --- Victory check 1: all enemies defeated ---
        if (go->victoryOnAllEnemiesDefeated) {
            bool anyEnemyAlive = false;
            for (auto dmgEntity : world->GetEntitiesWithComponent<ECS::DamageComponent>()) {
                auto* hp = world->GetComponent<ECS::HealthComponent>(dmgEntity);
                if (!hp) continue;  // Pure hazards (no HealthComponent) are not enemies
                // Skip player entities (they have controllers)
                if (world->GetComponent<ECS::Platformer2DController>(dmgEntity)) continue;
                if (world->GetComponent<ECS::TopDown2DController>(dmgEntity)) continue;
                if (world->GetComponent<ECS::FirstPersonController>(dmgEntity)) continue;
                if (world->GetComponent<ECS::ThirdPersonController>(dmgEntity)) continue;
                if (!hp->isDead) {
                    anyEnemyAlive = true;
                    break;
                }
            }
            // Only trigger victory if there were enemies to begin with
            bool hasEnemies = false;
            for (auto dmgEntity : world->GetEntitiesWithComponent<ECS::DamageComponent>()) {
                auto* hp = world->GetComponent<ECS::HealthComponent>(dmgEntity);
                if (!hp) continue;
                if (world->GetComponent<ECS::Platformer2DController>(dmgEntity)) continue;
                if (world->GetComponent<ECS::TopDown2DController>(dmgEntity)) continue;
                if (world->GetComponent<ECS::FirstPersonController>(dmgEntity)) continue;
                if (world->GetComponent<ECS::ThirdPersonController>(dmgEntity)) continue;
                hasEnemies = true;
                break;
            }
            if (hasEnemies && !anyEnemyAlive) {
                go->triggered = true;
                go->won = true;
                go->delayTimer = 0.0f;
                go->screenVisible = false;
                ENJIN_LOG_INFO(Game, "Game Over triggered: VICTORY (all enemies defeated)");
                continue;
            }
        }

        // --- Victory check 2: trigger zone reached ---
        if (go->victoryTriggerEntity != 0) {
            auto* trigger = world->GetComponent<ECS::TriggerZoneComponent>(go->victoryTriggerEntity);
            if (trigger && !trigger->entitiesInside.empty()) {
                // Check if any entity inside the trigger is a player
                for (auto inside : trigger->entitiesInside) {
                    bool isPlayer = world->GetComponent<ECS::Platformer2DController>(inside) ||
                                    world->GetComponent<ECS::TopDown2DController>(inside) ||
                                    world->GetComponent<ECS::FirstPersonController>(inside) ||
                                    world->GetComponent<ECS::ThirdPersonController>(inside);
                    if (isPlayer) {
                        go->triggered = true;
                        go->won = true;
                        go->delayTimer = 0.0f;
                        go->screenVisible = false;
                        ENJIN_LOG_INFO(Game, "Game Over triggered: VICTORY (trigger zone reached)");
                        break;
                    }
                }
            }
        }
    }

    return false;
}

} // namespace GameplayLoop
} // namespace Gameplay
} // namespace Enjin
