#include "Enjin/Renderer/OITManager.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <array>

namespace Enjin {
namespace Renderer {

OITManager::~OITManager() {
    if (m_Initialized) {
        Shutdown();
    }
}

bool OITManager::Initialize(VulkanContext* context, u32 width, u32 height, VkRenderPass opaqueRenderPass) {
    if (!context) return false;
    m_Context = context;
    m_Width = width;
    m_Height = height;
    m_OpaqueRenderPass = opaqueRenderPass;

    if (!CreateTextures(width, height)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT textures");
        return false;
    }

    if (!CreateTransparentRenderPass()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT transparent render pass");
        DestroyTextures();
        return false;
    }

    if (!CreateTransparentFramebuffer()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT transparent framebuffer");
        DestroyRenderResources();
        DestroyTextures();
        return false;
    }

    if (!CreateCompositeDescriptorSets()) {
        ENJIN_LOG_WARN(Renderer, "Failed to create OIT composite descriptors");
        // Non-fatal: composite pass will be a no-op until descriptor sets are available
    }

    if (m_OpaqueRenderPass != VK_NULL_HANDLE) {
        if (!CreateCompositePipeline(m_OpaqueRenderPass)) {
            ENJIN_LOG_WARN(Renderer, "Failed to create OIT composite pipeline");
            // Non-fatal: composite will be a no-op
        }
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "OIT manager initialized (%ux%u)", width, height);
    return true;
}

void OITManager::Shutdown() {
    DestroyRenderResources();
    DestroyTextures();
    m_Initialized = false;
    m_Context = nullptr;
}

void OITManager::Resize(u32 width, u32 height) {
    if (!m_Initialized) return;
    if (width == m_Width && height == m_Height) return;

    // Destroy framebuffer (depends on textures), then textures, then recreate
    VkDevice device = m_Context->GetDevice();

    if (m_TransparentFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_TransparentFramebuffer, nullptr);
        m_TransparentFramebuffer = VK_NULL_HANDLE;
    }

    DestroyTextures();
    m_Width = width;
    m_Height = height;

    if (CreateTextures(width, height)) {
        CreateTransparentFramebuffer();
        // Rebuild composite descriptor set with new image views
        if (m_CompositeDescSet != VK_NULL_HANDLE) {
            // Update descriptor writes for new image views
            VkDescriptorImageInfo accumInfo{};
            accumInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            accumInfo.imageView = m_AccumulationView;
            accumInfo.sampler = m_Sampler;

            VkDescriptorImageInfo revealInfo{};
            revealInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            revealInfo.imageView = m_RevealageView;
            revealInfo.sampler = m_Sampler;

            VkWriteDescriptorSet writes[2]{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = m_CompositeDescSet;
            writes[0].dstBinding = 0;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[0].descriptorCount = 1;
            writes[0].pImageInfo = &accumInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = m_CompositeDescSet;
            writes[1].dstBinding = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].descriptorCount = 1;
            writes[1].pImageInfo = &revealInfo;

            vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
        }
    }
}

// ============================================================================
// BeginTransparentPass
// ============================================================================
// Transitions OIT textures to COLOR_ATTACHMENT_OPTIMAL, begins a render pass
// with the two MRT outputs, and clears them to the correct initial values:
//   - Accumulation: vec4(0) (additive blend starts at zero)
//   - Revealage: 1.0 (product of (1-alpha) starts at 1)
//
// The transparent render pass uses two color attachments with special blend modes
// configured in the pipeline that renders transparent geometry:
//   Attachment 0 (Accumulation RGBA16F): ONE, ONE (additive)
//   Attachment 1 (Revealage R8):         ZERO, ONE_MINUS_SRC_ALPHA (multiplicative)
//
// The caller is responsible for binding the correct pipeline with these blend
// states before drawing transparent geometry.

void OITManager::BeginTransparentPass(VkCommandBuffer cmd) {
    if (!m_Initialized || !m_Config.enabled) return;
    if (m_TransparentRenderPass == VK_NULL_HANDLE || m_TransparentFramebuffer == VK_NULL_HANDLE) return;

    // Transition both images from UNDEFINED/SHADER_READ_ONLY to COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier barriers[2]{};

    // Accumulation: transition to color attachment
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = m_AccumulationImage;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = 0;
    barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Revealage: transition to color attachment
    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = m_RevealageImage;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);

