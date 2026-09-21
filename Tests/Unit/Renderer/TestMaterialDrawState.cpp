// The surfaceParam band cascade, pinned.
//
// Three near-copies of this cascade lived inside RenderSystem's three push-constant
// builders and had already drifted (measured 2026-09-21: 26 / 23 / 20 writes). The one
// with the fewest was RenderEntity -- the MAIN pass, which is editor play mode and every
// exported game -- so a CelToon rim strength and a MaterialExpression surface noise
// applied only in the editor viewport.
//
// The bands are a CASCADE and the order IS the meaning: each mode that claims
// surfaceParam1 excludes the ones after it that test for a free slot. So these tests
// assert the ORDER and the NUMBERS, not "whatever the function returns" -- a test that
// read the band back out of the implementation would pass through any renumbering and
// catch nothing. The numbers are a contract with triangle.frag.

#include "EnjinTest.h"
#include "Enjin/Renderer/MaterialDrawState.h"

using namespace Enjin;
using Renderer::BuildMaterialDrawState;
using Renderer::MaterialDrawState;
using Renderer::MaterialTextureBindings;
using Renderer::MaterialFlagOverrides;

namespace {
// The palette band a caller would pass for slot 3 with at least four tables loaded.
constexpr f32 kPaletteSlot3 = 503.0f;
constexpr f32 kNoPalette = 500.0f;
}  // namespace

ENJIN_TEST(MaterialDrawState, test_default_material_claims_no_band) {
    // Arrange
    ECS::MaterialComponent m;
    m.reflectivity = 0.25f;
    m.fresnelPower = 4.0f;
    m.rimLightStrength = 0.5f;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette);

    // Assert — below 100 the three slots are the artistic params, not a mode.
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 0.25f);
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam2, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam3, 0.5f);
}

ENJIN_TEST(MaterialDrawState, test_dither_gradient_claims_band_100_and_forces_flat_shading) {
    // Arrange
    ECS::MaterialComponent m;
    m.ditherGradient = true;
    m.ditherGradientBands = 4;
    m.ditherGradientPattern = 2;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette);

    // Assert — 100 + bands + pattern/10, and dithering is meaningless without flat shading.
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 104.2f);
    ENJIN_EXPECT_TRUE((s.flags & (1 << 20)) != 0);
}

ENJIN_TEST(MaterialDrawState, test_dither_transparency_overrides_dither_gradient) {
    // Arrange — both set, which the cascade resolves by order rather than by rejecting.
    ECS::MaterialComponent m;
    m.ditherGradient = true;
    m.ditherGradientBands = 4;
    m.ditherTransparency = true;
    m.ditherTransPattern = 3;
    m.ditherTransOpacity = 0.4f;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 203.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam2, 0.4f);
}

ENJIN_TEST(MaterialDrawState, test_elemental_claims_band_300_when_no_dither_mode_did) {
    // Arrange
    ECS::MaterialComponent m;
    ECS::ElementalSurfaceComponent e;
    e.charAmount = 0.5f;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette, &e);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 300.5f);
}

ENJIN_TEST(MaterialDrawState, test_elemental_below_threshold_claims_nothing) {
    // Arrange — an untouched ElementalSurfaceComponent must not cost the entity its band.
    ECS::MaterialComponent m;
    m.reflectivity = 0.1f;
    ECS::ElementalSurfaceComponent e;   // all defaults, all at or below 0.01

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette, &e);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 0.1f);
}

ENJIN_TEST(MaterialDrawState, test_dither_wins_over_elemental) {
    // Arrange
    ECS::MaterialComponent m;
    m.ditherGradient = true;
    m.ditherGradientBands = 2;
    ECS::ElementalSurfaceComponent e;
    e.wetness = 1.0f;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette, &e);

    // Assert — 102, not 300-something.
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 102.0f);
}

ENJIN_TEST(MaterialDrawState, test_surface_noise_claims_band_400_only_when_slot_is_free) {
    // Arrange
    ECS::MaterialComponent free;
    free.surfaceNoiseScale = 6.0f;
    free.surfaceNoiseStrength = 0.3f;

    ECS::MaterialComponent taken = free;
    taken.ditherTransparency = true;      // claims 200 first

    // Act
    const MaterialDrawState a = BuildMaterialDrawState(free, {}, {}, kNoPalette);
    const MaterialDrawState b = BuildMaterialDrawState(taken, {}, {}, kNoPalette);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(a.surfaceParam1, 406.0f);
    ENJIN_EXPECT_FLOAT_EQ(a.surfaceParam2, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(b.surfaceParam1, 200.0f);
}

ENJIN_TEST(MaterialDrawState, test_palette_band_comes_from_the_caller) {
    // Arrange — the band clamps against the palettes the SCENE has, which is renderer
    // state, so the caller resolves it and this must not invent one.
    ECS::MaterialComponent m;
    m.paletteIndexed = true;
    m.paletteSlot = 3;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kPaletteSlot3);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 503.0f);
}

