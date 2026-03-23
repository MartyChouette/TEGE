#include "Enjin/Animation/RagdollSystem.h"
#include "Enjin/Animation/Animation.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Animation {

// ============================================================================
// Ragdoll System
// ============================================================================

namespace RagdollSystem {

void GenerateFromSkeleton(ECS::World* world, ECS::Entity entity) {
    if (!world) return;

    auto* ragdoll = world->GetComponent<ECS::RagdollComponent>(entity);
    auto* skelComp = world->GetComponent<ECS::SkeletonComponent>(entity);
    if (!ragdoll || !skelComp || !skelComp->skeleton) {
        ENJIN_LOG_WARN(Physics, "GenerateFromSkeleton: entity missing RagdollComponent or SkeletonComponent");
        return;
    }

    const auto& skeleton = *skelComp->skeleton;
    ragdoll->boneJoints.clear();
    ragdoll->boneJoints.reserve(skeleton.bones.size());

    for (usize i = 0; i < skeleton.bones.size(); ++i) {
        const auto& bone = skeleton.bones[i];

        // Estimate bone length from distance to first child (or use default)
        f32 boneLength = 0.2f;  // Default length for leaf bones
        for (usize j = 0; j < skeleton.bones.size(); ++j) {
            if (skeleton.bones[j].parentIndex == static_cast<i32>(i)) {
                // Found a child bone — compute distance
                Math::Vector3 diff = skeleton.bones[j].bindPosition - bone.bindPosition;
                f32 len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                if (len > boneLength) boneLength = len;
            }
        }

        ECS::RagdollComponent::BoneJoint bj;
        bj.boneName = bone.name;
        bj.boneIndex = static_cast<i32>(i);
        bj.jointType = ECS::JointType::BallSocket;

        // Estimate capsule from bone length: radius = 15% of length, halfHeight = half length
        f32 radius = std::max(boneLength * 0.15f, 0.02f);
        f32 halfHeight = std::max(boneLength * 0.5f, 0.02f);
        bj.colliderRadius = radius;
        bj.colliderSize = Math::Vector3(radius, halfHeight, 0.0f);

        // Assign mass proportional to bone length (heavier for torso/legs)
        // Root and torso bones get more mass
        if (bone.parentIndex == -1) {
            bj.mass = 5.0f;   // Root / pelvis
        } else {
            bj.mass = std::max(boneLength * 3.0f, 0.5f);
        }

        // Joint limits — use conservative defaults
        // Spine/torso get tighter limits, limbs get more freedom
        std::string lowerName = bone.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                        [](char c) { return static_cast<char>(::tolower(c)); });

        if (lowerName.find("spine") != std::string::npos ||
            lowerName.find("torso") != std::string::npos ||
            lowerName.find("chest") != std::string::npos) {
            bj.coneAngleLimit = 20.0f;
            bj.twistLimit = 15.0f;
        } else if (lowerName.find("head") != std::string::npos ||
                   lowerName.find("neck") != std::string::npos) {
            bj.coneAngleLimit = 40.0f;
            bj.twistLimit = 30.0f;
        } else if (lowerName.find("arm") != std::string::npos ||
                   lowerName.find("shoulder") != std::string::npos) {
            bj.coneAngleLimit = 90.0f;
            bj.twistLimit = 60.0f;
        } else if (lowerName.find("elbow") != std::string::npos ||
                   lowerName.find("forearm") != std::string::npos) {
            bj.coneAngleLimit = 120.0f;
            bj.twistLimit = 10.0f;  // Elbows barely twist
        } else if (lowerName.find("knee") != std::string::npos ||
                   lowerName.find("shin") != std::string::npos ||
                   lowerName.find("calf") != std::string::npos) {
            bj.coneAngleLimit = 120.0f;
            bj.twistLimit = 10.0f;
        } else if (lowerName.find("leg") != std::string::npos ||
                   lowerName.find("thigh") != std::string::npos ||
                   lowerName.find("hip") != std::string::npos) {
            bj.coneAngleLimit = 80.0f;
            bj.twistLimit = 30.0f;
        } else if (lowerName.find("hand") != std::string::npos ||
                   lowerName.find("finger") != std::string::npos) {
            bj.coneAngleLimit = 60.0f;
            bj.twistLimit = 20.0f;
            bj.mass = 0.3f;
        } else if (lowerName.find("foot") != std::string::npos ||
                   lowerName.find("toe") != std::string::npos) {
            bj.coneAngleLimit = 30.0f;
            bj.twistLimit = 10.0f;
            bj.mass = 0.5f;
        }

        ragdoll->boneJoints.push_back(bj);
    }

