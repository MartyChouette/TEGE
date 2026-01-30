#include "Enjin/Renderer/Vulkan/VulkanImage.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <cstring>
#include <cmath>

// stb_image for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Enjin {
namespace Renderer {

VulkanImage::VulkanImage(VulkanContext* context)
    : m_Context(context) {
}

VulkanImage::~VulkanImage() {
    Destroy();
}

bool VulkanImage::LoadFromFile(const std::string& filepath) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    
    if (!pixels) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load image: %s", filepath.c_str());
        return false;
    }
    
    bool success = CreateFromData(pixels, static_cast<u32>(width), static_cast<u32>(height), 4, VK_FORMAT_R8G8B8A8_SRGB);
    
    stbi_image_free(pixels);
    
    if (success) {
        ENJIN_LOG_INFO(Renderer, "Loaded image: %s (%dx%d)", filepath.c_str(), width, height);
    }
    
    return success;
}

bool VulkanImage::Create(
    u32 width,
    u32 height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageTiling tiling,
    VkMemoryPropertyFlags properties
) {
    m_Width = width;
    m_Height = height;
    m_Format = format;
    m_MipLevels = 1;
    
    CreateImage(width, height, format, tiling, usage, m_MipLevels);
    
    if (!AllocateMemory(properties)) {
        return false;
    }
    
    vkBindImageMemory(m_Context->GetDevice(), m_Image, m_Memory, 0);
    
    if (!CreateImageView()) {
        return false;
    }
    
    return true;
}

bool VulkanImage::CreateFromData(
    const void* data,
    u32 width,
    u32 height,
    u32 channels,
    VkFormat format
) {
    m_Width = width;
    m_Height = height;
    m_Format = format;
    
    // Calculate mip levels
    m_MipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1;
    
    // Create staging buffer
    VkDeviceSize imageSize = width * height * channels;
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_Context->GetDevice(), &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create staging buffer");
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Context->GetDevice(), stagingBuffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    
    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate staging buffer memory");
        vkDestroyBuffer(m_Context->GetDevice(), stagingBuffer, nullptr);
        return false;
    }
    
    vkBindBufferMemory(m_Context->GetDevice(), stagingBuffer, stagingBufferMemory, 0);
    
    void* mapped;
    vkMapMemory(m_Context->GetDevice(), stagingBufferMemory, 0, imageSize, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_Context->GetDevice(), stagingBufferMemory);
    
    // Create image
    CreateImage(
        width,
        height,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        m_MipLevels
    );
    
    if (!AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        vkFreeMemory(m_Context->GetDevice(), stagingBufferMemory, nullptr);
        vkDestroyBuffer(m_Context->GetDevice(), stagingBuffer, nullptr);
        return false;
    }
    
    vkBindImageMemory(m_Context->GetDevice(), m_Image, m_Memory, 0);
    
    // Transition and copy (would need command buffer - simplified for now)
    // For now, just create the image view
    if (!CreateImageView()) {
        vkFreeMemory(m_Context->GetDevice(), stagingBufferMemory, nullptr);
        vkDestroyBuffer(m_Context->GetDevice(), stagingBuffer, nullptr);
        return false;
    }
    
    // Cleanup staging buffer
    vkFreeMemory(m_Context->GetDevice(), stagingBufferMemory, nullptr);
    vkDestroyBuffer(m_Context->GetDevice(), stagingBuffer, nullptr);
    
    ENJIN_LOG_INFO(Renderer, "Created image from data: %dx%d, format: %d", width, height, format);
    return true;
}

void VulkanImage::Destroy() {
    if (m_ImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Context->GetDevice(), m_ImageView, nullptr);
        m_ImageView = VK_NULL_HANDLE;
    }
    
    if (m_Image != VK_NULL_HANDLE) {
        vkDestroyImage(m_Context->GetDevice(), m_Image, nullptr);
        m_Image = VK_NULL_HANDLE;
    }
    
    if (m_Memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Context->GetDevice(), m_Memory, nullptr);
        m_Memory = VK_NULL_HANDLE;
    }
    
    m_Width = 0;
    m_Height = 0;
    m_MipLevels = 0;
    m_Format = VK_FORMAT_UNDEFINED;
}

void VulkanImage::TransitionLayout(VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmd) {
    if (m_Image == VK_NULL_HANDLE || !m_Context) return;

    bool ownCommandBuffer = (cmd == VK_NULL_HANDLE);
    VkCommandPool tempPool = VK_NULL_HANDLE;

    if (ownCommandBuffer) {
        // Create a temporary command pool and command buffer
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        if (vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create temp command pool for layout transition");
            return;
        }

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = tempPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &cmd) != VK_SUCCESS) {
            vkDestroyCommandPool(m_Context->GetDevice(), tempPool, nullptr);
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate temp command buffer for layout transition");
            return;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_MipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else {
        // General fallback
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                          0, nullptr, 0, nullptr, 1, &barrier);

    if (ownCommandBuffer) {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_Context->GetGraphicsQueue());

        vkDestroyCommandPool(m_Context->GetDevice(), tempPool, nullptr);
    }
}

void VulkanImage::GenerateMipmaps() {
    if (m_Image == VK_NULL_HANDLE || !m_Context || m_MipLevels <= 1) return;

    // Check if linear blitting is supported for this format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(m_Context->GetPhysicalDevice(), m_Format, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        ENJIN_LOG_WARN(Renderer, "Texture format does not support linear blitting for mipmaps");
        return;
    }

    // Create temporary command pool and buffer
    VkCommandPool tempPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create temp command pool for mipmaps");
        return;
    }

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = tempPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(m_Context->GetDevice(), tempPool, nullptr);
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = m_Image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    i32 mipWidth = static_cast<i32>(m_Width);
    i32 mipHeight = static_cast<i32>(m_Height);

    for (u32 i = 1; i < m_MipLevels; i++) {
        // Transition previous mip level to transfer source
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &barrier);

        // Blit from previous mip to current mip
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(cmd,
                        m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &blit, VK_FILTER_LINEAR);

        // Transition previous mip to shader read
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                              0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition last mip level to shader read
    barrier.subresourceRange.baseMipLevel = m_MipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                          0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    vkDestroyCommandPool(m_Context->GetDevice(), tempPool, nullptr);
}

bool VulkanImage::CreateImageView(VkImageAspectFlags aspectFlags) {
    if (m_Image == VK_NULL_HANDLE) {
        return false;
    }
    
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_Image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_Format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_MipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    VkResult result = vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_ImageView);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create image view: %d", result);
        return false;
    }
    
    return true;
}

bool VulkanImage::AllocateMemory(VkMemoryPropertyFlags properties) {
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Context->GetDevice(), m_Image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "Failed to find suitable memory type for image");
        return false;
    }
    
    VkResult result = vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_Memory);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate image memory: %d", result);
        return false;
    }
    
    return true;
}

void VulkanImage::CreateImage(
    u32 width,
    u32 height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    u32 mipLevels
) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult result = vkCreateImage(m_Context->GetDevice(), &imageInfo, nullptr, &m_Image);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create image: %d", result);
        m_Image = VK_NULL_HANDLE;
    }
}

} // namespace Renderer
} // namespace Enjin
