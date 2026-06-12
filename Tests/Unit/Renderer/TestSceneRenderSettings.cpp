#include "EnjinTest.h"
#include "Enjin/Renderer/SceneRenderSettings.h"

using namespace Enjin;
using namespace Enjin::Renderer;

// ===========================================================================
// SceneRenderSettings — Core Defaults
// ===========================================================================

ENJIN_TEST(Core, UseProjectDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.useProjectDefaults);
}

ENJIN_TEST(Core, ShadowDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.shadowsEnabled);
    ENJIN_EXPECT_EQ(s.shadowResolution, 2048u);
    ENJIN_EXPECT_FLOAT_EQ(s.shadowDistance, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.shadowStrength, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.shadowSoftness, 0.0f);
}

ENJIN_TEST(Core, RenderFlags) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.backfaceCulling);
    ENJIN_EXPECT_FALSE(s.wireframe);
}

ENJIN_TEST(Core, AmbientDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.ambientIntensity, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(s.ambientColor.x, 0.1f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(s.ambientColor.y, 0.1f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(s.ambientColor.z, 0.15f, 0.01f);
}

ENJIN_TEST(Core, FogDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.fogDensity, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.fogStart, 20.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.fogEnd, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.fogHeightFalloff, 0.1f);
}

// ===========================================================================
// Post-Processing Defaults
// ===========================================================================

ENJIN_TEST(PostFX, ToneMapping) {
    SceneRenderSettings s;
    ENJIN_EXPECT_EQ(s.toneMappingMode, 3u);  // ACES is the default
    ENJIN_EXPECT_FLOAT_EQ(s.exposure, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.gamma, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.whitePoint, 4.0f);
}

ENJIN_TEST(PostFX, BloomDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.bloomEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.bloomThreshold, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.bloomIntensity, 0.5f);
}

ENJIN_TEST(PostFX, VignetteDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.vignetteEnabled);
    ENJIN_EXPECT_FLOAT_NEAR(s.vignetteIntensity, 0.3f, 0.01f);
}

ENJIN_TEST(PostFX, FXAADefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_TRUE(s.fxaaEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.fxaaSpanMax, 8.0f);
}

ENJIN_TEST(PostFX, ColorGradingDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.colorFilter.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.saturation, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.contrast, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.brightness, 0.0f);
}

ENJIN_TEST(PostFX, FilmGrainDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.filmGrainEnabled);
    ENJIN_EXPECT_FLOAT_NEAR(s.filmGrainIntensity, 0.05f, 0.001f);
}

ENJIN_TEST(PostFX, DOFDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.dofEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.dofFocalDistance, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.dofFocalRange, 5.0f);
}

ENJIN_TEST(PostFX, TiltShiftDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.tiltShiftEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.tiltShiftFocusY, 0.5f);
}

// ===========================================================================
// Retro Defaults
// ===========================================================================

ENJIN_TEST(Retro, DitherDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.ditherEnabled);
    ENJIN_EXPECT_FALSE(s.colorQuantEnabled);
    ENJIN_EXPECT_EQ(s.colorBitDepth, 8u);
}

ENJIN_TEST(Retro, ResolutionDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.resDownscaleEnabled);
    ENJIN_EXPECT_EQ(s.internalWidth, 320u);
    ENJIN_EXPECT_EQ(s.internalHeight, 240u);
    ENJIN_EXPECT_TRUE(s.usePointFiltering);
}

ENJIN_TEST(Retro, CRTDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.crtEnabled);
    ENJIN_EXPECT_FLOAT_NEAR(s.scanlineIntensity, 0.3f, 0.01f);
}

ENJIN_TEST(Retro, VHSDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.vhsEnabled);
    ENJIN_EXPECT_FALSE(s.vhsScreenTear);
    ENJIN_EXPECT_FALSE(s.vhsInterlacing);
}

ENJIN_TEST(Retro, GlobalOverrides) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.globalFlatShading);
    ENJIN_EXPECT_FALSE(s.globalAffineTexturing);
    ENJIN_EXPECT_FALSE(s.globalVertexSnapping);
    ENJIN_EXPECT_FALSE(s.globalStippleTransparency);
    ENJIN_EXPECT_EQ(s.globalVertexSnapResolution, 160u);
}

// ===========================================================================
// Screen-Space Effects Defaults
// ===========================================================================

ENJIN_TEST(ScreenFX, GodRaysDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.godRaysEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.godRaysIntensity, 0.5f);
    ENJIN_EXPECT_EQ(s.godRaysSamples, 64u);
}

ENJIN_TEST(ScreenFX, SSAODefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.ssaoEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.ssaoRadius, 0.5f);
    ENJIN_EXPECT_EQ(s.ssaoSamples, 16u);
}

ENJIN_TEST(ScreenFX, ContactShadowsDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.contactShadowsEnabled);
    ENJIN_EXPECT_EQ(s.contactShadowsSteps, 16u);
}

ENJIN_TEST(ScreenFX, CausticsDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.causticsEnabled);
    ENJIN_EXPECT_FLOAT_NEAR(s.causticsIntensity, 0.3f, 0.01f);
}

ENJIN_TEST(ScreenFX, FogShaftsDefaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.fogShaftsEnabled);
    ENJIN_EXPECT_EQ(s.fogShaftsSamples, 16u);
    ENJIN_EXPECT_FLOAT_EQ(s.fogShaftsMaxDistance, 50.0f);
}

// ===========================================================================
// RT Fields in SceneRenderSettings
// ===========================================================================

ENJIN_TEST(RT, Defaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.rtEnabled);
    ENJIN_EXPECT_EQ(s.rtMode, 0u);
    ENJIN_EXPECT_TRUE(s.rtShadowsEnabled);
    ENJIN_EXPECT_TRUE(s.rtReflectionsEnabled);
    ENJIN_EXPECT_TRUE(s.rtAOEnabled);
    ENJIN_EXPECT_FALSE(s.rtGIEnabled);
    ENJIN_EXPECT_EQ(s.rtPathTracerMaxBounces, 4u);
    ENJIN_EXPECT_EQ(s.rtPathTracerTargetSPP, 1024u);
    ENJIN_EXPECT_TRUE(s.rtDenoiserEnabled);
}

ENJIN_TEST(RT, CompositeStrengths) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FLOAT_EQ(s.rtShadowStrength, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.rtReflectionStrength, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(s.rtAOStrength, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.rtGIStrength, 0.5f);
}

// ===========================================================================
// Cel Shading Defaults
// ===========================================================================

ENJIN_TEST(CelShading, Defaults) {
    SceneRenderSettings s;
    ENJIN_EXPECT_FALSE(s.celShadingEnabled);
    ENJIN_EXPECT_FLOAT_EQ(s.celDiffuseBands, 3.0f);
    ENJIN_EXPECT_FALSE(s.celOutlineEnabled);
}

// ===========================================================================
// Static Factory
// ===========================================================================

ENJIN_TEST(Static, DefaultsFactory) {
    auto s = SceneRenderSettings::Defaults();
    ENJIN_EXPECT_TRUE(s.useProjectDefaults);
    ENJIN_EXPECT_TRUE(s.shadowsEnabled);
    ENJIN_EXPECT_TRUE(s.fxaaEnabled);
}

ENJIN_TEST_MAIN()
