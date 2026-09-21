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
}

// ============================================================================
// BITS 3 AND 4 BELONG TO THE VERTEX SHADER, AND ONLY TO IT (adr-0008 phase 2)
//
// triangle.vert and triangle.frag declare the SAME push-constant block, so they
// read one `flags` word. The vert reads bit 3 as FLAG_SKINNED and bit 4 as
// FLAG_WIND_SWAY; the frag used to read the same two bits as SDF text and
// exclude-cel. Both meanings were live at once:
//
//   * a skinned mesh took the SDF text path, which hard-discards every fragment
//     under 180/255 alpha -- silent on an opaque texture, which is why it went
//     unseen, and destructive on an alpha-tested one (hair, foliage cards);
//   * wind-swaying vegetation was silently excluded from cel shading;
//   * and `excludeFromCelShading` did NOTHING on any direct draw, because
//     MaterialGPU::From set bit 4 for it while BuildMaterialFlagWord never did,
//     and direct draws read the push-constant word.
//
// Both are material-STATIC, so they are specialization constants now and the
// flag word must never claim either bit again.
// ============================================================================

ENJIN_TEST(MaterialFlagWord, test_sdf_text_does_not_touch_the_skinned_bit) {
    ECS::MaterialComponent m;
    m.sdfText = true;
    // Bit 3 is FLAG_SKINNED to the vertex shader. A material must not be able
    // to set it, or a text quad tells the vertex shader it has bones.
    ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 3), 0);
}

ENJIN_TEST(MaterialFlagWord, test_exclude_cel_does_not_touch_the_wind_sway_bit) {
    ECS::MaterialComponent m;
    m.excludeFromCelShading = true;
    // Bit 4 is FLAG_WIND_SWAY to the vertex shader.
    ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (1 << 4), 0);
}

ENJIN_TEST(MaterialFlagWord, test_the_two_words_agree_about_bits_3_and_4) {
    // The push-constant word and the material SSBO word are built by different
    // code and have to agree. They did not, and nothing checked: that is what
    // made the exclude-cel checkbox inert on every direct draw.
    ECS::MaterialComponent m;
    m.sdfText = true;
    m.excludeFromCelShading = true;

    const i32 pushWord = BuildMaterialFlagWord(m);
    const ECS::MaterialGPU gpu = ECS::MaterialGPU::FromComponent(m);

    ENJIN_EXPECT_EQ(pushWord & (1 << 3), 0);
    ENJIN_EXPECT_EQ(pushWord & (1 << 4), 0);
    ENJIN_EXPECT_EQ(gpu.flags & (1 << 3), 0);
    ENJIN_EXPECT_EQ(gpu.flags & (1 << 4), 0);
}

ENJIN_TEST(MaterialFlagWord, test_alpha_mode_left_the_push_constant_word) {
    // adr-0008 phase 4. alphaMode is material-static, so it is SPEC_ALPHA_MODE
    // and bits 8-9 of the push-constant word are free.
    ECS::MaterialComponent m;
    m.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;   // 2
    ENJIN_EXPECT_EQ(BuildMaterialFlagWord(m) & (3 << 8), 0);

    // It is still in the SSBO word, and that is NOT drift: the ray-tracing hit
    // shaders read it from there and cannot be specialized per material.
    const ECS::MaterialGPU gpu = ECS::MaterialGPU::FromComponent(m);
    ENJIN_EXPECT_EQ((gpu.flags >> 8) & 0x3, 2);
}

ENJIN_TEST(MaterialFlagWord, test_alpha_mode_is_masked_in_both_words) {
    // MaterialGPU::From shifted alphaMode without the & 0x3 that
    // BuildMaterialFlagWord applies, so an out-of-range value bled into bit 10
    // (HAS_HEIGHT_TEX) on the SSBO path only.
    ECS::MaterialComponent m;
    m.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;   // 2
    const ECS::MaterialGPU gpu = ECS::MaterialGPU::FromComponent(m);
    ENJIN_EXPECT_EQ(gpu.flags & (1 << 10), 0);
    ENJIN_EXPECT_EQ((gpu.flags >> 8) & 0x3, 2);
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
    // alphaMode is NOT here any more: it is SPEC_ALPHA_MODE as of adr-0008
    // phase 4, and bits 8-9 of this word are free.
    ENJIN_EXPECT_EQ((f >> 8) & 0x3, 0);
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
