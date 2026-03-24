#include "Enjin/Animation/Animation.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Animation {

// ============================================================================
// SpriteAnimator Implementation
// ============================================================================

void SpriteAnimator::AddAnimation(const SpriteAnimation& anim) {
    m_Animations[anim.name] = anim;
}

void SpriteAnimator::RemoveAnimation(const std::string& name) {
    if (m_CurrentAnimName == name) {
        Stop();
    }
    m_Animations.erase(name);
}

bool SpriteAnimator::HasAnimation(const std::string& name) const {
    return m_Animations.find(name) != m_Animations.end();
}

const SpriteAnimation* SpriteAnimator::GetAnimation(const std::string& name) const {
    auto it = m_Animations.find(name);
    return (it != m_Animations.end()) ? &it->second : nullptr;
}

void SpriteAnimator::Play(const std::string& name, bool restart) {
    auto it = m_Animations.find(name);
    if (it == m_Animations.end()) return;

    if (m_CurrentAnimName == name && !restart && m_IsPlaying) {
        return;  // Already playing this animation
    }

    m_CurrentAnimName = name;
    m_CurrentAnim = &it->second;
    m_CurrentFrameIndex = 0;
    m_FrameTimer = 0.0f;
    m_NormalizedTime = 0.0f;
    m_IsPlaying = true;
    m_IsPaused = false;
    m_IsFinished = false;
    m_IsReversing = false;
}

void SpriteAnimator::Stop() {
    m_IsPlaying = false;
    m_IsPaused = false;
    m_IsFinished = true;
    m_CurrentFrameIndex = 0;
    m_FrameTimer = 0.0f;
    m_NormalizedTime = 0.0f;
}

void SpriteAnimator::Pause() {
    m_IsPaused = true;
}

void SpriteAnimator::Resume() {
    m_IsPaused = false;
}

void SpriteAnimator::Update(f32 deltaTime) {
    if (!m_IsPlaying || m_IsPaused || !m_CurrentAnim || m_CurrentAnim->frames.empty()) {
        return;
    }

    f32 dt = deltaTime * m_Speed;
    if (m_IsReversing) dt = -dt;

    m_FrameTimer += dt;

    const SpriteFrame& currentFrame = m_CurrentAnim->frames[m_CurrentFrameIndex];

    // Check for frame advance
    while (m_FrameTimer >= currentFrame.duration) {
        if (currentFrame.duration <= 0.0f) { m_FrameTimer = 0.0f; break; }
        m_FrameTimer -= currentFrame.duration;

        // Trigger event
        if (!currentFrame.eventName.empty() && m_OnEvent) {
            m_OnEvent(currentFrame.eventName);
        }

        // Advance frame
        if (m_IsReversing) {
            if (m_CurrentFrameIndex == 0) {
                switch (m_CurrentAnim->playMode) {
                    case PlayMode::Once:
                        Stop();
                        return;
                    case PlayMode::Loop:
                        m_CurrentFrameIndex = static_cast<u32>(m_CurrentAnim->frames.size()) - 1;
                        break;
                    case PlayMode::PingPong:
                        m_IsReversing = false;
                        m_CurrentFrameIndex = 1;
                        break;
                    case PlayMode::ClampForever:
                        m_IsFinished = true;
                        m_FrameTimer = 0;
                        return;
                }
            } else {
                m_CurrentFrameIndex--;
            }
        } else {
            m_CurrentFrameIndex++;
            if (m_CurrentFrameIndex >= m_CurrentAnim->frames.size()) {
                switch (m_CurrentAnim->playMode) {
                    case PlayMode::Once:
                        Stop();
                        return;
                    case PlayMode::Loop:
                        m_CurrentFrameIndex = 0;
                        break;
                    case PlayMode::PingPong:
                        m_IsReversing = true;
                        m_CurrentFrameIndex = m_CurrentAnim->frames.size() > 2 ?
                            static_cast<u32>(m_CurrentAnim->frames.size()) - 2 : 0;
                        break;
                    case PlayMode::ClampForever:
                        m_CurrentFrameIndex = static_cast<u32>(m_CurrentAnim->frames.size()) - 1;
                        m_IsFinished = true;
                        m_FrameTimer = 0;
                        return;
                }
            }
        }
    }

    // Update normalized time
    f32 totalDuration = m_CurrentAnim->GetTotalDuration();
    if (totalDuration > 0.0f) {
        f32 elapsedTime = 0.0f;
        for (u32 i = 0; i < m_CurrentFrameIndex; ++i) {
            elapsedTime += m_CurrentAnim->frames[i].duration;
        }
        elapsedTime += m_FrameTimer;
        m_NormalizedTime = elapsedTime / totalDuration;
    }
}

