#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Animation/Animation.h"
#include <vector>
#include <string>

namespace Enjin {
namespace Animation {

// Feature vector for a single pose in the motion matching database.
// Features are compared using weighted Euclidean distance.
// All positions are in character-local space (relative to root).
struct PoseFeatures {
    // Current pose features
    Math::Vector3 leftFootPos;      // Left foot position relative to root
    Math::Vector3 rightFootPos;     // Right foot position relative to root
    Math::Vector3 hipVelocity;      // Root velocity (units/sec)

    // Trajectory prediction (future positions at fixed intervals)
    // Typically 3-4 points at 0.2s, 0.4s, 0.6s, 0.8s ahead
    static constexpr u32 TRAJECTORY_POINTS = 4;
    Math::Vector3 trajectoryPositions[TRAJECTORY_POINTS]; // Future root positions
    Math::Vector3 trajectoryDirections[TRAJECTORY_POINTS]; // Future facing directions

    // Metadata (not used in distance computation, but needed for playback)
    u32 clipIndex = 0;    // Which animation clip this pose is from
    f32 timeInClip = 0.0f; // Timestamp within the clip
};

// Feature weights for distance computation (tuned per character type)
struct FeatureWeights {
    f32 footPosition = 1.0f;
    f32 hipVelocity = 1.0f;
    f32 trajectoryPosition = 1.5f; // Trajectory usually weighted higher
    f32 trajectoryDirection = 1.0f;
};

// A reference to a clip in the database
struct MotionClipRef {
    std::string name;
    u32 clipIndex = 0;
    f32 duration = 0.0f;
    const SkeletalAnimation* animation = nullptr;
};

// Motion matching database — stores feature vectors for all poses across all clips.
// Built offline or at load time by sampling clips at regular intervals.
class ENJIN_API MotionDatabase {
public:
    MotionDatabase() = default;

    // Build the database from a set of animation clips.
    // Samples each clip at sampleInterval (seconds) and extracts features.
    // Requires a skeleton to compute bone world positions.
    void Build(const Skeleton& skeleton, const std::vector<MotionClipRef>& clips,
               f32 sampleInterval = 0.033f); // ~30 samples/sec

    // Find the best matching pose for the given query features.
    // Returns the index into m_Poses. Uses brute-force search with early termination.
    // For large databases (100K+ poses), use BuildKDTree() first.
    u32 FindBestMatch(const PoseFeatures& query, const FeatureWeights& weights,
                      u32 excludeClipIndex = UINT32_MAX, f32 excludeTimeWindow = 0.1f) const;

    // Access
    const PoseFeatures& GetPose(u32 index) const { return m_Poses[index]; }
    u32 GetPoseCount() const { return static_cast<u32>(m_Poses.size()); }
    const std::vector<MotionClipRef>& GetClips() const { return m_Clips; }

    // KD-tree acceleration for large databases (optional)
    void BuildKDTree();

    bool IsBuilt() const { return !m_Poses.empty(); }

private:
    // Extract features from a pose at a given time
    PoseFeatures ExtractFeatures(const Skeleton& skeleton, const SkeletalAnimation& clip,
                                  u32 clipIndex, f32 time) const;

    std::vector<PoseFeatures> m_Poses;
    std::vector<MotionClipRef> m_Clips;

    // KD-tree nodes (flat array) for accelerated search
    struct KDNode {
        u32 poseIndex;
        u32 splitAxis;
        u32 left;  // Index into m_KDNodes (UINT32_MAX = leaf)
        u32 right;
    };
    std::vector<KDNode> m_KDNodes;
    bool m_HasKDTree = false;
};

// Inertialization blending — spring-damper transition between poses.
// Eliminates foot-sliding that linear crossfades cause.
// Reference: "Inertialization: High-Performance Animation Transitions" (GDC 2018)
struct InertializationState {
    // Per-bone offset from target pose (decays over time via spring-damper)
    std::vector<Math::Vector3> positionOffsets;
    std::vector<Math::Vector3> positionVelocities;
    std::vector<Math::Quaternion> rotationOffsets;

    f32 halfLife = 0.15f;  // Decay half-life in seconds (lower = faster transition)
    bool active = false;

    void Initialize(const SkeletonPose& fromPose, const SkeletonPose& toPose);
    void Update(f32 deltaTime, SkeletonPose& pose);
    void Reset();
};

// Motion warping — adjusts root motion to align with gameplay targets.
// Example: vault animation's hand must hit the ledge position.
struct MotionWarpTarget {
    Math::Vector3 targetPosition;
    Math::Quaternion targetRotation;
    f32 warpStartTime = 0.0f;  // When to start warping (normalized 0-1 in clip)
    f32 warpEndTime = 1.0f;    // When warping is complete
    std::string syncBoneName;  // Bone that must reach the target (e.g., "RightHand")
};

// Apply motion warping to a pose — returns adjusted root transform
Math::Matrix4 ApplyMotionWarp(const SkeletonPose& pose, const Skeleton& skeleton,
                               const MotionWarpTarget& target, f32 normalizedTime);

// Runtime motion matching controller — replaces state machine for locomotion.
// Each frame: extracts current features, queries database, transitions to best match.
class ENJIN_API MotionMatchingController {
public:
    MotionMatchingController() = default;

    void SetDatabase(const MotionDatabase* db) { m_Database = db; }
    void SetSkeleton(const Skeleton* skeleton) { m_Skeleton = skeleton; }
    void SetWeights(const FeatureWeights& weights) { m_Weights = weights; }

    // Main update — call each frame.
    // desiredVelocity: character's intended movement direction + speed
    // desiredFacing: character's intended facing direction
    // Returns the current pose after motion matching + inertialization.
    void Update(f32 deltaTime, const Math::Vector3& desiredVelocity,
                const Math::Vector3& desiredFacing, SkeletonPose& outPose);

    // Query
    u32 GetCurrentClipIndex() const { return m_CurrentClipIndex; }
    f32 GetCurrentTimeInClip() const { return m_CurrentTime; }
    bool IsActive() const { return m_Database != nullptr && m_Database->IsBuilt(); }

    // Tuning
    f32 switchCostThreshold = 0.5f; // Minimum improvement to justify a clip switch
    f32 searchIntervalSeconds = 0.1f; // How often to search (not every frame)

private:
    PoseFeatures BuildQueryFeatures(const Math::Vector3& desiredVelocity,
                                     const Math::Vector3& desiredFacing) const;

    const MotionDatabase* m_Database = nullptr;
    const Skeleton* m_Skeleton = nullptr;
    FeatureWeights m_Weights;

    u32 m_CurrentClipIndex = 0;
    f32 m_CurrentTime = 0.0f;
    f32 m_TimeSinceLastSearch = 0.0f;

    InertializationState m_Inertialization;
    SkeletonPose m_CurrentPose;
};

} // namespace Animation
} // namespace Enjin
