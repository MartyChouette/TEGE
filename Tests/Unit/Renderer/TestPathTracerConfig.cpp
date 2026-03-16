#include "EnjinTest.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// ===========================================================================
// PathTracerConfig Defaults
// ===========================================================================

ENJIN_TEST(PathTracerConfig, MaxBounces) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_EQ(cfg.maxBounces, 4u);
}

ENJIN_TEST(PathTracerConfig, TargetSPP) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_EQ(cfg.targetSPP, 1024u);
}

ENJIN_TEST(PathTracerConfig, FireflyClamp) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_FLOAT_EQ(cfg.fireflyClampValue, 10.0f);
}

ENJIN_TEST(PathTracerConfig, NEEEnabled) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.enableNEE);
}

ENJIN_TEST(PathTracerConfig, MISEnabled) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.enableMIS);
}

ENJIN_TEST(PathTracerConfig, RussianRouletteMinBounce) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_FLOAT_EQ(cfg.russianRouletteMinBounce, 3.0f);
}

ENJIN_TEST(PathTracerConfig, RussianRouletteMinProb) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_FLOAT_NEAR(cfg.russianRouletteMinProb, 0.05f, 0.001f);
}

// ===========================================================================
// PathTracerConfig Value Ranges
// ===========================================================================

ENJIN_TEST(PathTracerRange, BouncesPositive) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.maxBounces > 0u);
}

ENJIN_TEST(PathTracerRange, TargetSPPPositive) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.targetSPP > 0u);
}

ENJIN_TEST(PathTracerRange, FireflyClampPositive) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.fireflyClampValue > 0.0f);
}

ENJIN_TEST(PathTracerRange, RRMinProbInRange) {
    PathTracerConfig cfg;
    ENJIN_EXPECT_TRUE(cfg.russianRouletteMinProb > 0.0f);
    ENJIN_EXPECT_TRUE(cfg.russianRouletteMinProb < 1.0f);
}

ENJIN_TEST_MAIN()