    // Clear values: accumulation = (0,0,0,0), revealage = 1.0
    VkClearValue clearValues[2]{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // Accumulation: zero (additive identity)
    clearValues[1].color = {{1.0f, 0.0f, 0.0f, 0.0f}};  // Revealage: 1.0 (multiplicative identity)

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = m_TransparentRenderPass;
    rpBegin.framebuffer = m_TransparentFramebuffer;
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = {m_Width, m_Height};
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor for the transparent pass
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

// ============================================================================
// EndTransparentPass
// ============================================================================
// Ends the transparent render pass and transitions the OIT textures from
// COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL so they can be
// sampled during the composite pass.

void OITManager::EndTransparentPass(VkCommandBuffer cmd) {
    if (!m_Initialized || !m_Config.enabled) return;
    if (m_TransparentRenderPass == VK_NULL_HANDLE) return;

    vkCmdEndRenderPass(cmd);

    // Transition both images to SHADER_READ_ONLY for composite sampling
    VkImageMemoryBarrier barriers[2]{};

    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = m_AccumulationImage;
    barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[0].subresourceRange.baseMipLevel = 0;
    barriers[0].subresourceRange.levelCount = 1;
    barriers[0].subresourceRange.baseArrayLayer = 0;
    barriers[0].subresourceRange.layerCount = 1;
    barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[1].image = m_RevealageImage;
    barriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barriers[1].subresourceRange.baseMipLevel = 0;
    barriers[1].subresourceRange.levelCount = 1;
    barriers[1].subresourceRange.baseArrayLayer = 0;
    barriers[1].subresourceRange.layerCount = 1;
    barriers[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);
}

// ============================================================================
// CompositePass
// ============================================================================
// Reads the accumulation and revealage textures and computes the final
// transparent color using the McGuire & Bavoil 2013 weighted blended formula:
//
//   finalColor.rgb = accumulation.rgb / max(accumulation.a, 1e-5)
//   finalColor.a   = 1.0 - revealage
//
// This is blended over the opaque scene using standard alpha blending
// (SRC_ALPHA, ONE_MINUS_SRC_ALPHA).
//
// Note: This requires a compiled composite SPIR-V shader to be fully
// operational. Without compiled shaders, this function logs a diagnostic
// and returns. The descriptor sets and texture bindings are fully wired
// so once compiled shaders are provided, this will activate automatically.

void OITManager::CompositePass(VkCommandBuffer cmd) {
    if (!m_Initialized || !m_Config.enabled) return;
    if (m_CompositePipeline == VK_NULL_HANDLE || m_CompositeDescSet == VK_NULL_HANDLE) return;

    // Bind composite pipeline (alpha blending: src_alpha, 1-src_alpha)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_CompositePipeline);

    // Bind descriptor set with accumulation (binding 0) and revealage (binding 1)
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_CompositePipelineLayout, 0, 1, &m_CompositeDescSet, 0, nullptr);

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

    // Draw fullscreen triangle (3 vertices, generated in vertex shader — no vertex buffer)
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

// ============================================================================
// RENDER PASS CREATION
// ============================================================================

static VkImageSubresourceRange MakeSubresourceRange() {
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    return range;
}

bool OITManager::CreateTransparentRenderPass() {
    if (!m_Context) return false;
    VkDevice device = m_Context->GetDevice();

    // Two color attachments: accumulation (RGBA16F) and revealage (R8_UNORM)
    // No depth attachment — transparent pass reads depth from the opaque pass
    // via depth test but does NOT write depth.
    VkAttachmentDescription attachments[2]{};

    // Attachment 0: Accumulation (RGBA16F)
    attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 1: Revealage (R8_UNORM)
    attachments[1].format = VK_FORMAT_R8_UNORM;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRefs[2]{};
    colorRefs[0].attachment = 0;
    colorRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[1].attachment = 1;
    colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 2;
    subpass.pColorAttachments = colorRefs;
    subpass.pDepthStencilAttachment = nullptr;  // No depth write for transparent pass

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attachments;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &rpInfo, nullptr, &m_TransparentRenderPass) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT transparent render pass");
        return false;
    }