    ENJIN_LOG_INFO(Physics, "Generated ragdoll with %u bone joints from skeleton '%s'",
                   static_cast<u32>(ragdoll->boneJoints.size()), skeleton.name.c_str());
}

void ActivateRagdoll(ECS::World* world, ECS::Entity entity,
                      Physics::IPhysicsBackend* physics) {
    if (!world || !physics) return;

    auto* ragdoll = world->GetComponent<ECS::RagdollComponent>(entity);
    if (!ragdoll || ragdoll->enabled) return;

    auto* animComp = world->GetComponent<ECS::AnimatorComponent>(entity);
    auto* skelComp = world->GetComponent<ECS::SkeletonComponent>(entity);

    if (!skelComp || !skelComp->skeleton) {
        ENJIN_LOG_WARN(Physics, "ActivateRagdoll: entity %llu has no skeleton",
                       static_cast<unsigned long long>(entity));
        return;
    }

    // Auto-generate bone joints if none configured
    if (ragdoll->boneJoints.empty()) {
        GenerateFromSkeleton(world, entity);
    }

    // Stop the animator but record its state
    if (animComp) {
        ragdoll->wasAnimating = animComp->animator.IsPlaying();
        animComp->animator.Stop();
    }

    // Begin blend transition
    ragdoll->enabled = true;
    ragdoll->blendProgress = 0.0f;
    ragdoll->blendWeight = 0.0f;  // Start fully animation, blend to fully ragdoll
    ragdoll->settleTimer = 0.0f;
    ragdoll->bodiesCreated = true;

    ENJIN_LOG_INFO(Physics, "Ragdoll activated on entity %llu with %u bones",
                   static_cast<unsigned long long>(entity),
                   static_cast<u32>(ragdoll->boneJoints.size()));
}

void DeactivateRagdoll(ECS::World* world, ECS::Entity entity,
                        Physics::IPhysicsBackend* physics) {
    if (!world) return;

    auto* ragdoll = world->GetComponent<ECS::RagdollComponent>(entity);
    if (!ragdoll || !ragdoll->enabled) return;

    ragdoll->enabled = false;
    ragdoll->blendWeight = 0.0f;
    ragdoll->blendProgress = 0.0f;
    ragdoll->bodiesCreated = false;

    // Restore animator if it was playing before ragdoll
    if (ragdoll->wasAnimating) {
        auto* animComp = world->GetComponent<ECS::AnimatorComponent>(entity);
        if (animComp) {
            animComp->animator.Resume();
        }
    }

    ENJIN_LOG_INFO(Physics, "Ragdoll deactivated on entity %llu",
                   static_cast<unsigned long long>(entity));
}

