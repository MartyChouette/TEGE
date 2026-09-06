#include "EnjinTest.h"
#include "Enjin/Gameplay/SimulationClock.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

// Pure accumulator math (ADR-0005). No world needed: Tick accepts a null
// world and still drives the step loop and alpha.

ENJIN_TEST(SimClock, DisabledIsLegacySingleStep) {
    SimulationClock c;
    c.Configure(false, 60.0f);
    int calls = 0; f32 lastDt = 0.0f;
    c.Tick(nullptr, 0.033f, [&](f32 dt) { ++calls; lastDt = dt; });
    ENJIN_EXPECT_EQ(calls, 1);
    ENJIN_EXPECT_FLOAT_EQ(lastDt, 0.033f);   // raw frame dt, untouched
}

ENJIN_TEST(SimClock, FixedStepsAtTickSize) {
    SimulationClock c;
    c.Configure(true, 60.0f);
    int calls = 0; f32 lastDt = 0.0f;
    // 2.5 ticks of time -> exactly 2 steps, half a tick carried over
    c.Tick(nullptr, 2.5f / 60.0f, [&](f32 dt) { ++calls; lastDt = dt; });
    ENJIN_EXPECT_EQ(calls, 2);
    ENJIN_EXPECT_FLOAT_EQ(lastDt, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(c.GetAlpha() > 0.45f && c.GetAlpha() < 0.55f);
}

ENJIN_TEST(SimClock, CarryOverAccumulates) {
    SimulationClock c;
    c.Configure(true, 60.0f);
    int calls = 0;
    // 0.6 ticks per frame: frame1 = 0 steps, frame2 crosses 1.2 -> 1 step
    c.Tick(nullptr, 0.6f / 60.0f, [&](f32) { ++calls; });
    ENJIN_EXPECT_EQ(calls, 0);
    c.Tick(nullptr, 0.6f / 60.0f, [&](f32) { ++calls; });
    ENJIN_EXPECT_EQ(calls, 1);
}

ENJIN_TEST(SimClock, SpiralOfDeathClampDropsBacklog) {
    SimulationClock c;
    c.Configure(true, 60.0f);
    int calls = 0;
    // A 100ms hitch at 60Hz owes 6 ticks; the clamp runs 4 and drops the rest
    c.Tick(nullptr, 0.1f, [&](f32) { ++calls; });
    ENJIN_EXPECT_EQ(calls, (int)SimulationClock::kMaxStepsPerFrame);
    // Backlog dropped: the next normal frame owes at most its own time
    calls = 0;
    c.Tick(nullptr, 1.0f / 60.0f, [&](f32) { ++calls; });
    ENJIN_EXPECT_TRUE(calls <= 1);
}

ENJIN_TEST(SimClock, AlphaStaysInUnitRange) {
    SimulationClock c;
    c.Configure(true, 60.0f);
    for (int i = 0; i < 100; ++i) {
        // jittered frame times, 5..50ms
        f32 dt = 0.005f + 0.00045f * static_cast<f32>(i % 100);
        c.Tick(nullptr, dt, [](f32) {});
        ENJIN_EXPECT_TRUE(c.GetAlpha() >= 0.0f && c.GetAlpha() < 1.0f);
    }
}

ENJIN_TEST(SimClock, TickRateIsClamped) {
    SimulationClock c;
    c.Configure(true, 1000.0f);   // absurd -> clamped to 240
    ENJIN_EXPECT_FLOAT_EQ(c.GetFixedDeltaTime(), 1.0f / 240.0f);
    c.Configure(true, 1.0f);      // absurd -> clamped to 15
    ENJIN_EXPECT_FLOAT_EQ(c.GetFixedDeltaTime(), 1.0f / 15.0f);
}

ENJIN_TEST(SimClock, ResetDropsAccumulatedTime) {
    SimulationClock c;
    c.Configure(true, 60.0f);
    c.Tick(nullptr, 0.9f / 60.0f, [](f32) {});   // accumulate most of a tick
    c.Reset();
    int calls = 0;
    c.Tick(nullptr, 0.5f / 60.0f, [&](f32) { ++calls; });
    ENJIN_EXPECT_EQ(calls, 0);   // old time gone: half a tick is not enough
}

// ---------------------------------------------------------------------------
// Who gets interpolated
//
// Regression: IsInterpolated accepted only a DYNAMIC RigidbodyComponent. ADR-
// 0005 also moved ControllerSystem::Update into the step loop, so 3D character
// and vehicle controllers advance on the tick too -- and a first/third-person
// player is driven by JPH::CharacterVirtual, writes its transform directly, and
// typically has no rigidbody at all. It therefore rendered at the raw tick pose
// while every dynamic body around it rendered interpolated, making the player
// the one juddering object on a high-refresh display.
// ---------------------------------------------------------------------------

namespace {

// One tick advances x by 1. Then a partial frame with no tick: an interpolated
// entity should sit between the two poses, a non-interpolated one should stay
// on the tick pose.
f32 RenderedXAfterHalfFrame(ECS::World& world, ECS::Entity e) {
    SimulationClock c;
    c.Configure(true, 60.0f);

    auto step = [&](f32) {
        if (auto* t = world.GetComponent<ECS::TransformComponent>(e)) {
            t->position.x += 1.0f;
        }
    };

    c.Tick(&world, 1.0f / 60.0f, step);   // exactly one tick: 0 -> 1
    c.Tick(&world, 0.5f / 60.0f, step);   // no tick; alpha = 0.5

    auto* t = world.GetComponent<ECS::TransformComponent>(e);
    return t ? t->position.x : -1.0f;
}

} // namespace

ENJIN_TEST(SimClockInterpolation, CharacterControllerIsInterpolated) {
    // Arrange: a first-person player as the engine actually builds one --
    // CharacterVirtual-driven, no RigidbodyComponent.
    ECS::World world;
    ECS::Entity player = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(player);
    world.AddComponent<ECS::FirstPersonController>(player);

    // Act
    const f32 x = RenderedXAfterHalfFrame(world, player);

    // Assert: halfway between the two tick poses, not snapped to either.
    ENJIN_EXPECT_FLOAT_NEAR(x, 0.5f, 0.01f);
}

ENJIN_TEST(SimClockInterpolation, DynamicRigidbodyStillInterpolated) {
    // Arrange: the population that already worked, to prove it was not traded.
    ECS::World world;
    ECS::Entity crate = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(crate);
    auto& rb = world.AddComponent<ECS::RigidbodyComponent>(crate);
    rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;

    // Act
    const f32 x = RenderedXAfterHalfFrame(world, crate);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(x, 0.5f, 0.01f);
}

ENJIN_TEST(SimClockInterpolation, PlainEntityIsNotInterpolated) {
    // Arrange: nothing the fixed step owns -- must stay on the tick pose.
    ECS::World world;
    ECS::Entity prop = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(prop);

    // Act
    const f32 x = RenderedXAfterHalfFrame(world, prop);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(x, 1.0f, 0.01f);
}

ENJIN_TEST_MAIN()