    return true;
}

bool OITManager::CreateTransparentFramebuffer() {
    if (!m_Context || m_TransparentRenderPass == VK_NULL_HANDLE) return false;
    VkDevice device = m_Context->GetDevice();

    std::array<VkImageView, 2> fbAttachments = { m_AccumulationView, m_RevealageView };

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_TransparentRenderPass;
    fbInfo.attachmentCount = static_cast<u32>(fbAttachments.size());
    fbInfo.pAttachments = fbAttachments.data();
    fbInfo.width = m_Width;
    fbInfo.height = m_Height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_TransparentFramebuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT transparent framebuffer");
        return false;
    }

    return true;
}

bool OITManager::CreateCompositeDescriptorSets() {
    if (!m_Context) return false;
    VkDevice device = m_Context->GetDevice();

    // Create sampler for reading OIT textures
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
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT sampler");
        return false;
    }

    // Descriptor set layout: binding 0 = accumulation, binding 1 = revealage
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_CompositeDescLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT composite descriptor layout");
        return false;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_CompositeDescPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT composite descriptor pool");
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_CompositeDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_CompositeDescLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_CompositeDescSet) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate OIT composite descriptor set");
        return false;
    }

    // Write descriptor set with OIT textures
    VkDescriptorImageInfo accumInfo{};
    accumInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    accumInfo.imageView = m_AccumulationView;
    accumInfo.sampler = m_Sampler;

    VkDescriptorImageInfo revealInfo{};
    revealInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    revealInfo.imageView = m_RevealageView;
    revealInfo.sampler = m_Sampler;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_CompositeDescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &accumInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_CompositeDescSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &revealInfo;

    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    ENJIN_LOG_INFO(Renderer, "OIT composite descriptor sets created");
    return true;
}

// ============================================================================
// COMPOSITE PIPELINE
// ============================================================================

