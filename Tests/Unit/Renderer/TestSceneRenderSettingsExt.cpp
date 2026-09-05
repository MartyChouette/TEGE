#include "EnjinTest.h"
#include "Enjin/Renderer/SceneRenderSettings.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// ===========================================================================
// SceneRenderSettings Defaults (Factory)
// ===========================================================================

ENJIN_TEST(RenderSettingsFactory, DefaultsMethod) {
    SceneRenderSettings s = SceneRenderSettings::Defaults();
    ENJIN_EXPECT_TRUE(s.useProjectDefaults);
}

// ===========================================================================
// Shadow Defaults
// ===========================================================================

ENJIN_TEST(RenderShadows, Enabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.shadowsEnabled);
    ENJIN_EXPECT_EQ(s.shadowResolution, 2048u);
}

ENJIN_TEST(RenderShadows, Distance) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.shadowDistance, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.shadowStrength, 1.0f);
}

ENJIN_TEST(RenderShadows, Softness) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.shadowSoftness, 0.0f);
}

ENJIN_TEST(RenderShadows, CascadeDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.cascadeProgressiveUpdate);
    ENJIN_EXPECT_EQ(s.cascadeFarUpdateInterval, 2u);
}

// ===========================================================================
// Post-Processing Defaults
// ===========================================================================

ENJIN_TEST(RenderPostProcess, ToneMapping) {
    SceneRenderSettings s;
    // Off by default. ACES is a strong look and should be chosen, not inherited.
    ENJIN_EXPECT_EQ(s.toneMappingMode, 0u);
    ENJIN_EXPECT_FLOAT_EQ(s.exposure, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.gamma, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.whitePoint, 4.0f);
}

ENJIN_TEST(RenderPostProcess, BloomDisabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.bloomEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.bloomThreshold, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.bloomIntensity, 0.5f);
}

ENJIN_TEST(RenderPostProcess, VignetteDisabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.vignetteEnabled);
    ENJIN_EXPECT_FLOAT_NEAR(s.vignetteIntensity, 0.3f, 0.01f);
}

ENJIN_TEST(RenderPostProcess, ChromaticAberrationDisabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.chromaticAberrationEnabled);
}

ENJIN_TEST(RenderPostProcess, FilmGrainDisabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.filmGrainEnabled);
    ENJIN_EXPECT_FLOAT_NEAR(s.filmGrainIntensity, 0.05f, 0.01f);
}

ENJIN_TEST(RenderPostProcess, ColorGrading) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.saturation, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.contrast, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.brightness, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.colorFilter.x, 1.0f);
}

// ===========================================================================
// Anti-Aliasing Defaults
// ===========================================================================

ENJIN_TEST(RenderAA, DefaultMode) {
    SceneRenderSettings s;
    ENJIN_EXPECT_EQ(s.aaMode, 1u); // FXAA
    ENJIN_EXPECT_TRUE(s.fxaaEnabled);
}

ENJIN_TEST(RenderAA, FXAADefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.fxaaSpanMax, 8.0f);
}

ENJIN_TEST(RenderAA, TAADefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_NEAR(s.taaSharpness, 0.1f, 0.01f);
    ENJIN_EXPECT_FLOAT_EQ(s.taaJitterScale, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(s.taaFeedbackMin, 0.88f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(s.taaFeedbackMax, 0.97f, 0.01f);
}

ENJIN_TEST(RenderAA, UpscalerDisabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_EQ(s.upscalerType, 0u);
    ENJIN_EXPECT_EQ(s.upscalerQuality, 2u);
    ENJIN_EXPECT_FLOAT_EQ(s.upscalerSharpness, 0.0f);
}

// ===========================================================================
// Depth of Field
// ===========================================================================

ENJIN_TEST(RenderDoF, Disabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.dofEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.dofFocalDistance, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.dofFocalRange, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.dofBokehSize, 4.0f);
    ENJIN_EXPECT_FALSE(s.dofDebugCoC);
}

// ===========================================================================
// Cel Shading
// ===========================================================================

ENJIN_TEST(RenderCelShading, Disabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.celShadingEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.celDiffuseBands, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.celSpecularCutoff, 0.5f);
    ENJIN_EXPECT_FALSE(s.celOutlineEnabled);
}

// ===========================================================================
// SSAO
// ===========================================================================

ENJIN_TEST(RenderSSAO, Disabled) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.ssaoEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.ssaoRadius, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(s.ssaoIntensity, 1.5f);
    ENJIN_EXPECT_EQ(s.ssaoSamples, 16u);
}

// ===========================================================================
// Ray Tracing Defaults
// ===========================================================================

ENJIN_TEST(RenderRT, DisabledByDefault) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.rtEnabled);
    ENJIN_EXPECT_EQ(s.rtMode, 0u); // Hybrid
}

ENJIN_TEST(RenderRT, ShadowDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.rtShadowsEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.rtShadowMaxDistance, 100.0f);
}

ENJIN_TEST(RenderRT, ReflectionDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.rtReflectionsEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.rtReflectionMaxDistance, 50.0f);
    ENJIN_EXPECT_TRUE(s.rtReflectionSDFFallback);
}

ENJIN_TEST(RenderRT, AODefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.rtAOEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.rtAORadius, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.rtAOPower, 1.5f);
}

ENJIN_TEST(RenderRT, GIDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.rtGIEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.rtGIIntensity, 1.0f);
    ENJIN_EXPECT_EQ(s.rtGIBounces, 1u);
}

ENJIN_TEST(RenderRT, PathTracerDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_EQ(s.rtPathTracerMaxBounces, 4u);
    ENJIN_EXPECT_EQ(s.rtPathTracerTargetSPP, 1024u);
    ENJIN_EXPECT_TRUE(s.rtPathTracerNEE);
    ENJIN_EXPECT_TRUE(s.rtPathTracerMIS);
}

ENJIN_TEST(RenderRT, DenoiserDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.rtDenoiserEnabled);
    ENJIN_EXPECT_EQ(s.rtDenoiserType, 0u); // SVGF
    ENJIN_EXPECT_EQ(s.rtDenoiserIterations, 5u);
    ENJIN_EXPECT_EQ(s.rtOIDNQuality, 1u);
}

ENJIN_TEST(RenderRT, CompositeStrengths) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.rtShadowStrength, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.rtReflectionStrength, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(s.rtAOStrength, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.rtGIStrength, 0.5f);
}

// ===========================================================================
// Shading Model Defaults
// ===========================================================================

ENJIN_TEST(RenderShading, Defaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_EQ(s.shadingModel, 0u); // Blinn-Phong
    ENJIN_EXPECT_FALSE(s.fresnelEnabled);
    ENJIN_EXPECT_FALSE(s.energyConservation);
    ENJIN_EXPECT_FALSE(s.geometryTerm);
}

// ===========================================================================
// Retro Overrides
// ===========================================================================

ENJIN_TEST(RenderRetro, GlobalOverridesOff) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.globalFlatShading);
    ENJIN_EXPECT_FALSE(s.globalAffineTexturing);
    ENJIN_EXPECT_FALSE(s.globalVertexSnapping);
    ENJIN_EXPECT_FALSE(s.globalStippleTransparency);
    ENJIN_EXPECT_FALSE(s.globalUVQuantize);
    ENJIN_EXPECT_FALSE(s.globalGouraudOnly);
    ENJIN_EXPECT_EQ(s.globalVertexSnapResolution, 160u);
}

ENJIN_TEST_MAIN()
