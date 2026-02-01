#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <vulkan/vulkan.h>
#include <string>

namespace Enjin {
namespace Renderer {

// Vulkan image wrapper - manages VkImage, VkDeviceMemory, and VkImageView
class ENJIN_API VulkanImage {
public:
    VulkanImage(VulkanContext* context);
    ~VulkanImage();

    // Create image from file (supports PNG, JPG, etc.)
    bool LoadFromFile(const std::string& filepath);

    // Create image from raw file bytes in memory (PNG, JPG, etc. decoded via stb_image)
    bool LoadFromMemory(const u8* data, usize size);
    
    // Create image from data
    bool Create(
        u32 width,
        u32 height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    // Create image from raw pixel data
    bool CreateFromData(
        const void* data,
        u32 width,
        u32 height,
        u32 channels,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB
    );
    
    void Destroy();
    
    // Transition image layout
    void TransitionLayout(VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmd = VK_NULL_HANDLE);
    
    // Generate mipmaps
    void GenerateMipmaps();
    
    // Getters
    VkImage GetImage() const { return m_Image; }
    VkImageView GetImageView() const { return m_ImageView; }
    VkFormat GetFormat() const { return m_Format; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    u32 GetMipLevels() const { return m_MipLevels; }
    
    // Create image view
    bool CreateImageView(VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

private:
    bool AllocateMemory(VkMemoryPropertyFlags properties);
    void CreateImage(
        u32 width,
        u32 height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        u32 mipLevels
    );
    
    VulkanContext* m_Context = nullptr;
    VkImage m_Image = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkImageView m_ImageView = VK_NULL_HANDLE;
    
    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_MipLevels = 1;
    VkFormat m_Format = VK_FORMAT_UNDEFINED;
};

} // namespace Renderer
} // namespace Enjin