// Fullscreen vertex shader (embedded SPIR-V, compiled from fullscreen.vert)
static const u32 OITFullscreenVertexShader[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000033,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0008000f, 0x00000000, 0x00000004, 0x6e69616d,
    0x00000000, 0x0000001f, 0x00000023, 0x0000002f,
    0x00030003, 0x00000002, 0x000001c2, 0x00040005,
    0x00000004, 0x6e69616d, 0x00000000, 0x00050005,
    0x0000000c, 0x69736f70, 0x6e6f6974, 0x00000073,
    0x00030005, 0x00000013, 0x00737675, 0x00060005,
    0x0000001d, 0x505f6c67, 0x65567265, 0x78657472,
    0x00000000, 0x00060006, 0x0000001d, 0x00000000,
    0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006,
    0x0000001d, 0x00000001, 0x505f6c67, 0x746e696f,
    0x657a6953, 0x00000000, 0x00070006, 0x0000001d,
    0x00000002, 0x435f6c67, 0x4470696c, 0x61747369,
    0x0065636e, 0x00070006, 0x0000001d, 0x00000003,
    0x435f6c67, 0x446c6c75, 0x61747369, 0x0065636e,
    0x00030005, 0x0000001f, 0x00000000, 0x00060005,
    0x00000023, 0x565f6c67, 0x65747265, 0x646e4978,
    0x00007865, 0x00040005, 0x0000002f, 0x67617266,
    0x00005655, 0x00030047, 0x0000001d, 0x00000002,
    0x00050048, 0x0000001d, 0x00000000, 0x0000000b,
    0x00000000, 0x00050048, 0x0000001d, 0x00000001,
    0x0000000b, 0x00000001, 0x00050048, 0x0000001d,
    0x00000002, 0x0000000b, 0x00000003, 0x00050048,
    0x0000001d, 0x00000003, 0x0000000b, 0x00000004,
    0x00040047, 0x00000023, 0x0000000b, 0x0000002a,
    0x00040047, 0x0000002f, 0x0000001e, 0x00000000,
    0x00020013, 0x00000002, 0x00030021, 0x00000003,
    0x00000002, 0x00030016, 0x00000006, 0x00000020,
    0x00040017, 0x00000007, 0x00000006, 0x00000002,
    0x00040015, 0x00000008, 0x00000020, 0x00000000,
    0x0004002b, 0x00000008, 0x00000009, 0x00000003,
    0x0004001c, 0x0000000a, 0x00000007, 0x00000009,
    0x00040020, 0x0000000b, 0x00000007, 0x0000000a,
    0x0004002b, 0x00000006, 0x0000000d, 0xbf800000,
    0x0005002c, 0x00000007, 0x0000000e, 0x0000000d,
    0x0000000d, 0x0004002b, 0x00000006, 0x0000000f,
    0x40400000, 0x0005002c, 0x00000007, 0x00000010,
    0x0000000f, 0x0000000d, 0x0005002c, 0x00000007,
    0x00000011, 0x0000000d, 0x0000000f, 0x0006002c,
    0x0000000a, 0x00000012, 0x0000000e, 0x00000010,
    0x00000011, 0x0004002b, 0x00000006, 0x00000014,
    0x00000000, 0x0005002c, 0x00000007, 0x00000015,
    0x00000014, 0x00000014, 0x0004002b, 0x00000006,
    0x00000016, 0x40000000, 0x0005002c, 0x00000007,
    0x00000017, 0x00000016, 0x00000014, 0x0005002c,
    0x00000007, 0x00000018, 0x00000014, 0x00000016,
    0x0006002c, 0x0000000a, 0x00000019, 0x00000015,
    0x00000017, 0x00000018, 0x00040017, 0x0000001a,
    0x00000006, 0x00000004, 0x0004002b, 0x00000008,
    0x0000001b, 0x00000001, 0x0004001c, 0x0000001c,
    0x00000006, 0x0000001b, 0x0006001e, 0x0000001d,
    0x0000001a, 0x00000006, 0x0000001c, 0x0000001c,
    0x00040020, 0x0000001e, 0x00000003, 0x0000001d,
    0x0004003b, 0x0000001e, 0x0000001f, 0x00000003,
    0x00040015, 0x00000020, 0x00000020, 0x00000001,
    0x0004002b, 0x00000020, 0x00000021, 0x00000000,
    0x00040020, 0x00000022, 0x00000001, 0x00000020,
    0x0004003b, 0x00000022, 0x00000023, 0x00000001,
    0x00040020, 0x00000025, 0x00000007, 0x00000007,
    0x0004002b, 0x00000006, 0x00000028, 0x3f800000,
    0x00040020, 0x0000002c, 0x00000003, 0x0000001a,
    0x00040020, 0x0000002e, 0x00000003, 0x00000007,
    0x0004003b, 0x0000002e, 0x0000002f, 0x00000003,
    0x00050036, 0x00000002, 0x00000004, 0x00000000,
    0x00000003, 0x000200f8, 0x00000005, 0x0004003b,
    0x0000000b, 0x0000000c, 0x00000007, 0x0004003b,
    0x0000000b, 0x00000013, 0x00000007, 0x0003003e,
    0x0000000c, 0x00000012, 0x0003003e, 0x00000013,
    0x00000019, 0x0004003d, 0x00000020, 0x00000024,
    0x00000023, 0x00050041, 0x00000025, 0x00000026,
    0x0000000c, 0x00000024, 0x0004003d, 0x00000007,
    0x00000027, 0x00000026, 0x00050051, 0x00000006,
    0x00000029, 0x00000027, 0x00000000, 0x00050051,
    0x00000006, 0x0000002a, 0x00000027, 0x00000001,
    0x00070050, 0x0000001a, 0x0000002b, 0x00000029,
    0x0000002a, 0x00000014, 0x00000028, 0x00050041,
    0x0000002c, 0x0000002d, 0x0000001f, 0x00000021,
    0x0003003e, 0x0000002d, 0x0000002b, 0x0004003d,
    0x00000020, 0x00000030, 0x00000023, 0x00050041,
    0x00000025, 0x00000031, 0x00000013, 0x00000030,
    0x0004003d, 0x00000007, 0x00000032, 0x00000031,
    0x0003003e, 0x0000002f, 0x00000032, 0x000100fd,
    0x00010038
};

