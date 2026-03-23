#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Matrix.h"
#include <vector>

namespace Enjin {
namespace ECS { class World; }
namespace Physics { class IPhysicsBackend; }

namespace Animation {

// Ragdoll physics transition system.
// Manages the lifecycle of ragdoll activation: stopping animation, creating
// per-bone physics bodies, blending from animation to physics-driven poses,
// and reading back physics transforms to drive the skeleton.
namespace RagdollSystem {

    // Activate ragdoll on an entity. Stops animator, creates physics bodies
    // for each bone in the RagdollComponent, and begins the blend transition.
    // If the component has no bone joints configured, auto-generates from skeleton.
    ENJIN_API void ActivateRagdoll(ECS::World* world, ECS::Entity entity,
                                    Physics::IPhysicsBackend* physics);

    // Deactivate ragdoll — destroys per-bone physics bodies and restores
    // animator control over the skeleton.
    ENJIN_API void DeactivateRagdoll(ECS::World* world, ECS::Entity entity,
                                      Physics::IPhysicsBackend* physics);

    // Per-frame update: advances blend progress and reads physics body
    // transforms back into the skeleton pose. Call after physics step.
    ENJIN_API void UpdateRagdolls(ECS::World* world, f32 deltaTime,
                                   Physics::IPhysicsBackend* physics);

    // Auto-generate ragdoll bone setup from a skeleton.
    // Estimates capsule sizes from bone lengths and assigns reasonable masses.
    // Populates the boneJoints vector on the RagdollComponent.
    ENJIN_API void GenerateFromSkeleton(ECS::World* world, ECS::Entity entity);

} // namespace RagdollSystem

// Animation recorder system.
// Captures bone transforms at configurable intervals and builds a
// SkeletalAnimation from the recorded data when stopped.
namespace AnimationRecorderSystem {

    // Per-frame update: if recording, samples bone transforms at the
    // configured interval and stores keyframes.
    ENJIN_API void Update(ECS::World* world, f32 deltaTime);

    // Start recording on an entity's AnimationRecorderComponent.
    // Clears any previous recording data.
    ENJIN_API void StartRecording(ECS::World* world, ECS::Entity entity);

    // Stop recording and build a SkeletalAnimation from the captured
    // keyframes. Adds the animation to the entity's AnimatorComponent.
    ENJIN_API void StopRecording(ECS::World* world, ECS::Entity entity);

} // namespace AnimationRecorderSystem

} // namespace Animation
} // namespace Enjin
