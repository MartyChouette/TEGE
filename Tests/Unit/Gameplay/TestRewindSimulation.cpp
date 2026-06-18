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

ENJIN_TEST_MAIN()
