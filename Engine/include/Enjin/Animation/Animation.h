#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Matrix.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Enjin {
namespace Animation {

// ============================================================================
// Animation Enums and Types
// ============================================================================

enum class PlayMode : u8 {
    Once,           // Play once and stop
    Loop,           // Loop forever
    PingPong,       // Play forward then backward
    ClampForever    // Play once, hold last frame
};

enum class BlendMode : u8 {
    Replace,        // New animation replaces current
    Additive,       // Add to current pose
    Blend           // Blend between animations
};

// ============================================================================
// 2D Sprite Animation
// ============================================================================

// Single frame of a sprite animation
struct SpriteFrame {
    // Source rectangle in texture atlas
    f32 srcX = 0;
    f32 srcY = 0;
    f32 srcWidth = 0;
    f32 srcHeight = 0;

    // Frame timing
    f32 duration = 0.1f;        // Seconds this frame is shown

    // Pivot offset (for varying frame sizes)
    Math::Vector2 pivot = Math::Vector2(0.5f, 0.5f);

    // Hitbox for this frame (optional)
    Math::Vector2 hitboxOffset;
    Math::Vector2 hitboxSize;

    // Event to trigger when this frame starts
    std::string eventName;
};

// A complete sprite animation clip
struct SpriteAnimation {
    std::string name;
    std::string texturePath;
    std::vector<SpriteFrame> frames;
    PlayMode playMode = PlayMode::Loop;

    // Calculated total duration
    f32 GetTotalDuration() const {
        f32 total = 0;
        for (const auto& frame : frames) {
            total += frame.duration;
        }
        return total;
    }
};

// Sprite animator component - plays sprite animations
class ENJIN_API SpriteAnimator {
public:
    SpriteAnimator() = default;

    // Animation management
    void AddAnimation(const SpriteAnimation& anim);
    void RemoveAnimation(const std::string& name);
    bool HasAnimation(const std::string& name) const;
    const SpriteAnimation* GetAnimation(const std::string& name) const;

    // Playback control
    void Play(const std::string& name, bool restart = true);
    void Stop();
    void Pause();
    void Resume();

    void SetSpeed(f32 speed) { m_Speed = speed; }
    f32 GetSpeed() const { return m_Speed; }

    // Update (call each frame)
    void Update(f32 deltaTime);

    // Get current frame data
    const SpriteFrame* GetCurrentFrame() const;
    f32 GetNormalizedTime() const { return m_NormalizedTime; }
    u32 GetCurrentFrameIndex() const { return m_CurrentFrameIndex; }
    bool IsPlaying() const { return m_IsPlaying; }
    bool IsFinished() const { return m_IsFinished; }

    // Events
    using EventCallback = std::function<void(const std::string&)>;
    void SetEventCallback(EventCallback callback) { m_OnEvent = callback; }

    // Current animation name
    const std::string& GetCurrentAnimationName() const { return m_CurrentAnimName; }

private:
    std::unordered_map<std::string, SpriteAnimation> m_Animations;
    std::string m_CurrentAnimName;
    const SpriteAnimation* m_CurrentAnim = nullptr;

    u32 m_CurrentFrameIndex = 0;
    f32 m_FrameTimer = 0.0f;
    f32 m_NormalizedTime = 0.0f;  // 0-1 progress through animation
    f32 m_Speed = 1.0f;
    bool m_IsPlaying = false;
    bool m_IsPaused = false;
    bool m_IsFinished = false;
    bool m_IsReversing = false;  // For ping-pong

    EventCallback m_OnEvent;
};

// ============================================================================
// 3D Skeletal Animation
// ============================================================================

// A bone in the skeleton hierarchy
struct Bone {
    std::string name;
    i32 parentIndex = -1;       // -1 = root bone

    // Bind pose (rest position)
    Math::Vector3 bindPosition;
    Math::Quaternion bindRotation;
    Math::Vector3 bindScale = Math::Vector3(1, 1, 1);

    // Inverse bind matrix (for skinning)
    Math::Matrix4 inverseBindMatrix;
};

// Skeleton definition
struct Skeleton {
    std::string name;
    std::vector<Bone> bones;

    i32 FindBoneIndex(const std::string& boneName) const {
        for (usize i = 0; i < bones.size(); ++i) {
            if (bones[i].name == boneName) {
                return static_cast<i32>(i);
            }
        }
        return -1;
    }
};

// Keyframe for a single bone
struct BoneKeyframe {
    f32 time;
    Math::Vector3 position;
    Math::Quaternion rotation;
    Math::Vector3 scale;
};

// Animation track for a single bone
struct BoneTrack {
    std::string boneName;
    i32 boneIndex = -1;

