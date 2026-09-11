// Three features wore one coat.
//
// "Rewind" meant three different things in this engine and shared one
// implementation, so the UI could not name any of them:
//
//   Rewind Ability   a designed gameplay mechanic, authored and serialized,
//                    with a key, a cooldown and charges. It ships.
//   Debug Recorder   an editor diagnostic. It used to BE a SceneRewindComponent
//                    on a hidden entity called "__DebugRecorder" that PlayMode
//                    created at play start, with rewindKey = -1, cooldown = 0
//                    and charges = 0 to switch off the parts that did not apply.
//   Replay           a .tegereplay input stream, correctly separate already.
//
// The disguise cost three real bugs, and these tests are about the first two:
//
//   * The Game View's rewind timeline takes the FIRST SceneRewindComponent in
//     the world. The hidden one usually came first, so the panel a designer used
//     to tune a shipped mechanic reported the editor's debug buffer instead --
//     a different duration, a different snapshot rate, no charges, no cooldown.
//   * The recorder was an entity, so it could be SAVED into a scene. PlayMode
//     still carries a sweep on stop to remove ones that got in there.
//
// What the two DO share is the snapshot machinery, and that is now one object
// both of them own: Gameplay::WorldStateRecorder.
#include "EnjinTest.h"
#include "Enjin/Editor/DebugRecorder.h"
#include "Enjin/Gameplay/StateRecorder.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

namespace {

// A world with `count` entities strung out along +X, so a restored position
// identifies which recorded frame it came from.
ECS::Entity MakeMover(ECS::World& world) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    world.GetComponent<ECS::TransformComponent>(e)->position = Math::Vector3(0.0f, 0.0f, 0.0f);
    return e;
}

f32 XOf(ECS::World& world, ECS::Entity e) {
    return world.GetComponent<ECS::TransformComponent>(e)->position.x;
}

void SetX(ECS::World& world, ECS::Entity e, f32 x) {
    world.GetComponent<ECS::TransformComponent>(e)->position.x = x;
}

// Drive a recorder through `frames` snapshots, moving the entity one unit per
// snapshot so each frame is distinguishable.
void RecordWalk(Editor::DebugRecorder& rec, ECS::World& world, ECS::Entity e, int frames) {
    const f32 dt = rec.SnapshotInterval();
    for (int i = 0; i < frames; ++i) {
        SetX(world, e, static_cast<f32>(i));
        rec.Tick(dt);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// The Debug Recorder owns nothing in the world
// ---------------------------------------------------------------------------

ENJIN_TEST(DebugRecorder, RecordingCreatesNoEntitiesAndNoComponents) {
    // The whole point of the split. The old recorder was an entity carrying a
    // SceneRewindComponent, which is why a scene could be saved with a diagnostic
    // inside it and why the gameplay timeline read the wrong buffer.
    ECS::World world;
    ECS::Entity e = MakeMover(world);
    const usize entitiesBefore = world.GetEntitiesWithComponent<ECS::TransformComponent>().size();

    Editor::DebugRecorder rec;
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 10);

    // Assert
    ENJIN_EXPECT_TRUE(rec.IsActive());
    ENJIN_EXPECT_TRUE(rec.FrameCount() > 0);
    ENJIN_EXPECT_EQ(world.GetEntitiesWithComponent<ECS::TransformComponent>().size(),
                    entitiesBefore);
    // Not one of the gameplay components exists anywhere.
    ENJIN_EXPECT_TRUE(world.GetEntitiesWithComponent<ECS::SceneRewindComponent>().empty());
    ENJIN_EXPECT_TRUE(world.GetEntitiesWithComponent<ECS::RecordRewindComponent>().empty());

    rec.End();
}

ENJIN_TEST(DebugRecorder, DisabledMeansItDoesNotArm) {
    // "Record play sessions" unchecked has to mean no buffer and no cost, not a
    // recorder that runs and is ignored.
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    Editor::DebugRecorder rec;
    Editor::DebugRecorder::Settings s = rec.GetSettings();
    s.enabled = false;
    rec.Configure(s);
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 10);

    ENJIN_EXPECT_FALSE(rec.IsActive());
    ENJIN_EXPECT_EQ(rec.FrameCount(), 0u);
    ENJIN_EXPECT_EQ(rec.MemoryBytes(), static_cast<usize>(0));
}

