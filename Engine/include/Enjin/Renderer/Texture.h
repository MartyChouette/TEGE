#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanImage.h"
#include "Enjin/Renderer/Vulkan/VulkanSampler.h"
#include <memory>
#include <string>

namespace Enjin {
namespace Renderer {

// Texture - combines a VulkanImage with a VulkanSampler
class ENJIN_API Texture {
public:
    Texture(VulkanContext* context);
    ~Texture();

    // Load texture from file
    bool LoadFromFile(const std::string& filepath, const SamplerConfig& samplerConfig = SamplerConfig{});

    // Create texture from raw data
    bool CreateFromData(
        const void* data,
        u32 width,
        u32 height,
        u32 channels = 4,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
        const SamplerConfig& samplerConfig = SamplerConfig{}
    );

    // Create a solid color texture (useful for defaults)
    bool CreateSolidColor(u8 r, u8 g, u8 b, u8 a = 255);

    void Destroy();

    // Getters
    VkImage GetImage() const { return m_Image ? m_Image->GetImage() : VK_NULL_HANDLE; }
    VkImageView GetImageView() const { return m_Image ? m_Image->GetImageView() : VK_NULL_HANDLE; }
    VkSampler GetSampler() const { return m_Sampler ? m_Sampler->GetSampler() : VK_NULL_HANDLE; }
    u32 GetWidth() const { return m_Image ? m_Image->GetWidth() : 0; }
    u32 GetHeight() const { return m_Image ? m_Image->GetHeight() : 0; }

    // Get descriptor info for shader binding
    VkDescriptorImageInfo GetDescriptorInfo() const;

    bool IsValid() const { return m_Image && m_Sampler && GetImageView() != VK_NULL_HANDLE && GetSampler() != VK_NULL_HANDLE; }

private:
    VulkanContext* m_Context = nullptr;
    std::unique_ptr<VulkanImage> m_Image;
    std::unique_ptr<VulkanSampler> m_Sampler;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