    std::vector<f32> positionTimes;
    std::vector<Math::Vector3> positions;

    std::vector<f32> rotationTimes;
    std::vector<Math::Quaternion> rotations;

    std::vector<f32> scaleTimes;
    std::vector<Math::Vector3> scales;

    // Sample the track at a given time
    Math::Vector3 SamplePosition(f32 time) const;
    Math::Quaternion SampleRotation(f32 time) const;
    Math::Vector3 SampleScale(f32 time) const;
};

// A complete skeletal animation clip
struct SkeletalAnimation {
    std::string name;
    f32 duration = 0.0f;
    f32 ticksPerSecond = 30.0f;
    std::vector<BoneTrack> tracks;
    PlayMode playMode = PlayMode::Loop;

    // Events at specific times
    struct AnimEvent {
        f32 time;
        std::string name;
    };
    std::vector<AnimEvent> events;
};

// Current pose of a skeleton (result of animation sampling)
struct SkeletonPose {
    std::vector<Math::Vector3> localPositions;
    std::vector<Math::Quaternion> localRotations;
    std::vector<Math::Vector3> localScales;

    // Cached world transforms
    std::vector<Math::Matrix4> worldTransforms;
    std::vector<Math::Matrix4> skinningMatrices;  // Final matrices for GPU

    void Resize(usize boneCount) {
        localPositions.resize(boneCount);
        localRotations.resize(boneCount);
        localScales.resize(boneCount, Math::Vector3(1, 1, 1));
        worldTransforms.resize(boneCount);
        skinningMatrices.resize(boneCount);
    }
};

// Skeletal animator - plays skeletal animations
class ENJIN_API SkeletalAnimator {
public:
    SkeletalAnimator() = default;

    // Copy constructor: re-resolve raw animation pointers into this object's map.
    // Without this, copied animators hold dangling pointers to the source's
    // m_Animations entries (crash on play mode stop/start).
    SkeletalAnimator(const SkeletalAnimator& other)
        : m_Skeleton(other.m_Skeleton), m_Animations(other.m_Animations),
          m_CurrentAnimName(other.m_CurrentAnimName), m_CurrentAnim(nullptr),
          m_NextAnimName(other.m_NextAnimName), m_NextAnim(nullptr),
          m_CurrentPose(other.m_CurrentPose), m_BlendPose(other.m_BlendPose),
          m_CurrentTime(other.m_CurrentTime), m_NormalizedTime(other.m_NormalizedTime),
          m_Speed(other.m_Speed), m_IsPlaying(other.m_IsPlaying), m_IsPaused(other.m_IsPaused),
          m_BlendTime(other.m_BlendTime), m_BlendProgress(other.m_BlendProgress),
          m_PingPongForward(other.m_PingPongForward) {
        // Re-resolve pointers into our own map
        if (!m_CurrentAnimName.empty()) {
            auto it = m_Animations.find(m_CurrentAnimName);
            if (it != m_Animations.end()) m_CurrentAnim = &it->second;
        }
        if (!m_NextAnimName.empty()) {
            auto it = m_Animations.find(m_NextAnimName);
            if (it != m_Animations.end()) m_NextAnim = &it->second;
        }
    }

    SkeletalAnimator& operator=(const SkeletalAnimator& other) {
        if (this == &other) return *this;
        m_Skeleton = other.m_Skeleton;
        m_Animations = other.m_Animations;
        m_CurrentAnimName = other.m_CurrentAnimName;
        m_NextAnimName = other.m_NextAnimName;
        m_CurrentPose = other.m_CurrentPose;
        m_BlendPose = other.m_BlendPose;
        m_CurrentTime = other.m_CurrentTime;
        m_NormalizedTime = other.m_NormalizedTime;
        m_Speed = other.m_Speed;
        m_IsPlaying = other.m_IsPlaying;
        m_IsPaused = other.m_IsPaused;
        m_BlendTime = other.m_BlendTime;
        m_BlendProgress = other.m_BlendProgress;
        m_PingPongForward = other.m_PingPongForward;
        m_CurrentAnim = nullptr;
        m_NextAnim = nullptr;
        if (!m_CurrentAnimName.empty()) {
            auto it = m_Animations.find(m_CurrentAnimName);
            if (it != m_Animations.end()) m_CurrentAnim = &it->second;
        }
        if (!m_NextAnimName.empty()) {
            auto it = m_Animations.find(m_NextAnimName);
            if (it != m_Animations.end()) m_NextAnim = &it->second;
        }
        return *this;
    }

