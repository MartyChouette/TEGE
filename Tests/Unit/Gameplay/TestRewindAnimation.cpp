// The rewind system recorded animation and threw it away.
//
// CaptureEntitySnapshot wrote animNormalizedTime into every snapshot whenever the
// Animation channel was ticked, and both restore paths said this:
//
//     // Animation restore: seek the animator to the recorded time
//     // (SkeletalAnimator doesn't expose a time setter, so we skip restore for now -
//     //  the animator will naturally resume from its current state after rewind stops)
//
// It does expose one. SkeletalAnimator::SetNormalizedTime seeks, resamples the clip
// and recalculates the world transforms and skinning matrices, and it is not new.
// So the Animation checkbox in the inspector recorded a channel that was discarded,
// and a rewound character walked backwards through its own footsteps with its legs
// still cycling forward -- which reads as a deliberate stylistic choice rather than
// as a missing feature, and so was never reported.
#include "EnjinTest.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/Gameplay/RewindChannel.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include <memory>

using namespace Enjin;
using namespace Enjin::Gameplay;

namespace {

// A one-bone skeleton with a two-second looping clip. The smallest rig
// SetNormalizedTime will act on: it early-returns without both a skeleton and a
// current clip, so a test that skipped either would pass while proving nothing.
std::shared_ptr<Animation::Skeleton> MakeSkeleton() {
    auto skel = std::make_shared<Animation::Skeleton>();
    skel->name = "TestRig";
    Animation::Bone root;
    root.name = "Root";
    root.parentIndex = -1;
    skel->bones.push_back(root);
    return skel;
}

Animation::SkeletalAnimation MakeClip() {
    Animation::SkeletalAnimation clip;
    clip.name = "Walk";
    clip.duration = 2.0f;
    clip.playMode = Animation::PlayMode::Loop;

    Animation::BoneTrack track;
    track.boneName = "Root";
    track.boneIndex = 0;
    // Two keys, so the pose actually differs across the clip.
    track.positionTimes = { 0.0f, 2.0f };
    track.positions = { Math::Vector3(0.0f, 0.0f, 0.0f),
                        Math::Vector3(0.0f, 1.0f, 0.0f) };
    clip.tracks.push_back(track);
    return clip;
}

ECS::AnimatorComponent* SetUpAnimator(ECS::World& world, ECS::Entity e) {
    world.AddComponent<ECS::AnimatorComponent>(e);
    auto* anim = world.GetComponent<ECS::AnimatorComponent>(e);
    anim->animator.SetSkeleton(MakeSkeleton());
    anim->animator.AddAnimation(MakeClip());
    anim->animator.Play("Walk");
    return anim;
}

} // namespace

// ---------------------------------------------------------------------------
// The loop-wrap interpolation
// ---------------------------------------------------------------------------

