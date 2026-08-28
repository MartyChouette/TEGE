#include "EnjinTest.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

// The GPU upload struct stride must match the material SSBO layout in the
// shaders. It is 128 bytes (base/emissive/metallic/roughness + SSS block +
// bindless texture indices + the scrolling-reflection row). If this changes,
// the matching shader struct AND ShaderData.h must change too, or the GPU
// reads wrong offsets. Keep the static_assert AND the runtime test below in
// lockstep - 7424305 updated only the static_assert and CI was red for 2 days.
static_assert(sizeof(MaterialGPU) == 128, "MaterialGPU stride must match the shader SSBO layout");

// ===========================================================================
// MaterialGPU layout invariant
// ===========================================================================

ENJIN_TEST(MaterialGPULayout, StrideMatchesShaderSSBO) {
    // Arrange / Act / Assert: guard the SSBO-offset invariant at runtime too.
    ENJIN_EXPECT_EQ(sizeof(MaterialGPU), (size_t)128);
}

// ===========================================================================
// MaterialComponent Defaults
// ===========================================================================

ENJIN_TEST(MaterialDefaults, BaseColor) {
    MaterialComponent mat;
    ENJIN_EXPECT_FLOAT_EQ(mat.baseColor.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(mat.baseColor.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(mat.baseColor.z, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(mat.opacity, 1.0f);
}

ENJIN_TEST(MaterialDefaults, PBR) {
    MaterialComponent mat;
    ENJIN_EXPECT_FLOAT_EQ(mat.metallic, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(mat.roughness, 0.5f);
}

ENJIN_TEST(MaterialDefaults, Emission) {
    MaterialComponent mat;
    ENJIN_EXPECT_VEC3_EQ(mat.emissiveColor, 0.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(mat.emissiveStrength, 0.0f);
}

ENJIN_TEST(MaterialDefaults, TextureIndices) {
    MaterialComponent mat;
    ENJIN_EXPECT_EQ(mat.baseColorTexture, -1);
    ENJIN_EXPECT_EQ(mat.normalTexture, -1);
    ENJIN_EXPECT_EQ(mat.metallicRoughnessTexture, -1);
    ENJIN_EXPECT_EQ(mat.emissiveTexture, -1);
    ENJIN_EXPECT_EQ(mat.heightTexture, -1);
}

ENJIN_TEST(MaterialDefaults, RenderingFlags) {
    MaterialComponent mat;
    ENJIN_EXPECT_FALSE(mat.doubleSided);
    ENJIN_EXPECT_TRUE(mat.castShadows);
    ENJIN_EXPECT_TRUE(mat.receiveShadows);
    ENJIN_EXPECT_EQ((int)mat.alphaMode, (int)MaterialComponent::AlphaMode::Opaque);
    ENJIN_EXPECT_FLOAT_EQ(mat.alphaCutoff, 0.5f);
}

ENJIN_TEST(MaterialDefaults, Transmission) {
    MaterialComponent mat;
    ENJIN_EXPECT_FLOAT_EQ(mat.transmission, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(mat.ior, 1.5f);
    ENJIN_EXPECT_FLOAT_EQ(mat.thickness, 0.0f);
}

ENJIN_TEST(MaterialDefaults, SSS) {
    MaterialComponent mat;
    ENJIN_EXPECT_FLOAT_EQ(mat.sssIntensity, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(mat.sssRadius, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.sssColor.x, 1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.sssColor.y, 0.2f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(mat.sssColor.z, 0.1f, 0.01f);
}

ENJIN_TEST(MaterialDefaults, RetroFlags) {
    MaterialComponent mat;
    ENJIN_EXPECT_FALSE(mat.flatShading);
    ENJIN_EXPECT_FALSE(mat.affineTexturing);
    ENJIN_EXPECT_FALSE(mat.vertexSnapping);
    ENJIN_EXPECT_FALSE(mat.stippleTransparency);
    ENJIN_EXPECT_FALSE(mat.uvQuantize);
    ENJIN_EXPECT_FALSE(mat.gouraudOnly);
    ENJIN_EXPECT_EQ(mat.vertexSnapResolution, (u8)160);
}

// ===========================================================================
// MaterialGPU FromComponent
// ===========================================================================

ENJIN_TEST(MaterialGPU, BasicConversion) {
    MaterialComponent mat;
    mat.baseColor = Vector3(0.5f, 0.3f, 0.1f);
    mat.metallic = 0.8f;
    mat.roughness = 0.2f;
    mat.emissiveColor = Vector3(1.0f, 0.0f, 0.0f);
    mat.emissiveStrength = 5.0f;
    mat.opacity = 0.7f;

    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    ENJIN_EXPECT_FLOAT_EQ(gpu.baseColor.x, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.metallic, 0.8f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.roughness, 0.2f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.emissiveStrength, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.opacity, 0.7f);
}

ENJIN_TEST(MaterialGPU, FlagsDoubleSided) {
    MaterialComponent mat;
    mat.doubleSided = true;
    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    ENJIN_EXPECT_TRUE((gpu.flags & 1) != 0);
}

ENJIN_TEST(MaterialGPU, FlagsCastShadows) {
    MaterialComponent mat;
    mat.castShadows = true;
    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    ENJIN_EXPECT_TRUE((gpu.flags & 2) != 0);
}

ENJIN_TEST(MaterialGPU, FlagsReceiveShadows) {
    MaterialComponent mat;
    mat.receiveShadows = true;
    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    ENJIN_EXPECT_TRUE((gpu.flags & 4) != 0);
}

ENJIN_TEST(MaterialGPU, AlphaModeEncoding) {
    MaterialComponent mat;
    mat.alphaMode = MaterialComponent::AlphaMode::Blend;
    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    i32 alphaModeField = (gpu.flags >> 8) & 0x3;
    ENJIN_EXPECT_EQ(alphaModeField, 2);
}

ENJIN_TEST(MaterialGPU, RetroFlagsFlatShading) {
    MaterialComponent mat;
    mat.flatShading = true;
    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    ENJIN_EXPECT_TRUE((gpu.flags & MaterialGPU::FLAG_FLAT_SHADING) != 0);
}

ENJIN_TEST(MaterialGPU, RetroFlagsAffineTexturing) {
    MaterialComponent mat;
    mat.affineTexturing = true;
    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    ENJIN_EXPECT_TRUE((gpu.flags & MaterialGPU::FLAG_AFFINE_TEXTURING) != 0);
}

ENJIN_TEST(MaterialGPU, TransmissionSSS) {
    MaterialComponent mat;
    mat.transmission = 0.8f;
    mat.ior = 1.33f;
    mat.thickness = 0.5f;
    mat.sssIntensity = 1.0f;
    mat.sssRadius = 2.0f;
    mat.sssColor = Vector3(1.0f, 0.5f, 0.3f);

    MaterialGPU gpu = MaterialGPU::FromComponent(mat);
    ENJIN_EXPECT_FLOAT_EQ(gpu.transmission, 0.8f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.ior, 1.33f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.thickness, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.sssIntensity, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.sssRadius, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(gpu.sssColor.x, 1.0f);
}

// ===========================================================================
// Sort Key
// ===========================================================================

ENJIN_TEST(SortKey, OpaqueLowerThanBlend) {
    MaterialComponent opaque;
    opaque.alphaMode = MaterialComponent::AlphaMode::Opaque;
    opaque.ComputeSortKey(10.0f);

    MaterialComponent blend;
    blend.alphaMode = MaterialComponent::AlphaMode::Blend;
    blend.ComputeSortKey(10.0f);

    // Pipeline bucket for opaque (0) should be less than blend (2)
    ENJIN_EXPECT_TRUE(opaque.cachedSortKey < blend.cachedSortKey);
}

ENJIN_TEST(SortKey, OpaqueFrontToBack) {
    MaterialComponent matNear;
    matNear.alphaMode = MaterialComponent::AlphaMode::Opaque;
    matNear.ComputeSortKey(5.0f);

    MaterialComponent matFar;
    matFar.alphaMode = MaterialComponent::AlphaMode::Opaque;
    matFar.ComputeSortKey(100.0f);

    // Near should have lower key (drawn first for overdraw reduction)
    u64 nearDist = matNear.cachedSortKey & 0xFFFF;
    u64 farDist = matFar.cachedSortKey & 0xFFFF;
    ENJIN_EXPECT_TRUE(nearDist < farDist);
}

ENJIN_TEST(SortKey, BlendBackToFront) {
    MaterialComponent matNear;
    matNear.alphaMode = MaterialComponent::AlphaMode::Blend;
    matNear.ComputeSortKey(5.0f);

    MaterialComponent matFar;
    matFar.alphaMode = MaterialComponent::AlphaMode::Blend;
    matFar.ComputeSortKey(100.0f);

    // Far should have lower key (drawn first for correct blending)
    u64 nearDist = matNear.cachedSortKey & 0xFFFF;
    u64 farDist = matFar.cachedSortKey & 0xFFFF;
    ENJIN_EXPECT_TRUE(farDist < nearDist);
}

// ===========================================================================
// Texture Cache Invalidation
// ===========================================================================

ENJIN_TEST(TextureCache, InvalidateClearsPointers) {
    MaterialComponent mat;
    mat.textureCacheDirty = false;
    mat.cachedBaseColorTexture = reinterpret_cast<Renderer::Texture*>(0xDEAD);
    mat.InvalidateTextureCache();
    ENJIN_EXPECT_TRUE(mat.textureCacheDirty);
    ENJIN_EXPECT_NULL(mat.cachedBaseColorTexture);
    ENJIN_EXPECT_NULL(mat.cachedNormalTexture);
}

// ===========================================================================
// TextureKey Comparison
// ===========================================================================

ENJIN_TEST(TextureKey, EqualWhenSame) {
    MaterialComponent::TextureKey a, b;
    ENJIN_EXPECT_TRUE(a == b);
}

ENJIN_TEST(TextureKey, NotEqualWhenDifferent) {
    MaterialComponent::TextureKey a, b;
    a.baseColor = reinterpret_cast<Renderer::Texture*>(0x1);
    ENJIN_EXPECT_TRUE(a != b);
}

ENJIN_TEST_MAIN()
