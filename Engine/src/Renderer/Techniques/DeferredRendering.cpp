#include "Enjin/Renderer/Techniques/DeferredRendering.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/RenderPipeline/RenderPipeline.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <array>

namespace Enjin {
namespace Renderer {

DeferredRendering::DeferredRendering()
    : RenderingTechnique("DeferredRendering") {
}

DeferredRendering::~DeferredRendering() {
    Shutdown();
}

bool DeferredRendering::Initialize(VulkanRenderer* renderer, RenderPipeline* pipeline) {
    m_Renderer = renderer;
    m_Pipeline = pipeline;
    
    ENJIN_LOG_INFO(Renderer, "Initializing Deferred Rendering technique...");
    
    if (!renderer || !renderer->GetContext()) {
        ENJIN_LOG_ERROR(Renderer, "Invalid renderer or context");
        return false;
    }
    
    VulkanContext* context = renderer->GetContext();
    VkExtent2D extent = renderer->GetSwapchainExtent();
    m_Width = extent.width;
    m_Height = extent.height;
    
    // Create G-Buffer
    if (!CreateGBuffer()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create G-Buffer");
        return false;
    }
    
    // Create geometry pass (uses existing pipeline)
    if (!CreateGeometryPass()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create geometry pass");
        return false;
    }
    
    // Create lighting pass
    if (!CreateLightingPass()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create lighting pass");
        return false;
    }
    
    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Deferred Rendering initialized");
    return true;
}

void DeferredRendering::Shutdown() {
    if (!m_Initialized) {
        return;
    }
    
    ENJIN_LOG_INFO(Renderer, "Shutting down Deferred Rendering");
    
    DestroyGBuffer();
    
    if (m_LightingPipeline) {
        m_LightingPipeline->Destroy();
        m_LightingPipeline.reset();
    }
    
    m_LightingVertexShader.reset();
    m_LightingFragmentShader.reset();
    
    if (m_LightingDescriptorPool != VK_NULL_HANDLE && m_Renderer && m_Renderer->GetContext()) {
        vkDestroyDescriptorPool(m_Renderer->GetContext()->GetDevice(), m_LightingDescriptorPool, nullptr);
        m_LightingDescriptorPool = VK_NULL_HANDLE;
    }
    
    if (m_LightingDescriptorSetLayout != VK_NULL_HANDLE && m_Renderer && m_Renderer->GetContext()) {
        vkDestroyDescriptorSetLayout(m_Renderer->GetContext()->GetDevice(), m_LightingDescriptorSetLayout, nullptr);
        m_LightingDescriptorSetLayout = VK_NULL_HANDLE;
    }
    
    m_Initialized = false;
}

void DeferredRendering::Render(f32 deltaTime) {
    if (!m_Renderer || !m_Pipeline) {
        return;
    }

    VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE) {
        return;
    }

    // ========================================================================
    // Step 1: Geometry Pass - Render scene to G-Buffer
    // ========================================================================
    if (m_GBuffer.renderPass != VK_NULL_HANDLE && m_GBuffer.framebuffer != VK_NULL_HANDLE) {
        std::array<VkClearValue, 4> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // Position
        clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // Normal
        clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};  // Albedo
        clearValues[3].depthStencil = {1.0f, 0};              // Depth

        VkRenderPassBeginInfo renderPassBegin{};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = m_GBuffer.renderPass;
        renderPassBegin.framebuffer = m_GBuffer.framebuffer;
        renderPassBegin.renderArea.offset = {0, 0};
        renderPassBegin.renderArea.extent = {m_Width, m_Height};
        renderPassBegin.clearValueCount = static_cast<u32>(clearValues.size());
        renderPassBegin.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

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

        // Geometry rendering would be dispatched here by the render pipeline.
        // The pipeline binds the G-Buffer geometry shader and draws all opaque meshes.
        // This integrates with RenderPipeline which provides the draw commands.

        vkCmdEndRenderPass(cmd);
    }

    // ========================================================================
    // Step 2: Lighting Pass - Fullscreen pass reading G-Buffer
    // ========================================================================
    if (m_LightingDescriptorSet != VK_NULL_HANDLE) {
        // Update descriptor set with current G-Buffer images
        std::array<VkDescriptorImageInfo, 3> imageInfos{};

        VkSampler sampler = VK_NULL_HANDLE; // Would use renderer's default sampler
        // Position
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = m_GBuffer.position ? m_GBuffer.position->GetImageView() : VK_NULL_HANDLE;
        imageInfos[0].sampler = sampler;
        // Normal
        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].imageView = m_GBuffer.normal ? m_GBuffer.normal->GetImageView() : VK_NULL_HANDLE;
        imageInfos[1].sampler = sampler;
        // Albedo
        imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[2].imageView = m_GBuffer.albedo ? m_GBuffer.albedo->GetImageView() : VK_NULL_HANDLE;
        imageInfos[2].sampler = sampler;

        std::array<VkWriteDescriptorSet, 3> writes{};
        for (u32 i = 0; i < 3; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = m_LightingDescriptorSet;
            writes[i].dstBinding = i;
            writes[i].dstArrayElement = 0;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].descriptorCount = 1;
            writes[i].pImageInfo = &imageInfos[i];
        }

        if (imageInfos[0].imageView && imageInfos[1].imageView && imageInfos[2].imageView && sampler) {
            vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(),
                static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
        }

        // The lighting pass would render a fullscreen triangle using the main render pass.
        // It reads the G-Buffer textures and computes lighting per-pixel.
        // This requires a dedicated lighting shader (deferred_lighting.frag).
        // Pipeline binding and draw call would go here once shaders are compiled.
    }

    // ========================================================================
    // Step 3: Forward Pass - Transparent objects
    // ========================================================================
    // Transparent objects are rendered in the main forward pass after deferred lighting.
    // This is handled by the standard RenderSystem with alpha-blended materials.

    (void)deltaTime;
}

