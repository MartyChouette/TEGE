#include "EnjinTest.h"
#include "Enjin/Gameplay/SimulationClock.h"

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

ENJIN_TEST_MAIN()
