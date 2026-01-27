#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

// Sampler configuration
struct SamplerConfig {
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    bool anisotropyEnable = true;
    f32 maxAnisotropy = 16.0f;
    VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    bool unnormalizedCoordinates = false;
    bool compareEnable = false;
    VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    f32 mipLodBias = 0.0f;
    f32 minLod = 0.0f;
    f32 maxLod = VK_LOD_CLAMP_NONE;
};

// Vulkan sampler wrapper
class ENJIN_API VulkanSampler {
public:
    VulkanSampler(VulkanContext* context);
    ~VulkanSampler();

    bool Create(const SamplerConfig& config = SamplerConfig{});
    void Destroy();

    VkSampler GetSampler() const { return m_Sampler; }

    // Common sampler presets
    static SamplerConfig Linear();
    static SamplerConfig Nearest();
    static SamplerConfig Shadow();

private:
    VulkanContext* m_Context = nullptr;
    VkSampler m_Sampler = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Enjin