const SpriteFrame* SpriteAnimator::GetCurrentFrame() const {
    if (!m_CurrentAnim || m_CurrentAnim->frames.empty()) {
        return nullptr;
    }
    if (m_CurrentFrameIndex >= m_CurrentAnim->frames.size()) {
        return nullptr;
    }
    return &m_CurrentAnim->frames[m_CurrentFrameIndex];
}

// ============================================================================
// BoneTrack Sampling
// ============================================================================

template<typename T>
static T SampleKeyframes(const std::vector<f32>& times, const std::vector<T>& values, f32 time,
                         T (*lerpFunc)(const T&, const T&, f32)) {
    if (times.size() != values.size() || times.empty()) return T{};
    if (values.size() == 1) return values[0];

    // Clamp time
    if (time <= times.front()) return values.front();
    if (time >= times.back()) return values.back();

    // Binary search for the keyframe interval
    auto it = std::upper_bound(times.begin(), times.end(), time);
    usize i = (it == times.begin()) ? 0 : static_cast<usize>(std::distance(times.begin(), it) - 1);
    if (i >= times.size() - 1) i = times.size() - 2;

    // Interpolate
    f32 t1 = times[i];
    f32 t2 = times[i + 1];
    f32 denom = t2 - t1;
    f32 t = (denom > 1e-7f) ? (time - t1) / denom : 0.0f;

    return lerpFunc(values[i], values[i + 1], t);
}

static Math::Vector3 LerpVec3(const Math::Vector3& a, const Math::Vector3& b, f32 t) {
    return Math::Lerp(a, b, t);
}

static Math::Quaternion SlerpQuat(const Math::Quaternion& a, const Math::Quaternion& b, f32 t) {
    return Math::Quaternion::Slerp(a, b, t);
}

Math::Vector3 BoneTrack::SamplePosition(f32 time) const {
    return SampleKeyframes(positionTimes, positions, time, LerpVec3);
}

Math::Quaternion BoneTrack::SampleRotation(f32 time) const {
    // Return identity quaternion instead of zero quaternion when keyframes are empty
    if (rotations.empty() || rotationTimes.size() != rotations.size()) {
        return Math::Quaternion(0, 0, 0, 1);
    }
    return SampleKeyframes(rotationTimes, rotations, time, SlerpQuat);
}

Math::Vector3 BoneTrack::SampleScale(f32 time) const {
    if (scales.empty()) return Math::Vector3(1, 1, 1);
    return SampleKeyframes(scaleTimes, scales, time, LerpVec3);
}

// ============================================================================
// SkeletalAnimator Implementation
// ============================================================================

void SkeletalAnimator::SetSkeleton(std::shared_ptr<Skeleton> skeleton) {
    m_Skeleton = skeleton;
    if (skeleton) {
        m_CurrentPose.Resize(skeleton->bones.size());
        m_BlendPose.Resize(skeleton->bones.size());

        // Initialize to bind pose.
        for (usize i = 0; i < skeleton->bones.size(); ++i) {
            m_CurrentPose.localPositions[i] = skeleton->bones[i].bindPosition;
            m_CurrentPose.localRotations[i] = skeleton->bones[i].bindRotation;
            m_CurrentPose.localScales[i] = skeleton->bones[i].bindScale;
        }

        // Compute proper bind-pose skinning matrices: worldTransform * inverseBindMatrix.
        // Bones must be topologically sorted (parent index < child index) for
        // CalculateWorldTransforms to work correctly. The importer guarantees
        // this ordering after the topological sort step.
        CalculateWorldTransforms();
        CalculateSkinningMatrices();
    }
}

