// The material flag word, pinned.
//
// Seven sites in RenderSystem.cpp build this word by hand, so every material
// mode is a seven-place edit and nothing makes the seven agree. They were
// measured and DO agree today; this test is what keeps that true, because the
// failure it guards against is invisible to every other check the project has:
// a wrong flag bit compiles, links, passes 266 suites and renders a picture
// that is merely wrong.
//
// The bit numbers are a contract with the shaders -- triangle.frag and
// triangle.vert on Vulkan, WebShaderData.h on WebGPU -- so this file asserts
// the NUMBERS, not "whatever the function returns". A test that read the
// constant back out of the implementation would pass through any renumbering
// and catch nothing.
#include "EnjinTest.h"
#include "Enjin/Renderer/MaterialFlagWord.h"

using namespace Enjin;
using Renderer::BuildMaterialFlagWord;
using Renderer::MaterialTextureBindings;
using Renderer::MaterialFlagOverrides;

ENJIN_TEST(MaterialFlagWord, test_a_default_material_sets_only_what_it_should) {
    // Arrange
    ECS::MaterialComponent m;

    // Act
    const i32 f = BuildMaterialFlagWord(m);

    // Assert: castShadows and receiveShadows default true, so bits 1 and 2 are
    // expected; nothing else is.
    ENJIN_EXPECT_EQ(f & 1, 0);              // not double sided
    ENJIN_EXPECT_EQ(f & 2, 2);              // casts
    ENJIN_EXPECT_EQ(f & 4, 4);              // receives
    ENJIN_EXPECT_EQ(f & (1 << 16), 0);      // no texture bound
    ENJIN_EXPECT_EQ(f & (1 << 20), 0);      // no retro mode
}

ENJIN_TEST(MaterialFlagWord, test_each_retro_mode_owns_exactly_its_own_bit) {
    // The bit numbers are the contract with the shaders. Wrong numbers here
    // render a lake as palette-indexed text, which is a real thing that
    // happened on the web path.
    // Arrange / Act / Assert
    {
        ECS::MaterialComponent m; m.flatShading = true;
        ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 20), (1 << 20));
    }
    {
        ECS::MaterialComponent m; m.affineTexturing = true;
        ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 21), (1 << 21));
    }
    {
        ECS::MaterialComponent m; m.vertexSnapping = true;
        ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 22), (1 << 22));
    }
    {
        ECS::MaterialComponent m; m.stippleTransparency = true;
        ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 23), (1 << 23));
    }
    {
        ECS::MaterialComponent m; m.uvQuantize = true;
        ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 12), (1 << 12));
    }
    {
        ECS::MaterialComponent m; m.gouraudOnly = true;
        ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 13), (1 << 13));
    }
    {
        ECS::MaterialComponent m; m.sdfText = true;
        ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 3), (1 << 3));
    }
}

ENJIN_TEST(MaterialFlagWord, test_texture_bits_describe_what_is_BOUND_not_what_is_named) {
    // A material can name a texture that failed to load. The flag has to say
    // what the shader will really find, or the shader samples a slot nothing
    // filled.
    // Arrange
    ECS::MaterialComponent m;
    MaterialTextureBindings bound;
    bound.baseColor = true;
    bound.height = true;

    // Act
    const i32 f = BuildMaterialFlagWord(m, bound);

    // Assert
    ENJIN_EXPECT_EQ(f & (1 << 16), (1 << 16));
    ENJIN_EXPECT_EQ(f & (1 << 10), (1 << 10));
    ENJIN_EXPECT_EQ(f & (1 << 17), 0);
    ENJIN_EXPECT_EQ(f & (1 << 18), 0);
    ENJIN_EXPECT_EQ(f & (1 << 19), 0);
}

ENJIN_TEST(MaterialFlagWord, test_a_global_override_forces_a_mode_on_and_never_off) {
    // Matching every hand-written site: the art-style globals are an OR, so a
    // scene-wide flat-shading switch cannot un-flat a material that asked for
    // it. Worth pinning because the opposite reading is the natural one.
    // Arrange
    ECS::MaterialComponent off;
    ECS::MaterialComponent on; on.flatShading = true;
    MaterialFlagOverrides global; global.flatShading = true;

    // Act / Assert
    ENJIN_EXPECT_EQ(BuildMaterialFlagWord(off, {}, global) & (1 << 20), (1 << 20));
    ENJIN_EXPECT_EQ(BuildMaterialFlagWord(on, {}, {}) & (1 << 20), (1 << 20));
    ENJIN_EXPECT_EQ(BuildMaterialFlagWord(off, {}, {}) & (1 << 20), 0);
}

ENJIN_TEST(MaterialFlagWord, test_packed_fields_land_in_their_own_ranges) {
    // Arrange: every packed field at a distinctive value.
    ECS::MaterialComponent m;
    m.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;   // 2
    m.shadowDitherMode = 3;
    m.shadowDitherPattern = 5;
    m.vertexSnapResolution = 64;                               // /8 = 8

    // Act
    const i32 f = BuildMaterialFlagWord(m);

    // Assert
    ENJIN_EXPECT_EQ((f >> 8) & 0x3, 2);
    ENJIN_EXPECT_EQ((f >> 14) & 0x3, 3);
    ENJIN_EXPECT_EQ((f >> 24) & 0x1F, 8);
    ENJIN_EXPECT_EQ((f >> 29) & 0x7, 5);
}

ENJIN_TEST(MaterialFlagWord, test_an_out_of_range_packed_value_cannot_bleed_into_its_neighbour) {
    // The failure a hand-written site is one missing mask away from: an
    // authored value wider than its field silently corrupts the bits above it,
    // and the symptom is an unrelated feature switching itself on.
    // Arrange
    ECS::MaterialComponent m;
    m.shadowDitherPattern = 255;        // 3 bits of room
    m.shadowDitherMode = 255;           // 2 bits of room
    m.vertexSnapResolution = 8 * 255;   // 5 bits of room after /8

    // Act
    const i32 f = BuildMaterialFlagWord(m);

    // Assert: each field is clamped to its own width, and the retro bits below
    // them are untouched.
    ENJIN_EXPECT_EQ((f >> 29) & 0x7, 7);
    ENJIN_EXPECT_EQ((f >> 14) & 0x3, 3);
    ENJIN_EXPECT_EQ((f >> 24) & 0x1F, 31);
    ENJIN_EXPECT_EQ(f & (1 << 20), 0);
    ENJIN_EXPECT_EQ(f & (1 << 23), 0);
}

ENJIN_TEST_MAIN()
