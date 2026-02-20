#include "EnjinTest.h"
#include "Enjin/Animation/Animation.h"

using namespace Enjin;
using namespace Enjin::Animation;

// ===========================================================================
// PlayMode & BlendMode Enums
// ===========================================================================

ENJIN_TEST(Enums, PlayModeValues) {
    ENJIN_EXPECT_EQ((int)PlayMode::Once, 0);
    ENJIN_EXPECT_EQ((int)PlayMode::Loop, 1);
    ENJIN_EXPECT_EQ((int)PlayMode::PingPong, 2);
    ENJIN_EXPECT_EQ((int)PlayMode::ClampForever, 3);
}

ENJIN_TEST(Enums, BlendModeValues) {
    ENJIN_EXPECT_EQ((int)BlendMode::Replace, 0);
    ENJIN_EXPECT_EQ((int)BlendMode::Additive, 1);
    ENJIN_EXPECT_EQ((int)BlendMode::Blend, 2);
}

// ===========================================================================
// SpriteFrame Defaults
// ===========================================================================

ENJIN_TEST(SpriteFrame, Defaults) {
    SpriteFrame frame;
    ENJIN_EXPECT_FLOAT_EQ(frame.srcX, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(frame.srcY, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(frame.srcWidth, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(frame.srcHeight, 0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(frame.duration, 0.1f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(frame.pivot.x, 0.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(frame.pivot.y, 0.5f, 0.001f);
    ENJIN_EXPECT_TRUE(frame.eventName.empty());
}

// ===========================================================================
// SpriteAnimation
// ===========================================================================

ENJIN_TEST(SpriteAnim, DefaultPlayMode) {
    SpriteAnimation anim;
    ENJIN_EXPECT_EQ((int)anim.playMode, (int)PlayMode::Loop);
    ENJIN_EXPECT_TRUE(anim.name.empty());
    ENJIN_EXPECT_TRUE(anim.texturePath.empty());
    ENJIN_EXPECT_EQ(anim.frames.size(), (size_t)0);
}

ENJIN_TEST(SpriteAnim, GetTotalDuration) {
    SpriteAnimation anim;
    SpriteFrame f1, f2, f3;
    f1.duration = 0.1f;
    f2.duration = 0.2f;
    f3.duration = 0.3f;
    anim.frames = {f1, f2, f3};
    ENJIN_EXPECT_FLOAT_NEAR(anim.GetTotalDuration(), 0.6f, 0.001f);
}

ENJIN_TEST(SpriteAnim, EmptyDuration) {
    SpriteAnimation anim;
    ENJIN_EXPECT_FLOAT_EQ(anim.GetTotalDuration(), 0.0f);
}

// ===========================================================================
// SpriteAnimator
// ===========================================================================

ENJIN_TEST(SpriteAnimator, InitialState) {
    SpriteAnimator animator;
    ENJIN_EXPECT_FALSE(animator.IsPlaying());
    ENJIN_EXPECT_FALSE(animator.IsFinished());
    ENJIN_EXPECT_FLOAT_EQ(animator.GetSpeed(), 1.0f);
    ENJIN_EXPECT_TRUE(animator.GetCurrentAnimationName().empty());
    ENJIN_EXPECT_TRUE(animator.GetCurrentFrame() == nullptr);
}

ENJIN_TEST(SpriteAnimator, AddAndPlay) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "walk";
    SpriteFrame f;
    f.duration = 0.1f;
    anim.frames.push_back(f);
    animator.AddAnimation(anim);

    ENJIN_EXPECT_TRUE(animator.HasAnimation("walk"));
    animator.Play("walk");
    ENJIN_EXPECT_TRUE(animator.IsPlaying());
    ENJIN_EXPECT_STR_EQ(animator.GetCurrentAnimationName().c_str(), "walk");
}

ENJIN_TEST(SpriteAnimator, StopAndPause) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "idle";
    SpriteFrame f;
    f.duration = 1.0f;
    anim.frames.push_back(f);
    animator.AddAnimation(anim);

    animator.Play("idle");
    animator.Pause();
    ENJIN_EXPECT_TRUE(animator.IsPlaying()); // Paused but still "playing"

    animator.Resume();
    animator.Stop();
    ENJIN_EXPECT_FALSE(animator.IsPlaying());
}

ENJIN_TEST(SpriteAnimator, SpeedControl) {
    SpriteAnimator animator;
    ENJIN_EXPECT_FLOAT_EQ(animator.GetSpeed(), 1.0f);
    animator.SetSpeed(2.0f);
    ENJIN_EXPECT_FLOAT_EQ(animator.GetSpeed(), 2.0f);
}

ENJIN_TEST(SpriteAnimator, FrameAdvance) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "run";
    anim.playMode = PlayMode::Loop;
    SpriteFrame f1, f2;
    f1.duration = 0.1f;
    f2.duration = 0.1f;
    anim.frames = {f1, f2};
    animator.AddAnimation(anim);

    animator.Play("run");
    ENJIN_EXPECT_EQ(animator.GetCurrentFrameIndex(), 0u);

    // Advance past first frame
    animator.Update(0.15f);
    ENJIN_EXPECT_EQ(animator.GetCurrentFrameIndex(), 1u);
}

ENJIN_TEST(SpriteAnimator, PlayModeOnceFinishes) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "die";
    anim.playMode = PlayMode::Once;
    SpriteFrame f;
    f.duration = 0.1f;
    anim.frames.push_back(f);
    animator.AddAnimation(anim);

    animator.Play("die");
    animator.Update(0.2f); // Past duration
    ENJIN_EXPECT_TRUE(animator.IsFinished());
}

ENJIN_TEST(SpriteAnimator, RemoveAnimation) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "test";
    SpriteFrame f;
    f.duration = 0.1f;
    anim.frames.push_back(f);
    animator.AddAnimation(anim);
    ENJIN_EXPECT_TRUE(animator.HasAnimation("test"));
    animator.RemoveAnimation("test");
    ENJIN_EXPECT_FALSE(animator.HasAnimation("test"));
}

// ===========================================================================
// Bone & Skeleton
// ===========================================================================

ENJIN_TEST(Bone, Defaults) {
    Bone bone;
    ENJIN_EXPECT_EQ(bone.parentIndex, -1);
    ENJIN_EXPECT_FLOAT_EQ(bone.bindScale.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(bone.bindScale.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(bone.bindScale.z, 1.0f);
}

ENJIN_TEST(Skeleton, FindBoneIndex) {
    Skeleton skel;
    Bone b1, b2, b3;
    b1.name = "root";
    b2.name = "spine";
    b3.name = "head";
    skel.bones = {b1, b2, b3};

    ENJIN_EXPECT_EQ(skel.FindBoneIndex("root"), 0);
    ENJIN_EXPECT_EQ(skel.FindBoneIndex("head"), 2);
    ENJIN_EXPECT_EQ(skel.FindBoneIndex("nonexistent"), -1);
}

// ===========================================================================
// BoneTrack Keyframe Sampling
// ===========================================================================

ENJIN_TEST(BoneTrack, Defaults) {
    BoneTrack track;
    ENJIN_EXPECT_EQ(track.boneIndex, -1);
    ENJIN_EXPECT_TRUE(track.boneName.empty());
    ENJIN_EXPECT_EQ(track.positionTimes.size(), (size_t)0);
}

ENJIN_TEST(BoneTrack, SamplePositionSingleKey) {
    BoneTrack track;
    track.positionTimes = {0.0f};
    track.positions = {Math::Vector3(1, 2, 3)};

    Math::Vector3 p = track.SamplePosition(0.5f);
    ENJIN_EXPECT_FLOAT_EQ(p.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.y, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.z, 3.0f);
}

ENJIN_TEST(BoneTrack, SamplePositionInterpolation) {
    BoneTrack track;
    track.positionTimes = {0.0f, 1.0f};
    track.positions = {Math::Vector3(0, 0, 0), Math::Vector3(10, 0, 0)};

    Math::Vector3 mid = track.SamplePosition(0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.x, 5.0f, 0.1f);
}

ENJIN_TEST(BoneTrack, SamplePositionClampsBefore) {
    BoneTrack track;
    track.positionTimes = {1.0f, 2.0f};
    track.positions = {Math::Vector3(5, 0, 0), Math::Vector3(10, 0, 0)};

    Math::Vector3 p = track.SamplePosition(0.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.x, 5.0f);
}

ENJIN_TEST(BoneTrack, SamplePositionClampsAfter) {
    BoneTrack track;
    track.positionTimes = {0.0f, 1.0f};
    track.positions = {Math::Vector3(0, 0, 0), Math::Vector3(10, 0, 0)};

    Math::Vector3 p = track.SamplePosition(5.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.x, 10.0f);
}

// ===========================================================================
// SkeletalAnimation Defaults
// ===========================================================================

ENJIN_TEST(SkelAnim, Defaults) {
    SkeletalAnimation anim;
    ENJIN_EXPECT_FLOAT_EQ(anim.duration, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(anim.ticksPerSecond, 30.0f);
    ENJIN_EXPECT_EQ((int)anim.playMode, (int)PlayMode::Loop);
    ENJIN_EXPECT_EQ(anim.tracks.size(), (size_t)0);
    ENJIN_EXPECT_EQ(anim.events.size(), (size_t)0);
}

// ===========================================================================
// SkeletonPose
// ===========================================================================

ENJIN_TEST(Pose, Resize) {
    SkeletonPose pose;
    pose.Resize(3);
    ENJIN_EXPECT_EQ(pose.localPositions.size(), (size_t)3);
    ENJIN_EXPECT_EQ(pose.localRotations.size(), (size_t)3);
    ENJIN_EXPECT_EQ(pose.localScales.size(), (size_t)3);
    // Scales should initialize to (1,1,1)
    ENJIN_EXPECT_FLOAT_EQ(pose.localScales[0].x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(pose.localScales[2].y, 1.0f);
}

// ===========================================================================
// SkeletalAnimator
// ===========================================================================

ENJIN_TEST(SkelAnimator, InitialState) {
    SkeletalAnimator animator;
    ENJIN_EXPECT_FALSE(animator.IsPlaying());
    ENJIN_EXPECT_FALSE(animator.IsBlending());
    ENJIN_EXPECT_FLOAT_EQ(animator.GetSpeed(), 1.0f);
    ENJIN_EXPECT_TRUE(animator.GetCurrentAnimationName().empty());
}

ENJIN_TEST(SkelAnimator, SetSkeleton) {
    SkeletalAnimator animator;
    auto skel = std::make_shared<Skeleton>();
    Bone root;
    root.name = "root";
    root.bindScale = Math::Vector3(1, 1, 1);
    skel->bones.push_back(root);
    animator.SetSkeleton(skel);
    ENJIN_EXPECT_TRUE(animator.GetSkeleton() != nullptr);
    ENJIN_EXPECT_EQ(animator.GetSkeleton()->bones.size(), (size_t)1);
}

ENJIN_TEST(SkelAnimator, AddAndPlay) {
    SkeletalAnimator animator;
    auto skel = std::make_shared<Skeleton>();
    Bone root;
    root.name = "root";
    root.bindScale = Math::Vector3(1, 1, 1);
    skel->bones.push_back(root);
    animator.SetSkeleton(skel);

    SkeletalAnimation anim;
    anim.name = "walk";
    anim.duration = 1.0f;
    animator.AddAnimation(anim);

    animator.Play("walk");
    ENJIN_EXPECT_TRUE(animator.IsPlaying());
    ENJIN_EXPECT_STR_EQ(animator.GetCurrentAnimationName().c_str(), "walk");
}

ENJIN_TEST(SkelAnimator, SpeedControl) {
    SkeletalAnimator animator;
    animator.SetSpeed(0.5f);
    ENJIN_EXPECT_FLOAT_EQ(animator.GetSpeed(), 0.5f);
}

// ===========================================================================
// AnimationStateMachine
// ===========================================================================

ENJIN_TEST(StateMachine, ParameterSetGet) {
    AnimationStateMachine sm;
    sm.SetBool("grounded", true);
    sm.SetFloat("speed", 5.0f);
    sm.SetInt("health", 100);

    ENJIN_EXPECT_TRUE(sm.GetBool("grounded"));
    ENJIN_EXPECT_FLOAT_EQ(sm.GetFloat("speed"), 5.0f);
    ENJIN_EXPECT_EQ(sm.GetInt("health"), 100);
}

ENJIN_TEST(StateMachine, TriggerSetReset) {
    AnimationStateMachine sm;
    sm.SetTrigger("jump");
    // Trigger should be set (implementation detail — check via transitions)
    sm.ResetTrigger("jump");
    // Should not crash
}

ENJIN_TEST(StateMachine, AddStatesAndDefault) {
    AnimationStateMachine sm;
    AnimationState idle;
    idle.name = "Idle";
    idle.animationName = "idle_anim";
    idle.speed = 1.0f;
    sm.AddState(idle);
    sm.SetDefaultState("Idle");

    ENJIN_EXPECT_STR_EQ(sm.GetDefaultState().c_str(), "Idle");
    ENJIN_EXPECT_EQ(sm.GetStates().size(), (size_t)1);
}

ENJIN_TEST(StateMachine, TransitionDefaults) {
    AnimationTransition t;
    ENJIN_EXPECT_FLOAT_NEAR(t.blendTime, 0.2f, 0.01f);
    ENJIN_EXPECT_FALSE(t.hasExitTime);
    ENJIN_EXPECT_FLOAT_EQ(t.exitTime, 1.0f);
    ENJIN_EXPECT_EQ(t.conditions.size(), (size_t)0);
}

ENJIN_TEST(StateMachine, ConditionDefaults) {
    TransitionCondition cond;
    ENJIN_EXPECT_EQ((int)cond.type, (int)TransitionCondition::Type::Bool);
    ENJIN_EXPECT_EQ((int)cond.comparison, (int)TransitionCondition::Comparison::Equal);
}

ENJIN_TEST(StateMachine, AnimationStateDefaults) {
    AnimationState state;
    ENJIN_EXPECT_FLOAT_EQ(state.speed, 1.0f);
    ENJIN_EXPECT_EQ((int)state.playMode, (int)PlayMode::Loop);
}

// ===========================================================================
// AnimationUtils
// ===========================================================================

ENJIN_TEST(Utils, CreateFromSpriteSheet) {
    auto anim = AnimationUtils::CreateFromSpriteSheet(
        "walk", "sprites.png", 32, 32, 4, 12.0f, 4);
    ENJIN_EXPECT_STR_EQ(anim.name.c_str(), "walk");
    ENJIN_EXPECT_EQ(anim.frames.size(), (size_t)4);
    ENJIN_EXPECT_FLOAT_NEAR(anim.frames[0].srcWidth, 32.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(anim.frames[0].srcHeight, 32.0f, 0.1f);
}

ENJIN_TEST(Utils, LerpPosition) {
    Math::Vector3 a(0, 0, 0), b(10, 20, 30);
    Math::Vector3 mid = AnimationUtils::LerpPosition(a, b, 0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.x, 5.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.y, 10.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.z, 15.0f, 0.1f);
}

ENJIN_TEST_MAIN()