void SkeletalAnimator::AddAnimation(const SkeletalAnimation& anim) {
    m_Animations[anim.name] = anim;

    // Resolve bone indices
    if (m_Skeleton) {
        auto& storedAnim = m_Animations[anim.name];
        for (auto& track : storedAnim.tracks) {
            track.boneIndex = m_Skeleton->FindBoneIndex(track.boneName);
        }
    }
}

void SkeletalAnimator::RemoveAnimation(const std::string& name) {
    if (m_CurrentAnimName == name) {
        Stop();
    }
    m_Animations.erase(name);
}

bool SkeletalAnimator::HasAnimation(const std::string& name) const {
    return m_Animations.find(name) != m_Animations.end();
}

void SkeletalAnimator::Play(const std::string& name, f32 blendTime) {
    auto it = m_Animations.find(name);
    if (it == m_Animations.end()) return;

    if (blendTime > 0.0f && m_IsPlaying) {
        CrossFade(name, blendTime);
        return;
    }

    m_CurrentAnimName = name;
    m_CurrentAnim = &it->second;
    m_CurrentTime = 0.0f;
    m_NormalizedTime = 0.0f;
    m_IsPlaying = true;
    m_IsPaused = false;
    m_BlendProgress = 1.0f;
}

void SkeletalAnimator::CrossFade(const std::string& name, f32 fadeTime) {
    auto it = m_Animations.find(name);
    if (it == m_Animations.end()) return;

    // Store current pose for blending
    m_BlendPose = m_CurrentPose;

    m_NextAnimName = name;
    m_NextAnim = &it->second;
    m_BlendTime = fadeTime;
    m_BlendProgress = 0.0f;
}

void SkeletalAnimator::Stop() {
    m_IsPlaying = false;
    m_IsPaused = false;
    // Preserve m_CurrentTime and m_NormalizedTime so the timeline slider
    // shows where the animation was when stopped.
}

void SkeletalAnimator::StopAndReset() {
    m_IsPlaying = false;
    m_IsPaused = false;
    m_CurrentTime = 0.0f;
    m_NormalizedTime = 0.0f;
    // Re-compute bind pose so the mesh snaps back to rest position
    if (m_Skeleton && m_CurrentAnim) {
        SkeletonPose bindPose;
        bindPose.Resize(m_Skeleton->bones.size());
        SampleAnimation(*m_CurrentAnim, 0.0f, bindPose);
        m_CurrentPose = bindPose;
        CalculateWorldTransforms();
        CalculateSkinningMatrices();
    }
}

void SkeletalAnimator::Pause() {
    m_IsPaused = true;
}

void SkeletalAnimator::Resume() {
    m_IsPaused = false;
}

void SkeletalAnimator::SetNormalizedTime(f32 t) {
    if (!m_CurrentAnim || !m_Skeleton) return;
    t = Math::Clamp(t, 0.0f, 1.0f);
    m_NormalizedTime = t;
    m_CurrentTime = t * m_CurrentAnim->duration;
    // Sample and update pose at the new time
    SkeletonPose sampledPose;
    sampledPose.Resize(m_Skeleton->bones.size());
    SampleAnimation(*m_CurrentAnim, m_CurrentTime, sampledPose);
    m_CurrentPose = sampledPose;
    CalculateWorldTransforms();
    CalculateSkinningMatrices();
}

