// ArtStyleComponent: fifty fields, thirteen consumers.
//
// Thirteen of its fields reached push constants or the material SSBO. The rest
// were authored through a full inspector, saved into the scene, reloaded, and read
// by nothing -- and the place they should have been consumed said this:
//
//   case ArtStyleType::NPR:
//   case ArtStyleType::PixelArt:
//   case ArtStyleType::Analog:
//       // These styles are primarily post-process driven (outlines, palettes,
//       // film effects). The per-entity component stores parameters but the
//       // actual rendering happens in the post-process pass, which queries
//       // ArtStyleComponent on the camera entity or scene default.
//
// No such query existed anywhere in the engine. Three whole styles -- around
// twenty-five fields -- were a comment describing code nobody had written.
//
// The effects themselves were never the missing part: every one of these fields
// has a working counterpart in SceneRenderSettings, which the post-process chain
// already reads. What was missing was the wiring between the two structs. That
// wiring is ApplyArtStyleComponentToSettings, and because it is a pure function
// over two plain structs it can be checked here, with no renderer and no GPU.
#include "EnjinTest.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/ECS/Components/ArtStyle.h"

using namespace Enjin;
using namespace Enjin::Renderer;
using namespace Enjin::ECS;

namespace {

bool Near(f32 a, f32 b, f32 eps = 0.001f) { return (a - b) < eps && (b - a) < eps; }

} // namespace

// ---------------------------------------------------------------------------
// Inherit means "do not touch the scene"
// ---------------------------------------------------------------------------

ENJIN_TEST(ArtStyleMapping, InheritChangesNothingAtAll) {
    // The default style. A component left alone must not alter one setting, or
    // adding the component to an entity to configure one thing would silently
    // restyle the whole scene.
    SceneRenderSettings before;
    SceneRenderSettings after = before;

    ArtStyleComponent art;   // style defaults to Inherit
    ApplyArtStyleComponentToSettings(art, after);

    ENJIN_EXPECT_EQ(after.shadingModel, before.shadingModel);
    ENJIN_EXPECT_EQ(after.celShadingEnabled, before.celShadingEnabled);
    ENJIN_EXPECT_EQ(after.filmGrainEnabled, before.filmGrainEnabled);
    ENJIN_EXPECT_EQ(after.crtEnabled, before.crtEnabled);
    ENJIN_EXPECT_EQ(after.paletteEnabled, before.paletteEnabled);
    ENJIN_EXPECT_EQ(after.globalVertexSnapping, before.globalVertexSnapping);
    ENJIN_EXPECT_TRUE(Near(after.saturation, before.saturation));
}

// ---------------------------------------------------------------------------
// Analog -- the group the comment claimed was handled and was not
// ---------------------------------------------------------------------------

ENJIN_TEST(ArtStyleMapping, AnalogReachesEveryFilmEffectItNames) {
    SceneRenderSettings s;
    ArtStyleComponent art;
    art.style = ArtStyleType::Analog;
    art.analog_filmGrain = true;
    art.analog_filmGrainIntensity = 0.12f;
    art.analog_chromaticAberration = true;
    art.analog_chromaticIntensity = 0.009f;
    art.analog_vhsEnabled = true;
    art.analog_vhsTrackingIntensity = 0.44f;
    art.analog_crtEnabled = true;
    art.analog_scanlineIntensity = 0.66f;

    ApplyArtStyleComponentToSettings(art, s);

    ENJIN_EXPECT_TRUE(s.filmGrainEnabled);
    ENJIN_EXPECT_TRUE(Near(s.filmGrainIntensity, 0.12f));
    ENJIN_EXPECT_TRUE(s.chromaticAberrationEnabled);
    ENJIN_EXPECT_TRUE(Near(s.chromaticAberrationIntensity, 0.009f));
    ENJIN_EXPECT_TRUE(s.vhsEnabled);
    ENJIN_EXPECT_TRUE(Near(s.vhsTrackingIntensity, 0.44f));
    ENJIN_EXPECT_TRUE(s.crtEnabled);
    ENJIN_EXPECT_TRUE(Near(s.scanlineIntensity, 0.66f));
}

ENJIN_TEST(ArtStyleMapping, AnalogWithEverythingOffTurnsThoseEffectsOff) {
    // The mapping has to carry a FALSE as faithfully as a true. Setting only the
    // enables would make the component a one-way switch: you could turn grain on
    // and never off again.
    SceneRenderSettings s;
    s.filmGrainEnabled = true;
    s.crtEnabled = true;
    s.vhsEnabled = true;
    s.chromaticAberrationEnabled = true;

    ArtStyleComponent art;
    art.style = ArtStyleType::Analog;
    art.analog_filmGrain = false;
    art.analog_chromaticAberration = false;
    art.analog_vhsEnabled = false;
    art.analog_crtEnabled = false;

    ApplyArtStyleComponentToSettings(art, s);

    ENJIN_EXPECT_FALSE(s.filmGrainEnabled);
    ENJIN_EXPECT_FALSE(s.chromaticAberrationEnabled);
    ENJIN_EXPECT_FALSE(s.vhsEnabled);
    ENJIN_EXPECT_FALSE(s.crtEnabled);
}