// OIT composite fragment shader (embedded SPIR-V, compiled from oit_composite.frag)
static const u32 OITCompositeFragmentShader[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x00000039,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000004, 0x00000004, 0x6e69616d,
    0x00000000, 0x00000011, 0x00000031, 0x00030010,
    0x00000004, 0x00000007, 0x00030003, 0x00000002,
    0x000001c2, 0x00040005, 0x00000004, 0x6e69616d,
    0x00000000, 0x00040005, 0x00000009, 0x75636361,
    0x0000006d, 0x00060005, 0x0000000d, 0x75636361,
    0x7865546d, 0x65727574, 0x00000000, 0x00040005,
    0x00000011, 0x67617266, 0x00005655, 0x00040005,
    0x00000015, 0x65766572, 0x00006c61, 0x00060005,
    0x00000016, 0x65766572, 0x65546c61, 0x72757478,
    0x00000065, 0x00050005, 0x00000026, 0x43677661,
    0x726f6c6f, 0x00000000, 0x00050005, 0x00000031,
    0x4374756f, 0x726f6c6f, 0x00000000, 0x00040047,
    0x0000000d, 0x00000021, 0x00000000, 0x00040047,
    0x0000000d, 0x00000022, 0x00000000, 0x00040047,
    0x00000011, 0x0000001e, 0x00000000, 0x00040047,
    0x00000016, 0x00000021, 0x00000001, 0x00040047,
    0x00000016, 0x00000022, 0x00000000, 0x00040047,
    0x00000031, 0x0000001e, 0x00000000, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002,
    0x00030016, 0x00000006, 0x00000020, 0x00040017,
    0x00000007, 0x00000006, 0x00000004, 0x00040020,
    0x00000008, 0x00000007, 0x00000007, 0x00090019,
    0x0000000a, 0x00000006, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x00000000,
    0x0003001b, 0x0000000b, 0x0000000a, 0x00040020,
    0x0000000c, 0x00000000, 0x0000000b, 0x0004003b,
    0x0000000c, 0x0000000d, 0x00000000, 0x00040017,
    0x0000000f, 0x00000006, 0x00000002, 0x00040020,
    0x00000010, 0x00000001, 0x0000000f, 0x0004003b,
    0x00000010, 0x00000011, 0x00000001, 0x00040020,
    0x00000014, 0x00000007, 0x00000006, 0x0004003b,
    0x0000000c, 0x00000016, 0x00000000, 0x00040015,
    0x0000001a, 0x00000020, 0x00000000, 0x0004002b,
    0x0000001a, 0x0000001b, 0x00000000, 0x0004002b,
    0x00000006, 0x0000001e, 0x3f800000, 0x00020014,
    0x0000001f, 0x00040017, 0x00000024, 0x00000006,
    0x00000003, 0x00040020, 0x00000025, 0x00000007,
    0x00000024, 0x0004002b, 0x0000001a, 0x00000029,
    0x00000003, 0x0004002b, 0x00000006, 0x0000002c,
    0x3727c5ac, 0x00040020, 0x00000030, 0x00000003,
    0x00000007, 0x0004003b, 0x00000030, 0x00000031,
    0x00000003, 0x00050036, 0x00000002, 0x00000004,
    0x00000000, 0x00000003, 0x000200f8, 0x00000005,
    0x0004003b, 0x00000008, 0x00000009, 0x00000007,
    0x0004003b, 0x00000014, 0x00000015, 0x00000007,
    0x0004003b, 0x00000025, 0x00000026, 0x00000007,
    0x0004003d, 0x0000000b, 0x0000000e, 0x0000000d,
    0x0004003d, 0x0000000f, 0x00000012, 0x00000011,
    0x00050057, 0x00000007, 0x00000013, 0x0000000e,
    0x00000012, 0x0003003e, 0x00000009, 0x00000013,
    0x0004003d, 0x0000000b, 0x00000017, 0x00000016,
    0x0004003d, 0x0000000f, 0x00000018, 0x00000011,
    0x00050057, 0x00000007, 0x00000019, 0x00000017,
    0x00000018, 0x00050051, 0x00000006, 0x0000001c,
    0x00000019, 0x00000000, 0x0003003e, 0x00000015,
    0x0000001c, 0x0004003d, 0x00000006, 0x0000001d,
    0x00000015, 0x000500be, 0x0000001f, 0x00000020,
    0x0000001d, 0x0000001e, 0x000300f7, 0x00000022,
    0x00000000, 0x000400fa, 0x00000020, 0x00000021,
    0x00000022, 0x000200f8, 0x00000021, 0x000100fc,
    0x000200f8, 0x00000022, 0x0004003d, 0x00000007,
    0x00000027, 0x00000009, 0x0008004f, 0x00000024,
    0x00000028, 0x00000027, 0x00000027, 0x00000000,
    0x00000001, 0x00000002, 0x00050041, 0x00000014,
    0x0000002a, 0x00000009, 0x00000029, 0x0004003d,
    0x00000006, 0x0000002b, 0x0000002a, 0x0007000c,
    0x00000006, 0x0000002d, 0x00000001, 0x00000028,
    0x0000002b, 0x0000002c, 0x00060050, 0x00000024,
    0x0000002e, 0x0000002d, 0x0000002d, 0x0000002d,
    0x00050088, 0x00000024, 0x0000002f, 0x00000028,
    0x0000002e, 0x0003003e, 0x00000026, 0x0000002f,
    0x0004003d, 0x00000024, 0x00000032, 0x00000026,
    0x0004003d, 0x00000006, 0x00000033, 0x00000015,
    0x00050083, 0x00000006, 0x00000034, 0x0000001e,
    0x00000033, 0x00050051, 0x00000006, 0x00000035,
    0x00000032, 0x00000000, 0x00050051, 0x00000006,
    0x00000036, 0x00000032, 0x00000001, 0x00050051,
    0x00000006, 0x00000037, 0x00000032, 0x00000002,
    0x00070050, 0x00000007, 0x00000038, 0x00000035,
    0x00000036, 0x00000037, 0x00000034, 0x0003003e,
    0x00000031, 0x00000038, 0x000100fd, 0x00010038,
};

