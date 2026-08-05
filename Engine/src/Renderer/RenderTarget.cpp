#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

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
    if (!CreatePPRenderPass()) return false;
    if (!CreatePPFramebuffer()) return false;
    if (!CreateSampler()) return false;

    InitializeColorLayout();
    RegisterTexture();

    ENJIN_LOG_INFO(Renderer, "RenderTarget created (%ux%u)", width, height);
    return true;
}

void RenderTarget::Destroy() {
    if (m_Renderer) {
        m_Renderer->WaitForAllFrames();
    } else if (m_Context) {
        vkDeviceWaitIdle(m_Context->GetDevice());
    }
    UnregisterTexture();
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

    UnregisterTexture();
    DestroyResources();

    m_Width = width;
    m_Height = height;

    if (!CreateImages()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffer()) return false;
    if (!CreatePPRenderPass()) return false;
    if (!CreatePPFramebuffer()) return false;
    if (!CreateSampler()) return false;

    InitializeColorLayout();
    RegisterTexture();

    ENJIN_LOG_INFO(Renderer, "RenderTarget resized to %ux%u", width, height);
    return true;
}

void RenderTarget::Begin(VkCommandBuffer cmd) {
    if (m_ColorImage == VK_NULL_HANDLE || m_RenderPass == VK_NULL_HANDLE || m_Framebuffer == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "RenderTarget::Begin: null resource! color=%p pass=%p fb=%p depth=%p",
            (void*)(uintptr_t)m_ColorImage, (void*)(uintptr_t)m_RenderPass,
            (void*)(uintptr_t)m_Framebuffer,
            (void*)(uintptr_t)m_DepthImage);
        return;
    }
    // Transition color image to attachment layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
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

void RenderTarget::InitializeColorLayout() {
    if (!m_Context || m_ColorImage == VK_NULL_HANDLE) return;

    VkDevice device = m_Context->GetDevice();
    VkQueue queue = m_Context->GetGraphicsQueue();

    VkCommandPool tempPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) return;

    VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool = tempPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, tempPool, nullptr);
        return;
    }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkDestroyCommandPool(device, tempPool, nullptr);
        return;
    }

    // UNDEFINED -> TRANSFER_DST, clear to the same dark background Begin() uses,
    // then -> SHADER_READ_ONLY so any pre-first-render sample is legal and clean
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkClearColorValue clearColor{{0.1f, 0.1f, 0.15f, 1.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, m_ColorImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    if (vkEndCommandBuffer(cmd) == VK_SUCCESS) {
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS) {
            vkQueueWaitIdle(queue);
        }
    }
    vkDestroyCommandPool(device, tempPool, nullptr);
}

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
    // TRANSFER_SRC/DST: the editor's no-post-process fallback path blits the scene
    // render target into the game-view target (validation 01212/01213/00219/00224 when
    // these bits are missing -- a blit can't transition the image to TRANSFER_*_OPTIMAL).
    colorInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
    if (vkBindImageMemory(device, m_ColorImage, m_ColorMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind render target color image memory");
        return false;
    }

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
    if (vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind render target depth image memory");
        return false;
    }

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

    // Attachment 1: Depth
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
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

    // External dependency ordering the PREVIOUS frame's attachment writes
    // against this frame's implicit layout transitions (depth goes
    // UNDEFINED -> DSA every frame). Depth writes COMPLETE at
    // LATE_FRAGMENT_TESTS — the old mask stopped at EARLY, and srcAccessMask
    // was 0 (prior writes never made available), so the depth transition
    // raced the prior frame's depth store: SYNC-HAZARD-WRITE-AFTER-WRITE,
    // caught by the first synchronization-validation probe (2026-08-04).
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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

bool RenderTarget::CreatePPRenderPass() {
    VkDevice device = m_Context->GetDevice();

    // Single color attachment — no velocity, no depth
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // Clear to black — if we see black, PP pipeline isn't drawing
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_PPRenderPass) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create PP render pass");
        return false;
    }
    return true;
}

bool RenderTarget::CreatePPFramebuffer() {
    VkDevice device = m_Context->GetDevice();

    VkImageView attachment = m_ColorImageView;

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_PPRenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &attachment;
    fbInfo.width = m_Width;
    fbInfo.height = m_Height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_PPFramebuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create PP framebuffer");
        return false;
    }
    return true;
}

void RenderTarget::BeginPPPass(VkCommandBuffer cmd) {
    if (m_ColorImage == VK_NULL_HANDLE || m_PPRenderPass == VK_NULL_HANDLE || m_PPFramebuffer == VK_NULL_HANDLE) {
        return;
    }

    // Transition color image for writing.
    // The image may be in SHADER_READ_ONLY_OPTIMAL (from previous EndPPPass or End)
    // or UNDEFINED (first use). Using SHADER_READ_ONLY as oldLayout is safe for both
    // cases because the render pass loadOp=CLEAR discards old contents anyway.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = m_PPRenderPass;
    rpBegin.framebuffer = m_PPFramebuffer;
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = {m_Width, m_Height};
    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};  // Black — visible if PP doesn't draw
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

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

void RenderTarget::EndPPPass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);

    // Transition color image to shader-read for ImGui sampling
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_ColorImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
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

void RenderTarget::SetTextureCallbacks(TextureRegisterFn onRegister, TextureUnregisterFn onUnregister) {
    m_OnTextureRegister = std::move(onRegister);
    m_OnTextureUnregister = std::move(onUnregister);
}

void RenderTarget::RegisterTexture() {
    if (m_Sampler == VK_NULL_HANDLE || m_ColorImageView == VK_NULL_HANDLE) return;
    if (!m_OnTextureRegister) return;  // Headless — no UI texture binding

    m_UIDescriptor = m_OnTextureRegister(
        m_Sampler, m_ColorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void RenderTarget::UnregisterTexture() {
    if (m_UIDescriptor != VK_NULL_HANDLE && m_OnTextureUnregister) {
        m_OnTextureUnregister(m_UIDescriptor);
        m_UIDescriptor = VK_NULL_HANDLE;
    }
}

void RenderTarget::DestroyResources() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();

    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
    if (m_PPFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_PPFramebuffer, nullptr);
        m_PPFramebuffer = VK_NULL_HANDLE;
    }
    if (m_PPRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_PPRenderPass, nullptr);
        m_PPRenderPass = VK_NULL_HANDLE;
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
    if (vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind render target staging buffer memory");
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkDestroyCommandPool(device, tempPool, nullptr);
        return {};
    }

    // Allocate one-shot command buffer
    VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool = tempPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate render target readback command buffer");
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkDestroyCommandPool(device, tempPool, nullptr);
        return {};
    }

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to begin render target readback command buffer");
        vkFreeCommandBuffers(device, tempPool, 1, &cmd);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkDestroyCommandPool(device, tempPool, nullptr);
        return {};
    }

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

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to end render target readback command buffer");
        vkFreeCommandBuffers(device, tempPool, 1, &cmd);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkDestroyCommandPool(device, tempPool, nullptr);
        return {};
    }

    // Submit and wait
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit render target readback commands");
    }
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
