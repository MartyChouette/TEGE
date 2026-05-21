#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <unordered_map>

#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace Renderer {

// Material specialization key — encodes which static shader features are active.
// Used to select pre-compiled pipeline variants instead of runtime flag branching.
//
// Static flags (baked into pipeline via specialization constants):
//   Bit 0: HAS_BASE_COLOR_TEXTURE
//   Bit 1: HAS_NORMAL_TEXTURE
//   Bit 2: HAS_METALLIC_ROUGHNESS_TEXTURE
//   Bit 3: HAS_EMISSIVE_TEXTURE
//   Bit 4: HAS_HEIGHT_TEXTURE
//   Bit 5: DOUBLE_SIDED
//   Bit 6: FLAT_SHADING
//   Bits 7-8: ALPHA_MODE (0=Opaque, 1=Mask, 2=Blend)
//
// Dynamic flags (remain in push constants, not part of the key):
//   SKINNED, WIND_SWAY, WATER_*, GOURAUD_ONLY, retro art style flags, etc.

struct MaterialSpecKey {
    u32 bits = 0;

    // Static flag builders
    static constexpr u32 BASE_COLOR_TEX  = (1u << 0);
    static constexpr u32 NORMAL_TEX      = (1u << 1);
    static constexpr u32 METALLIC_TEX    = (1u << 2);
    static constexpr u32 EMISSIVE_TEX    = (1u << 3);
    static constexpr u32 HEIGHT_TEX      = (1u << 4);
    static constexpr u32 DOUBLE_SIDED    = (1u << 5);
    static constexpr u32 FLAT_SHADING    = (1u << 6);
    static constexpr u32 ALPHA_MODE_MASK = (3u << 7);

    void SetAlphaMode(u32 mode) { bits = (bits & ~ALPHA_MODE_MASK) | ((mode & 3u) << 7); }
    u32 GetAlphaMode() const { return (bits >> 7) & 3u; }

    bool operator==(const MaterialSpecKey& other) const { return bits == other.bits; }
    bool operator!=(const MaterialSpecKey& other) const { return bits != other.bits; }
};

struct MaterialSpecKeyHash {
    usize operator()(const MaterialSpecKey& k) const { return static_cast<usize>(k.bits); }
};

// Specialization constant data layout matching GLSL layout(constant_id=N) declarations.
// Must be tightly packed — Vulkan reads at byte offsets defined by VkSpecializationMapEntry.
struct SpecConstantData {
    u32 hasBaseColorTex;    // constant_id = 0
    u32 hasNormalTex;       // constant_id = 1
    u32 hasMetallicTex;     // constant_id = 2
    u32 hasEmissiveTex;     // constant_id = 3
    u32 hasHeightTex;       // constant_id = 4
    u32 doubleSided;        // constant_id = 5
    u32 flatShading;        // constant_id = 6
    u32 alphaMode;          // constant_id = 7
};

inline SpecConstantData MaterialSpecKeyToData(const MaterialSpecKey& key) {
    SpecConstantData d{};
    d.hasBaseColorTex = (key.bits & MaterialSpecKey::BASE_COLOR_TEX) ? 1 : 0;
    d.hasNormalTex    = (key.bits & MaterialSpecKey::NORMAL_TEX)     ? 1 : 0;
    d.hasMetallicTex  = (key.bits & MaterialSpecKey::METALLIC_TEX)  ? 1 : 0;
    d.hasEmissiveTex  = (key.bits & MaterialSpecKey::EMISSIVE_TEX)  ? 1 : 0;
    d.hasHeightTex    = (key.bits & MaterialSpecKey::HEIGHT_TEX)    ? 1 : 0;
    d.doubleSided     = (key.bits & MaterialSpecKey::DOUBLE_SIDED)  ? 1 : 0;
    d.flatShading     = (key.bits & MaterialSpecKey::FLAT_SHADING)  ? 1 : 0;
    d.alphaMode       = key.GetAlphaMode();
    return d;
}

#if !ENJIN_RENDERER_WEBGPU

// Cache of VkPipeline handles keyed by MaterialSpecKey.
// Each unique combination of static material flags gets its own pre-compiled pipeline.
class ENJIN_API PipelineVariantCache {
public:
    PipelineVariantCache() = default;
    ~PipelineVariantCache();

    // Look up or create a pipeline variant for the given specialization key.
    // On first miss, creates a new VkPipeline with VkSpecializationInfo.
    // Returns VK_NULL_HANDLE on creation failure.
    VkPipeline GetOrCreate(
        VkDevice device,
        VkPipelineLayout layout,
        VkRenderPass renderPass,
        const VkGraphicsPipelineCreateInfo& templateCI,
        const MaterialSpecKey& key
    );

    void Destroy(VkDevice device);
    usize GetVariantCount() const { return m_Cache.size(); }

private:
    std::unordered_map<MaterialSpecKey, VkPipeline, MaterialSpecKeyHash> m_Cache;
};

#endif // !ENJIN_RENDERER_WEBGPU

} // namespace Renderer
} // namespace Enjin
