#include "EnjinTest.h"
#include "Enjin/Renderer/AdaptiveQuality.h"

using namespace Enjin;
using namespace Enjin::Renderer;

static AdaptiveQualityConfig MakeConfig() {
    AdaptiveQualityConfig c;
    c.targetFPS = 60.0f;
    c.minFPS = 24.0f;
    c.hysteresis = 5.0f;
    c.adjustInterval = 3.0f;
    c.minLevel = QualityLevel::VeryLow;
    c.maxLevel = QualityLevel::Ultra;
    return c;
}

ENJIN_TEST(AdaptiveQuality, InitializesAtMaxLevel) {
    AdaptiveQualitySystem sys;
    sys.Initialize(MakeConfig());
    ENJIN_EXPECT_EQ((int)sys.GetCurrentLevel(), (int)QualityLevel::Ultra);
}

ENJIN_TEST(AdaptiveQuality, SustainedLowFPSDowngrades) {
    // ~40 FPS (below target - hysteresis) sustained past the adjust interval must
    // downgrade quality — the whole point of the system.
    AdaptiveQualitySystem sys;
    sys.Initialize(MakeConfig());
    for (int i = 0; i < 40; ++i) sys.Update(0.1f, 40.0f);
    ENJIN_ASSERT_TRUE((int)sys.GetCurrentLevel() < (int)QualityLevel::Ultra);
    ENJIN_ASSERT_TRUE(sys.GetAdjustmentCount() > 0);
}

ENJIN_TEST(AdaptiveQuality, EmergencyDowngradeBelowMinFPS) {
    // A single frame below minFPS triggers an immediate downgrade (no waiting for
    // the interval) — the "don't let it crater" safety.
    AdaptiveQualitySystem sys;
    sys.Initialize(MakeConfig());
    QualityLevel before = sys.GetCurrentLevel();
    sys.Update(0.016f, 15.0f);
    ENJIN_ASSERT_TRUE((int)sys.GetCurrentLevel() < (int)before);
}

ENJIN_TEST(AdaptiveQuality, ChangeCallbackFires) {
    AdaptiveQualitySystem sys;
    sys.Initialize(MakeConfig());
    int calls = 0;
    QualityLevel seenTo = QualityLevel::Ultra;
    sys.SetQualityChangeCallback([&](QualityLevel, QualityLevel to) { calls++; seenTo = to; });
    sys.Update(0.016f, 10.0f);   // emergency downgrade
    ENJIN_EXPECT_EQ(calls, 1);
    ENJIN_ASSERT_TRUE((int)seenTo < (int)QualityLevel::Ultra);
}

ENJIN_TEST(AdaptiveQuality, RecommendationsTrackLevel) {
    AdaptiveQualitySystem sys;
    sys.Initialize(MakeConfig());
    sys.Reset(QualityLevel::VeryLow);
    ENJIN_EXPECT_EQ(sys.GetRecommendedShadowResolution(), (u32)256);
    ENJIN_EXPECT_FALSE(sys.ShouldEnableVolumetrics());        // off below High
    sys.Reset(QualityLevel::Ultra);
    ENJIN_EXPECT_EQ(sys.GetRecommendedShadowResolution(), (u32)4096);
    ENJIN_EXPECT_TRUE(sys.GetRecommendedRenderScale() <= 1.0f);
    ENJIN_EXPECT_TRUE(sys.ShouldEnableVolumetrics());
}

ENJIN_TEST(AdaptiveQuality, DisabledDoesNothing) {
    // With the system disabled, even terrible FPS leaves the level untouched.
    AdaptiveQualitySystem sys;
    sys.Initialize(MakeConfig());
    sys.SetEnabled(false);
    for (int i = 0; i < 40; ++i) sys.Update(0.1f, 5.0f);
    ENJIN_EXPECT_EQ((int)sys.GetCurrentLevel(), (int)QualityLevel::Ultra);
}

ENJIN_TEST_MAIN()
