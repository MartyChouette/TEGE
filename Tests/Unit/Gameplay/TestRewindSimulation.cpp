// Behavioral rewind test: drive RecordRewindSystem to record an entity's motion,
// then restore an earlier state via the programmatic seek API. The existing
// TestRewindSystem only checks the ring buffer and struct defaults; this proves
// the record-and-restore path actually works.

#include "EnjinTest.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;

ENJIN_TEST(RewindSim, RecordsThenRestoresEarlierTransform) {
    // Arrange: an entity that records its transform over time.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    world.AddComponent<ECS::RecordRewindComponent>(e);  // defaults: 20 Hz, Transform channel on

    Gameplay::RecordRewindSystem sys;
    sys.SetWorld(&world);

    const f32 dt = 1.0f / 20.0f;  // == default recordInterval, one snapshot per step

    // Act 1: record 40 steps (~2s). Position.x tracks the step index, so the
    // recorded history is a known ramp 0..39.
    for (int i = 0; i < 40; ++i) {
        world.GetComponent<ECS::TransformComponent>(e)->position.x = static_cast<f32>(i);
        sys.Update(dt);
    }

    // Assert 1: seek to the latest -> newest recorded x (~39), overwriting a
    // deliberately-wrong current value (proves restore actually writes).
    world.GetComponent<ECS::TransformComponent>(e)->position.x = 999.0f;
    sys.SeekEntityToTime(e, 0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(world.GetComponent<ECS::TransformComponent>(e)->position.x, 39.0f, 1.5f);

    // Assert 2: seek ~1 second back -> the entity is restored to its earlier
    // position (~20 steps earlier, x ~ 19), not the latest.
    world.GetComponent<ECS::TransformComponent>(e)->position.x = 999.0f;
    sys.SeekEntityToTime(e, 1.0f);
    f32 x = world.GetComponent<ECS::TransformComponent>(e)->position.x;
    ENJIN_EXPECT_TRUE(x > 15.0f && x < 24.0f);
}

ENJIN_TEST(RewindSim, SeekIsSafeWithNoHistory) {
    // Arrange / Act / Assert: seeking before anything is recorded must not crash
    // or corrupt the transform.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e).position = Math::Vector3(7.0f, 0.0f, 0.0f);
    world.AddComponent<ECS::RecordRewindComponent>(e);

    Gameplay::RecordRewindSystem sys;
    sys.SetWorld(&world);
    sys.SeekEntityToTime(e, 1.0f);  // empty history -> no-op

    ENJIN_EXPECT_FLOAT_NEAR(world.GetComponent<ECS::TransformComponent>(e)->position.x, 7.0f, 0.001f);
}

// The editor debug-recorder path: a keyless SceneRewindComponent (rewindKey -1,
// the play-mode timeline configuration) records the whole scene, and
// SeekSceneToTime restores a past frame - the machinery behind pause + step-back.
ENJIN_TEST(RewindSim, KeylessSceneRecorderSeeksBackForDebugTimeline) {
    // Arrange: one moving entity and the editor's keyless scene recorder.
    ECS::World world;
    ECS::Entity mover = world.CreateEntity();
    auto& tf = world.AddComponent<ECS::TransformComponent>(mover);
    tf.position = Math::Vector3(0.0f, 0.0f, 0.0f);

    ECS::Entity recorder = world.CreateEntity();
    auto& sr = world.AddComponent<ECS::SceneRewindComponent>(recorder);
    sr.maxDuration = 10.0f;
    sr.recordInterval = 1.0f / 30.0f;
    sr.rewindKey = -1;

    Gameplay::RecordRewindSystem system;
    system.SetWorld(&world);

    // Act: 2 simulated seconds at 30fps, moving +1 unit x per second.
    const f32 dt = 1.0f / 30.0f;
    for (int i = 0; i < 60; ++i) {
        world.GetComponent<ECS::TransformComponent>(mover)->position.x += 1.0f * dt;
        system.Update(dt);
    }
    f32 xAtEnd = world.GetComponent<ECS::TransformComponent>(mover)->position.x;
    ENJIN_EXPECT_TRUE(xAtEnd > 1.9f && xAtEnd < 2.1f);

    // Assert: seek 1s back restores ~the halfway position...
    system.SeekSceneToTime(1.0f);
    f32 xPast = world.GetComponent<ECS::TransformComponent>(mover)->position.x;
    ENJIN_EXPECT_TRUE(xPast > 0.8f && xPast < 1.2f);

    // ...seeking to the live edge returns to ~the end state...
    system.SeekSceneToTime(0.0f);
    f32 xLive = world.GetComponent<ECS::TransformComponent>(mover)->position.x;
    ENJIN_EXPECT_TRUE(xLive > 1.8f && xLive < 2.1f);

    // ...and the recorder reports a sensible scrubber range.
    ENJIN_EXPECT_TRUE(system.GetSceneRecordedDuration() > 1.5f);
    ENJIN_EXPECT_TRUE(system.GetSceneFrameCount() > 30);
}

ENJIN_TEST_MAIN()