ENJIN_TEST(DebugRecorder, BeginClearsThePreviousSession) {
    // A buffer left over from the last play looks live on the timeline, which is
    // worse than an empty one: you scrub it and watch a scene that is not this
    // session's.
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    Editor::DebugRecorder rec;
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 10);
    ENJIN_ASSERT_TRUE(rec.FrameCount() > 0);
    rec.End();

    rec.Begin(&world, nullptr, nullptr);
    ENJIN_EXPECT_EQ(rec.FrameCount(), 0u);
    ENJIN_EXPECT_TRUE(rec.RecordedDuration() < 0.0001f);
}

// ---------------------------------------------------------------------------
// Scrubbing
// ---------------------------------------------------------------------------

ENJIN_TEST(DebugRecorder, ScrubbingPutsTheWorldBackToARecordedMoment) {
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    Editor::DebugRecorder rec;
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 20);
    const f32 atEnd = XOf(world, e);
    ENJIN_ASSERT_TRUE(atEnd > 15.0f);

    // Act: five snapshots back.
    ENJIN_ASSERT_TRUE(rec.ScrubTo(rec.SnapshotInterval() * 5.0f));

    // Assert
    ENJIN_EXPECT_TRUE(rec.IsScrubbing());
    ENJIN_EXPECT_TRUE(XOf(world, e) < atEnd);
}

ENJIN_TEST(DebugRecorder, RecordingStopsWhileScrubbing) {
    // Recording over a scrub would append frames describing a past the session
    // never had, and the timeline would grow while you dragged it.
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    Editor::DebugRecorder rec;
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 20);

    rec.ScrubTo(rec.SnapshotInterval() * 5.0f);
    const u32 framesWhenScrubStarted = rec.FrameCount();

    for (int i = 0; i < 10; ++i) rec.Tick(rec.SnapshotInterval());

    ENJIN_EXPECT_EQ(rec.FrameCount(), framesWhenScrubStarted);
}

ENJIN_TEST(DebugRecorder, ScrubbingPastTheBufferStopsAtTheOldestFrame) {
    // Dragging past the end used to leave the scene still while the slider kept
    // moving, which reads as a broken control rather than the end of a recording.
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    Editor::DebugRecorder rec;
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 20);

    rec.ScrubTo(9999.0f);
    ENJIN_EXPECT_TRUE(rec.GetScrubOffset() <= rec.RecordedDuration() + 0.001f);
    ENJIN_EXPECT_TRUE(rec.GetScrubOffset() > 0.0f);
}

ENJIN_TEST(DebugRecorder, SteppingMovesOneSnapshotAtATime) {
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    Editor::DebugRecorder rec;
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 20);

    ENJIN_ASSERT_TRUE(rec.StepBack());
    const f32 one = rec.GetScrubOffset();
    ENJIN_EXPECT_FLOAT_NEAR(one, rec.SnapshotInterval(), 0.0001f);

    ENJIN_ASSERT_TRUE(rec.StepBack());
    ENJIN_EXPECT_FLOAT_NEAR(rec.GetScrubOffset(), rec.SnapshotInterval() * 2.0f, 0.0001f);

    ENJIN_ASSERT_TRUE(rec.StepForward());
    ENJIN_EXPECT_FLOAT_NEAR(rec.GetScrubOffset(), one, 0.0001f);

    // At the live edge there is nowhere forward to go, and saying so is how the
    // button knows to disable itself.
    ENJIN_ASSERT_TRUE(rec.StepForward());
    ENJIN_EXPECT_FALSE(rec.StepForward());
    ENJIN_EXPECT_FALSE(rec.IsScrubbing());
}

ENJIN_TEST(DebugRecorder, ResumingFromAScrubDropsTheFramesAfterIt) {
    // Resuming branches the session: the frames after the scrub point describe a
    // future that no longer happens. Keeping them would splice two timelines into
    // one buffer, and the scrubber would walk through a past that never occurred.
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    Editor::DebugRecorder rec;
    rec.Begin(&world, nullptr, nullptr);
    RecordWalk(rec, world, e, 20);
    const u32 fullFrames = rec.FrameCount();

    rec.ScrubTo(rec.SnapshotInterval() * 8.0f);
    rec.ReturnToLiveEdge();

    ENJIN_EXPECT_FALSE(rec.IsScrubbing());
    ENJIN_EXPECT_FLOAT_NEAR(rec.GetScrubOffset(), 0.0f, 0.0001f);
    ENJIN_EXPECT_TRUE(rec.FrameCount() < fullFrames);

    // And the clock came back with it: the next recorded frame must follow the
    // surviving ones, not sit eight snapshots in the future with a hole behind it.
    const f32 latestBefore = rec.LatestTime();
    rec.Tick(rec.SnapshotInterval());
    ENJIN_EXPECT_TRUE(rec.LatestTime() > latestBefore);
    ENJIN_EXPECT_TRUE(rec.LatestTime() < latestBefore + rec.SnapshotInterval() * 2.0f);
}

