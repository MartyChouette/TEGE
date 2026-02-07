#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

#include <backends/imgui_impl_vulkan.h>
#include <array>

namespace Enjin {
namespace Renderer {

RenderTarget::RenderTarget() = default;

RenderTarget::~RenderTarget() {
    Destroy();
}

bool RenderTarget::Create(VulkanRenderer* renderer, u32 width, u32 height) {
    if (!renderer || width == 0 || height == 0) {
        ENJIN_LOG_ERROR(Renderer, "Invalid parameters for RenderTarget::Create");
        return false;
    }

    m_Renderer = renderer;
    m_Context = renderer->GetContext();
    m_Width = width;
    m_Height = height;

    if (!CreateImages()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffer()) return false;
    if (!CreateSampler()) return false;

    RegisterImGuiTexture();

    ENJIN_LOG_INFO(Renderer, "RenderTarget created (%ux%u)", width, height);
    return true;
}

void RenderTarget::Destroy() {
    if (m_Renderer) {
        m_Renderer->WaitForAllFrames();
    } else if (m_Context) {
        vkDeviceWaitIdle(m_Context->GetDevice());
    }
    UnregisterImGuiTexture();
    DestroyResources();
    m_Renderer = nullptr;
    m_Context = nullptr;
    m_Width = 0;
    m_Height = 0;
}

bool RenderTarget::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return true;
    if (!m_Renderer || width == 0 || height == 0) return false;

    m_Renderer->WaitForAllFrames();

    UnregisterImGuiTexture();
    DestroyResources();

    m_Width = width;
    m_Height = height;

    if (!CreateImages()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffer()) return false;
    if (!CreateSampler()) return false;

    RegisterImGuiTexture();

    ENJIN_LOG_INFO(Renderer, "RenderTarget resized to %ux%u", width, height);
    return true;
}

void RenderTarget::Begin(VkCommandBuffer cmd) {
    // Transition color image to attachment layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Begin render pass
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = m_RenderPass;
    rpBegin.framebuffer = m_Framebuffer;
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = {m_Width, m_Height};

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.15f, 1.0f}};  // Dark background
    clearValues[1].depthStencil = {1.0f, 0};
    rpBegin.clearValueCount = static_cast<u32>(clearValues.size());
    rpBegin.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(m_Width);
    viewport.height = static_cast<f32>(m_Height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_Width, m_Height};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void RenderTarget::End(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);

    // Transition color image to shader-read layout for ImGui sampling
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// --- Private implementation ---

bool RenderTarget::CreateImages() {
    VkDevice device = m_Context->GetDevice();

    // Color image (BGRA8 for ImGui compatibility)
    VkImageCreateInfo colorInfo{};
    colorInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    colorInfo.imageType = VK_IMAGE_TYPE_2D;
    colorInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorInfo.extent = {m_Width, m_Height, 1};
    colorInfo.mipLevels = 1;
    colorInfo.arrayLayers = 1;
    colorInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    colorInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    colorInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    colorInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    colorInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &colorInfo, nullptr, &m_ColorImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target color image");
        return false;
    }

    // Allocate color image memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_ColorImage, &memReqs);

    u32 memType = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_ColorMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate render target color memory");
        return false;
    }
    vkBindImageMemory(device, m_ColorImage, m_ColorMemory, 0);

    // Color image view
    VkImageViewCreateInfo colorViewInfo{};
    colorViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    colorViewInfo.image = m_ColorImage;
    colorViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    colorViewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorViewInfo.subresourceRange.baseMipLevel = 0;
    colorViewInfo.subresourceRange.levelCount = 1;
    colorViewInfo.subresourceRange.baseArrayLayer = 0;
    colorViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &colorViewInfo, nullptr, &m_ColorImageView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target color image view");
        return false;
    }

    // Depth image
    VkImageCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.format = VK_FORMAT_D32_SFLOAT;
    depthInfo.extent = {m_Width, m_Height, 1};
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &depthInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target depth image");
        return false;
    }

    // Allocate depth image memory
    vkGetImageMemoryRequirements(device, m_DepthImage, &memReqs);

    memType = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate render target depth memory");
        return false;
    }
    vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0);

    // Depth image view
    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = m_DepthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &depthViewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target depth image view");
        return false;
    }

    return true;
}

bool RenderTarget::CreateRenderPass() {
    VkDevice device = m_Context->GetDevice();

    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<u32>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target render pass");
        return false;
    }

    return true;
}

bool RenderTarget::CreateFramebuffer() {
    VkDevice device = m_Context->GetDevice();

    std::array<VkImageView, 2> attachments = {m_ColorImageView, m_DepthImageView};

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_RenderPass;
    fbInfo.attachmentCount = static_cast<u32>(attachments.size());
    fbInfo.pAttachments = attachments.data();
    fbInfo.width = m_Width;
    fbInfo.height = m_Height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target framebuffer");
        return false;
    }

    return true;
}

bool RenderTarget::CreateSampler() {
    VkDevice device = m_Context->GetDevice();

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target sampler");
        return false;
    }

    return true;
}

void RenderTarget::RegisterImGuiTexture() {
    if (m_Sampler == VK_NULL_HANDLE || m_ColorImageView == VK_NULL_HANDLE) return;

    m_ImGuiDescriptor = ImGui_ImplVulkan_AddTexture(
        m_Sampler, m_ColorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void RenderTarget::UnregisterImGuiTexture() {
    if (m_ImGuiDescriptor != VK_NULL_HANDLE) {
        ImGui_ImplVulkan_RemoveTexture(m_ImGuiDescriptor);
        m_ImGuiDescriptor = VK_NULL_HANDLE;
    }
}

void RenderTarget::DestroyResources() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();

    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
    if (m_Framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
        m_Framebuffer = VK_NULL_HANDLE;
    }
    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }
    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
    }
    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }
    if (m_DepthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_DepthMemory, nullptr);
        m_DepthMemory = VK_NULL_HANDLE;
    }
    if (m_ColorImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_ColorImageView, nullptr);
        m_ColorImageView = VK_NULL_HANDLE;
    }
    if (m_ColorImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_ColorImage, nullptr);
        m_ColorImage = VK_NULL_HANDLE;
    }
    if (m_ColorMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_ColorMemory, nullptr);
        m_ColorMemory = VK_NULL_HANDLE;
    }
}

} // namespace Renderer
} // namespace Enjin
