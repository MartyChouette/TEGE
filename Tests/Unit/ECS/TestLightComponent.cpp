#include "EnjinTest.h"
#include "Enjin/ECS/Components/Light.h"

using namespace Enjin;
using namespace Enjin::ECS;

// ===========================================================================
// LightComponent Defaults
// ===========================================================================

ENJIN_TEST(LightDefaults, Type) {
    LightComponent light;
    ENJIN_EXPECT_EQ((int)light.type, (int)LightType::Point);
}

ENJIN_TEST(LightDefaults, Color) {
    LightComponent light;
    ENJIN_EXPECT_FLOAT_EQ(light.color.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(light.color.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(light.color.z, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(light.intensity, 1.0f);
}

ENJIN_TEST(LightDefaults, Attenuation) {
    LightComponent light;
    ENJIN_EXPECT_FLOAT_EQ(light.range, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(light.constantAttenuation, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(light.linearAttenuation, 0.09f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(light.quadraticAttenuation, 0.032f, 0.001f);
}

ENJIN_TEST(LightDefaults, SpotAngles) {
    LightComponent light;
    ENJIN_EXPECT_FLOAT_EQ(light.innerConeAngle, 12.5f);
    ENJIN_EXPECT_FLOAT_EQ(light.outerConeAngle, 17.5f);
}

ENJIN_TEST(LightDefaults, CastShadows) {
    LightComponent light;
    ENJIN_EXPECT_TRUE(light.castShadows);
}

// ===========================================================================
// Attenuation Calculation
// ===========================================================================

ENJIN_TEST(Attenuation, AtZeroDistance) {
    LightComponent light;
    f32 atten = light.CalculateAttenuation(0.0f);
    // 1 / (1 + 0 + 0) = 1
    ENJIN_EXPECT_FLOAT_EQ(atten, 1.0f);
}

ENJIN_TEST(Attenuation, BeyondRangeReturnsZero) {
    LightComponent light;
    light.range = 10.0f;
    f32 atten = light.CalculateAttenuation(11.0f);
    ENJIN_EXPECT_FLOAT_EQ(atten, 0.0f);
}

ENJIN_TEST(Attenuation, AtRange) {
    LightComponent light;
    light.range = 10.0f;
    f32 atten = light.CalculateAttenuation(10.0f);
    // Should still return a valid value at exact range
    ENJIN_EXPECT_TRUE(atten >= 0.0f);
    ENJIN_EXPECT_TRUE(atten <= 1.0f);
}

ENJIN_TEST(Attenuation, DecreasesWithDistance) {
    LightComponent light;
    light.range = 100.0f;
    f32 nearAtten = light.CalculateAttenuation(1.0f);
    f32 farAtten = light.CalculateAttenuation(50.0f);
    ENJIN_EXPECT_TRUE(nearAtten > farAtten);
}

ENJIN_TEST(Attenuation, ConstantOnlyAttenuation) {
    LightComponent light;
    light.constantAttenuation = 2.0f;
    light.linearAttenuation = 0.0f;
    light.quadraticAttenuation = 0.0f;
    light.range = 100.0f;
    f32 atten = light.CalculateAttenuation(50.0f);
    ENJIN_EXPECT_FLOAT_EQ(atten, 0.5f);
}

// ===========================================================================
// Light Limits
// ===========================================================================

ENJIN_TEST(LightLimits, MaxCounts) {
    ENJIN_EXPECT_EQ(MAX_DIRECTIONAL_LIGHTS, 4u);
    ENJIN_EXPECT_EQ(MAX_POINT_LIGHTS, 64u);
    ENJIN_EXPECT_EQ(MAX_SPOT_LIGHTS, 32u);
    ENJIN_EXPECT_EQ(MAX_SHADOW_POINT_LIGHTS, 4u);
    ENJIN_EXPECT_EQ(MAX_SHADOW_SPOT_LIGHTS, 4u);
}

// ===========================================================================
// LightingUBO Default State
// ===========================================================================

ENJIN_TEST(LightingUBO, DefaultAmbient) {
    LightingUBO ubo{};
    ENJIN_EXPECT_FLOAT_EQ(ubo.ambientIntensity, 0.0f);
    ENJIN_EXPECT_EQ(ubo.directionalLightCount, 0u);
    ENJIN_EXPECT_EQ(ubo.pointLightCount, 0u);
    ENJIN_EXPECT_EQ(ubo.spotLightCount, 0u);
}

ENJIN_TEST_MAIN()