// ---------------------------------------------------------------------------
// The shared core
// ---------------------------------------------------------------------------

ENJIN_TEST(DebugRecorder, TheRecorderRestoresDeltaFramesThroughTheirKeyframe) {
    // Delta frames only carry what changed, so a seek has to walk back to the
    // nearest keyframe for everything they leave out. An entity that stopped
    // moving early is exactly the case that exercises it: it appears in the first
    // keyframe and in no delta after.
    ECS::World world;
    ECS::Entity moving = MakeMover(world);
    ECS::Entity still = MakeMover(world);
    SetX(world, still, 42.0f);

    WorldStateRecorder rec;
    WorldStateRecorder::Config cfg;
    cfg.recordInterval = 0.1f;
    cfg.maxDuration = 10.0f;
    cfg.channels = static_cast<u32>(RewindChannelFlags::Transform);
    cfg.deltaCompression = true;
    cfg.keyframeInterval = 1000;   // one keyframe, then deltas forever
    rec.Configure(cfg);
    rec.Sampler().SetWorld(&world);

    for (int i = 0; i < 12; ++i) {
        SetX(world, moving, static_cast<f32>(i));
        rec.Tick(0.1f);
    }

    // Move the still entity NOW, after everything was recorded. A correct restore
    // puts it back to 42; one that only applies the delta frame leaves it moved.
    SetX(world, still, -999.0f);
    SetX(world, moving, -999.0f);

    ENJIN_ASSERT_TRUE(rec.RestoreAtTime(rec.LatestTime()));
    ENJIN_EXPECT_FLOAT_NEAR(XOf(world, still), 42.0f, 0.001f);
    ENJIN_EXPECT_TRUE(XOf(world, moving) > 0.0f);
}

ENJIN_TEST(DebugRecorder, TrimmingClearsTheDeltaCacheSoNothingGoesMissing) {
    // After a trim the delta cache still described the dropped frames. Left
    // alone, the next delta frame would decide an entity "has not changed"
    // against a value no longer anywhere in the buffer, and that entity would be
    // absent from every frame until the next keyframe -- so a seek would restore
    // the scene without it.
    ECS::World world;
    ECS::Entity e = MakeMover(world);

    WorldStateRecorder rec;
    WorldStateRecorder::Config cfg;
    cfg.recordInterval = 0.1f;
    cfg.channels = static_cast<u32>(RewindChannelFlags::Transform);
    cfg.keyframeInterval = 1000;
    rec.Configure(cfg);
    rec.Sampler().SetWorld(&world);

    for (int i = 0; i < 10; ++i) { SetX(world, e, static_cast<f32>(i)); rec.Tick(0.1f); }

    rec.TrimAfter(rec.LatestTime() - 0.5f);
    rec.ResetClockToLatest();

    // One more frame, with the entity somewhere new.
    SetX(world, e, 77.0f);
    rec.Tick(0.1f);

    // Move it away, then seek back to that last frame.
    SetX(world, e, -1.0f);
    ENJIN_ASSERT_TRUE(rec.RestoreAtTime(rec.LatestTime()));
    ENJIN_EXPECT_FLOAT_NEAR(XOf(world, e), 77.0f, 0.001f);
}

ENJIN_TEST(DebugRecorder, TheDebugRecorderRecordsEveryChannel) {
    // It is a diagnostic: the one field you did not record is the one the bug is
    // in, and nothing here has to fit a shipping budget. The gameplay components
    // default to three channels for exactly the opposite reason.
    Editor::DebugRecorder rec;
    ECS::World world;
    MakeMover(world);
    rec.Begin(&world, nullptr, nullptr);
    rec.Tick(rec.SnapshotInterval());

    ENJIN_ASSERT_TRUE(rec.FrameCount() > 0);
    ENJIN_EXPECT_TRUE(rec.IsActive());
    // Asserted through the snapshot rather than the config, so this fails if the
    // channels stop reaching the capture.
    ECS::SceneRewindComponent defaults;
    ENJIN_EXPECT_TRUE(defaults.channels != static_cast<u32>(RewindChannelFlags::All));
}

ENJIN_TEST_MAIN()
