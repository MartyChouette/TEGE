#include "EnjinTest.h"
#include "Enjin/Debug/Profiler.h"

using namespace Enjin;
using namespace Enjin::Debug;

// Helper to reset singleton between tests
static void ResetProfiler() {
    Profiler::Instance().Reset();
    Profiler::Instance().SetEnabled(true);
}

// ===========================================================================
// Singleton
// ===========================================================================

ENJIN_TEST(Singleton, ReturnsSameInstance) {
    auto& a = Profiler::Instance();
    auto& b = Profiler::Instance();
    ENJIN_EXPECT_EQ(&a, &b);
}

// ===========================================================================
// Defaults & Reset
// ===========================================================================

ENJIN_TEST(Defaults, EnabledByDefault) {
    ResetProfiler();
    ENJIN_EXPECT_TRUE(Profiler::Instance().IsEnabled());
}

ENJIN_TEST(Defaults, ResetClearsScopes) {
    ResetProfiler();
    auto& p = Profiler::Instance();
    p.RecordScope("test", 1.0);
    p.Reset();
    ENJIN_EXPECT_EQ(p.GetScopes().size(), (size_t)0);
}

ENJIN_TEST(Defaults, HistorySizeConstant) {
    ENJIN_EXPECT_EQ((size_t)Profiler::HISTORY_SIZE, (size_t)240);
}

// ===========================================================================
// Enable/Disable
// ===========================================================================

ENJIN_TEST(Control, DisableAndEnable) {
    ResetProfiler();
    auto& p = Profiler::Instance();
    p.SetEnabled(false);
    ENJIN_EXPECT_FALSE(p.IsEnabled());
    p.SetEnabled(true);
    ENJIN_EXPECT_TRUE(p.IsEnabled());
}

// ===========================================================================
// RecordScope
// ===========================================================================

ENJIN_TEST(RecordScope, CreatesEntry) {
    ResetProfiler();
    auto& p = Profiler::Instance();
    p.RecordScope("Render", 5.0);
    auto& scopes = p.GetScopes();
    ENJIN_EXPECT_TRUE(scopes.find("Render") != scopes.end());
}

ENJIN_TEST(RecordScope, UpdatesTiming) {
    ResetProfiler();
    auto& p = Profiler::Instance();
    p.RecordScope("Physics", 2.5);
    auto& scopes = p.GetScopes();
    auto it = scopes.find("Physics");
    ENJIN_ASSERT_TRUE(it != scopes.end());
    ENJIN_EXPECT_FLOAT_NEAR(it->second.lastTimeMs, 2.5, 0.01);
    ENJIN_EXPECT_EQ(it->second.callCount, (u64)1);
}

ENJIN_TEST(RecordScope, MultipleCallsAccumulate) {
    ResetProfiler();
    auto& p = Profiler::Instance();
    p.RecordScope("AI", 1.0);
    p.RecordScope("AI", 3.0);
    p.RecordScope("AI", 2.0);
    auto& scopes = p.GetScopes();
    auto it = scopes.find("AI");
    ENJIN_ASSERT_TRUE(it != scopes.end());
    ENJIN_EXPECT_EQ(it->second.callCount, (u64)3);
    ENJIN_EXPECT_FLOAT_NEAR(it->second.totalTimeMs, 6.0, 0.01);
    ENJIN_EXPECT_FLOAT_NEAR(it->second.maxTimeMs, 3.0, 0.01);
}

// ===========================================================================
// Frame Data
// ===========================================================================

ENJIN_TEST(FrameData, DefaultValues) {
    FrameData fd;
    ENJIN_EXPECT_FLOAT_EQ(fd.totalMs, 0.0);
    ENJIN_EXPECT_FLOAT_EQ(fd.renderMs, 0.0);
    ENJIN_EXPECT_FLOAT_EQ(fd.physicsMs, 0.0);
    ENJIN_EXPECT_EQ(fd.drawCalls, 0u);
    ENJIN_EXPECT_EQ(fd.entityCount, 0u);
    ENJIN_EXPECT_EQ(fd.triangleCount, 0u);
}

ENJIN_TEST(FrameData, SetCounters) {
    ResetProfiler();
    auto& p = Profiler::Instance();
    p.SetDrawCalls(100);
    p.SetEntityCount(500);
    p.SetTriangleCount(50000);
    p.SetMemoryUsed(1024 * 1024);

    const FrameData& fd = p.GetCurrentFrame();
    ENJIN_EXPECT_EQ(fd.drawCalls, 100u);
    ENJIN_EXPECT_EQ(fd.entityCount, 500u);
    ENJIN_EXPECT_EQ(fd.triangleCount, 50000u);
    ENJIN_EXPECT_EQ(fd.memoryUsedBytes, (usize)(1024 * 1024));
}

ENJIN_TEST(FrameData, IncrementDrawCalls) {
    ResetProfiler();
    auto& p = Profiler::Instance();
    p.SetDrawCalls(0);
    p.IncrementDrawCalls(5);
    p.IncrementDrawCalls(3);
    ENJIN_EXPECT_EQ(p.GetCurrentFrame().drawCalls, 8u);
}

// ===========================================================================
// Frame History
// ===========================================================================

ENJIN_TEST(History, BeginEndFrameRecords) {
    ResetProfiler();
    auto& p = Profiler::Instance();

    // Simulate a few frames
    for (int i = 0; i < 5; ++i) {
        p.BeginFrame();
        p.EndFrame();
    }

    ENJIN_EXPECT_GT(p.GetFrameTimeHistory().size(), (size_t)0);
}

ENJIN_TEST(History, FPSPositiveAfterFrames) {
    ResetProfiler();
    auto& p = Profiler::Instance();

    for (int i = 0; i < 100; ++i) {
        p.BeginFrame();
        p.EndFrame();
    }

    // FPS should be calculable (may be very high since frames are instant)
    f64 fps = p.GetFPS();
    ENJIN_EXPECT_GE(fps, 0.0);
}

// ===========================================================================
// ProfileScope Struct
// ===========================================================================

ENJIN_TEST(ProfileScope, DefaultValues) {
    ProfileScope scope;
    ENJIN_EXPECT_FLOAT_EQ(scope.lastTimeMs, 0.0);
    ENJIN_EXPECT_FLOAT_EQ(scope.avgTimeMs, 0.0);
    ENJIN_EXPECT_FLOAT_EQ(scope.maxTimeMs, 0.0);
    ENJIN_EXPECT_FLOAT_EQ(scope.totalTimeMs, 0.0);
    ENJIN_EXPECT_EQ(scope.callCount, (u64)0);
}

ENJIN_TEST_MAIN()
