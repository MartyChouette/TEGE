#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <cstdint>
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
//   Bit 9:    SDF_TEXT      (adr-0008 phase 2)
//   Bit 10:   EXCLUDE_CEL   (adr-0008 phase 2)
//
// Dynamic flags (remain in push constants, not part of the key):
//   SKINNED, WIND_SWAY, WATER_*, GOURAUD_ONLY, retro art style flags, etc.
//
// SDF_TEXT and EXCLUDE_CEL moved here from the flag word because they are
// material-STATIC and the word had no room for them: both were squatting on
// bits the VERTEX shader reads as SKINNED and WIND_SWAY. Bit 3 and bit 4 now
// mean one thing each again. See adr-0008.

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
    static constexpr u32 SDF_TEXT        = (1u << 9);
    static constexpr u32 EXCLUDE_CEL     = (1u << 10);

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
    u32 sdfText;            // constant_id = 8
    u32 excludeCel;         // constant_id = 9
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
    d.sdfText         = (key.bits & MaterialSpecKey::SDF_TEXT)    ? 1 : 0;
    d.excludeCel      = (key.bits & MaterialSpecKey::EXCLUDE_CEL) ? 1 : 0;
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
    // THE RENDER PASS IS PART OF THE KEY.
    //
    // It was not, and a Vulkan pipeline is only usable in a render pass
    // COMPATIBLE with the one it was built against -- same attachment formats,
    // same attachment count. This engine has two that are neither: the
    // swapchain pass is B8G8R8A8_SRGB with MRT (colour + velocity), and an
    // offscreen target is B8G8R8A8_UNORM with one colour attachment.
    //
    // Keyed on material bits alone, whichever pass asked FIRST for a given
    // material combination owned that pipeline forever, and every later draw
    // with the same material in the other pass got it back and bound it. The
    // editor renders the game view offscreen first, so the offscreen variant
    // won, and the swapchain draws were the ones reported:
    //
    //   pAttachments[0].format (VK_FORMAT_B8G8R8A8_SRGB) !=
    //   pAttachments[0].format (VK_FORMAT_B8G8R8A8_UNORM)
    //   pColorAttachments[1].attachment ... first is 1, second is VK_ATTACHMENT_UNUSED
    //
    // It needed a scene with a material combination drawn in BOTH passes to
    // show at all, which is why it reproduced on a 237-mesh scene and on
    // neither of the small ones -- and why the render smoke, which runs a small
    // scene, was never going to catch it.
    struct VariantKey {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        u32 bits = 0;
        bool operator==(const VariantKey& o) const {
            return renderPass == o.renderPass && bits == o.bits;
        }
    };
    struct VariantKeyHash {
        usize operator()(const VariantKey& k) const {
            const usize a = static_cast<usize>(reinterpret_cast<uintptr_t>(k.renderPass));
            return a * 1099511628211ull ^ static_cast<usize>(k.bits);
        }
    };
    std::unordered_map<VariantKey, VkPipeline, VariantKeyHash> m_Cache;
};

#endif // !ENJIN_RENDERER_WEBGPU

} // namespace Renderer
} // namespace Enjin