// ---------------------------------------------------------------------------
// The rest of the styles
// ---------------------------------------------------------------------------

ENJIN_TEST(ArtStyleMapping, PrePBRSelectsBlinnPhongAndItsShadingFlags) {
    SceneRenderSettings s;
    s.shadingModel = 1;   // PBR
    ArtStyleComponent art;
    art.style = ArtStyleType::PrePBR;
    art.prePBR_halfLambert = true;
    art.prePBR_flatShading = true;
    art.prePBR_gouraudOnly = true;

    ApplyArtStyleComponentToSettings(art, s);

    ENJIN_EXPECT_EQ(s.shadingModel, 0u);      // the whole point of "pre-PBR"
    ENJIN_EXPECT_TRUE(s.halfLambert);
    ENJIN_EXPECT_TRUE(s.globalFlatShading);
    ENJIN_EXPECT_TRUE(s.globalGouraudOnly);
}

ENJIN_TEST(ArtStyleMapping, HandPaintedTranslatesAnAdditiveBoostIntoAMultiplier) {
    // The component's saturationBoost is additive around 0; the scene's saturation
    // is a multiplier around 1. Assigning one to the other directly would make a
    // default component (boost 0) desaturate the scene to nothing.
    SceneRenderSettings s;
    ArtStyleComponent art;
    art.style = ArtStyleType::HandPainted;
    art.handPainted_saturationBoost = 0.0f;

    ApplyArtStyleComponentToSettings(art, s);
    ENJIN_EXPECT_TRUE(Near(s.saturation, 1.0f));

    art.handPainted_saturationBoost = 0.25f;
    ApplyArtStyleComponentToSettings(art, s);
    ENJIN_EXPECT_TRUE(Near(s.saturation, 1.25f));

    art.handPainted_saturationBoost = -0.4f;
    ApplyArtStyleComponentToSettings(art, s);
    ENJIN_EXPECT_TRUE(Near(s.saturation, 0.6f));
}

ENJIN_TEST(ArtStyleMapping, CelToonTurnsOutlinesOnOnlyWhenTheWidthIsNonZero) {
    // A zero width means "no outline". Enabling outlines regardless and passing a
    // zero width would put the outline pass in the frame doing nothing, which
    // costs a pass and looks like the setting is broken.
    SceneRenderSettings s;
    ArtStyleComponent art;
    art.style = ArtStyleType::CelToon;
    art.cel_diffuseBands = 5.0f;
    art.cel_outlineWidth = 0.0f;

    ApplyArtStyleComponentToSettings(art, s);
    ENJIN_EXPECT_TRUE(s.celShadingEnabled);
    ENJIN_EXPECT_TRUE(Near(s.celDiffuseBands, 5.0f));
    ENJIN_EXPECT_FALSE(s.geometryOutlinesEnabled);

    art.cel_outlineWidth = 0.03f;
    ApplyArtStyleComponentToSettings(art, s);
    ENJIN_EXPECT_TRUE(s.geometryOutlinesEnabled);
    ENJIN_EXPECT_TRUE(Near(s.geometryOutlineWidth, 0.03f));
}

ENJIN_TEST(ArtStyleMapping, NPRCarriesOutlinesAndStipple) {
    SceneRenderSettings s;
    ArtStyleComponent art;
    art.style = ArtStyleType::NPR;
    art.npr_celOutline = true;
    art.npr_outlineThickness = 3.0f;
    art.npr_curvatureWeight = 1.25f;
    art.npr_stipplePatternMask = 5;
    art.npr_stippleDensity = 0.35f;
    art.npr_stippleStrength = 0.7f;
    art.npr_diffuseBands = 3;

    ApplyArtStyleComponentToSettings(art, s);

    ENJIN_EXPECT_TRUE(s.celOutlineEnabled);
    ENJIN_EXPECT_TRUE(Near(s.celOutlineThickness, 3.0f));
    ENJIN_EXPECT_TRUE(Near(s.celOutlineCurvatureWeight, 1.25f));
    ENJIN_EXPECT_TRUE(s.stippleEnabled);
    ENJIN_EXPECT_EQ(s.stipplePatternMask, 5u);
    ENJIN_EXPECT_TRUE(Near(s.stippleDensity, 0.35f));
    ENJIN_EXPECT_TRUE(Near(s.stippleStrength, 0.7f));
    ENJIN_EXPECT_TRUE(Near(s.celDiffuseBands, 3.0f));
}