bool OITManager::CreateCompositePipeline(VkRenderPass opaqueRenderPass) {
    if (!m_Context || opaqueRenderPass == VK_NULL_HANDLE || m_CompositeDescLayout == VK_NULL_HANDLE)
        return false;

    VkDevice device = m_Context->GetDevice();

    // Create shader modules from embedded SPIR-V
    VkShaderModuleCreateInfo vertInfo{};
    vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertInfo.codeSize = sizeof(OITFullscreenVertexShader);
    vertInfo.pCode = OITFullscreenVertexShader;

    VkShaderModule vertModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &vertInfo, nullptr, &vertModule) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT composite vertex shader module");
        return false;
    }

    VkShaderModuleCreateInfo fragInfo{};
    fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragInfo.codeSize = sizeof(OITCompositeFragmentShader);
    fragInfo.pCode = OITCompositeFragmentShader;

    VkShaderModule fragModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &fragInfo, nullptr, &fragModule) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT composite fragment shader module");
        return false;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName = "main";

    // No vertex input (fullscreen triangle generated in vertex shader)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Dynamic viewport and scissor
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Alpha blending: src_alpha, 1-src_alpha (composite transparent OIT over opaque)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // No depth testing for composite fullscreen pass
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // Pipeline layout from composite descriptor set layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_CompositeDescLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_CompositePipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT composite pipeline layout");
        return false;
    }

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_CompositePipelineLayout;
    pipelineInfo.renderPass = opaqueRenderPass;
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_CompositePipeline);

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT composite pipeline: %d", result);
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "OIT composite pipeline created");
    return true;
}

// ============================================================================
// TEXTURE CREATION
// ============================================================================

