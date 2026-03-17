#include "EnjinTest.h"
#include "Enjin/Animation/Animation.h"

using namespace Enjin;
using namespace Enjin::Animation;

// ===========================================================================
// SpriteFrame Defaults
// ===========================================================================

ENJIN_TEST(SpriteFrame, Defaults) {
    SpriteFrame frame;
    ENJIN_EXPECT_FLOAT_EQ(frame.srcX, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(frame.srcY, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(frame.duration, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(frame.pivot.x, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(frame.pivot.y, 0.5f);
}

// ===========================================================================
// SpriteAnimation
// ===========================================================================

ENJIN_TEST(SpriteAnim, EmptyDuration) {
    SpriteAnimation anim;
    ENJIN_EXPECT_FLOAT_EQ(anim.GetTotalDuration(), 0.0f);
}

ENJIN_TEST(SpriteAnim, TotalDuration) {
    SpriteAnimation anim;
    SpriteFrame f1, f2, f3;
    f1.duration = 0.1f;
    f2.duration = 0.2f;
    f3.duration = 0.3f;
    anim.frames = {f1, f2, f3};
    ENJIN_EXPECT_FLOAT_NEAR(anim.GetTotalDuration(), 0.6f, 0.001f);
}

ENJIN_TEST(SpriteAnim, DefaultPlayMode) {
    SpriteAnimation anim;
    ENJIN_EXPECT_EQ((int)anim.playMode, (int)PlayMode::Loop);
}

// ===========================================================================
// SpriteAnimator
// ===========================================================================

ENJIN_TEST(SpriteAnimator, InitialState) {
    SpriteAnimator animator;
    ENJIN_EXPECT_FALSE(animator.IsPlaying());
    ENJIN_EXPECT_FALSE(animator.IsFinished());
    ENJIN_EXPECT_FLOAT_EQ(animator.GetSpeed(), 1.0f);
}

ENJIN_TEST(SpriteAnimator, AddAnimation) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "walk";
    animator.AddAnimation(anim);
    ENJIN_EXPECT_TRUE(animator.HasAnimation("walk"));
    ENJIN_EXPECT_FALSE(animator.HasAnimation("run"));
}

ENJIN_TEST(SpriteAnimator, RemoveAnimation) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "idle";
    animator.AddAnimation(anim);
    animator.RemoveAnimation("idle");
    ENJIN_EXPECT_FALSE(animator.HasAnimation("idle"));
}

ENJIN_TEST(SpriteAnimator, PlaySetsPlaying) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "run";
    SpriteFrame f;
    f.duration = 0.1f;
    anim.frames = {f, f, f};
    animator.AddAnimation(anim);
    animator.Play("run");
    ENJIN_EXPECT_TRUE(animator.IsPlaying());
    ENJIN_EXPECT_STR_EQ(animator.GetCurrentAnimationName(), "run");
}

ENJIN_TEST(SpriteAnimator, Stop) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "test";
    SpriteFrame f;
    f.duration = 0.1f;
    anim.frames = {f};
    animator.AddAnimation(anim);
    animator.Play("test");
    animator.Stop();
    ENJIN_EXPECT_FALSE(animator.IsPlaying());
}

ENJIN_TEST(SpriteAnimator, PauseResume) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "anim";
    SpriteFrame f;
    f.duration = 1.0f;
    anim.frames = {f, f};
    animator.AddAnimation(anim);
    animator.Play("anim");
    animator.Pause();
    // Should still be "playing" conceptually but paused
    animator.Resume();
    ENJIN_EXPECT_TRUE(animator.IsPlaying());
}

ENJIN_TEST(SpriteAnimator, SetSpeed) {
    SpriteAnimator animator;
    animator.SetSpeed(2.0f);
    ENJIN_EXPECT_FLOAT_EQ(animator.GetSpeed(), 2.0f);
}

ENJIN_TEST(SpriteAnimator, GetAnimation) {
    SpriteAnimator animator;
    SpriteAnimation anim;
    anim.name = "jump";
    animator.AddAnimation(anim);
    auto* got = animator.GetAnimation("jump");
    ENJIN_ASSERT_NOT_NULL(got);
    ENJIN_EXPECT_STR_EQ(got->name, "jump");
}

// ===========================================================================
// Skeleton
// ===========================================================================

ENJIN_TEST(Skeleton, FindBoneIndex) {
    Skeleton skel;
    Bone b0, b1, b2;
    b0.name = "root";
    b0.parentIndex = -1;
    b1.name = "spine";
    b1.parentIndex = 0;
    b2.name = "head";
    b2.parentIndex = 1;
    skel.bones = {b0, b1, b2};

    ENJIN_EXPECT_EQ(skel.FindBoneIndex("root"), 0);
    ENJIN_EXPECT_EQ(skel.FindBoneIndex("spine"), 1);
    ENJIN_EXPECT_EQ(skel.FindBoneIndex("head"), 2);
    ENJIN_EXPECT_EQ(skel.FindBoneIndex("nonexistent"), -1);
}

// ===========================================================================
// AnimationStateMachine
// ===========================================================================

ENJIN_TEST(AnimSM, InitialState) {
    AnimationStateMachine sm;
    ENJIN_EXPECT_TRUE(sm.GetCurrentState().empty());
    ENJIN_EXPECT_EQ(sm.GetStates().size(), (size_t)0);
    ENJIN_EXPECT_EQ(sm.GetTransitions().size(), (size_t)0);
}

ENJIN_TEST(AnimSM, AddState) {
    AnimationStateMachine sm;
    AnimationState state;
    state.name = "Idle";
    state.animationName = "idle_anim";
    sm.AddState(state);
    ENJIN_EXPECT_EQ(sm.GetStates().size(), (size_t)1);
}

ENJIN_TEST(AnimSM, SetParameters) {
    AnimationStateMachine sm;
    sm.SetBool("grounded", true);
    sm.SetFloat("speed", 5.0f);
    sm.SetInt("health", 100);
    ENJIN_EXPECT_TRUE(sm.GetBool("grounded"));
    ENJIN_EXPECT_FLOAT_EQ(sm.GetFloat("speed"), 5.0f);
    ENJIN_EXPECT_EQ(sm.GetInt("health"), 100);
}

ENJIN_TEST(AnimSM, DefaultState) {
    AnimationStateMachine sm;
    sm.SetDefaultState("Idle");
    ENJIN_EXPECT_STR_EQ(sm.GetDefaultState(), "Idle");
}

// ===========================================================================
// PlayMode Enum
// ===========================================================================

ENJIN_TEST(PlayModeEnum, Values) {
    ENJIN_EXPECT_EQ((int)PlayMode::Once, 0);
    ENJIN_EXPECT_EQ((int)PlayMode::Loop, 1);
    ENJIN_EXPECT_EQ((int)PlayMode::PingPong, 2);
    ENJIN_EXPECT_EQ((int)PlayMode::ClampForever, 3);
}

ENJIN_TEST_MAIN()
