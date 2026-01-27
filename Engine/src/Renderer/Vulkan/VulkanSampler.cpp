#include "Enjin/Renderer/Vulkan/VulkanSampler.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

VulkanSampler::VulkanSampler(VulkanContext* context)
    : m_Context(context) {
}

VulkanSampler::~VulkanSampler() {
    Destroy();
}

bool VulkanSampler::Create(const SamplerConfig& config) {
    // Check if anisotropy is supported
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(m_Context->GetPhysicalDevice(), &supportedFeatures);

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_Context->GetPhysicalDevice(), &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = config.magFilter;
    samplerInfo.minFilter = config.minFilter;
    samplerInfo.addressModeU = config.addressModeU;
    samplerInfo.addressModeV = config.addressModeV;
    samplerInfo.addressModeW = config.addressModeW;
    samplerInfo.borderColor = config.borderColor;
    samplerInfo.unnormalizedCoordinates = config.unnormalizedCoordinates ? VK_TRUE : VK_FALSE;
    samplerInfo.compareEnable = config.compareEnable ? VK_TRUE : VK_FALSE;
    samplerInfo.compareOp = config.compareOp;
    samplerInfo.mipmapMode = config.mipmapMode;
    samplerInfo.mipLodBias = config.mipLodBias;
    samplerInfo.minLod = config.minLod;
    samplerInfo.maxLod = config.maxLod;

    // Use anisotropy if supported and requested
    if (config.anisotropyEnable && supportedFeatures.samplerAnisotropy) {
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = std::min(config.maxAnisotropy, properties.limits.maxSamplerAnisotropy);
    } else {
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
    }

    VkResult result = vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_Sampler);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create sampler: %d", result);
        return false;
    }

    ENJIN_LOG_DEBUG(Renderer, "Created sampler (anisotropy: %s, max: %.1f)",
        samplerInfo.anisotropyEnable ? "enabled" : "disabled",
        samplerInfo.maxAnisotropy);
    return true;
}

void VulkanSampler::Destroy() {
    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_Context->GetDevice(), m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
}

SamplerConfig VulkanSampler::Linear() {
    SamplerConfig config;
    config.magFilter = VK_FILTER_LINEAR;
    config.minFilter = VK_FILTER_LINEAR;
    config.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    return config;
}

SamplerConfig VulkanSampler::Nearest() {
    SamplerConfig config;
    config.magFilter = VK_FILTER_NEAREST;
    config.minFilter = VK_FILTER_NEAREST;
    config.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    config.anisotropyEnable = false;
    return config;
}

SamplerConfig VulkanSampler::Shadow() {
    SamplerConfig config;
    config.magFilter = VK_FILTER_LINEAR;
    config.minFilter = VK_FILTER_LINEAR;
    config.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    config.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    config.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    config.compareEnable = true;
    config.compareOp = VK_COMPARE_OP_LESS;
    config.anisotropyEnable = false;
    return config;
}

} // namespace Renderer
} // namespace Enjin