bool OITManager::CreateTextures(u32 width, u32 height) {
    if (!m_Context || width == 0 || height == 0) return false;

    VkDevice device = m_Context->GetDevice();

    // --- Accumulation texture (RGBA16F) ---
    VkImageCreateInfo accumImageInfo{};
    accumImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    accumImageInfo.imageType = VK_IMAGE_TYPE_2D;
    accumImageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    accumImageInfo.extent = { width, height, 1 };
    accumImageInfo.mipLevels = 1;
    accumImageInfo.arrayLayers = 1;
    accumImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    accumImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    accumImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    accumImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &accumImageInfo, nullptr, &m_AccumulationImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements accumMemReqs;
    vkGetImageMemoryRequirements(device, m_AccumulationImage, &accumMemReqs);

    VkMemoryAllocateInfo accumAllocInfo{};
    accumAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    accumAllocInfo.allocationSize = accumMemReqs.size;
    accumAllocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        accumMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &accumAllocInfo, nullptr, &m_AccumulationMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(device, m_AccumulationImage, m_AccumulationMemory, 0);

    VkImageViewCreateInfo accumViewInfo{};
    accumViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    accumViewInfo.image = m_AccumulationImage;
    accumViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    accumViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    accumViewInfo.subresourceRange = MakeSubresourceRange();

    if (vkCreateImageView(device, &accumViewInfo, nullptr, &m_AccumulationView) != VK_SUCCESS) {
        return false;
    }

    // --- Revealage texture (R8) ---
    VkImageCreateInfo revealImageInfo{};
    revealImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    revealImageInfo.imageType = VK_IMAGE_TYPE_2D;
    revealImageInfo.format = VK_FORMAT_R8_UNORM;
    revealImageInfo.extent = { width, height, 1 };
    revealImageInfo.mipLevels = 1;
    revealImageInfo.arrayLayers = 1;
    revealImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    revealImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    revealImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    revealImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &revealImageInfo, nullptr, &m_RevealageImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements revealMemReqs;
    vkGetImageMemoryRequirements(device, m_RevealageImage, &revealMemReqs);

    VkMemoryAllocateInfo revealAllocInfo{};
    revealAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    revealAllocInfo.allocationSize = revealMemReqs.size;
    revealAllocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        revealMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &revealAllocInfo, nullptr, &m_RevealageMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(device, m_RevealageImage, m_RevealageMemory, 0);

    VkImageViewCreateInfo revealViewInfo{};
    revealViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    revealViewInfo.image = m_RevealageImage;
    revealViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    revealViewInfo.format = VK_FORMAT_R8_UNORM;
    revealViewInfo.subresourceRange = MakeSubresourceRange();

    if (vkCreateImageView(device, &revealViewInfo, nullptr, &m_RevealageView) != VK_SUCCESS) {
        return false;
    }

    return true;
}

// ============================================================================
// CLEANUP
// ============================================================================

void OITManager::DestroyTextures() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_AccumulationView) { vkDestroyImageView(device, m_AccumulationView, nullptr); m_AccumulationView = VK_NULL_HANDLE; }
    if (m_AccumulationImage) { vkDestroyImage(device, m_AccumulationImage, nullptr); m_AccumulationImage = VK_NULL_HANDLE; }
    if (m_AccumulationMemory) { vkFreeMemory(device, m_AccumulationMemory, nullptr); m_AccumulationMemory = VK_NULL_HANDLE; }

    if (m_RevealageView) { vkDestroyImageView(device, m_RevealageView, nullptr); m_RevealageView = VK_NULL_HANDLE; }
    if (m_RevealageImage) { vkDestroyImage(device, m_RevealageImage, nullptr); m_RevealageImage = VK_NULL_HANDLE; }
    if (m_RevealageMemory) { vkFreeMemory(device, m_RevealageMemory, nullptr); m_RevealageMemory = VK_NULL_HANDLE; }
}

void OITManager::DestroyRenderResources() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_CompositePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_CompositePipeline, nullptr);
        m_CompositePipeline = VK_NULL_HANDLE;
    }
    if (m_CompositePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_CompositePipelineLayout, nullptr);
        m_CompositePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_TransparentFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_TransparentFramebuffer, nullptr);
        m_TransparentFramebuffer = VK_NULL_HANDLE;
    }
    if (m_TransparentRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_TransparentRenderPass, nullptr);
        m_TransparentRenderPass = VK_NULL_HANDLE;
    }
    if (m_CompositeDescPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_CompositeDescPool, nullptr);
        m_CompositeDescPool = VK_NULL_HANDLE;
        m_CompositeDescSet = VK_NULL_HANDLE;  // Freed with pool
    }
    if (m_CompositeDescLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_CompositeDescLayout, nullptr);
        m_CompositeDescLayout = VK_NULL_HANDLE;
    }
    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
}

} // namespace Renderer
} // namespace Enjin