ENJIN_TEST(RewindAnimation, InterpolatingWithinTheClipIsAPlainLerp) {
    // Arrange / act / assert
    ENJIN_EXPECT_FLOAT_NEAR(LerpNormalizedAnimTime(0.2f, 0.6f, 0.0f), 0.2f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(LerpNormalizedAnimTime(0.2f, 0.6f, 0.5f), 0.4f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(LerpNormalizedAnimTime(0.2f, 0.6f, 1.0f), 0.6f, 0.0001f);
}

ENJIN_TEST(RewindAnimation, AClipThatWrappedGoesForwardsThroughTheLoopPoint) {
    // The regression. A looping clip that passed 1.0 between two recorded frames
    // gives to < from, and a direct lerp then runs the animation BACKWARDS through
    // the middle of the clip for that one interval -- a stutter at exactly the loop
    // point, which is the frame a viewer is already watching for.
    //
    // 0.9 -> 0.1 is one tenth of the clip forward, so halfway is 1.0, which is 0.0.
    const f32 mid = LerpNormalizedAnimTime(0.9f, 0.1f, 0.5f);
    ENJIN_EXPECT_TRUE(mid >= 0.9f || mid <= 0.1f);   // near the seam, not the middle
    ENJIN_EXPECT_FALSE(mid > 0.2f && mid < 0.8f);    // what the naive lerp produced

    // A quarter of the way is 0.95, still before the wrap.
    ENJIN_EXPECT_FLOAT_NEAR(LerpNormalizedAnimTime(0.9f, 0.1f, 0.25f), 0.95f, 0.0001f);
    // Three quarters is 1.05, which comes back as 0.05.
    ENJIN_EXPECT_FLOAT_NEAR(LerpNormalizedAnimTime(0.9f, 0.1f, 0.75f), 0.05f, 0.0001f);
}

ENJIN_TEST(RewindAnimation, TheResultAlwaysStaysInRange) {
    // SetNormalizedTime clamps, so an out-of-range result would silently pin the
    // animation to one end of the clip rather than fail.
    const f32 samples[] = { 0.0f, 0.1f, 0.33f, 0.5f, 0.75f, 0.99f, 1.0f };
    for (f32 from : samples) {
        for (f32 to : samples) {
            for (f32 t : samples) {
                const f32 r = LerpNormalizedAnimTime(from, to, t);
                ENJIN_ASSERT_TRUE(r >= -0.0001f && r <= 1.0001f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// The animator itself
// ---------------------------------------------------------------------------

ENJIN_TEST(RewindAnimation, TheAnimatorHasATimeSetterAndItChangesThePose) {
    // The claim the two comments rested on, checked directly. If this ever fails the
    // comments were right and the restore has to go back to doing nothing.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    auto* anim = SetUpAnimator(world, e);

    anim->animator.SetNormalizedTime(0.0f);
    ENJIN_ASSERT_FALSE(anim->animator.GetCurrentPose().localPositions.empty());
    const Math::Vector3 atStart = anim->animator.GetCurrentPose().localPositions[0];

    anim->animator.SetNormalizedTime(1.0f);
    ENJIN_ASSERT_FALSE(anim->animator.GetCurrentPose().localPositions.empty());
    const Math::Vector3 atEnd = anim->animator.GetCurrentPose().localPositions[0];

    ENJIN_EXPECT_FLOAT_NEAR(anim->animator.GetNormalizedTime(), 1.0f, 0.0001f);
    // Seeking resamples: the pose is not the same at both ends of the clip.
    ENJIN_EXPECT_TRUE((atEnd - atStart).LengthSquared() > 0.0001f);
}

// ---------------------------------------------------------------------------
// End to end through the rewind system
// ---------------------------------------------------------------------------

ENJIN_TEST(RewindAnimation, RewindingSeeksTheAnimatorBackToARecordedTime) {
    // Arrange: an entity that records the Animation channel.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    auto* anim = SetUpAnimator(world, e);

    world.AddComponent<ECS::RecordRewindComponent>(e);
    {
        auto* rr = world.GetComponent<ECS::RecordRewindComponent>(e);
        rr->enabled = true;
        rr->recordInterval = 0.1f;
        rr->rewindSpeed = 1.0f;
        rr->channels = static_cast<u32>(RewindChannelFlags::Transform) |
                       static_cast<u32>(RewindChannelFlags::Animation);
    }

    RecordRewindSystem system;
    system.SetWorld(&world);

    // Act 1: record a second of playback. Re-fetch the component each iteration --
    // ComponentStorage is a dense vector and any Add would move it.
    for (int i = 0; i < 10; ++i) {
        anim->animator.Update(0.1f);
        anim = world.GetComponent<ECS::AnimatorComponent>(e);
        system.Update(0.1f);
    }
    const f32 latest = anim->animator.GetNormalizedTime();
    ENJIN_ASSERT_TRUE(latest > 0.1f);   // it really did advance while recording

    // Act 2: rewind.
    system.StartEntityRewind(e);
    ENJIN_ASSERT_TRUE(system.IsAnyRewinding() ||
                      world.GetComponent<ECS::RecordRewindComponent>(e)->rewinding);
    for (int i = 0; i < 4; ++i) {
        system.Update(0.1f);
    }

    // Assert: the animator was seeked back, not left where playback had got to.
    anim = world.GetComponent<ECS::AnimatorComponent>(e);
    const f32 afterRewind = anim->animator.GetNormalizedTime();
    ENJIN_EXPECT_TRUE(afterRewind < latest);
}

ENJIN_TEST(RewindAnimation, AnEntityWithTheChannelOffIsLeftAlone) {
    // The channel checkboxes are the contract. Seeking an animator whose owner did
    // not ask for animation rewind would be the same class of bug in the other
    // direction: doing something the UI says is off.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    auto* anim = SetUpAnimator(world, e);

    world.AddComponent<ECS::RecordRewindComponent>(e);
    {
        auto* rr = world.GetComponent<ECS::RecordRewindComponent>(e);
        rr->enabled = true;
        rr->recordInterval = 0.1f;
        rr->rewindSpeed = 1.0f;
        rr->channels = static_cast<u32>(RewindChannelFlags::Transform);  // no Animation
    }

    RecordRewindSystem system;
    system.SetWorld(&world);
    for (int i = 0; i < 10; ++i) {
        anim->animator.Update(0.1f);
        anim = world.GetComponent<ECS::AnimatorComponent>(e);
        system.Update(0.1f);
    }
    const f32 beforeRewind = anim->animator.GetNormalizedTime();

    system.StartEntityRewind(e);
    for (int i = 0; i < 4; ++i) {
        system.Update(0.1f);
    }

    anim = world.GetComponent<ECS::AnimatorComponent>(e);
    ENJIN_EXPECT_FLOAT_NEAR(anim->animator.GetNormalizedTime(), beforeRewind, 0.0001f);
}

// ---------------------------------------------------------------------------
// The programmatic API
// ---------------------------------------------------------------------------

ENJIN_TEST(RewindAnimation, StartEntityRewindActuallyRewindsWithNoKeyHeld) {
    // StartEntityRewind is the runtime and scripting entry point -- the whole of
    // Rewind_StartEntityRewind() -- and it set rewinding = true against an update
    // whose rewind branch was gated purely on the key being physically down. So the
    // very next frame took the else branch, cleared the flag, started the cooldown
    // and went back to recording. The scripted rewind worked only while the player
    // happened to be holding R, which is the one situation where calling it is
    // pointless.
    //
    // No key is pressed anywhere in this test, which is the point.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    auto* tr = world.GetComponent<ECS::TransformComponent>(e);
    tr->position = Math::Vector3(0.0f, 0.0f, 0.0f);

    world.AddComponent<ECS::RecordRewindComponent>(e);
    {
        auto* rr = world.GetComponent<ECS::RecordRewindComponent>(e);
        rr->enabled = true;
        rr->recordInterval = 0.1f;
        rr->rewindSpeed = 1.0f;
        rr->channels = static_cast<u32>(RewindChannelFlags::Transform);
    }

    RecordRewindSystem system;
    system.SetWorld(&world);

    // Record a walk along +X.
    for (int i = 0; i < 10; ++i) {
        world.GetComponent<ECS::TransformComponent>(e)->position.x = static_cast<f32>(i);
        system.Update(0.1f);
    }
    ENJIN_ASSERT_TRUE(world.GetComponent<ECS::TransformComponent>(e)->position.x > 8.0f);

    // Act
    system.StartEntityRewind(e);
    for (int i = 0; i < 4; ++i) {
        system.Update(0.1f);
        // The flag has to SURVIVE the update. This is the exact assertion that
        // failed before: one frame was enough to clear it.
        ENJIN_ASSERT_TRUE(world.GetComponent<ECS::RecordRewindComponent>(e)->rewinding);
    }

    // Assert: it moved back along the recorded path.
    ENJIN_EXPECT_TRUE(world.GetComponent<ECS::TransformComponent>(e)->position.x < 8.0f);
    ENJIN_EXPECT_TRUE(system.IsAnyRewinding());

    // And Stop really stops it.
    system.StopEntityRewind(e);
    system.Update(0.1f);
    ENJIN_EXPECT_FALSE(world.GetComponent<ECS::RecordRewindComponent>(e)->rewinding);
}

ENJIN_TEST_MAIN()
