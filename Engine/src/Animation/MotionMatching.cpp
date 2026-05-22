#include "Enjin/Animation/MotionMatching.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Enjin {
namespace Animation {

// --- MotionDatabase ---

void MotionDatabase::Build(const Skeleton& skeleton, const std::vector<MotionClipRef>& clips,
                            f32 sampleInterval) {
    m_Clips = clips;
    m_Poses.clear();

    for (u32 clipIdx = 0; clipIdx < static_cast<u32>(clips.size()); ++clipIdx) {
        const auto& clipRef = clips[clipIdx];
        if (!clipRef.animation) continue;

        f32 duration = clipRef.duration;
        u32 sampleCount = static_cast<u32>(duration / sampleInterval) + 1;

        for (u32 s = 0; s < sampleCount; ++s) {
            f32 time = static_cast<f32>(s) * sampleInterval;
            if (time > duration) time = duration;

            PoseFeatures features = ExtractFeatures(skeleton, *clipRef.animation, clipIdx, time);
            m_Poses.push_back(features);
        }
    }

    ENJIN_LOG_INFO(Animation, "MotionDatabase built: %u poses from %u clips (%.1f KB)",
                   static_cast<u32>(m_Poses.size()), static_cast<u32>(clips.size()),
                   static_cast<f32>(m_Poses.size() * sizeof(PoseFeatures)) / 1024.0f);
}

PoseFeatures MotionDatabase::ExtractFeatures(const Skeleton& skeleton,
                                              const SkeletalAnimation& clip,
                                              u32 clipIndex, f32 time) const {
    PoseFeatures f;
    f.clipIndex = clipIndex;
    f.timeInClip = time;

    // Sample the pose at this time
    SkeletonPose pose;
    pose.Resize(skeleton.bones.size());

    // Sample bone tracks
    for (const auto& track : clip.tracks) {
        if (track.boneIndex < 0 || track.boneIndex >= static_cast<i32>(pose.localPositions.size())) continue;
        pose.localPositions[track.boneIndex] = track.SamplePosition(time);
        pose.localRotations[track.boneIndex] = track.SampleRotation(time);
    }

    // Extract foot positions (look for common bone names)
    i32 leftFootIdx = skeleton.FindBoneIndex("LeftFoot");
    if (leftFootIdx < 0) leftFootIdx = skeleton.FindBoneIndex("mixamorig:LeftFoot");
    if (leftFootIdx < 0) leftFootIdx = skeleton.FindBoneIndex("foot_l");

    i32 rightFootIdx = skeleton.FindBoneIndex("RightFoot");
    if (rightFootIdx < 0) rightFootIdx = skeleton.FindBoneIndex("mixamorig:RightFoot");
    if (rightFootIdx < 0) rightFootIdx = skeleton.FindBoneIndex("foot_r");

    i32 hipIdx = skeleton.FindBoneIndex("Hips");
    if (hipIdx < 0) hipIdx = skeleton.FindBoneIndex("mixamorig:Hips");
    if (hipIdx < 0) hipIdx = skeleton.FindBoneIndex("pelvis");

    if (leftFootIdx >= 0) f.leftFootPos = pose.localPositions[leftFootIdx];
    if (rightFootIdx >= 0) f.rightFootPos = pose.localPositions[rightFootIdx];

    // Hip velocity: finite difference (sample pose at time ± epsilon)
    f32 eps = 0.016f; // ~1 frame
    if (hipIdx >= 0) {
        Math::Vector3 hipNow = pose.localPositions[hipIdx];
        SkeletonPose poseFuture;
        poseFuture.Resize(skeleton.bones.size());
        f32 futureTime = std::min(time + eps, clip.duration);
        for (const auto& track : clip.tracks) {
            if (track.boneIndex == hipIdx) {
                poseFuture.localPositions[hipIdx] = track.SamplePosition(futureTime);
                break;
            }
        }
        f.hipVelocity = (poseFuture.localPositions[hipIdx] - hipNow) / eps;
    }

    // Trajectory prediction: sample root position at future intervals
    for (u32 t = 0; t < PoseFeatures::TRAJECTORY_POINTS; ++t) {
        f32 futureTime = time + (static_cast<f32>(t) + 1.0f) * 0.2f;
        futureTime = std::min(futureTime, clip.duration);

        if (hipIdx >= 0) {
            for (const auto& track : clip.tracks) {
                if (track.boneIndex == hipIdx) {
                    f.trajectoryPositions[t] = track.SamplePosition(futureTime);
                    break;
                }
            }
        }
        // Direction approximation: velocity direction at future point
        f.trajectoryDirections[t] = f.hipVelocity.Length() > 0.01f
            ? f.hipVelocity.Normalized()
            : Math::Vector3(0, 0, 1);
    }

    return f;
}

u32 MotionDatabase::FindBestMatch(const PoseFeatures& query, const FeatureWeights& w,
                                   u32 excludeClipIndex, f32 excludeTimeWindow) const {
    if (m_Poses.empty()) return 0;

    f32 bestCost = std::numeric_limits<f32>::max();
    u32 bestIndex = 0;

    for (u32 i = 0; i < static_cast<u32>(m_Poses.size()); ++i) {
        const PoseFeatures& p = m_Poses[i];

        // Skip poses too close to current position in same clip (avoid micro-switches)
        if (p.clipIndex == excludeClipIndex &&
            std::abs(p.timeInClip - query.timeInClip) < excludeTimeWindow) {
            continue;
        }

        // Weighted feature distance
        f32 cost = 0.0f;

        // Foot positions
        cost += (p.leftFootPos - query.leftFootPos).LengthSquared() * w.footPosition;
        cost += (p.rightFootPos - query.rightFootPos).LengthSquared() * w.footPosition;

        // Hip velocity
        cost += (p.hipVelocity - query.hipVelocity).LengthSquared() * w.hipVelocity;

        // Trajectory
        for (u32 t = 0; t < PoseFeatures::TRAJECTORY_POINTS; ++t) {
            cost += (p.trajectoryPositions[t] - query.trajectoryPositions[t]).LengthSquared() * w.trajectoryPosition;
            cost += (p.trajectoryDirections[t] - query.trajectoryDirections[t]).LengthSquared() * w.trajectoryDirection;
        }

        // Early termination
        if (cost >= bestCost) continue;
        bestCost = cost;
        bestIndex = i;
    }

    return bestIndex;
}

void MotionDatabase::BuildKDTree() {
    // TODO: Build KD-tree for accelerated search (>100K poses)
    // For now, brute-force is sufficient for typical databases (10K-50K poses)
    m_HasKDTree = false;
    ENJIN_LOG_INFO(Animation, "MotionDatabase: KD-tree build skipped (brute-force sufficient for %u poses)",
                   static_cast<u32>(m_Poses.size()));
}

// --- InertializationState ---

void InertializationState::Initialize(const SkeletonPose& fromPose, const SkeletonPose& toPose) {
    usize boneCount = std::min(fromPose.localPositions.size(), toPose.localPositions.size());
    positionOffsets.resize(boneCount);
    positionVelocities.resize(boneCount, Math::Vector3(0, 0, 0));
    rotationOffsets.resize(boneCount);

    for (usize i = 0; i < boneCount; ++i) {
        positionOffsets[i] = fromPose.localPositions[i] - toPose.localPositions[i];
        // Rotation offset: from * inverse(to) = delta to apply
        rotationOffsets[i] = fromPose.localRotations[i] * toPose.localRotations[i].Conjugate();
    }

    active = true;
}

void InertializationState::Update(f32 deltaTime, SkeletonPose& pose) {
    if (!active) return;

    // Spring-damper decay: offset *= exp(-lambda * dt)
    // lambda = ln(2) / halfLife
    f32 lambda = 0.693147f / std::max(halfLife, 0.001f);
    f32 decay = std::exp(-lambda * deltaTime);

    bool allSettled = true;
    f32 threshold = 0.0001f;

    for (usize i = 0; i < positionOffsets.size() && i < pose.localPositions.size(); ++i) {
        // Decay position offset
        positionOffsets[i] = positionOffsets[i] * decay;
        pose.localPositions[i] = pose.localPositions[i] + positionOffsets[i];

        if (positionOffsets[i].LengthSquared() > threshold) allSettled = false;

        // Decay rotation offset (slerp toward identity)
        rotationOffsets[i] = Math::Quaternion::Slerp(
            Math::Quaternion(0, 0, 0, 1), // Identity
            rotationOffsets[i],
            decay);
        if (i < pose.localRotations.size()) {
            pose.localRotations[i] = rotationOffsets[i] * pose.localRotations[i];
        }
    }

    if (allSettled) {
        active = false;
    }
}

void InertializationState::Reset() {
    positionOffsets.clear();
    positionVelocities.clear();
    rotationOffsets.clear();
    active = false;
}

// --- MotionWarp ---

Math::Matrix4 ApplyMotionWarp(const SkeletonPose& pose, const Skeleton& skeleton,
                               const MotionWarpTarget& target, f32 normalizedTime) {
    // Interpolate warp factor
    f32 warpFactor = 0.0f;
    if (normalizedTime >= target.warpStartTime && normalizedTime <= target.warpEndTime) {
        warpFactor = (normalizedTime - target.warpStartTime) /
                     std::max(target.warpEndTime - target.warpStartTime, 0.001f);
        warpFactor = std::clamp(warpFactor, 0.0f, 1.0f);
        // Smooth step for natural easing
        warpFactor = warpFactor * warpFactor * (3.0f - 2.0f * warpFactor);
    }

    // Find sync bone position in current pose
    i32 syncIdx = skeleton.FindBoneIndex(target.syncBoneName);
    Math::Vector3 currentPos = (syncIdx >= 0 && syncIdx < static_cast<i32>(pose.localPositions.size()))
        ? pose.localPositions[syncIdx]
        : Math::Vector3(0, 0, 0);

    // Lerp toward target
    Math::Vector3 warpedPos = Math::Vector3(
        currentPos.x + (target.targetPosition.x - currentPos.x) * warpFactor,
        currentPos.y + (target.targetPosition.y - currentPos.y) * warpFactor,
        currentPos.z + (target.targetPosition.z - currentPos.z) * warpFactor
    );

    return Math::Matrix4::Translation(warpedPos - currentPos);
}

// --- MotionMatchingController ---

void MotionMatchingController::Update(f32 deltaTime, const Math::Vector3& desiredVelocity,
                                       const Math::Vector3& desiredFacing,
                                       SkeletonPose& outPose) {
    if (!m_Database || !m_Skeleton || !m_Database->IsBuilt()) return;

    // Advance current clip
    m_CurrentTime += deltaTime;
    m_TimeSinceLastSearch += deltaTime;

    // Periodic search (not every frame — too expensive)
    if (m_TimeSinceLastSearch >= searchIntervalSeconds) {
        m_TimeSinceLastSearch = 0.0f;

        // Build query from desired movement
        PoseFeatures query = BuildQueryFeatures(desiredVelocity, desiredFacing);

        // Search database
        u32 bestMatch = m_Database->FindBestMatch(query, m_Weights,
                                                    m_CurrentClipIndex, 0.1f);

        const PoseFeatures& bestPose = m_Database->GetPose(bestMatch);

        // Compute cost of switching vs staying
        PoseFeatures currentFeatures = query; // Approximate
        currentFeatures.clipIndex = m_CurrentClipIndex;
        currentFeatures.timeInClip = m_CurrentTime;

        // Only switch if improvement exceeds threshold (prevents jittering)
        f32 currentCost = 0.0f; // Would need full cost computation here
        (void)currentCost;

        if (bestPose.clipIndex != m_CurrentClipIndex ||
            std::abs(bestPose.timeInClip - m_CurrentTime) > 0.2f) {

            // Save current pose for inertialization
            SkeletonPose prevPose = m_CurrentPose;

            // Switch to best match
            m_CurrentClipIndex = bestPose.clipIndex;
            m_CurrentTime = bestPose.timeInClip;

            // Sample new pose
            const auto& clips = m_Database->GetClips();
            if (m_CurrentClipIndex < clips.size() && clips[m_CurrentClipIndex].animation) {
                const auto& clip = *clips[m_CurrentClipIndex].animation;
                m_CurrentPose.Resize(m_Skeleton->bones.size());
                for (const auto& track : clip.tracks) {
                    if (track.boneIndex < 0 || track.boneIndex >= static_cast<i32>(m_CurrentPose.localPositions.size())) continue;
                    m_CurrentPose.localPositions[track.boneIndex] = track.SamplePosition(m_CurrentTime);
                    m_CurrentPose.localRotations[track.boneIndex] = track.SampleRotation(m_CurrentTime);
                }

                // Initialize inertialization for smooth transition
                if (prevPose.localPositions.size() == m_CurrentPose.localPositions.size()) {
                    m_Inertialization.Initialize(prevPose, m_CurrentPose);
                }
            }
        }
    }

    // Sample current clip at current time
    const auto& clips = m_Database->GetClips();
    if (m_CurrentClipIndex < clips.size() && clips[m_CurrentClipIndex].animation) {
        const auto& clip = *clips[m_CurrentClipIndex].animation;
        m_CurrentPose.Resize(m_Skeleton->bones.size());
        for (const auto& track : clip.tracks) {
            if (track.boneIndex < 0 || track.boneIndex >= static_cast<i32>(m_CurrentPose.localPositions.size())) continue;
            m_CurrentPose.localPositions[track.boneIndex] = track.SamplePosition(m_CurrentTime);
            m_CurrentPose.localRotations[track.boneIndex] = track.SampleRotation(m_CurrentTime);
        }
    }

    // Apply inertialization (spring-damper decay of transition offset)
    m_Inertialization.Update(deltaTime, m_CurrentPose);

    outPose = m_CurrentPose;
}

PoseFeatures MotionMatchingController::BuildQueryFeatures(const Math::Vector3& desiredVelocity,
                                                           const Math::Vector3& desiredFacing) const {
    PoseFeatures query;

    // Current foot positions from last pose
    i32 leftIdx = m_Skeleton ? m_Skeleton->FindBoneIndex("LeftFoot") : -1;
    if (leftIdx < 0 && m_Skeleton) leftIdx = m_Skeleton->FindBoneIndex("mixamorig:LeftFoot");
    i32 rightIdx = m_Skeleton ? m_Skeleton->FindBoneIndex("RightFoot") : -1;
    if (rightIdx < 0 && m_Skeleton) rightIdx = m_Skeleton->FindBoneIndex("mixamorig:RightFoot");

    if (leftIdx >= 0 && leftIdx < static_cast<i32>(m_CurrentPose.localPositions.size()))
        query.leftFootPos = m_CurrentPose.localPositions[leftIdx];
    if (rightIdx >= 0 && rightIdx < static_cast<i32>(m_CurrentPose.localPositions.size()))
        query.rightFootPos = m_CurrentPose.localPositions[rightIdx];

    query.hipVelocity = desiredVelocity;

    // Desired trajectory (straight line in desired direction)
    for (u32 t = 0; t < PoseFeatures::TRAJECTORY_POINTS; ++t) {
        f32 futureTime = (static_cast<f32>(t) + 1.0f) * 0.2f;
        query.trajectoryPositions[t] = desiredVelocity * futureTime;
        query.trajectoryDirections[t] = desiredFacing.Length() > 0.01f
            ? desiredFacing.Normalized()
            : Math::Vector3(0, 0, 1);
    }

    query.clipIndex = m_CurrentClipIndex;
    query.timeInClip = m_CurrentTime;

    return query;
}

} // namespace Animation
} // namespace Enjin