void SkeletalAnimator::Update(f32 deltaTime) {
    if (!m_Skeleton || !m_IsPlaying || m_IsPaused) {
        return;
    }

    f32 dt = deltaTime * m_Speed;

    // Update blend if in progress
    if (m_BlendProgress < 1.0f && m_NextAnim) {
        m_BlendProgress += dt / m_BlendTime;
        if (m_BlendProgress >= 1.0f) {
            m_BlendProgress = 1.0f;
            m_CurrentAnimName = m_NextAnimName;
            m_CurrentAnim = m_NextAnim;
            m_NextAnim = nullptr;
            m_CurrentTime = m_BlendProgress * m_BlendTime;  // Approximate
        }
    }

    if (!m_CurrentAnim) return;

    // Track previous time for event firing
    m_PreviousTime = m_CurrentTime;

    // Update time
    m_CurrentTime += dt;
    f32 duration = m_CurrentAnim->duration;

    // Handle play modes
    bool looped = false;
    switch (m_CurrentAnim->playMode) {
        case PlayMode::Once:
            if (m_CurrentTime >= duration) {
                m_CurrentTime = duration;
                m_IsPlaying = false;
            }
            break;
        case PlayMode::Loop:
            if (duration <= 0.0f) { m_CurrentTime = 0.0f; break; }
            while (m_CurrentTime >= duration) {
                m_CurrentTime -= duration;
                looped = true;
            }
            break;
        case PlayMode::PingPong:
            if (m_PingPongForward) {
                if (m_CurrentTime >= duration) {
                    m_CurrentTime = 2.0f * duration - m_CurrentTime;
                    m_PingPongForward = false;
                }
            } else {
                // Playing backward: time was decremented by speed*dt
                // We advanced forward by dt, but in reverse we need to go backward
                // Undo the forward advance and subtract instead
                m_CurrentTime -= 2.0f * dt;
                if (m_CurrentTime <= 0.0f) {
                    m_CurrentTime = -m_CurrentTime;
                    m_PingPongForward = true;
                }
            }
            m_CurrentTime = Math::Clamp(m_CurrentTime, 0.0f, duration);
            break;
        case PlayMode::ClampForever:
            if (m_CurrentTime >= duration) {
                m_CurrentTime = duration;
            }
            break;
    }

    m_NormalizedTime = (duration > 0.0f) ? m_CurrentTime / duration : 0.0f;

    // Sample current animation
    SkeletonPose sampledPose;
    sampledPose.Resize(m_Skeleton->bones.size());
    SampleAnimation(*m_CurrentAnim, m_CurrentTime, sampledPose);

    // Blend if needed
    if (m_BlendProgress < 1.0f) {
        BlendPoses(m_BlendPose, sampledPose, m_BlendProgress, m_CurrentPose);
    } else {
        m_CurrentPose = sampledPose;
    }

    // Calculate world transforms
    CalculateWorldTransforms();
    CalculateSkinningMatrices();

    // Fire animation events that occurred between m_PreviousTime and m_CurrentTime.
    // Handles loop wrapping: if we looped, fire events from [prevTime, duration) and [0, currentTime).
    if (m_OnEvent && !m_CurrentAnim->events.empty()) {
        for (const auto& event : m_CurrentAnim->events) {
            bool shouldFire = false;
            if (looped) {
                // Wrapped around: check [prevTime, duration) and [0, currentTime)
                shouldFire = (event.time >= m_PreviousTime && event.time < duration) ||
                             (event.time >= 0.0f && event.time < m_CurrentTime);
            } else {
                shouldFire = (event.time >= m_PreviousTime && event.time < m_CurrentTime);
            }
            if (shouldFire) {
                m_OnEvent(event.name);
            }
        }
    }
}

void SkeletalAnimator::SampleAnimation(const SkeletalAnimation& anim, f32 time, SkeletonPose& outPose) {
    // Initialize to bind pose
    for (usize i = 0; i < m_Skeleton->bones.size(); ++i) {
        outPose.localPositions[i] = m_Skeleton->bones[i].bindPosition;
        outPose.localRotations[i] = m_Skeleton->bones[i].bindRotation;
        outPose.localScales[i] = m_Skeleton->bones[i].bindScale;
    }

    // Apply animation tracks
    for (const auto& track : anim.tracks) {
        if (track.boneIndex < 0 || track.boneIndex >= static_cast<i32>(outPose.localPositions.size())) {
            continue;
        }

        if (!track.positions.empty()) {
            outPose.localPositions[track.boneIndex] = track.SamplePosition(time);
        }
        if (!track.rotations.empty()) {
            outPose.localRotations[track.boneIndex] = track.SampleRotation(time);
        }
        if (!track.scales.empty()) {
            outPose.localScales[track.boneIndex] = track.SampleScale(time);
        }
    }
}