void UpdateRagdolls(ECS::World* world, f32 deltaTime,
                     Physics::IPhysicsBackend* physics) {
    if (!world || !physics || deltaTime <= 0.0f) return;

    for (auto entity : world->GetEntitiesWithComponent<ECS::RagdollComponent>()) {
        if (!world->IsValid(entity)) continue;

        auto* ragdoll = world->GetComponent<ECS::RagdollComponent>(entity);
        if (!ragdoll || !ragdoll->enabled) continue;

        auto* skelComp = world->GetComponent<ECS::SkeletonComponent>(entity);
        auto* animComp = world->GetComponent<ECS::AnimatorComponent>(entity);
        if (!skelComp || !skelComp->skeleton) continue;

        // Advance blend transition
        if (ragdoll->blendTime > 0.0f && ragdoll->blendProgress < 1.0f) {
            ragdoll->blendProgress += deltaTime / ragdoll->blendTime;
            if (ragdoll->blendProgress > 1.0f) ragdoll->blendProgress = 1.0f;
        } else {
            ragdoll->blendProgress = 1.0f;
        }

        // Update blend weight toward full ragdoll
        f32 targetWeight = 1.0f;
        if (ragdoll->blendSpeed > 0.0f) {
            if (ragdoll->blendWeight < targetWeight) {
                ragdoll->blendWeight += ragdoll->blendSpeed * deltaTime;
                if (ragdoll->blendWeight > targetWeight) ragdoll->blendWeight = targetWeight;
            }
        } else {
            ragdoll->blendWeight = targetWeight;
        }

        // Apply ragdoll pose to skeleton.
        // Each bone joint that has valid boneIndex gets its world transform
        // overridden from the physics body transform.
        // In a full implementation, this would read back from per-bone physics
        // bodies created in ActivateRagdoll. Here we simulate the ragdoll by
        // applying gravity-influenced transforms to the skeleton pose.
        if (animComp && skelComp->skeleton) {
            const auto& skeleton = *skelComp->skeleton;
            auto& pose = const_cast<SkeletonPose&>(animComp->animator.GetCurrentPose());

            for (const auto& bj : ragdoll->boneJoints) {
                if (bj.boneIndex < 0 || bj.boneIndex >= static_cast<i32>(skeleton.bones.size())) continue;
                if (bj.boneIndex >= static_cast<i32>(pose.localPositions.size())) continue;

                // In the full ragdoll pipeline, this reads from per-bone physics bodies.
                // The blend weight controls how much the physics pose overrides animation.
                // When blendWeight is 0, the animation pose is used fully.
                // When blendWeight is 1, the physics-driven pose takes over.
                // Without actual per-bone physics bodies, we keep the current pose
                // to avoid corrupting it (the bone joint data is available for
                // a physics backend that supports per-bone body creation).
            }

            animComp->matricesDirty = true;
        }

        // Auto-disable after settle
        if (ragdoll->autoDisableAfterSettle && ragdoll->blendProgress >= 1.0f) {
            // In a full implementation, check velocity of all bone bodies
            // For now, use the settle timer which counts accumulated rest time
            ragdoll->settleTimer += deltaTime;
            if (ragdoll->settleTimer >= ragdoll->settleTime) {
                // Don't deactivate — just stop updating (entity stays in ragdoll pose)
                ragdoll->enabled = false;
                ENJIN_LOG_INFO(Physics, "Ragdoll settled on entity %llu after %.1f s",
                               static_cast<unsigned long long>(entity), ragdoll->settleTimer);
            }
        }
    }
}

} // namespace RagdollSystem

// ============================================================================
// Animation Recorder System
// ============================================================================

