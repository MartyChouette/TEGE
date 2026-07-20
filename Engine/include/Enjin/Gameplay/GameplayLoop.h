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

    // 3D hazard overlap check. The 2D CheckHazardOverlaps needs a Body2DComponent
    // and works on the XY plane, so it can't hurt a 3D controller. CharacterVirtual
    // also doesn't generate contact events with static hazard bodies, so we check
    // each frame: any 3D player overlapping a DamageComponent-without-HealthComponent
    // entity takes contact damage (respects i-frames / damageOnce). Without this a
    // 3D game has no working lose condition.
    ENJIN_API void CheckHazardOverlaps3D(ECS::World* world,
                                          std::vector<ECS::Entity>& deferredDestroys);

    // 2D pickup AABB overlap check. Box2D v3 kinematic-kinematic sensor events
    // are unreliable, so we manually check each frame for 2D controllers.
    ENJIN_API void CheckPickupOverlaps2D(ECS::World* world,
                                          std::vector<ECS::Entity>& deferredDestroys);

    // 2D enemy contact damage overlap. Enemies have both DamageComponent AND
    // HealthComponent (unlike hazards which only have DamageComponent). Box2D
    // sensor events are unreliable for kinematic-kinematic, so check manually.
    // Includes Mario-style stomp detection.
    ENJIN_API void CheckEnemyOverlaps2D(ECS::World* world, f32 deltaTime,
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

    // Populate every TriggerZoneComponent's entitiesInside list by AABB/sphere
    // overlap against player-controlled entities (2D + 3D controllers), and fire
    // onEnter/onExit notify entities. Nothing else in the engine fills
    // entitiesInside, so without this call trigger zones (and the
    // victoryTriggerEntity "reach the goal" win condition) never activate.
    // Call once per frame before UpdateGameOverState.
    ENJIN_API void UpdateTriggerZones(ECS::World* world);

    // Update game over state: checks player death (defeat) and enemy
    // elimination / victory trigger (victory). Call once per frame after
    // UpdateHealthSystems. Returns true when a GameOverComponent has been
    // triggered and its delay has elapsed (i.e. the game over screen should
    // be shown).
    ENJIN_API bool UpdateGameOverState(ECS::World* world, f32 deltaTime);

} // namespace GameplayLoop

} // namespace Gameplay
} // namespace Enjin