void SkeletalAnimator::BlendPoses(const SkeletonPose& a, const SkeletonPose& b, f32 t, SkeletonPose& out) {
    usize count = a.localPositions.size();
    for (usize i = 0; i < count; ++i) {
        out.localPositions[i] = Math::Lerp(a.localPositions[i], b.localPositions[i], t);
        out.localRotations[i] = Math::Quaternion::Slerp(a.localRotations[i], b.localRotations[i], t);
        out.localScales[i] = Math::Lerp(a.localScales[i], b.localScales[i], t);
    }
}

void SkeletalAnimator::CalculateWorldTransforms() {
    if (!m_Skeleton) return;

    for (usize i = 0; i < m_Skeleton->bones.size(); ++i) {
        const Bone& bone = m_Skeleton->bones[i];

        // Build local transform
        Math::Matrix4 local = Math::Matrix4::Identity();
        local = Math::Matrix4::Translation(m_CurrentPose.localPositions[i]);
        local = local * m_CurrentPose.localRotations[i].ToMatrix4();
        local = local * Math::Matrix4::Scale(m_CurrentPose.localScales[i]);

        // Multiply by parent
        if (bone.parentIndex >= 0) {
            m_CurrentPose.worldTransforms[i] = m_CurrentPose.worldTransforms[bone.parentIndex] * local;
        } else {
            m_CurrentPose.worldTransforms[i] = local;
        }
    }
}

void SkeletalAnimator::CalculateSkinningMatrices() {
    if (!m_Skeleton) return;

    for (usize i = 0; i < m_Skeleton->bones.size(); ++i) {
        // Standard GPU skinning: worldTransform * inverseBindMatrix
        // Transforms vertices from bind-pose bone space to current world space.
        m_CurrentPose.skinningMatrices[i] = m_CurrentPose.worldTransforms[i] * m_Skeleton->bones[i].inverseBindMatrix;
    }
}

std::vector<Math::Matrix4> SkeletalAnimator::SampleSkinningMatricesAtTime(f32 time) const {
    if (!m_Skeleton || !m_CurrentAnim) return {};

    usize boneCount = m_Skeleton->bones.size();
    if (boneCount == 0) return {};

    // Sample animation into a temporary pose
    SkeletonPose tempPose;
    tempPose.Resize(boneCount);

    // Initialize to bind pose
    for (usize i = 0; i < boneCount; ++i) {
        tempPose.localPositions[i] = m_Skeleton->bones[i].bindPosition;
        tempPose.localRotations[i] = m_Skeleton->bones[i].bindRotation;
        tempPose.localScales[i] = m_Skeleton->bones[i].bindScale;
    }

    // Apply animation tracks at the requested time
    for (const auto& track : m_CurrentAnim->tracks) {
        if (track.boneIndex < 0 || track.boneIndex >= static_cast<i32>(boneCount)) continue;
        if (!track.positions.empty()) {
            tempPose.localPositions[track.boneIndex] = track.SamplePosition(time);
        }
        if (!track.rotations.empty()) {
            tempPose.localRotations[track.boneIndex] = track.SampleRotation(time);
        }
        if (!track.scales.empty()) {
            tempPose.localScales[track.boneIndex] = track.SampleScale(time);
        }
    }

    // Calculate world transforms
    for (usize i = 0; i < boneCount; ++i) {
        const Bone& bone = m_Skeleton->bones[i];
        Math::Matrix4 local = Math::Matrix4::Translation(tempPose.localPositions[i]);
        local = local * tempPose.localRotations[i].ToMatrix4();
        local = local * Math::Matrix4::Scale(tempPose.localScales[i]);

        if (bone.parentIndex >= 0) {
            tempPose.worldTransforms[i] = tempPose.worldTransforms[bone.parentIndex] * local;
        } else {
            tempPose.worldTransforms[i] = local;
        }
    }

    // Calculate skinning matrices
    std::vector<Math::Matrix4> result(boneCount);
    for (usize i = 0; i < boneCount; ++i) {
        result[i] = tempPose.worldTransforms[i] * m_Skeleton->bones[i].inverseBindMatrix;
    }

    return result;
}