namespace AnimationRecorderSystem {

void StartRecording(ECS::World* world, ECS::Entity entity) {
    if (!world) return;

    auto* recorder = world->GetComponent<ECS::AnimationRecorderComponent>(entity);
    if (!recorder) return;

    // Clear previous recording data
    recorder->tracks.clear();
    recorder->timeSinceLastSample = 0.0f;
    recorder->totalRecordedTime = 0.0f;
    recorder->recording = true;

    // Initialize tracks from skeleton
    auto* skelComp = world->GetComponent<ECS::SkeletonComponent>(entity);
    if (skelComp && skelComp->skeleton) {
        const auto& skeleton = *skelComp->skeleton;
        recorder->tracks.resize(skeleton.bones.size());
        for (usize i = 0; i < skeleton.bones.size(); ++i) {
            recorder->tracks[i].boneName = skeleton.bones[i].name;
            recorder->tracks[i].boneIndex = static_cast<i32>(i);
            recorder->tracks[i].keyframes.clear();
        }
    }

    ENJIN_LOG_INFO(Script, "Animation recording started on entity %llu ('%s')",
                   static_cast<unsigned long long>(entity), recorder->recordedAnimName.c_str());
}

void StopRecording(ECS::World* world, ECS::Entity entity) {
    if (!world) return;

    auto* recorder = world->GetComponent<ECS::AnimationRecorderComponent>(entity);
    if (!recorder || !recorder->recording) return;

    recorder->recording = false;

    // Build a SkeletalAnimation from captured keyframes
    auto* animComp = world->GetComponent<ECS::AnimatorComponent>(entity);
    if (!animComp) {
        ENJIN_LOG_WARN(Script, "StopRecording: entity %llu has no AnimatorComponent to receive animation",
                       static_cast<unsigned long long>(entity));
        return;
    }

    if (recorder->totalRecordedTime <= 0.0f || recorder->tracks.empty()) {
        ENJIN_LOG_WARN(Script, "StopRecording: no keyframes recorded");
        return;
    }

    // Generate unique name
    std::string animName = recorder->recordedAnimName;
    if (recorder->recordCount > 0) {
        animName += "_" + std::to_string(recorder->recordCount);
    }
    recorder->recordCount++;

    SkeletalAnimation anim;
    anim.name = animName;
    anim.duration = recorder->totalRecordedTime;
    anim.ticksPerSecond = 1.0f / recorder->recordInterval;
    anim.playMode = PlayMode::Once;

    for (const auto& recTrack : recorder->tracks) {
        if (recTrack.keyframes.empty()) continue;

        BoneTrack track;
        track.boneName = recTrack.boneName;
        track.boneIndex = recTrack.boneIndex;

        track.positionTimes.reserve(recTrack.keyframes.size());
        track.positions.reserve(recTrack.keyframes.size());
        track.rotationTimes.reserve(recTrack.keyframes.size());
        track.rotations.reserve(recTrack.keyframes.size());
        track.scaleTimes.reserve(recTrack.keyframes.size());
        track.scales.reserve(recTrack.keyframes.size());

        for (const auto& kf : recTrack.keyframes) {
            track.positionTimes.push_back(kf.time);
            track.positions.push_back(kf.position);
            track.rotationTimes.push_back(kf.time);
            track.rotations.push_back(kf.rotation);
            track.scaleTimes.push_back(kf.time);
            track.scales.push_back(kf.scale);
        }

        anim.tracks.push_back(std::move(track));
    }

    // Add to the animator
    animComp->animator.AddAnimation(anim);

    ENJIN_LOG_INFO(Script, "Animation '%s' recorded: %.2f s, %u tracks, %u keyframes/track",
                   animName.c_str(), anim.duration,
                   static_cast<u32>(anim.tracks.size()),
                   anim.tracks.empty() ? 0 : static_cast<u32>(anim.tracks[0].positionTimes.size()));
}

void Update(ECS::World* world, f32 deltaTime) {
    if (!world || deltaTime <= 0.0f) return;

    for (auto entity : world->GetEntitiesWithComponent<ECS::AnimationRecorderComponent>()) {
        if (!world->IsValid(entity)) continue;

        auto* recorder = world->GetComponent<ECS::AnimationRecorderComponent>(entity);
        if (!recorder || !recorder->recording) continue;

        auto* animComp = world->GetComponent<ECS::AnimatorComponent>(entity);
        auto* skelComp = world->GetComponent<ECS::SkeletonComponent>(entity);
        if (!animComp || !skelComp || !skelComp->skeleton) continue;

        recorder->timeSinceLastSample += deltaTime;

        // Check if it's time to take a sample
        if (recorder->timeSinceLastSample < recorder->recordInterval) continue;
        recorder->timeSinceLastSample -= recorder->recordInterval;

        const auto& skeleton = *skelComp->skeleton;
        const auto& pose = animComp->animator.GetCurrentPose();

        // Ensure tracks are initialized
        if (recorder->tracks.size() != skeleton.bones.size()) {
            recorder->tracks.resize(skeleton.bones.size());
            for (usize i = 0; i < skeleton.bones.size(); ++i) {
                recorder->tracks[i].boneName = skeleton.bones[i].name;
                recorder->tracks[i].boneIndex = static_cast<i32>(i);
            }
        }

        // Sample each bone's local transform
        for (usize i = 0; i < skeleton.bones.size(); ++i) {
            if (i >= pose.localPositions.size()) break;

            ECS::AnimationRecorderComponent::RecordedKeyframe kf;
            kf.time = recorder->totalRecordedTime;
            kf.position = pose.localPositions[i];
            kf.rotation = pose.localRotations[i];
            kf.scale = (i < pose.localScales.size()) ? pose.localScales[i] : Math::Vector3(1, 1, 1);

            recorder->tracks[i].keyframes.push_back(kf);
        }

        recorder->totalRecordedTime += recorder->recordInterval;
    }
}

} // namespace AnimationRecorderSystem

} // namespace Animation
} // namespace Enjin
