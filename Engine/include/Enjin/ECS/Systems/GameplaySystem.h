#pragma once

// The eight gameplay components that had nothing behind them.
//
// LockComponent, PushableComponent, SwitchComponent, GoalZoneComponent,
// ConveyorComponent, TeleporterComponent, MovingPlatformComponent and
// SpawnPointComponent could all be added from the Add Component menu, filled in
// through a full inspector, saved into a scene, reloaded, and read from script.
// Nothing anywhere made a single one of them DO anything: outside the editor, the
// serializer and the script bindings, no file in the engine even named them.
//
// That is the sharpest version of the failure this whole sweep is about. A missing
// feature announces itself. A component with a complete authoring surface and no
// system does not: you place a conveyor belt, set its direction and speed, press
// Play, and nothing moves -- which reads as "I set it up wrong", and there is
// nothing you could have set up that would have worked.
//
// This is the system. One per runtime, ticked once per frame in the editor's Play,
// the player and the web player, exactly like ActionTriggerSystem.
//
// Thread rules (adr-0004): everything here runs on the owner thread. SpawnPoint
// creates entities and that is structural mutation, so it must never move to a
// worker.

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Math/Vector.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Enjin {

namespace InputSystem { class InputActionMap; }
namespace Scene { class SceneManager; }
namespace Gameplay { class TieredSaveSystem; }

namespace ECS {

// An axis-aligned box in world space, used for every overlap this system does.
//
// Colliders in this engine are WORLD SPACE and are NOT multiplied by the entity
// scale (see CLAUDE.md), so this reads the collider's own size and does not touch
// the transform's scale. An entity with no collider is a point at its position --
// not an infinite box, and not skipped: a pushable crate with no collider should
// still ride a conveyor it is standing on.
struct WorldBounds {
    Math::Vector3 min;
    Math::Vector3 max;

    bool Overlaps(const WorldBounds& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
    Math::Vector3 Center() const {
        return Math::Vector3((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f,
                             (min.z + max.z) * 0.5f);
    }
};

ENJIN_API WorldBounds ComputeWorldBounds(World* world, Entity entity);

class ENJIN_API GameplaySystem {
public:
    // Where the Interact action comes from. Null means nothing can be interacted
    // with -- locks still animate and switches on timers still run, but a Toggle
    // switch never fires. That is a real difference, so it is logged once rather
    // than left to look like a broken switch.
    void SetInputActionMap(InputSystem::InputActionMap* map) { m_InputMap = map; }

    // Where a GoalZone of type LevelExit sends its request. Null skips the
    // transition and warns once, because a level with no exit is a level you
    // cannot finish and that must not be silent.
    void SetSceneManager(Scene::SceneManager* scenes) { m_Scenes = scenes; }

    // Where a GoalZone of type Checkpoint reports to. Null means checkpoints are
    // reached and nothing is saved, which is a legitimate setup (a game with no
    // save system), so it is not warned about.
    void SetSaveSystem(Gameplay::TieredSaveSystem* saves) { m_Saves = saves; }

    // Call once when play begins, before the first Update. Captures the rest
    // position of anything this system animates, and runs spawnOnStart.
    //
    // The rest position has to be captured rather than read from the component,
    // because LockComponent::closedPosition and SwitchComponent::offPosition are
    // OFFSETS an author types relative to where they placed the entity. Treating
    // them as world positions teleports every door to the origin on the first
    // frame of play, which is what an author would see if this system had been
    // written without asking.
    void OnPlayStart(World* world);

    void Update(World* world, f32 dt);

    // Put back everything play changed, so Stop returns the scene to its authored
    // state. Entities spawned by a SpawnPoint are destroyed here.
    void Reset(World* world);

private:
    void UpdateLocks(World* world, f32 dt);
    void UpdateSwitches(World* world, f32 dt);
    void UpdateGoalZones(World* world, f32 dt);
    void UpdateConveyors(World* world, f32 dt);
    void UpdateTeleporters(World* world, f32 dt);
    void UpdateMovingPlatforms(World* world, f32 dt);
    void UpdatePushables(World* world, f32 dt);
    void UpdateSpawnPoints(World* world, f32 dt);

    // Everything a switch or a goal drives. Kept in one place so "what does
    // activating this do" has a single answer.
    void ApplyLinkedActivation(World* world, Entity source, bool active);

    bool InteractPressed() const;
    bool EntityMatchesTag(World* world, Entity entity, const std::string& tag) const;
    bool EntityHasKey(World* world, Entity entity, const std::string& key,
                      bool consume) const;

    InputSystem::InputActionMap* m_InputMap = nullptr;
    Scene::SceneManager* m_Scenes = nullptr;
    Gameplay::TieredSaveSystem* m_Saves = nullptr;

    // Authored rest positions, captured at OnPlayStart and restored by Reset.
    std::unordered_map<u64, Math::Vector3> m_RestPositions;
    std::unordered_map<u64, Math::Vector3> m_RestRotations;

    // Entities this system created, so Reset can remove exactly those.
    std::vector<Entity> m_Spawned;

    // Spawn points that cannot do their job (no prefab set, or one that will not
    // load). Held here rather than by writing currentSpawns = maxSpawns on the
    // component: that stops the retries but also tells the inspector and any script
    // reading it that N things spawned, when nothing did.
    std::unordered_set<u64> m_GaveUp;

    // Spawn scatter comes from here rather than from rand(), so two runs of the
    // same scene place the same things. rand() is process-global state that every
    // other system shares, which makes a replay diverge for reasons nothing in the
    // scene can explain.
    struct Rng {
        u32 state = 0x9E3779B9u;
        f32 Next01() {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<f32>(state & 0xFFFFFFu) / static_cast<f32>(0x1000000u);
        }
        void Reset() { state = 0x9E3779B9u; }
    };
    Rng m_Random;

    bool m_Started = false;
    bool m_WarnedNoInputMap = false;
    bool m_WarnedNoSceneManager = false;
};

} // namespace ECS
} // namespace Enjin