void SkeletalAnimator::UpdateBlendTree(const BlendTree& blendTree, f32 paramValue, f32 deltaTime) {
    if (!m_Skeleton) return;

    f32 dt = deltaTime * m_Speed;
    usize boneCount = m_Skeleton->bones.size();

    // Evaluate which two animations to blend
    std::string animNameA, animNameB;
    f32 blendFactor = 0.0f;
    blendTree.Evaluate(paramValue, animNameA, animNameB, blendFactor);

    // Look up animations
    const SkeletalAnimation* animA = nullptr;
    const SkeletalAnimation* animB = nullptr;
    if (!animNameA.empty()) {
        auto itA = m_Animations.find(animNameA);
        if (itA != m_Animations.end()) animA = &itA->second;
    }
    if (!animNameB.empty()) {
        auto itB = m_Animations.find(animNameB);
        if (itB != m_Animations.end()) animB = &itB->second;
    }

    if (!animA) return;  // No valid animation

    // Advance time (use the primary animation's duration for the timeline)
    m_CurrentTime += dt;
    f32 durationA = animA->duration;
    if (durationA > 0.0f) {
        while (m_CurrentTime >= durationA) m_CurrentTime -= durationA;
        while (m_CurrentTime < 0.0f) m_CurrentTime += durationA;
    } else {
        m_CurrentTime = 0.0f;
    }
    m_NormalizedTime = (durationA > 0.0f) ? m_CurrentTime / durationA : 0.0f;

    // Ensure scratch poses are sized
    m_BlendTreePoseA.Resize(boneCount);

    // Sample animation A
    SampleAnimation(*animA, m_CurrentTime, m_BlendTreePoseA);

    if (animB && blendFactor > 0.0f) {
        // Sample animation B at corresponding normalized time
        f32 durationB = animB->duration;
        f32 timeB = (durationB > 0.0f) ? m_NormalizedTime * durationB : 0.0f;

        m_BlendTreePoseB.Resize(boneCount);
        SampleAnimation(*animB, timeB, m_BlendTreePoseB);

        // Blend between A and B
        m_CurrentPose.Resize(boneCount);
        BlendPoses(m_BlendTreePoseA, m_BlendTreePoseB, blendFactor, m_CurrentPose);
    } else {
        m_CurrentPose = m_BlendTreePoseA;
    }

    // Recalculate transforms
    CalculateWorldTransforms();
    CalculateSkinningMatrices();

    // Mark as playing for inspector display
    m_IsPlaying = true;

    // Fire events from primary animation
    if (m_OnEvent && !animA->events.empty()) {
        f32 prevTime = m_CurrentTime - dt;
        bool looped = (prevTime < 0.0f);
        if (looped) prevTime += durationA;
        for (const auto& event : animA->events) {
            bool shouldFire = false;
            if (looped) {
                shouldFire = (event.time >= prevTime && event.time < durationA) ||
                             (event.time >= 0.0f && event.time < m_CurrentTime);
            } else {
                shouldFire = (event.time >= prevTime && event.time < m_CurrentTime);
            }
            if (shouldFire) {
                m_OnEvent(event.name);
            }
        }
    }
}

void SkeletalAnimator::SetBoneLocalRotation(const std::string& boneName, const Math::Quaternion& rotation) {
    if (!m_Skeleton) return;
    i32 idx = m_Skeleton->FindBoneIndex(boneName);
    if (idx >= 0) {
        m_CurrentPose.localRotations[idx] = rotation;
    }
}

void SkeletalAnimator::SetBoneLocalPosition(const std::string& boneName, const Math::Vector3& position) {
    if (!m_Skeleton) return;
    i32 idx = m_Skeleton->FindBoneIndex(boneName);
    if (idx >= 0) {
        m_CurrentPose.localPositions[idx] = position;
    }
}

Math::Matrix4 SkeletalAnimator::GetBoneWorldTransform(const std::string& boneName) const {
    if (!m_Skeleton) return Math::Matrix4::Identity();
    i32 idx = m_Skeleton->FindBoneIndex(boneName);
    return GetBoneWorldTransform(idx);
}

Math::Matrix4 SkeletalAnimator::GetBoneWorldTransform(i32 boneIndex) const {
    if (boneIndex < 0 || boneIndex >= static_cast<i32>(m_CurrentPose.worldTransforms.size())) {
        return Math::Matrix4::Identity();
    }
    return m_CurrentPose.worldTransforms[boneIndex];
}

// ============================================================================
// AnimationStateMachine Implementation
// ============================================================================