    SkeletalAnimator(SkeletalAnimator&&) = default;
    SkeletalAnimator& operator=(SkeletalAnimator&&) = default;

    // Setup
    void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
    const Skeleton* GetSkeleton() const { return m_Skeleton.get(); }

    // Animation management
    void AddAnimation(const SkeletalAnimation& anim);
    void RemoveAnimation(const std::string& name);
    bool HasAnimation(const std::string& name) const;

    // Playback control
    void Play(const std::string& name, f32 blendTime = 0.0f);
    void CrossFade(const std::string& name, f32 fadeTime);
    void Stop();          // Stop playback, preserve current time
    void StopAndReset();   // Stop playback and reset to frame 0
    void Pause();
    void Resume();
    void SetNormalizedTime(f32 t); // Seek to position (0-1), updates pose

    void SetSpeed(f32 speed) { m_Speed = speed; }
    f32 GetSpeed() const { return m_Speed; }

    // Update
    void Update(f32 deltaTime);

    // Get results
    const SkeletonPose& GetCurrentPose() const { return m_CurrentPose; }
    const std::vector<Math::Matrix4>& GetSkinningMatrices() const { return m_CurrentPose.skinningMatrices; }

    f32 GetNormalizedTime() const { return m_NormalizedTime; }
    bool IsPlaying() const { return m_IsPlaying; }
    bool IsPaused() const { return m_IsPaused; }
    bool IsBlending() const { return m_BlendTime > 0.0f && m_BlendProgress < 1.0f; }

    // Events
    using EventCallback = std::function<void(const std::string&)>;
    void SetEventCallback(EventCallback callback) { m_OnEvent = callback; }

    // Accessors for serialization
    const std::unordered_map<std::string, SkeletalAnimation>& GetAnimations() const { return m_Animations; }
    const std::string& GetCurrentAnimationName() const { return m_CurrentAnimName; }
    std::shared_ptr<Skeleton> GetSharedSkeleton() const { return m_Skeleton; }

    // Bone manipulation (for IK, procedural animation)
    void SetBoneLocalRotation(const std::string& boneName, const Math::Quaternion& rotation);
    void SetBoneLocalPosition(const std::string& boneName, const Math::Vector3& position);

    // Get bone transforms
    Math::Matrix4 GetBoneWorldTransform(const std::string& boneName) const;
    Math::Matrix4 GetBoneWorldTransform(i32 boneIndex) const;

private:
    void SampleAnimation(const SkeletalAnimation& anim, f32 time, SkeletonPose& outPose);
    void BlendPoses(const SkeletonPose& a, const SkeletonPose& b, f32 t, SkeletonPose& out);
    void CalculateWorldTransforms();
    void CalculateSkinningMatrices();

    std::shared_ptr<Skeleton> m_Skeleton;
    std::unordered_map<std::string, SkeletalAnimation> m_Animations;

    std::string m_CurrentAnimName;
    const SkeletalAnimation* m_CurrentAnim = nullptr;
    std::string m_NextAnimName;
    const SkeletalAnimation* m_NextAnim = nullptr;

    SkeletonPose m_CurrentPose;
    SkeletonPose m_BlendPose;

    f32 m_CurrentTime = 0.0f;
    f32 m_NormalizedTime = 0.0f;
    f32 m_Speed = 1.0f;
    bool m_IsPlaying = false;
    bool m_IsPaused = false;

    // Blending
    f32 m_BlendTime = 0.0f;
    f32 m_BlendProgress = 1.0f;

    // PingPong direction (true = forward, false = reverse)
    bool m_PingPongForward = true;

    EventCallback m_OnEvent;
};

// ============================================================================
// Animation State Machine
// ============================================================================

// Transition condition
struct TransitionCondition {
    enum class Type : u8 {
        Bool,
        Float,
        Int,
        Trigger
    };

    std::string parameterName;
    Type type = Type::Bool;

    // Comparison
    enum class Comparison : u8 {
        Equal,
        NotEqual,
        Greater,
        Less,
        GreaterEqual,
        LessEqual
    };
    Comparison comparison = Comparison::Equal;