ENJIN_TEST(ArtStyleMapping, ZeroStippleStrengthLeavesStippleOff) {
    // Strength 0 is "no stipple". Enabling the pass anyway would run a full-screen
    // effect that blends nothing.
    SceneRenderSettings s;
    ArtStyleComponent art;
    art.style = ArtStyleType::NPR;
    art.npr_stippleStrength = 0.0f;

    ApplyArtStyleComponentToSettings(art, s);
    ENJIN_EXPECT_FALSE(s.stippleEnabled);
}

ENJIN_TEST(ArtStyleMapping, RetroCarriesThePS1FlagsAndSnapResolution) {
    SceneRenderSettings s;
    ArtStyleComponent art;
    art.style = ArtStyleType::Retro;
    art.retro_vertexSnapping = true;
    art.retro_snapResolution = 96;
    art.retro_affineTexturing = true;
    art.retro_uvQuantize = true;
    art.retro_flatShading = true;
    art.retro_posterizeLevels = 32.0f;

    ApplyArtStyleComponentToSettings(art, s);

    ENJIN_EXPECT_TRUE(s.globalVertexSnapping);
    ENJIN_EXPECT_EQ(s.globalVertexSnapResolution, 96u);
    ENJIN_EXPECT_TRUE(s.globalAffineTexturing);
    ENJIN_EXPECT_TRUE(s.globalUVQuantize);
    ENJIN_EXPECT_TRUE(s.globalFlatShading);
    ENJIN_EXPECT_TRUE(Near(s.posterizeLevels, 32.0f));
}

ENJIN_TEST(ArtStyleMapping, PixelArtSetsThePaletteAndPointFiltering) {
    SceneRenderSettings s;
    s.textureFilter = 2;   // Trilinear
    ArtStyleComponent art;
    art.style = ArtStyleType::PixelArt;
    art.pixel_paletteColors = 8;
    art.pixel_pointFiltering = true;
    art.pixel_normalQuantizeSteps = 6;

    ApplyArtStyleComponentToSettings(art, s);

    ENJIN_EXPECT_TRUE(s.resDownscaleEnabled);
    ENJIN_EXPECT_TRUE(s.paletteEnabled);
    ENJIN_EXPECT_EQ(s.paletteColors, 8u);
    ENJIN_EXPECT_TRUE(s.colorQuantEnabled);
    ENJIN_EXPECT_EQ(s.normalQuantizeSteps, 6u);
    // Point filtering is the GLOBAL texture filter (0 = Point), which is what
    // stops every texture in a pixel-art scene being bilinear-smeared.
    ENJIN_EXPECT_EQ(s.textureFilter, 0u);
}

ENJIN_TEST(ArtStyleMapping, MaterialExpressionTouchesNothingSceneWide) {
    // Surface noise and subsurface scattering are genuinely per-entity and already
    // reach the SSBO and push constants. Setting anything scene-wide here would
    // override the whole scene for one entity's sake.
    SceneRenderSettings before;
    SceneRenderSettings after = before;

    ArtStyleComponent art;
    art.style = ArtStyleType::MaterialExpression;
    art.matExpr_sssIntensity = 0.8f;
    art.matExpr_surfaceNoiseScale = 9.0f;

    ApplyArtStyleComponentToSettings(art, after);

    ENJIN_EXPECT_EQ(after.shadingModel, before.shadingModel);
    ENJIN_EXPECT_EQ(after.celShadingEnabled, before.celShadingEnabled);
    ENJIN_EXPECT_EQ(after.filmGrainEnabled, before.filmGrainEnabled);
    ENJIN_EXPECT_TRUE(Near(after.saturation, before.saturation));
}

// ---------------------------------------------------------------------------
// One style at a time
// ---------------------------------------------------------------------------

ENJIN_TEST(ArtStyleMapping, AStyleOnlyWritesItsOwnGroup) {
    // Applying Retro must not switch on film grain, and applying Analog must not
    // snap vertices. The component holds every group's parameters at once, so a
    // mapping that wrote all of them would make choosing a style apply all eight.
    SceneRenderSettings s;

    ArtStyleComponent retro;
    retro.style = ArtStyleType::Retro;
    retro.analog_filmGrain = true;       // set, but belongs to another group
    retro.analog_crtEnabled = true;
    ApplyArtStyleComponentToSettings(retro, s);

    ENJIN_EXPECT_TRUE(s.globalVertexSnapping);
    ENJIN_EXPECT_FALSE(s.filmGrainEnabled);
    ENJIN_EXPECT_FALSE(s.crtEnabled);

    SceneRenderSettings s2;
    ArtStyleComponent analog;
    analog.style = ArtStyleType::Analog;
    analog.retro_vertexSnapping = true;  // set, but belongs to another group
    analog.analog_crtEnabled = true;
    ApplyArtStyleComponentToSettings(analog, s2);

    ENJIN_EXPECT_TRUE(s2.crtEnabled);
    ENJIN_EXPECT_FALSE(s2.globalVertexSnapping);
}

ENJIN_TEST_MAIN()
