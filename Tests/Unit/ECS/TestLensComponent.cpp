#include "EnjinTest.h"
#include "Enjin/ECS/Components/Lens.h"

using namespace Enjin;
using namespace Enjin::ECS;

ENJIN_TEST(LensDefaults, NeutralByDefault) {
    LensComponent lens;
    ENJIN_EXPECT_TRUE(lens.enabled);
    ENJIN_EXPECT_EQ((int)lens.type, (int)LensType::Standard);
    ENJIN_EXPECT_FLOAT_EQ(lens.distortion, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.anamorphicSqueeze, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.chromaticAberration, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.vignetteIntensity, 0.0f);
}

ENJIN_TEST(LensPreset, FisheyeBendsOutward) {
    // Arrange / Act
    LensComponent lens;
    lens.ApplyPreset(LensType::Fisheye);
    // Assert: strong barrel (negative) distortion and the type is recorded.
    ENJIN_EXPECT_TRUE(lens.distortion < -0.3f);
    ENJIN_EXPECT_EQ((int)lens.type, (int)LensType::Fisheye);
}

ENJIN_TEST(LensPreset, TelephotoPinchesAndVignettes) {
    LensComponent lens;
    lens.ApplyPreset(LensType::Telephoto);
    ENJIN_EXPECT_TRUE(lens.distortion > 0.0f);          // pincushion
    ENJIN_EXPECT_TRUE(lens.vignetteIntensity > 0.2f);   // tighter vignette
}

ENJIN_TEST(LensPreset, AnamorphicSqueezesHorizontally) {
    LensComponent lens;
    lens.ApplyPreset(LensType::Anamorphic);
    ENJIN_EXPECT_TRUE(lens.anamorphicSqueeze > 1.0f);
    ENJIN_EXPECT_TRUE(lens.chromaticAberration > 0.0f);
}

ENJIN_TEST(LensPreset, StandardResetsToNeutral) {
    LensComponent lens;
    lens.ApplyPreset(LensType::Fisheye);   // dirty it
    lens.ApplyPreset(LensType::Standard);  // reset
    ENJIN_EXPECT_FLOAT_EQ(lens.distortion, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.anamorphicSqueeze, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.vignetteIntensity, 0.0f);
}

ENJIN_TEST(LensPreset, CustomLeavesValuesAlone) {
    // Custom is the "I tweaked it myself" mode: ApplyPreset must not stomp values.
    LensComponent lens;
    lens.distortion = -0.22f;
    lens.vignetteIntensity = 0.33f;
    lens.ApplyPreset(LensType::Custom);
    ENJIN_EXPECT_FLOAT_EQ(lens.distortion, -0.22f);
    ENJIN_EXPECT_FLOAT_EQ(lens.vignetteIntensity, 0.33f);
    ENJIN_EXPECT_EQ((int)lens.type, (int)LensType::Custom);
}

ENJIN_TEST(LensPreset, EveryPresetIsApplicable) {
    // No preset should crash or leave an out-of-range squeeze.
    LensComponent lens;
    for (int i = 0; i < (int)LensType::COUNT; ++i) {
        lens.ApplyPreset((LensType)i);
        ENJIN_EXPECT_TRUE(lens.anamorphicSqueeze > 0.0f);
        ENJIN_EXPECT_TRUE(lens.vignetteIntensity >= 0.0f);
    }
}

ENJIN_TEST_MAIN()