void AnimationStateMachine::AddState(const AnimationState& state) {
    m_States[state.name] = state;
    if (m_DefaultState.empty()) {
        m_DefaultState = state.name;
    }
}

void AnimationStateMachine::RemoveState(const std::string& name) {
    m_States.erase(name);
}

void AnimationStateMachine::AddTransition(const AnimationTransition& transition) {
    m_Transitions.push_back(transition);
}

void AnimationStateMachine::RemoveTransition(const std::string& from, const std::string& to) {
    m_Transitions.erase(
        std::remove_if(m_Transitions.begin(), m_Transitions.end(),
            [&from, &to](const AnimationTransition& t) {
                return t.fromState == from && t.toState == to;
            }),
        m_Transitions.end()
    );
}

void AnimationStateMachine::SetBool(const std::string& name, bool value) {
    m_BoolParams[name] = value;
}

void AnimationStateMachine::SetFloat(const std::string& name, f32 value) {
    m_FloatParams[name] = value;
}

void AnimationStateMachine::SetInt(const std::string& name, i32 value) {
    m_IntParams[name] = value;
}

void AnimationStateMachine::SetTrigger(const std::string& name) {
    m_Triggers[name] = true;
}

void AnimationStateMachine::ResetTrigger(const std::string& name) {
    m_Triggers[name] = false;
}

bool AnimationStateMachine::GetBool(const std::string& name) const {
    auto it = m_BoolParams.find(name);
    return (it != m_BoolParams.end()) ? it->second : false;
}

f32 AnimationStateMachine::GetFloat(const std::string& name) const {
    auto it = m_FloatParams.find(name);
    return (it != m_FloatParams.end()) ? it->second : 0.0f;
}

i32 AnimationStateMachine::GetInt(const std::string& name) const {
    auto it = m_IntParams.find(name);
    return (it != m_IntParams.end()) ? it->second : 0;
}

void AnimationStateMachine::Update(f32 deltaTime) {
    (void)deltaTime;

    if (!m_Animator) return;

    // Initialize if needed
    if (m_CurrentState.empty() && !m_DefaultState.empty()) {
        m_CurrentState = m_DefaultState;
        auto it = m_States.find(m_CurrentState);
        if (it != m_States.end()) {
            m_Animator->Play(it->second.animationName, 0.0f);
        }
    }

    // Check transitions
    for (const auto& transition : m_Transitions) {
        if (transition.fromState != m_CurrentState) continue;

        // Check exit time if required
        if (transition.hasExitTime) {
            if (m_Animator->GetNormalizedTime() < transition.exitTime) {
                continue;
            }
        }

        // Check conditions
        if (CheckTransitionConditions(transition)) {
            // Transition!
            m_CurrentState = transition.toState;
            auto it = m_States.find(m_CurrentState);
            if (it != m_States.end()) {
                m_Animator->CrossFade(it->second.animationName, transition.blendTime);
            }

            // Reset triggers used in this transition
            for (const auto& cond : transition.conditions) {
                if (cond.type == TransitionCondition::Type::Trigger) {
                    m_Triggers[cond.parameterName] = false;
                }
            }

            break;  // Only one transition per frame
        }
    }
}

