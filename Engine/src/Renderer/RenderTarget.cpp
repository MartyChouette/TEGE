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
    // Transition color and velocity images to attachment layout
    std::array<VkImageMemoryBarrier, 2> barriers{};

    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = m_ColorImage;
    barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = m_VelocityImage;
    barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr,
        static_cast<u32>(barriers.size()), barriers.data());

    // Begin render pass
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = m_RenderPass;
    rpBegin.framebuffer = m_Framebuffer;
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = {m_Width, m_Height};

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.15f, 1.0f}};  // Dark background
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};   // Velocity: zero motion
    clearValues[2].depthStencil = {1.0f, 0};
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

    // Velocity image (RG16F — matches main render pass MRT for pipeline compatibility)
    VkImageCreateInfo velInfo{};
    velInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    velInfo.imageType = VK_IMAGE_TYPE_2D;
    velInfo.format = VK_FORMAT_R16G16_SFLOAT;
    velInfo.extent = {m_Width, m_Height, 1};
    velInfo.mipLevels = 1;
    velInfo.arrayLayers = 1;
    velInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    velInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    velInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    velInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    velInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &velInfo, nullptr, &m_VelocityImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target velocity image");
        return false;
    }

    vkGetImageMemoryRequirements(device, m_VelocityImage, &memReqs);
    memType = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_VelocityMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate render target velocity memory");
        return false;
    }
    vkBindImageMemory(device, m_VelocityImage, m_VelocityMemory, 0);

    VkImageViewCreateInfo velViewInfo{};
    velViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    velViewInfo.image = m_VelocityImage;
    velViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    velViewInfo.format = VK_FORMAT_R16G16_SFLOAT;
    velViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    velViewInfo.subresourceRange.baseMipLevel = 0;
    velViewInfo.subresourceRange.levelCount = 1;
    velViewInfo.subresourceRange.baseArrayLayer = 0;
    velViewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &velViewInfo, nullptr, &m_VelocityImageView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target velocity image view");
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
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

    // Attachment 0: Color
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 1: Velocity (RG16F — matches main render pass MRT for pipeline compatibility)
    VkAttachmentDescription velocityAttachment{};
    velocityAttachment.format = VK_FORMAT_R16G16_SFLOAT;
    velocityAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    velocityAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    velocityAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    velocityAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    velocityAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    velocityAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    velocityAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 2: Depth
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Color attachment references (MRT: color + velocity — matches main render pass)
    std::array<VkAttachmentReference, 2> colorAttachmentRefs{};
    colorAttachmentRefs[0].attachment = 0;
    colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentRefs[1].attachment = 1;
    colorAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 2;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<u32>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();
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

    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, velocityAttachment, depthAttachment};

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

    std::array<VkImageView, 3> attachments = {m_ColorImageView, m_VelocityImageView, m_DepthImageView};

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
    if (m_VelocityImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_VelocityImageView, nullptr);
        m_VelocityImageView = VK_NULL_HANDLE;
    }
    if (m_VelocityImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_VelocityImage, nullptr);
        m_VelocityImage = VK_NULL_HANDLE;
    }
    if (m_VelocityMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_VelocityMemory, nullptr);
        m_VelocityMemory = VK_NULL_HANDLE;
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

std::vector<u8> RenderTarget::CaptureToPixels() const {
    if (!m_Context || m_ColorImage == VK_NULL_HANDLE || m_Width == 0 || m_Height == 0)
        return {};

    VkDevice device = m_Context->GetDevice();
    VkQueue queue = m_Context->GetGraphicsQueue();

    u32 imageSize = m_Width * m_Height * 4;

    // Create temporary command pool
    VkCommandPool tempPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) != VK_SUCCESS)
        return {};

    // Create staging buffer (host-visible)
    VkBufferCreateInfo bufInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufInfo.size = imageSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        vkDestroyCommandPool(device, tempPool, nullptr);
        return {};
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkDestroyCommandPool(device, tempPool, nullptr);
        return {};
    }
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    // Allocate one-shot command buffer
    VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool = tempPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Transition color image to TRANSFER_SRC
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.image = m_ColorImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy image to buffer
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {m_Width, m_Height, 1};
    vkCmdCopyImageToBuffer(cmd, m_ColorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        stagingBuffer, 1, &region);

    // Transition back to SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, tempPool, 1, &cmd);

    // Map and read pixels
    void* data = nullptr;
    if (vkMapMemory(device, stagingMemory, 0, imageSize, 0, &data) != VK_SUCCESS || !data) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return {};
    }

    std::vector<u8> pixels(imageSize);
    const u8* src = static_cast<const u8*>(data);
    // BGRA → RGBA swizzle
    for (u32 i = 0; i < imageSize; i += 4) {
        pixels[i + 0] = src[i + 2]; // R ← B
        pixels[i + 1] = src[i + 1]; // G
        pixels[i + 2] = src[i + 0]; // B ← R
        pixels[i + 3] = src[i + 3]; // A
    }

    vkUnmapMemory(device, stagingMemory);

    // Cleanup staging + temp pool
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyCommandPool(device, tempPool, nullptr);

    return pixels;
}

} // namespace Renderer
} // namespace Enjin
