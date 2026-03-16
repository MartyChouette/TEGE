#include "EnjinTest.h"
#include "Enjin/Effects/RetroEffects.h"

using namespace Enjin;
using namespace Enjin::Effects;

// ===========================================================================
// RetroEffects Default State
// ===========================================================================

ENJIN_TEST(RetroDefault, DisabledByDefault) {
    RetroEffects retro;
    ENJIN_EXPECT_FALSE(retro.IsEnabled());
}

ENJIN_TEST(RetroDefault, DitherPattern) {
    RetroEffects retro;
    ENJIN_EXPECT_EQ((int)retro.GetDitherPattern(), (int)DitherPattern::None);
}

ENJIN_TEST(RetroDefault, ColorMode) {
    RetroEffects retro;
    ENJIN_EXPECT_EQ((int)retro.GetColorMode(), (int)ColorMode::TrueColor);
}

ENJIN_TEST(RetroDefault, ColorPreset) {
    RetroEffects retro;
    ENJIN_EXPECT_EQ((int)retro.GetColorPreset(), (int)ColorPreset::None);
}

ENJIN_TEST(RetroDefault, NotTransitioning) {
    RetroEffects retro;
    ENJIN_EXPECT_FALSE(retro.IsTransitioning());
}

// ===========================================================================
// Enable/Disable
// ===========================================================================

ENJIN_TEST(RetroToggle, SetEnabled) {
    RetroEffects retro;
    retro.SetEnabled(true);
    ENJIN_EXPECT_TRUE(retro.IsEnabled());
    retro.SetEnabled(false);
    ENJIN_EXPECT_FALSE(retro.IsEnabled());
}

// ===========================================================================
// Settings
// ===========================================================================

ENJIN_TEST(RetroSettings, DitherPattern) {
    RetroEffects retro;
    retro.SetDitherPattern(DitherPattern::Bayer4x4);
    ENJIN_EXPECT_EQ((int)retro.GetDitherPattern(), (int)DitherPattern::Bayer4x4);
}

ENJIN_TEST(RetroSettings, ColorMode) {
    RetroEffects retro;
    retro.SetColorMode(ColorMode::HighColor);
    ENJIN_EXPECT_EQ((int)retro.GetColorMode(), (int)ColorMode::HighColor);
}

ENJIN_TEST(RetroSettings, Resolution) {
    RetroEffects retro;
    ResolutionSettings res;
    res.renderWidth = 640;
    res.renderHeight = 480;
    res.pointFiltering = false;
    retro.SetResolution(res);
    ENJIN_EXPECT_EQ(retro.GetResolution().renderWidth, 640u);
    ENJIN_EXPECT_EQ(retro.GetResolution().renderHeight, 480u);
    ENJIN_EXPECT_FALSE(retro.GetResolution().pointFiltering);
}

ENJIN_TEST(RetroSettings, AffineSettings) {
    RetroEffects retro;
    AffineSettings affine;
    affine.enabled = true;
    affine.warpStrength = 0.8f;
    retro.SetAffineSettings(affine);
    ENJIN_EXPECT_TRUE(retro.GetAffineSettings().enabled);
    ENJIN_EXPECT_FLOAT_EQ(retro.GetAffineSettings().warpStrength, 0.8f);
}

ENJIN_TEST(RetroSettings, VertexJitter) {
    RetroEffects retro;
    VertexJitterSettings jitter;
    jitter.enabled = true;
    jitter.jitterAmount = 1.0f;
    jitter.gridResolution = 320;
    retro.SetVertexJitter(jitter);
    ENJIN_EXPECT_TRUE(retro.GetVertexJitter().enabled);
    ENJIN_EXPECT_FLOAT_EQ(retro.GetVertexJitter().jitterAmount, 1.0f);
    ENJIN_EXPECT_EQ(retro.GetVertexJitter().gridResolution, 320u);
}

ENJIN_TEST(RetroSettings, CRT) {
    RetroEffects retro;
    CRTSettings crt;
    crt.enabled = true;
    crt.scanlineIntensity = 0.5f;
    retro.SetCRTSettings(crt);
    ENJIN_EXPECT_TRUE(retro.GetCRTSettings().enabled);
    ENJIN_EXPECT_FLOAT_EQ(retro.GetCRTSettings().scanlineIntensity, 0.5f);
}

ENJIN_TEST(RetroSettings, VHS) {
    RetroEffects retro;
    VHSSettings vhs;
    vhs.enabled = true;
    vhs.trackingIntensity = 0.5f;
    retro.SetVHSSettings(vhs);
    ENJIN_EXPECT_TRUE(retro.GetVHSSettings().enabled);
}

ENJIN_TEST(RetroSettings, GouraudOnly) {
    RetroEffects retro;
    ENJIN_EXPECT_FALSE(retro.GetGouraudOnly());
    retro.SetGouraudOnly(true);
    ENJIN_EXPECT_TRUE(retro.GetGouraudOnly());
}

ENJIN_TEST(RetroSettings, SphereEnvMap) {
    RetroEffects retro;
    retro.SetSphereEnvMap(true);
    retro.SetSphereEnvStrength(0.8f);
    ENJIN_EXPECT_TRUE(retro.GetSphereEnvMap());
    ENJIN_EXPECT_FLOAT_EQ(retro.GetSphereEnvStrength(), 0.8f);
}

ENJIN_TEST(RetroSettings, PosterizeLevels) {
    RetroEffects retro;
    retro.SetPosterizeLevels(16.0f);
    ENJIN_EXPECT_FLOAT_EQ(retro.GetPosterizeLevels(), 16.0f);
}

// ===========================================================================
// CRT Model Specs
// ===========================================================================

ENJIN_TEST(CRTModels, SpecCount) {
    // Should have specs for all CRT models including Custom
    ENJIN_EXPECT_EQ((int)CRTModel::Count, 11);
}

ENJIN_TEST(CRTModels, SonyTrinitronIsApertureGrille) {
    // Sony Trinitron uses aperture grille (maskType=0)
    ENJIN_EXPECT_EQ(CRT_MODEL_SPECS[1].maskType, 0u);
}

ENJIN_TEST(CRTModels, ArcadeHasHighBloom) {
    // Generic arcade should have high bloom
    ENJIN_EXPECT_TRUE(CRT_MODEL_SPECS[8].bloomStrength > 0.4f);
}

// ===========================================================================
// Resolution Defaults
// ===========================================================================

ENJIN_TEST(Resolution, Defaults) {
    ResolutionSettings res;
    ENJIN_EXPECT_EQ(res.renderWidth, 320u);
    ENJIN_EXPECT_EQ(res.renderHeight, 240u);
    ENJIN_EXPECT_TRUE(res.pointFiltering);
    ENJIN_EXPECT_TRUE(res.integerScaling);
}

// ===========================================================================
// Transition Settings
// ===========================================================================

ENJIN_TEST(Transition, Defaults) {
    TransitionSettings ts;
    ENJIN_EXPECT_EQ((int)ts.type, (int)TransitionType::Fade);
    ENJIN_EXPECT_FLOAT_EQ(ts.duration, 0.5f);
    ENJIN_EXPECT_FALSE(ts.reversed);
}

ENJIN_TEST_MAIN()