bool AnimationStateMachine::CheckTransitionConditions(const AnimationTransition& transition) const {
    for (const auto& cond : transition.conditions) {
        bool conditionMet = false;

        switch (cond.type) {
            case TransitionCondition::Type::Bool: {
                bool value = GetBool(cond.parameterName);
                switch (cond.comparison) {
                    case TransitionCondition::Comparison::Equal:
                        conditionMet = (value == cond.value.boolValue);
                        break;
                    case TransitionCondition::Comparison::NotEqual:
                        conditionMet = (value != cond.value.boolValue);
                        break;
                    default:
                        conditionMet = false;
                        break;
                }
                break;
            }

            case TransitionCondition::Type::Float: {
                f32 value = GetFloat(cond.parameterName);
                switch (cond.comparison) {
                    case TransitionCondition::Comparison::Equal:
                        conditionMet = Math::Abs(value - cond.value.floatValue) < 0.001f;
                        break;
                    case TransitionCondition::Comparison::NotEqual:
                        conditionMet = Math::Abs(value - cond.value.floatValue) >= 0.001f;
                        break;
                    case TransitionCondition::Comparison::Greater:
                        conditionMet = value > cond.value.floatValue;
                        break;
                    case TransitionCondition::Comparison::Less:
                        conditionMet = value < cond.value.floatValue;
                        break;
                    case TransitionCondition::Comparison::GreaterEqual:
                        conditionMet = value >= cond.value.floatValue;
                        break;
                    case TransitionCondition::Comparison::LessEqual:
                        conditionMet = value <= cond.value.floatValue;
                        break;
                }
                break;
            }

            case TransitionCondition::Type::Int: {
                i32 value = GetInt(cond.parameterName);
                switch (cond.comparison) {
                    case TransitionCondition::Comparison::Equal:
                        conditionMet = (value == cond.value.intValue);
                        break;
                    case TransitionCondition::Comparison::NotEqual:
                        conditionMet = (value != cond.value.intValue);
                        break;
                    case TransitionCondition::Comparison::Greater:
                        conditionMet = value > cond.value.intValue;
                        break;
                    case TransitionCondition::Comparison::Less:
                        conditionMet = value < cond.value.intValue;
                        break;
                    case TransitionCondition::Comparison::GreaterEqual:
                        conditionMet = value >= cond.value.intValue;
                        break;
                    case TransitionCondition::Comparison::LessEqual:
                        conditionMet = value <= cond.value.intValue;
                        break;
                }
                break;
            }

            case TransitionCondition::Type::Trigger: {
                auto it = m_Triggers.find(cond.parameterName);
                conditionMet = (it != m_Triggers.end() && it->second);
                break;
            }
        }

        if (!conditionMet) return false;  // All conditions must be met
    }

    return true;
}

// ============================================================================
// Animation Utilities
// ============================================================================

namespace AnimationUtils {

SpriteAnimation CreateFromSpriteSheet(
    const std::string& name,
    const std::string& texturePath,
    u32 frameWidth, u32 frameHeight,
    u32 frameCount,
    f32 framesPerSecond,
    u32 columns) {

    SpriteAnimation anim;
    anim.name = name;
    anim.texturePath = texturePath;

    f32 frameDuration = 1.0f / framesPerSecond;

    // Auto-calculate columns if not specified
    // (Assumes texture width can be inferred or is a common size)
    if (columns == 0) {
        columns = frameCount;  // Default: all in one row
    }

    for (u32 i = 0; i < frameCount; ++i) {
        SpriteFrame frame;
        frame.srcX = static_cast<f32>((i % columns) * frameWidth);
        frame.srcY = static_cast<f32>((i / columns) * frameHeight);
        frame.srcWidth = static_cast<f32>(frameWidth);
        frame.srcHeight = static_cast<f32>(frameHeight);
        frame.duration = frameDuration;
        anim.frames.push_back(frame);
    }

    return anim;
}

SpriteAnimation CreateFromFrames(
    const std::string& name,
    const std::string& texturePath,
    const std::vector<Math::Vector4>& frameRects,
    f32 framesPerSecond) {

    SpriteAnimation anim;
    anim.name = name;
    anim.texturePath = texturePath;

    f32 frameDuration = 1.0f / framesPerSecond;

    for (const auto& rect : frameRects) {
        SpriteFrame frame;
        frame.srcX = rect.x;
        frame.srcY = rect.y;
        frame.srcWidth = rect.z;
        frame.srcHeight = rect.w;
        frame.duration = frameDuration;
        anim.frames.push_back(frame);
    }

    return anim;
}

Math::Vector3 LerpPosition(const Math::Vector3& a, const Math::Vector3& b, f32 t) {
    return Math::Lerp(a, b, t);
}

Math::Quaternion SlerpRotation(const Math::Quaternion& a, const Math::Quaternion& b, f32 t) {
    return Math::Quaternion::Slerp(a, b, t);
}

Math::Vector3 LerpScale(const Math::Vector3& a, const Math::Vector3& b, f32 t) {
    return Math::Lerp(a, b, t);
}

} // namespace AnimationUtils

} // namespace Animation
} // namespace Enjin
