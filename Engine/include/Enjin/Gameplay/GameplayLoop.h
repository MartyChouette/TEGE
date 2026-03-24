#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <vector>

namespace Enjin {
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }
namespace Scripting { class ScriptEngine; }
namespace ECS { class VisualScriptSystem; }

namespace Gameplay {

// Shared gameplay processing logic used by both PlayMode (editor) and
// the standalone Player. Extracted to avoid code duplication.
namespace GameplayLoop {

    // Process contact damage between two colliding entities.
    // Checks both orderings (A damages B, B damages A).
    // Entities marked destroyOnHit are appended to deferredDestroys.
    ENJIN_API void ProcessContactDamage(ECS::World* world, ECS::Entity entityA,
                                         ECS::Entity entityB,
                                         std::vector<ECS::Entity>& deferredDestroys);

    // Process pickup collection between two colliding entities.
    // Checks both orderings (A is pickup for B, B is pickup for A).
    // Entities that should be destroyed are appended to deferredDestroys.
    ENJIN_API void ProcessPickup(ECS::World* world, ECS::Entity entityA,
                                  ECS::Entity entityB,
                                  std::vector<ECS::Entity>& deferredDestroys);

    // Per-frame AABB overlap check for damage hazards (spikes, lava).
    // Bypasses Box2D sensor system which doesn't reliably detect stationary
    // kinematic sensors overlapping moving kinematic visitors.
    ENJIN_API void CheckHazardOverlaps(ECS::World* world, f32 deltaTime,
                                        std::vector<ECS::Entity>& deferredDestroys);

    // 3D pickup AABB overlap check. CharacterVirtual doesn't fire collision
    // events with static bodies, so we manually check each frame.
    ENJIN_API void CheckPickupOverlaps3D(ECS::World* world,
                                          std::vector<ECS::Entity>& deferredDestroys);

    // Update health regeneration, shield regeneration, invulnerability timers,
    // death handling (player respawn vs NPC destroy), and pickup respawn timers.
    ENJIN_API void UpdateHealthSystems(ECS::World* world, f32 deltaTime,
                                        std::vector<ECS::Entity>& deferredDestroys);

    // Destroy all entities in the deferred destroy list (verifying they still exist).
    ENJIN_API void FlushDeferredDestroys(ECS::World* world,
                                          std::vector<ECS::Entity>& deferredDestroys);

    // Dispatch 3D collision events from physics to visual scripts and gameplay systems.
    // Calls OnCollisionEnter/Exit and OnTriggerEnter/Exit on the VisualScriptSystem,
    // and processes contact damage and pickups.
    ENJIN_API void DispatchCollisionEvents3D(ECS::World* world,
                                              Physics::IPhysicsBackend* physics,
                                              ECS::VisualScriptSystem* vsSystem,
                                              f32 deltaTime,
                                              std::vector<ECS::Entity>& deferredDestroys);

    // Wire 2D physics collision callbacks to visual script system and gameplay processing.
    // Sets up OnCollisionEnter/Exit and OnSensorEnter/Exit callbacks on the 2D physics backend.
    // The callbacks capture world, vsSystem, and deferredDestroys by pointer/reference.
    ENJIN_API void Wire2DCollisionCallbacks(Physics::IPhysicsBackend2D* physics2D,
                                             ECS::World* world,
                                             ECS::VisualScriptSystem* vsSystem,
                                             std::vector<ECS::Entity>& deferredDestroys);

    // Update game over state: checks player death (defeat) and enemy
    // elimination / victory trigger (victory). Call once per frame after
    // UpdateHealthSystems. Returns true when a GameOverComponent has been
    // triggered and its delay has elapsed (i.e. the game over screen should
    // be shown).
    ENJIN_API bool UpdateGameOverState(ECS::World* world, f32 deltaTime);

} // namespace GameplayLoop

} // namespace Gameplay
} // namespace Enjin