bool DeferredRendering::CreateGBuffer() {
    VulkanContext* context = m_Renderer->GetContext();
    
    // Create position buffer (RGB16F)
    m_GBuffer.position = std::make_unique<VulkanImage>(context);
    if (!m_GBuffer.position->Create(
        m_Width, m_Height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    )) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create position G-Buffer");
        return false;
    }
    
    // Create normal buffer (RGB16F)
    m_GBuffer.normal = std::make_unique<VulkanImage>(context);
    if (!m_GBuffer.normal->Create(
        m_Width, m_Height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    )) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create normal G-Buffer");
        return false;
    }
    
    // Create albedo buffer (RGBA8)
    m_GBuffer.albedo = std::make_unique<VulkanImage>(context);
    if (!m_GBuffer.albedo->Create(
        m_Width, m_Height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    )) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create albedo G-Buffer");
        return false;
    }
    
    // Create depth buffer
    m_GBuffer.depth = std::make_unique<VulkanImage>(context);
    if (!m_GBuffer.depth->Create(
        m_Width, m_Height,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    )) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create depth G-Buffer");
        return false;
    }
    
    ENJIN_LOG_INFO(Renderer, "G-Buffer created: %dx%d", m_Width, m_Height);
    return true;
}

void DeferredRendering::DestroyGBuffer() {
    m_GBuffer.position.reset();
    m_GBuffer.normal.reset();
    m_GBuffer.albedo.reset();
    m_GBuffer.depth.reset();
    
    if (m_GBuffer.framebuffer != VK_NULL_HANDLE && m_Renderer && m_Renderer->GetContext()) {
        vkDestroyFramebuffer(m_Renderer->GetContext()->GetDevice(), m_GBuffer.framebuffer, nullptr);
        m_GBuffer.framebuffer = VK_NULL_HANDLE;
    }
    
    if (m_GBuffer.renderPass != VK_NULL_HANDLE && m_Renderer && m_Renderer->GetContext()) {
        vkDestroyRenderPass(m_Renderer->GetContext()->GetDevice(), m_GBuffer.renderPass, nullptr);
        m_GBuffer.renderPass = VK_NULL_HANDLE;
    }
}

bool DeferredRendering::CreateGeometryPass() {
    VulkanContext* context = m_Renderer->GetContext();

    // Create render pass for G-Buffer
    std::array<VkAttachmentDescription, 4> attachments{};

    // Position attachment (R16G16B16A16_SFLOAT)
    attachments[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Normal attachment (same format)
    attachments[1] = attachments[0];

    // Albedo attachment (R8G8B8A8_SRGB)
    attachments[2].format = VK_FORMAT_R8G8B8A8_SRGB;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment (D32_SFLOAT)
    attachments[3].format = VK_FORMAT_D32_SFLOAT;
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentReference, 3> colorRefs{};
    colorRefs[0] = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorRefs[1] = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    colorRefs[2] = {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkAttachmentReference depthRef{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    // Subpass dependency to ensure color attachment writes are visible
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<u32>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(
        context->GetDevice(), &renderPassInfo, nullptr, &m_GBuffer.renderPass);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create G-Buffer render pass: %d", result);
        return false;
    }

    // Create framebuffer using G-Buffer image views
    std::array<VkImageView, 4> fbAttachments = {
        m_GBuffer.position->GetImageView(),
        m_GBuffer.normal->GetImageView(),
        m_GBuffer.albedo->GetImageView(),
        m_GBuffer.depth->GetImageView()
    };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_GBuffer.renderPass;
    framebufferInfo.attachmentCount = static_cast<u32>(fbAttachments.size());
    framebufferInfo.pAttachments = fbAttachments.data();
    framebufferInfo.width = m_Width;
    framebufferInfo.height = m_Height;
    framebufferInfo.layers = 1;

    result = vkCreateFramebuffer(
        context->GetDevice(), &framebufferInfo, nullptr, &m_GBuffer.framebuffer);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create G-Buffer framebuffer: %d", result);
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Geometry pass created with framebuffer (%dx%d)", m_Width, m_Height);
    return true;
}

bool DeferredRendering::CreateLightingPass() {
    VulkanContext* context = m_Renderer->GetContext();
    
    // Create descriptor set layout for G-Buffer textures
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
    
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    
    VkResult result = vkCreateDescriptorSetLayout(
        context->GetDevice(), &layoutInfo, nullptr, &m_LightingDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create lighting descriptor set layout: %d", result);
        return false;
    }
    
    // Create descriptor pool
    std::array<VkDescriptorPoolSize, 1> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 3;
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    
    result = vkCreateDescriptorPool(
        context->GetDevice(), &poolInfo, nullptr, &m_LightingDescriptorPool);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create lighting descriptor pool: %d", result);
        return false;
    }
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_LightingDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_LightingDescriptorSetLayout;
    
    result = vkAllocateDescriptorSets(
        context->GetDevice(), &allocInfo, &m_LightingDescriptorSet);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate lighting descriptor set: %d", result);
        return false;
    }
    
    ENJIN_LOG_INFO(Renderer, "Lighting pass created (shader loading would be implemented here)");
    return true;
}

} // namespace Renderer
} // namespace Enjin