ENJIN_TEST(MaterialDrawState, test_lightmapped_wins_over_palette) {
    // Arrange — they cannot coexist: every mode shares one float, so the order decides.
    ECS::MaterialComponent m;
    m.paletteIndexed = true;
    m.paletteSlot = 3;
    m.lightmapped = true;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kPaletteSlot3);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam1, 600.0f);
}

ENJIN_TEST(MaterialDrawState, test_global_vertex_snap_resolution_replaces_the_authored_one) {
    // Arrange
    ECS::MaterialComponent m;
    m.vertexSnapping = true;
    m.vertexSnapResolution = 80;          // -> 10 in bits 24-28
    MaterialFlagOverrides global;
    global.vertexSnapping = true;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, global, kNoPalette, nullptr, nullptr, 160);

    // Assert — the global 160/8 = 20 replaces the material's 10 rather than OR-ing into it.
    ENJIN_EXPECT_EQ((s.flags >> 24) & 0x1F, 20);
}

ENJIN_TEST(MaterialDrawState, test_cel_art_style_overrides_rim_strength) {
    // Arrange — this is one of the two overrides the main pass was missing entirely.
    ECS::MaterialComponent m;
    m.rimLightStrength = 0.1f;
    ECS::ArtStyleComponent art;
    art.style = ECS::ArtStyleType::CelToon;
    art.cel_rimStrength = 2.5f;

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette, nullptr, &art);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(s.surfaceParam3, 2.5f);
}

ENJIN_TEST(MaterialDrawState, test_material_expression_noise_respects_a_claimed_band) {
    // Arrange — the other missing override. A band already claimed is a mode already
    // chosen, so the art style must not stamp over it.
    ECS::MaterialComponent claimed;
    claimed.paletteIndexed = true;
    claimed.paletteSlot = 3;
    ECS::MaterialComponent freeSlot;

    ECS::ArtStyleComponent art;
    art.style = ECS::ArtStyleType::MaterialExpression;
    art.matExpr_surfaceNoiseScale = 5.0f;
    art.matExpr_surfaceNoiseStrength = 0.2f;

    // Act
    const MaterialDrawState a = BuildMaterialDrawState(claimed, {}, {}, kPaletteSlot3, nullptr, &art);
    const MaterialDrawState b = BuildMaterialDrawState(freeSlot, {}, {}, kNoPalette, nullptr, &art);

    // Assert
    ENJIN_EXPECT_FLOAT_EQ(a.surfaceParam1, 503.0f);
    ENJIN_EXPECT_FLOAT_EQ(b.surfaceParam1, 405.0f);
    ENJIN_EXPECT_FLOAT_EQ(b.surfaceParam2, 0.2f);
}

ENJIN_TEST(MaterialDrawState, test_retro_art_style_sets_its_four_flags_and_resolution) {
    // Arrange
    ECS::MaterialComponent m;
    ECS::ArtStyleComponent art;
    art.style = ECS::ArtStyleType::Retro;
    art.retro_flatShading = true;
    art.retro_affineTexturing = true;
    art.retro_vertexSnapping = true;
    art.retro_uvQuantize = true;
    art.retro_snapResolution = 80;        // -> 10

    // Act
    const MaterialDrawState s = BuildMaterialDrawState(m, {}, {}, kNoPalette, nullptr, &art);

    // Assert — the bit numbers are a contract with triangle.vert, so assert the numbers.
    ENJIN_EXPECT_TRUE((s.flags & (1 << 20)) != 0);   // flat shading
    ENJIN_EXPECT_TRUE((s.flags & (1 << 21)) != 0);   // affine texturing
    ENJIN_EXPECT_TRUE((s.flags & (1 << 22)) != 0);   // vertex snapping
    ENJIN_EXPECT_TRUE((s.flags & (1 << 12)) != 0);   // uv quantise
    ENJIN_EXPECT_EQ((s.flags >> 24) & 0x1F, 10);
}

ENJIN_TEST(MaterialDrawState, test_inherit_art_style_changes_nothing) {
    // Arrange — Inherit means "use the scene preset", so a component set to it must be
    // indistinguishable from having no component at all.
    ECS::MaterialComponent m;
    m.rimLightStrength = 0.7f;
    ECS::ArtStyleComponent art;           // style defaults to Inherit

    // Act
    const MaterialDrawState with = BuildMaterialDrawState(m, {}, {}, kNoPalette, nullptr, &art);
    const MaterialDrawState without = BuildMaterialDrawState(m, {}, {}, kNoPalette);

    // Assert
    ENJIN_EXPECT_EQ(with.flags, without.flags);
    ENJIN_EXPECT_FLOAT_EQ(with.surfaceParam3, without.surfaceParam3);
}

ENJIN_TEST_MAIN()