    // Value to compare against
    union {
        bool boolValue;
        f32 floatValue;
        i32 intValue;
    } value;
};

// State transition
struct AnimationTransition {
    std::string fromState;
    std::string toState;
    f32 blendTime = 0.2f;
    bool hasExitTime = false;
    f32 exitTime = 1.0f;  // Normalized time when transition can occur
    std::vector<TransitionCondition> conditions;
};

// Animation state
struct AnimationState {
    std::string name;
    std::string animationName;
    f32 speed = 1.0f;
    PlayMode playMode = PlayMode::Loop;
    Math::Vector2 editorPosition = Math::Vector2(0, 0);  // Node position in graph editor
};

// Animation state machine
class ENJIN_API AnimationStateMachine {
public:
    AnimationStateMachine() = default;

    // Setup
    void SetAnimator(SkeletalAnimator* animator) { m_Animator = animator; }

    // States
    void AddState(const AnimationState& state);
    void RemoveState(const std::string& name);
    void SetDefaultState(const std::string& name) { m_DefaultState = name; }

    // Transitions
    void AddTransition(const AnimationTransition& transition);
    void RemoveTransition(const std::string& from, const std::string& to);

    // Parameters
    void SetBool(const std::string& name, bool value);
    void SetFloat(const std::string& name, f32 value);
    void SetInt(const std::string& name, i32 value);
    void SetTrigger(const std::string& name);
    void ResetTrigger(const std::string& name);

    bool GetBool(const std::string& name) const;
    f32 GetFloat(const std::string& name) const;
    i32 GetInt(const std::string& name) const;

    // Update
    void Update(f32 deltaTime);

    // Get current state
    const std::string& GetCurrentState() const { return m_CurrentState; }

    // Public accessors for graph editor
    const std::unordered_map<std::string, AnimationState>& GetStates() const { return m_States; }
    std::unordered_map<std::string, AnimationState>& GetStatesMut() { return m_States; }
    const std::vector<AnimationTransition>& GetTransitions() const { return m_Transitions; }
    std::vector<AnimationTransition>& GetTransitionsMut() { return m_Transitions; }
    const std::string& GetDefaultState() const { return m_DefaultState; }
    const std::unordered_map<std::string, bool>& GetBoolParams() const { return m_BoolParams; }
    const std::unordered_map<std::string, f32>& GetFloatParams() const { return m_FloatParams; }
    const std::unordered_map<std::string, i32>& GetIntParams() const { return m_IntParams; }
    const std::unordered_map<std::string, bool>& GetTriggers() const { return m_Triggers; }

private:
    bool CheckTransitionConditions(const AnimationTransition& transition) const;

    SkeletalAnimator* m_Animator = nullptr;

    std::unordered_map<std::string, AnimationState> m_States;
    std::vector<AnimationTransition> m_Transitions;
    std::string m_DefaultState;
    std::string m_CurrentState;

    // Parameters
    std::unordered_map<std::string, bool> m_BoolParams;
    std::unordered_map<std::string, f32> m_FloatParams;
    std::unordered_map<std::string, i32> m_IntParams;
    std::unordered_map<std::string, bool> m_Triggers;
};

// ============================================================================
// Animation Utilities
// ============================================================================

namespace AnimationUtils {
    // Create a simple sprite animation from a sprite sheet
    ENJIN_API SpriteAnimation CreateFromSpriteSheet(
        const std::string& name,
        const std::string& texturePath,
        u32 frameWidth, u32 frameHeight,
        u32 frameCount,
        f32 framesPerSecond = 12.0f,
        u32 columns = 0  // 0 = auto-calculate
    );

    // Create a simple animation from individual frame positions
    ENJIN_API SpriteAnimation CreateFromFrames(
        const std::string& name,
        const std::string& texturePath,
        const std::vector<Math::Vector4>& frameRects,  // x, y, w, h for each frame
        f32 framesPerSecond = 12.0f
    );

    // Interpolation helpers
    ENJIN_API Math::Vector3 LerpPosition(const Math::Vector3& a, const Math::Vector3& b, f32 t);
    ENJIN_API Math::Quaternion SlerpRotation(const Math::Quaternion& a, const Math::Quaternion& b, f32 t);
    ENJIN_API Math::Vector3 LerpScale(const Math::Vector3& a, const Math::Vector3& b, f32 t);
}

} // namespace Animation
} // namespace Enjin
