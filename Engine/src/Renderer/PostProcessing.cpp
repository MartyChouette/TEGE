#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Logging/Log.h"
#include "stb_image.h"
#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace Enjin {
namespace Renderer {

// Fullscreen quad vertex shader (embedded SPIR-V)
// Simple fullscreen triangle that covers the entire screen
// The fullscreen-triangle vertex and post-process fragment shaders come from
// the generated ShaderData.h (python _gen_all.py), same as every other
// embedded shader. They were previously BAKED here as static u32 arrays,
// which silently froze the post pipeline: postprocess.frag edits compiled
// into ShaderData.h but this file kept using its fossil copy, and as the
// PostProcessSettings struct grew, the fossil shader read the UBO at stale
// offsets (2026-08-07 glow hunt root cause).



PostProcessing::PostProcessing() = default;

PostProcessing::~PostProcessing() {
    Shutdown();
}

bool PostProcessing::Initialize(VulkanContext* context, VkRenderPass renderPass, u32 width, u32 height,
                                VulkanRenderer* renderer, u32 colorAttachmentCount) {
    if (m_Initialized) {
        return true;
    }

    m_Context = context;
    m_Renderer = renderer;
    m_RenderPass = renderPass;
    m_ColorAttachmentCount = (colorAttachmentCount > 0) ? colorAttachmentCount : 1;
    m_Width = width;
    m_Height = height;
    m_Settings.screenWidth = width;
    m_Settings.screenHeight = height;

    ENJIN_LOG_INFO(Renderer, "Initializing post-processing (%ux%u)", width, height);

    // Create a sampler for reading the source scene texture
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 0.0f;
        if (vkCreateSampler(context->GetDevice(), &samplerInfo, nullptr, &m_SceneSampler) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create PP scene sampler");
            return false;
        }
    }

    // Create uniform buffer
    if (!CreateUniformBuffer()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create post-process uniform buffer");
        return false;
    }

    // Create descriptor sets (must be before pipeline, needs layout)
    if (!CreateDescriptorSets()) {
        ENJIN_LOG_WARN(Renderer, "Failed to create post-process descriptor sets - effects disabled");
    }

    // Create the graphics pipeline
    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        if (!CreatePipeline()) {
            ENJIN_LOG_ERROR(Renderer, "PP PIPELINE CREATION FAILED - renderPass=%p layout=%p",
                (void*)(uintptr_t)m_RenderPass, (void*)(uintptr_t)m_DescriptorSetLayout);
        } else {
            ENJIN_LOG_INFO(Renderer, "PP pipeline created OK - handle=%p renderPass=%p",
                (void*)(uintptr_t)m_Pipeline, (void*)(uintptr_t)m_RenderPass);
        }
    } else {
        ENJIN_LOG_ERROR(Renderer, "PP descriptor set layout is NULL - cannot create pipeline");
    }

    // Create TAA compute pipeline and history buffers
    if (!CreateTAAResources()) {
        ENJIN_LOG_WARN(Renderer, "TAA resources not created - TAA disabled (compute shader may need compilation)");
    }

    m_Initialized = true;

    ENJIN_LOG_INFO(Renderer, "Post-processing initialized");
    return true;
}

void PostProcessing::Shutdown() {
    if (!m_Initialized || !m_Context) {
        return;
    }

    VkDevice device = m_Context->GetDevice();
    m_Context->WaitForGPU();

    // Clean up pipeline
    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }
    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    // Clean up TAA resources
    DestroyTAAResources();

    // Clean up DoF staging buffers
    DestroyDofStagingBuffers();

    // Clean up LUT resources
    DestroyLUTResources();
    if (m_LUTSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_LUTSampler, nullptr);
        m_LUTSampler = VK_NULL_HANDLE;
    }

    // Clean up uniform buffer
    m_UniformBuffer.reset();

    // Clean up scene sampler
    if (m_SceneSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_SceneSampler, nullptr);
        m_SceneSampler = VK_NULL_HANDLE;
    }

    m_Initialized = false;
    ENJIN_LOG_INFO(Renderer, "Post-processing shutdown");
}

void PostProcessing::OnResize(u32 width, u32 height) {
    if (width == 0 || height == 0 || !m_Initialized) {
        return;
    }

    if (width == m_Width && height == m_Height) {
        return;
    }

    ENJIN_LOG_INFO(Renderer, "Post-processing resize: %ux%u -> %ux%u", m_Width, m_Height, width, height);

    m_Width = width;
    m_Height = height;
    m_Settings.screenWidth = width;
    m_Settings.screenHeight = height;

    // Recreate render target and invalidate DoF staging buffers (size changed)
    if (m_Renderer) m_Renderer->WaitForAllFrames();
    else vkDeviceWaitIdle(m_Context->GetDevice());
    DestroyDofStagingBuffers();
    // No internal scene RT to recreate — PP is a pure pass-through.
    // Descriptor bindings are refreshed by EditorLayer calling UpdateSourceImage()
    // after resize, which sets bindings 0, 2, 3 to valid external image views.

    // Recreate TAA history buffers at the new resolution
    DestroyTAAResources();
    if (!CreateTAAResources()) {
        ENJIN_LOG_WARN(Renderer, "TAA resources not recreated after resize");
    }
}

void PostProcessing::Apply(VkCommandBuffer cmd, VkImageView sourceImage, VkFramebuffer targetFramebuffer) {
    if (!m_Initialized || m_Pipeline == VK_NULL_HANDLE) {
        return;
    }

    // Update uniform buffer with current settings
    UpdateUniformBuffer();

    bool needsDepth = m_Settings.NeedsDepthBuffer() && m_DepthImage != VK_NULL_HANDLE;

    // Transition scene image for shader reading
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_SceneImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    if (needsDepth) {
        // Transition depth image for shader reading (DoF/Tilt-Shift/CelOutline)
        VkImageMemoryBarrier depthBarrier{};
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = m_DepthImage;
        depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBarrier.subresourceRange.baseMipLevel = 0;
        depthBarrier.subresourceRange.levelCount = 1;
        depthBarrier.subresourceRange.baseArrayLayer = 0;
        depthBarrier.subresourceRange.layerCount = 1;
        depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkImageMemoryBarrier barriers[2] = { barrier, depthBarrier };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 2, barriers);
    } else {
        // Color-only barrier (skip depth transition)
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Bind post-process pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout,
        0, 1, &m_DescriptorSet, 0, nullptr);

    // Draw fullscreen triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // Transition scene image back for next frame
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (needsDepth) {
        VkImageMemoryBarrier depthBarrier{};
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = m_DepthImage;
        depthBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBarrier.subresourceRange.baseMipLevel = 0;
        depthBarrier.subresourceRange.levelCount = 1;
        depthBarrier.subresourceRange.baseArrayLayer = 0;
        depthBarrier.subresourceRange.layerCount = 1;
        depthBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkImageMemoryBarrier restoreBarriers[2] = { barrier, depthBarrier };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr, 2, restoreBarriers);
    } else {
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
}

void PostProcessing::ApplyToCurrentPass(VkCommandBuffer cmd, u32 width, u32 height) {
    if (!m_Initialized || m_Pipeline == VK_NULL_HANDLE) {
        // Not a silent no-op: the game view keeps last frame's content when
        // this returns. Rare (init failure), but say so once.
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            ENJIN_LOG_ERROR(Renderer, "PostProcessing::ApplyToCurrentPass skipped (init=%d pipeline=%p) — game view will show stale content",
                (int)m_Initialized, (void*)m_Pipeline);
        }
        return;
    }

    // Update settings with current screen dimensions
    m_Settings.screenWidth = width;
    m_Settings.screenHeight = height;

    // Update uniform buffer with current settings
    UpdateUniformBuffer();

    // Set viewport and scissor for the output dimensions
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(width);
    viewport.height = static_cast<f32>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind post-process pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout,
        0, 1, &m_DescriptorSet, 0, nullptr);

    // Draw fullscreen triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void PostProcessing::UpdateSourceImage(VkImageView imageView, VkSampler sampler) {
    if (!m_Initialized || !m_Context || m_DescriptorSet == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    // Update binding 0 (scene source texture) and binding 3 (depth placeholder).
    // If a LUT is loaded, DON'T overwrite binding 2 — LoadLUT() already wrote the
    // real LUT image view there. Only write a placeholder to binding 2 when no LUT
    // is loaded (Vulkan requires valid descriptors at all bindings).
    VkDescriptorImageInfo lutPlaceholderInfo = imageInfo;
    if (m_LUTSampler != VK_NULL_HANDLE) {
        lutPlaceholderInfo.sampler = m_LUTSampler;
    }

    // Hybrid RT overlay (bindings 4-5): the RT shadow/AO views when hybrid is
    // active, else the scene placeholder (the shader ignores them unless
    // rtHybridEnable is set). Always written so the set is complete.
    VkDescriptorImageInfo rtShadowInfo = imageInfo;
    VkDescriptorImageInfo rtAOInfo = imageInfo;
    VkDescriptorImageInfo rtReflectInfo = imageInfo;
    VkDescriptorImageInfo rtGIInfo = imageInfo;
    if (m_RTShadowView != VK_NULL_HANDLE && m_RTAOView != VK_NULL_HANDLE &&
        m_RTReflectView != VK_NULL_HANDLE && m_RTGIView != VK_NULL_HANDLE &&
        m_RTHybridSampler != VK_NULL_HANDLE) {
        auto setInfo = [&](VkDescriptorImageInfo& info, VkImageView v) {
            info.imageView = v;
            info.sampler = m_RTHybridSampler;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        };
        setInfo(rtShadowInfo, m_RTShadowView);
        setInfo(rtAOInfo, m_RTAOView);
        setInfo(rtReflectInfo, m_RTReflectView);
        setInfo(rtGIInfo, m_RTGIView);
    }

    // If a real LUT is loaded, preserve binding 2 — only update bindings 0 and 3.
    // If no LUT is loaded, also write a placeholder to binding 2.
    u32 writeCount = 0;
    VkWriteDescriptorSet writes[7]{};

    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = m_DescriptorSet;
    writes[writeCount].dstBinding = 0;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].pImageInfo = &imageInfo;
    writeCount++;

    if (!m_LUTLoaded) {
        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].dstSet = m_DescriptorSet;
        writes[writeCount].dstBinding = 2;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].pImageInfo = &lutPlaceholderInfo;
        writeCount++;
    }

    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = m_DescriptorSet;
    writes[writeCount].dstBinding = 3;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].pImageInfo = &imageInfo;
    writeCount++;

    // Binding 4: RT hybrid shadow
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = m_DescriptorSet;
    writes[writeCount].dstBinding = 4;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].pImageInfo = &rtShadowInfo;
    writeCount++;

    // Binding 5: RT hybrid AO
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = m_DescriptorSet;
    writes[writeCount].dstBinding = 5;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].pImageInfo = &rtAOInfo;
    writeCount++;

    // Binding 6: RT hybrid reflections
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = m_DescriptorSet;
    writes[writeCount].dstBinding = 6;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].pImageInfo = &rtReflectInfo;
    writeCount++;

    // Binding 7: RT hybrid GI
    writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[writeCount].dstSet = m_DescriptorSet;
    writes[writeCount].dstBinding = 7;
    writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[writeCount].descriptorCount = 1;
    writes[writeCount].pImageInfo = &rtGIInfo;
    writeCount++;

    vkUpdateDescriptorSets(m_Context->GetDevice(), writeCount, writes, 0, nullptr);
    m_DepthSourceReady = false;  // Depth placeholder, not real depth
    m_LastDepthView = VK_NULL_HANDLE;  // Set was rewritten; force next UpdateDepthSource to re-bind real depth
}

void PostProcessing::UpdateRenderPass(VkRenderPass newPass, u32 colorAttachmentCount) {
    if (!m_Initialized || !m_Context || newPass == VK_NULL_HANDLE) return;
    if (newPass == m_RenderPass && colorAttachmentCount == m_ColorAttachmentCount) return;

    m_RenderPass = newPass;
    m_ColorAttachmentCount = (colorAttachmentCount > 0) ? colorAttachmentCount : 1;

    // Recreate the pipeline against the new render pass
    VkDevice device = m_Context->GetDevice();
    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    CreatePipeline();
}

void PostProcessing::SetEffectEnabled(PostProcessEffect effect, bool enabled) {
    if (enabled) {
        m_EnabledEffects = m_EnabledEffects | effect;
    } else {
        m_EnabledEffects = static_cast<PostProcessEffect>(
            static_cast<u32>(m_EnabledEffects) & ~static_cast<u32>(effect));
    }

    // Update settings based on enabled effects
    m_Settings.bloomEnabled = HasEffect(m_EnabledEffects, PostProcessEffect::Bloom) ? 1 : 0;
    m_Settings.vignetteEnabled = HasEffect(m_EnabledEffects, PostProcessEffect::Vignette) ? 1 : 0;
    m_Settings.chromaticAberrationEnabled = HasEffect(m_EnabledEffects, PostProcessEffect::ChromaticAberration) ? 1 : 0;
    m_Settings.filmGrainEnabled = HasEffect(m_EnabledEffects, PostProcessEffect::FilmGrain) ? 1 : 0;
    m_Settings.fxaaEnabled = HasEffect(m_EnabledEffects, PostProcessEffect::FXAA) ? 1 : 0;

    if (!HasEffect(m_EnabledEffects, PostProcessEffect::ToneMapping)) {
        m_Settings.toneMappingMode = static_cast<u32>(ToneMappingMode::None);
    }
}

bool PostProcessing::IsEffectEnabled(PostProcessEffect effect) const {
    return HasEffect(m_EnabledEffects, effect);
}

VkImage PostProcessing::GetSceneImage() const {
    return m_SceneImage;
}

VkImageView PostProcessing::GetSceneImageView() const {
    return m_SceneImageView;
}

VkFramebuffer PostProcessing::GetSceneFramebuffer() const {
    return m_SceneFramebuffer;
}

bool PostProcessing::CreateSceneRenderTarget(u32 width, u32 height) {
    VkDevice device = m_Context->GetDevice();
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    // Create HDR color image (RGBA16F for HDR rendering)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;  // HDR format
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_SceneImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create scene HDR image");
        return false;
    }

    // Get memory requirements and allocate
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_SceneImage, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    u32 memTypeIndex = UINT32_MAX;
    for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIndex = i;
            break;
        }
    }

    if (memTypeIndex == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "Failed to find suitable memory type");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_SceneImageMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate scene image memory");
        return false;
    }

    if (vkBindImageMemory(device, m_SceneImage, m_SceneImageMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind scene image memory");
        return false;
    }

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_SceneImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_SceneImageView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create scene image view");
        return false;
    }

    // Create depth image
    VkImageCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.format = VK_FORMAT_D32_SFLOAT;
    depthInfo.extent.width = width;
    depthInfo.extent.height = height;
    depthInfo.extent.depth = 1;
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &depthInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create depth image");
        return false;
    }

    vkGetImageMemoryRequirements(device, m_DepthImage, &memReqs);

    allocInfo.allocationSize = memReqs.size;
    // Re-query memory type for depth format (may differ from color image)
    for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthImageMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate depth image memory");
        return false;
    }

    if (vkBindImageMemory(device, m_DepthImage, m_DepthImageMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind depth image memory");
        return false;
    }

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
        ENJIN_LOG_ERROR(Renderer, "Failed to create depth image view");
        return false;
    }

    // Create scene render pass (HDR output)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_SceneRenderPass) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create scene render pass");
        return false;
    }

    // Create scene framebuffer
    std::array<VkImageView, 2> fbAttachments = { m_SceneImageView, m_DepthImageView };

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_SceneRenderPass;
    fbInfo.attachmentCount = static_cast<u32>(fbAttachments.size());
    fbInfo.pAttachments = fbAttachments.data();
    fbInfo.width = width;
    fbInfo.height = height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_SceneFramebuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create scene framebuffer");
        return false;
    }

    // Create sampler for scene image
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

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_SceneSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create scene sampler");
        return false;
    }

    return true;
}

void PostProcessing::DestroySceneRenderTarget() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();

    if (m_SceneSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_SceneSampler, nullptr);
        m_SceneSampler = VK_NULL_HANDLE;
    }
    if (m_SceneFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_SceneFramebuffer, nullptr);
        m_SceneFramebuffer = VK_NULL_HANDLE;
    }
    if (m_SceneRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_SceneRenderPass, nullptr);
        m_SceneRenderPass = VK_NULL_HANDLE;
    }
    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
    }
    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }
    if (m_DepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_DepthImageMemory, nullptr);
        m_DepthImageMemory = VK_NULL_HANDLE;
    }
    if (m_SceneImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_SceneImageView, nullptr);
        m_SceneImageView = VK_NULL_HANDLE;
    }
    if (m_SceneImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_SceneImage, nullptr);
        m_SceneImage = VK_NULL_HANDLE;
    }
    if (m_SceneImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_SceneImageMemory, nullptr);
        m_SceneImageMemory = VK_NULL_HANDLE;
    }
}

bool PostProcessing::CreateUniformBuffer() {
    m_UniformBuffer = std::make_unique<VulkanBuffer>(m_Context);
    return m_UniformBuffer->Create(sizeof(PostProcessSettings),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        true);  // hostVisible = true for CPU updates
}

void PostProcessing::UpdateUniformBuffer() {
    if (!m_UniformBuffer) return;

    PostProcessSettings safeSettings = m_Settings;

    // LUT safety: never sample the LUT unless a real LUT image is loaded. When
    // no LUT is loaded, binding 2 holds the scene placeholder, so an enabled LUT
    // would make applyLUT() sample the scene as a color cube and wreck the image.
    // lutEnabled can arrive true without a loaded LUT via scene deserialization,
    // so clamp it here at the single upload chokepoint.
    if (!m_LUTLoaded) {
        safeSettings.lutEnabled = 0;
    }

    // If depth-needing effects are enabled but no valid depth source was bound,
    // suppress them in the uploaded copy to prevent sampling an invalid descriptor.
    if (safeSettings.NeedsDepthBuffer() && !m_DepthSourceReady) {
        safeSettings.dofEnabled = 0;
        safeSettings.tiltShiftEnabled = 0;
        safeSettings.celOutlineEnabled = 0;
        safeSettings.ssaoEnabled = 0;
        safeSettings.contactShadowsEnabled = 0;
        safeSettings.causticsEnabled = 0;
        safeSettings.fogShaftsEnabled = 0;
        safeSettings.godRaysEnabled = 0;
    }

    m_UniformBuffer->UploadData(&safeSettings, sizeof(PostProcessSettings));
}

void PostProcessing::UpdateDepthSource(VkImageView depthView) {
    if (!m_Initialized || !m_Context || m_DescriptorSet == VK_NULL_HANDLE) {
        return;
    }

    if (depthView == VK_NULL_HANDLE) {
        m_DepthSourceReady = false;
        m_LastDepthView = VK_NULL_HANDLE;
        return;
    }

    // The post-process descriptor set is reused across frames in flight, and its
    // binding-3 layout has no UPDATE_AFTER_BIND flag. Rewriting it every frame
    // invalidates any command buffer still pending that has it bound (validation
    // VUID-vkUpdateDescriptorSets-None-03047 -> the whole frame's commands then
    // fault as "not in recording state"). The depth view only changes on viewport
    // resize, which recreates the render target under device-idle, so only write
    // the descriptor when the view actually changes.
    if (depthView == m_LastDepthView) {
        m_DepthSourceReady = true;
        return;
    }

    // Update descriptor binding 3 with the external depth image view
    VkDescriptorImageInfo depthImageInfo{};
    depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthImageInfo.imageView = depthView;
    depthImageInfo.sampler = m_SceneSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 3;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &depthImageInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
    m_LastDepthView = depthView;
    m_DepthSourceReady = true;
}

bool PostProcessing::CreatePipeline() {
    if (!m_Context || m_RenderPass == VK_NULL_HANDLE) return false;

    VkDevice device = m_Context->GetDevice();

    // Create shader modules from embedded SPIR-V
    VkShaderModuleCreateInfo vertInfo{};
    vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertInfo.codeSize = ShaderData::FullscreenVertexShaderDataSize;
    vertInfo.pCode = reinterpret_cast<const uint32_t*>(ShaderData::FullscreenVertexShaderData);

    VkShaderModule vertModule;
    if (vkCreateShaderModule(device, &vertInfo, nullptr, &vertModule) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create post-process vertex shader module");
        return false;
    }

    VkShaderModuleCreateInfo fragInfo{};
    fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragInfo.codeSize = ShaderData::PostProcessFragmentShaderDataSize;
    fragInfo.pCode = reinterpret_cast<const uint32_t*>(ShaderData::PostProcessFragmentShaderData);

    VkShaderModule fragModule;
    if (vkCreateShaderModule(device, &fragInfo, nullptr, &fragModule) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        ENJIN_LOG_ERROR(Renderer, "Failed to create post-process fragment shader module");
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

    // Blend states must match the render pass's colorAttachmentCount (VUID-07609):
    // 1 for the editor's offscreen PP pass, 2 when compositing into the player's
    // swapchain MRT pass. The shader writes location 0 only; extra attachments
    // (velocity) get colorWriteMask = 0 so they're left untouched.
    VkPipelineColorBlendAttachmentState blendAttachments[2] = {};
    blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachments[0].blendEnable = VK_FALSE;

    u32 blendCount = (m_ColorAttachmentCount <= 2) ? m_ColorAttachmentCount : 2;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = blendCount;
    colorBlending.pAttachments = blendAttachments;

    // No depth testing for post-process
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // Pipeline layout with descriptor set
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_DescriptorSetLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        ENJIN_LOG_ERROR(Renderer, "Failed to create post-process pipeline layout");
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
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_RenderPass;
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create post-process pipeline: %d", result);
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Post-process pipeline created");
    return true;
}

bool PostProcessing::CreateDescriptorSets() {
    if (!m_Context) return false;

    VkDevice device = m_Context->GetDevice();

    // Descriptor set layout: 0=scene, 1=settings UBO, 2=LUT, 3=depth,
    // 4-7=RT hybrid shadow/AO/reflect/GI (all fragment-stage samplers)
    VkDescriptorSetLayoutBinding bindings[8]{};
    for (int i = 0; i < 8; ++i) {
        bindings[i].binding = static_cast<u32>(i);
        bindings[i].descriptorType = (i == 1)
            ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 8;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create post-process descriptor set layout");
        return false;
    }

    // Descriptor pool (7 samplers + 1 UBO)
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 7;  // scene + LUT + depth + RT shadow/AO/reflect/GI
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create post-process descriptor pool");
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate post-process descriptor set");
        return false;
    }

    // Create LUT sampler (used for both placeholder and real LUT)
    VkSamplerCreateInfo lutSamplerInfo{};
    lutSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    lutSamplerInfo.magFilter = VK_FILTER_LINEAR;
    lutSamplerInfo.minFilter = VK_FILTER_LINEAR;
    lutSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    lutSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    lutSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    lutSamplerInfo.maxLod = 0.0f;
    if (vkCreateSampler(device, &lutSamplerInfo, nullptr, &m_LUTSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create LUT sampler");
        return false;
    }

    // Write only the UBO binding at init. Image bindings (0=scene, 2=LUT, 3=depth)
    // are set later by UpdateSourceImage() and UpdateDepthSource() with valid external
    // image views. No internal scene RT — PP is a pure pass-through.
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_UniformBuffer ? m_UniformBuffer->GetBuffer() : VK_NULL_HANDLE;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(PostProcessSettings);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    ENJIN_LOG_INFO(Renderer, "Post-process descriptor sets created");
    return true;
}

void PostProcessing::DestroyLUTResources() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_LUTImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_LUTImageView, nullptr);
        m_LUTImageView = VK_NULL_HANDLE;
    }
    if (m_LUTImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_LUTImage, nullptr);
        m_LUTImage = VK_NULL_HANDLE;
    }
    if (m_LUTImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_LUTImageMemory, nullptr);
        m_LUTImageMemory = VK_NULL_HANDLE;
    }
    m_LUTLoaded = false;
}

bool PostProcessing::LoadLUT(const std::string& filepath) {
    if (!m_Context || !m_Initialized) return false;

    // Load image using stb_image
    int width, height, channels;
    unsigned char* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load LUT image: %s", filepath.c_str());
        return false;
    }

    VkDevice device = m_Context->GetDevice();
    if (m_Renderer) m_Renderer->WaitForAllFrames();
    else vkDeviceWaitIdle(device);

    // Clean up previous LUT
    DestroyLUTResources();

    // Determine LUT size from image dimensions (strip format: width = size*size, height = size)
    u32 lutSize = static_cast<u32>(height);
    m_Settings.lutSize = lutSize;

    // Create Vulkan image
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent = { static_cast<u32>(width), static_cast<u32>(height), 1 };
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageCreateInfo, nullptr, &m_LUTImage) != VK_SUCCESS) {
        stbi_image_free(pixels);
        ENJIN_LOG_ERROR(Renderer, "Failed to create LUT image");
        return false;
    }

    // Allocate memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_LUTImage, &memReqs);

    VkMemoryAllocateInfo memAlloc{};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &memAlloc, nullptr, &m_LUTImageMemory) != VK_SUCCESS) {
        stbi_image_free(pixels);
        DestroyLUTResources();
        return false;
    }
    if (vkBindImageMemory(device, m_LUTImage, m_LUTImageMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind LUT image memory");
        stbi_image_free(pixels);
        DestroyLUTResources();
        return false;
    }

    // Upload via staging buffer — cast to VkDeviceSize before multiply to prevent int overflow
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * 4;
    VulkanBuffer stagingBuffer(m_Context);
    stagingBuffer.Create(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true);
    stagingBuffer.UploadData(pixels, imageSize);
    stbi_image_free(pixels);

    // Transition and copy using a temporary command pool
    VkCommandPool tempPool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
            DestroyLUTResources();
            return false;
        }
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = tempPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &allocInfo, &cmd) != VK_SUCCESS) {
            vkDestroyCommandPool(device, tempPool, nullptr);
            DestroyLUTResources();
            return false;
        }
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to begin LUT command buffer");
        vkDestroyCommandPool(device, tempPool, nullptr);
        DestroyLUTResources();
        return false;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_LUTImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { static_cast<u32>(width), static_cast<u32>(height), 1 };
    vkCmdCopyBufferToImage(cmd, stagingBuffer.GetBuffer(), m_LUTImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to end LUT command buffer");
        vkDestroyCommandPool(device, tempPool, nullptr);
        DestroyLUTResources();
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit LUT upload commands");
    }
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    vkDestroyCommandPool(device, tempPool, nullptr);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_LUTImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_LUTImageView) != VK_SUCCESS) {
        DestroyLUTResources();
        return false;
    }

    // Update descriptor set binding 2 with the LUT texture
    VkDescriptorImageInfo lutDescInfo{};
    lutDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    lutDescInfo.imageView = m_LUTImageView;
    lutDescInfo.sampler = m_LUTSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 2;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &lutDescInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    m_LUTPath = filepath;
    m_LUTLoaded = true;
    m_Settings.lutEnabled = 1;

    ENJIN_LOG_INFO(Renderer, "Loaded LUT: %s (%dx%d, size=%u)", filepath.c_str(), width, height, lutSize);
    return true;
}

void PostProcessing::ClearLUT() {
    if (!m_Context || !m_Initialized) return;

    VkDevice device = m_Context->GetDevice();
    if (m_Renderer) m_Renderer->WaitForAllFrames();
    else vkDeviceWaitIdle(device);

    DestroyLUTResources();

    // Rebind default image to binding 2 (shader skips LUT when lutEnabled==0)
    VkDescriptorImageInfo lutDescInfo{};
    lutDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    lutDescInfo.imageView = m_SceneImageView;
    lutDescInfo.sampler = m_LUTSampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 2;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &lutDescInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    m_LUTPath.clear();
    m_Settings.lutEnabled = 0;

    ENJIN_LOG_INFO(Renderer, "LUT cleared");
}

// ============================================================================
// DoF / TILT-SHIFT CPU STAGING BUFFERS
// ============================================================================

bool PostProcessing::CreateDofStagingBuffers() {
    if (!m_Context || m_Width == 0 || m_Height == 0) return false;
    VkDevice device = m_Context->GetDevice();

    // Color staging: RGBA16F = 8 bytes per pixel
    m_DofColorStagingSize = static_cast<usize>(m_Width) * m_Height * 8;
    // Depth staging: D32F = 4 bytes per pixel
    m_DofDepthStagingSize = static_cast<usize>(m_Width) * m_Height * 4;

    auto createStagingBuffer = [&](VkDeviceSize size, VkBufferUsageFlags usage,
                                    VkBuffer& buffer, VkDeviceMemory& memory) -> bool {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = size;
        bufInfo.usage = usage;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufInfo, nullptr, &buffer) != VK_SUCCESS) return false;

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
            memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
            return false;
        }
        if (vkBindBufferMemory(device, buffer, memory, 0) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to bind staging buffer memory");
            vkFreeMemory(device, memory, nullptr);
            vkDestroyBuffer(device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
            memory = VK_NULL_HANDLE;
            return false;
        }
        return true;
    };

    // Color readback (GPU -> CPU)
    if (!createStagingBuffer(m_DofColorStagingSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              m_DofColorStagingBuffer, m_DofColorStagingMemory)) {
        ENJIN_LOG_WARN(Renderer, "DoF: Failed to create color staging buffer");
        return false;
    }

    // Depth readback (GPU -> CPU)
    if (!createStagingBuffer(m_DofDepthStagingSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              m_DofDepthStagingBuffer, m_DofDepthStagingMemory)) {
        ENJIN_LOG_WARN(Renderer, "DoF: Failed to create depth staging buffer");
        return false;
    }

    // Upload buffer (CPU -> GPU) for writing back blurred result
    if (!createStagingBuffer(m_DofColorStagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              m_DofUploadBuffer, m_DofUploadMemory)) {
        ENJIN_LOG_WARN(Renderer, "DoF: Failed to create upload staging buffer");
        return false;
    }

    m_DofStagingReady = true;
    return true;
}

void PostProcessing::DestroyDofStagingBuffers() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    auto destroyBuf = [&](VkBuffer& buf, VkDeviceMemory& mem) {
        if (buf != VK_NULL_HANDLE) { vkDestroyBuffer(device, buf, nullptr); buf = VK_NULL_HANDLE; }
        if (mem != VK_NULL_HANDLE) { vkFreeMemory(device, mem, nullptr); mem = VK_NULL_HANDLE; }
    };

    destroyBuf(m_DofColorStagingBuffer, m_DofColorStagingMemory);
    destroyBuf(m_DofDepthStagingBuffer, m_DofDepthStagingMemory);
    destroyBuf(m_DofUploadBuffer, m_DofUploadMemory);
    m_DofStagingReady = false;
}

// ============================================================================
// SEPARABLE WEIGHTED BLUR (shared by DoF and Tilt-Shift)
// ============================================================================
// Applies a separable Gaussian blur weighted per-pixel by a CoC (circle of
// confusion) or blur weight map. Pixels with zero weight are not blurred.
// Uses 2 passes: horizontal then vertical.

void PostProcessing::SeparableWeightedBlur(std::vector<f32>& color, const std::vector<f32>& coc,
                                            u32 w, u32 h, u32 channels, f32 maxRadius,
                                            std::vector<f32>& temp) {
    if (maxRadius < 0.5f) return;

    i32 iw = static_cast<i32>(w);
    i32 ih = static_cast<i32>(h);
    i32 maxR = static_cast<i32>(maxRadius + 0.5f);
    if (maxR < 1) maxR = 1;
    if (maxR > 16) maxR = 16;  // Cap kernel size for performance

    // Precompute Gaussian weights for max kernel size
    std::vector<f32> gaussWeights(maxR + 1);
    f32 sigma = maxRadius * 0.5f;
    if (sigma < 0.1f) sigma = 0.1f;
    f32 twoSigmaSq = 2.0f * sigma * sigma;
    f32 weightSum = 0.0f;
    for (i32 i = 0; i <= maxR; ++i) {
        gaussWeights[i] = std::exp(-static_cast<f32>(i * i) / twoSigmaSq);
        weightSum += (i == 0) ? gaussWeights[i] : 2.0f * gaussWeights[i];
    }
    for (i32 i = 0; i <= maxR; ++i) {
        gaussWeights[i] /= weightSum;
    }

    temp.resize(color.size());

    // Horizontal pass
    for (i32 y = 0; y < ih; ++y) {
        for (i32 x = 0; x < iw; ++x) {
            usize idx = (static_cast<usize>(y) * iw + x);
            f32 pixelCoC = coc[idx];
            i32 r = static_cast<i32>(pixelCoC * static_cast<f32>(maxR) + 0.5f);
            if (r < 1) {
                // No blur for this pixel
                for (u32 c = 0; c < channels; ++c)
                    temp[idx * channels + c] = color[idx * channels + c];
                continue;
            }
            if (r > maxR) r = maxR;

            f32 totalWeight = 0.0f;
            f32 accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (i32 dx = -r; dx <= r; ++dx) {
                i32 sx = x + dx;
                if (sx < 0) sx = 0;
                if (sx >= iw) sx = iw - 1;
                usize sidx = (static_cast<usize>(y) * iw + sx);
                f32 gw = gaussWeights[std::abs(dx)];
                totalWeight += gw;
                for (u32 c = 0; c < channels; ++c)
                    accum[c] += color[sidx * channels + c] * gw;
            }
            if (totalWeight > 0.0f) {
                f32 invW = 1.0f / totalWeight;
                for (u32 c = 0; c < channels; ++c)
                    temp[idx * channels + c] = accum[c] * invW;
            }
        }
    }

    // Vertical pass (read from temp, write to color)
    for (i32 y = 0; y < ih; ++y) {
        for (i32 x = 0; x < iw; ++x) {
            usize idx = (static_cast<usize>(y) * iw + x);
            f32 pixelCoC = coc[idx];
            i32 r = static_cast<i32>(pixelCoC * static_cast<f32>(maxR) + 0.5f);
            if (r < 1) {
                for (u32 c = 0; c < channels; ++c)
                    color[idx * channels + c] = temp[idx * channels + c];
                continue;
            }
            if (r > maxR) r = maxR;

            f32 totalWeight = 0.0f;
            f32 accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (i32 dy = -r; dy <= r; ++dy) {
                i32 sy = y + dy;
                if (sy < 0) sy = 0;
                if (sy >= ih) sy = ih - 1;
                usize sidx = (static_cast<usize>(sy) * iw + x);
                f32 gw = gaussWeights[std::abs(dy)];
                totalWeight += gw;
                for (u32 c = 0; c < channels; ++c)
                    accum[c] += temp[sidx * channels + c] * gw;
            }
            if (totalWeight > 0.0f) {
                f32 invW = 1.0f / totalWeight;
                for (u32 c = 0; c < channels; ++c)
                    color[idx * channels + c] = accum[c] * invW;
            }
        }
    }
}

// ============================================================================
// DEPTH OF FIELD (CPU-side fallback)
// ============================================================================
// Reads back the scene color and depth buffers, computes per-pixel circle of
// confusion (CoC) based on focus distance/range, then applies a separable
// Gaussian blur weighted by CoC. Near-field objects (closer than focus) and
// far-field objects (beyond focus) are blurred with independent strengths.
//
// This is a CPU-side fallback. Once DoF compute shaders are compiled to
// SPIR-V, this can be replaced with a GPU-only compute dispatch.

void PostProcessing::ApplyDepthOfField(VkCommandBuffer cmd) {
    if (!m_Initialized || !m_Context) return;
    if (m_Settings.dofEnabled == 0) return;
    if (m_SceneImage == VK_NULL_HANDLE || m_DepthImage == VK_NULL_HANDLE) return;

    // Lazy-create staging buffers
    if (!m_DofStagingReady) {
        if (!CreateDofStagingBuffers()) return;
    }

    VkDevice device = m_Context->GetDevice();

    // --- Step 1: Transition scene image to TRANSFER_SRC ---
    VkImageMemoryBarrier colorBarrier{};
    colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = m_SceneImage;
    colorBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    colorBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkImageMemoryBarrier depthBarrier{};
    depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthBarrier.image = m_DepthImage;
    depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkImageMemoryBarrier barriers[2] = {colorBarrier, depthBarrier};
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);

    // --- Step 2: Copy color image to staging buffer ---
    VkBufferImageCopy colorCopy{};
    colorCopy.bufferOffset = 0;
    colorCopy.bufferRowLength = 0;
    colorCopy.bufferImageHeight = 0;
    colorCopy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    colorCopy.imageOffset = {0, 0, 0};
    colorCopy.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyImageToBuffer(cmd, m_SceneImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            m_DofColorStagingBuffer, 1, &colorCopy);

    // Copy depth image to staging buffer
    VkBufferImageCopy depthCopy{};
    depthCopy.bufferOffset = 0;
    depthCopy.bufferRowLength = 0;
    depthCopy.bufferImageHeight = 0;
    depthCopy.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
    depthCopy.imageOffset = {0, 0, 0};
    depthCopy.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyImageToBuffer(cmd, m_DepthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            m_DofDepthStagingBuffer, 1, &depthCopy);

    // --- Step 3: Submit and wait (synchronous CPU-side processing) ---
    // Note: In production, this would use timeline semaphores or async compute.
    // For the CPU-side fallback, we must submit the command buffer, wait, and
    // process on CPU. Since we are within a recorded command buffer, we cannot
    // do synchronous readback inline here. Instead, we record the barriers and
    // copies, and the actual CPU processing happens after the command buffer
    // is submitted. This requires a fence-based approach.
    //
    // To avoid restructuring the entire rendering pipeline, we use a
    // host-visible coherent staging buffer that we fence-wait on.

    // Insert a buffer memory barrier so the copy is visible on the host
    VkBufferMemoryBarrier bufBarriers[2]{};
    bufBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarriers[0].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarriers[0].buffer = m_DofColorStagingBuffer;
    bufBarriers[0].offset = 0;
    bufBarriers[0].size = m_DofColorStagingSize;

    bufBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufBarriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarriers[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarriers[1].buffer = m_DofDepthStagingBuffer;
    bufBarriers[1].offset = 0;
    bufBarriers[1].size = m_DofDepthStagingSize;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 2, bufBarriers, 0, nullptr);

    // --- Step 4: Transition images back ---
    // Color: TRANSFER_SRC -> TRANSFER_DST (so we can write back the blurred result)
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    // Depth: TRANSFER_SRC -> DEPTH_STENCIL_ATTACHMENT (restore)
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);

    // --- Step 5: Map staging buffers and process on CPU ---
    // Map color data (RGBA16F = half-float, 2 bytes per component, 4 components)
    void* colorData = nullptr;
    void* depthData = nullptr;
    vkMapMemory(device, m_DofColorStagingMemory, 0, m_DofColorStagingSize, 0, &colorData);
    vkMapMemory(device, m_DofDepthStagingMemory, 0, m_DofDepthStagingSize, 0, &depthData);

    if (!colorData || !depthData) {
        if (colorData) vkUnmapMemory(device, m_DofColorStagingMemory);
        if (depthData) vkUnmapMemory(device, m_DofDepthStagingMemory);
        return;
    }

    usize pixelCount = static_cast<usize>(m_Width) * m_Height;

    // Convert RGBA16F to f32 for processing
    const u16* halfPixels = static_cast<const u16*>(colorData);
    const f32* depthPixels = static_cast<const f32*>(depthData);

    // Half-float to float conversion (IEEE 754 half-precision)
    auto halfToFloat = [](u16 h) -> f32 {
        u32 sign = (h >> 15) & 0x1;
        u32 expo = (h >> 10) & 0x1F;
        u32 mant = h & 0x3FF;
        if (expo == 0) {
            if (mant == 0) return sign ? -0.0f : 0.0f;
            // Denormalized
            f32 m = static_cast<f32>(mant) / 1024.0f;
            f32 val = m * (1.0f / 16384.0f);  // 2^(-14)
            return sign ? -val : val;
        }
        if (expo == 31) {
            return (mant == 0) ? (sign ? -INFINITY : INFINITY) : NAN;
        }
        f32 val = std::ldexp(1.0f + static_cast<f32>(mant) / 1024.0f, static_cast<int>(expo) - 15);
        return sign ? -val : val;
    };

    // Float to half-float conversion
    auto floatToHalf = [](f32 f) -> u16 {
        u32 fi;
        std::memcpy(&fi, &f, sizeof(f32));
        u32 sign = (fi >> 31) & 0x1;
        i32 expo = static_cast<i32>((fi >> 23) & 0xFF) - 127 + 15;
        u32 mant = fi & 0x7FFFFF;
        if (expo <= 0) return static_cast<u16>(sign << 15);
        if (expo >= 31) return static_cast<u16>((sign << 15) | (31 << 10));
        return static_cast<u16>((sign << 15) | (expo << 10) | (mant >> 13));
    };

    // Convert to float arrays
    std::vector<f32> colorF32(pixelCount * 4);
    std::vector<f32> cocMap(pixelCount);
    std::vector<f32> blurTemp;

    for (usize i = 0; i < pixelCount; ++i) {
        colorF32[i * 4 + 0] = halfToFloat(halfPixels[i * 4 + 0]);
        colorF32[i * 4 + 1] = halfToFloat(halfPixels[i * 4 + 1]);
        colorF32[i * 4 + 2] = halfToFloat(halfPixels[i * 4 + 2]);
        colorF32[i * 4 + 3] = halfToFloat(halfPixels[i * 4 + 3]);
    }

    // Compute Circle of Confusion per pixel
    f32 focalDist = m_Settings.dofFocalDistance;
    f32 focalRange = m_Settings.dofFocalRange;
    f32 nearStrength = m_Settings.dofNearBlurStrength;
    f32 farStrength = m_Settings.dofFarBlurStrength;

    if (focalRange < 0.001f) focalRange = 0.001f;

    for (usize i = 0; i < pixelCount; ++i) {
        f32 depth = depthPixels[i];
        // Linearize depth (reverse-Z perspective: near=1, far=0)
        // For standard depth: CoC = abs(depth - focalDist) / focalRange
        f32 linearDepth = depth;  // Already linear for D32_SFLOAT with standard projection
        f32 dist = linearDepth - focalDist;
        f32 strength = (dist < 0.0f) ? nearStrength : farStrength;
        f32 coc = std::abs(dist) / focalRange;
        coc = std::min(coc, 1.0f) * strength;
        cocMap[i] = coc;
    }

    // Apply weighted separable blur
    f32 maxBlurRadius = m_Settings.dofBokehSize;
    SeparableWeightedBlur(colorF32, cocMap, m_Width, m_Height, 4, maxBlurRadius, blurTemp);

    // Debug CoC visualization (if enabled)
    if (m_Settings.dofDebugCoC != 0) {
        for (usize i = 0; i < pixelCount; ++i) {
            f32 c = cocMap[i];
            colorF32[i * 4 + 0] = c;   // Red = CoC
            colorF32[i * 4 + 1] = 0.0f;
            colorF32[i * 4 + 2] = 1.0f - c;  // Blue = in-focus
            colorF32[i * 4 + 3] = 1.0f;
        }
    }

    // Convert back to half-float and write to upload buffer
    void* uploadData = nullptr;
    vkMapMemory(device, m_DofUploadMemory, 0, m_DofColorStagingSize, 0, &uploadData);
    if (uploadData) {
        u16* outHalf = static_cast<u16*>(uploadData);
        for (usize i = 0; i < pixelCount * 4; ++i) {
            outHalf[i] = floatToHalf(colorF32[i]);
        }
        vkUnmapMemory(device, m_DofUploadMemory);
    }

    vkUnmapMemory(device, m_DofColorStagingMemory);
    vkUnmapMemory(device, m_DofDepthStagingMemory);

    // --- Step 6: Copy blurred result back to scene image ---
    VkBufferImageCopy uploadCopy{};
    uploadCopy.bufferOffset = 0;
    uploadCopy.bufferRowLength = 0;
    uploadCopy.bufferImageHeight = 0;
    uploadCopy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    uploadCopy.imageOffset = {0, 0, 0};
    uploadCopy.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyBufferToImage(cmd, m_DofUploadBuffer, m_SceneImage,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &uploadCopy);

    // Transition scene image back to COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier finalBarrier{};
    finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    finalBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.image = m_SceneImage;
    finalBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &finalBarrier);
}

// ============================================================================
// TILT-SHIFT (CPU-side fallback)
// ============================================================================
// Similar to DoF but driven by screen-space Y position rather than depth.
// The blur amount is zero within the focus band (centered at focusY with
// width bandWidth) and increases linearly toward the screen edges.

void PostProcessing::ApplyTiltShift(VkCommandBuffer cmd) {
    if (!m_Initialized || !m_Context) return;
    if (m_Settings.tiltShiftEnabled == 0) return;
    if (m_SceneImage == VK_NULL_HANDLE) return;

    // Lazy-create staging buffers (shared with DoF)
    if (!m_DofStagingReady) {
        if (!CreateDofStagingBuffers()) return;
    }

    VkDevice device = m_Context->GetDevice();

    // --- Transition scene image to TRANSFER_SRC ---
    VkImageMemoryBarrier colorBarrier{};
    colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = m_SceneImage;
    colorBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    colorBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    colorBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &colorBarrier);

    // Copy color image to staging
    VkBufferImageCopy colorCopy{};
    colorCopy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    colorCopy.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyImageToBuffer(cmd, m_SceneImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            m_DofColorStagingBuffer, 1, &colorCopy);

    // Buffer barrier for host read
    VkBufferMemoryBarrier bufBarrier{};
    bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.buffer = m_DofColorStagingBuffer;
    bufBarrier.offset = 0;
    bufBarrier.size = m_DofColorStagingSize;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 1, &bufBarrier, 0, nullptr);

    // Transition to TRANSFER_DST for write-back
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    colorBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    colorBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &colorBarrier);

    // --- CPU-side processing ---
    void* colorData = nullptr;
    vkMapMemory(device, m_DofColorStagingMemory, 0, m_DofColorStagingSize, 0, &colorData);
    if (!colorData) return;

    usize pixelCount = static_cast<usize>(m_Width) * m_Height;
    const u16* halfPixels = static_cast<const u16*>(colorData);

    auto halfToFloat = [](u16 h) -> f32 {
        u32 sign = (h >> 15) & 0x1;
        u32 expo = (h >> 10) & 0x1F;
        u32 mant = h & 0x3FF;
        if (expo == 0) {
            if (mant == 0) return sign ? -0.0f : 0.0f;
            f32 m = static_cast<f32>(mant) / 1024.0f;
            f32 val = m * (1.0f / 16384.0f);
            return sign ? -val : val;
        }
        if (expo == 31) return (mant == 0) ? (sign ? -INFINITY : INFINITY) : NAN;
        f32 val = std::ldexp(1.0f + static_cast<f32>(mant) / 1024.0f, static_cast<int>(expo) - 15);
        return sign ? -val : val;
    };

    auto floatToHalf = [](f32 f) -> u16 {
        u32 fi;
        std::memcpy(&fi, &f, sizeof(f32));
        u32 sign = (fi >> 31) & 0x1;
        i32 expo = static_cast<i32>((fi >> 23) & 0xFF) - 127 + 15;
        u32 mant = fi & 0x7FFFFF;
        if (expo <= 0) return static_cast<u16>(sign << 15);
        if (expo >= 31) return static_cast<u16>((sign << 15) | (31 << 10));
        return static_cast<u16>((sign << 15) | (expo << 10) | (mant >> 13));
    };

    // Convert to float
    std::vector<f32> colorF32(pixelCount * 4);
    std::vector<f32> blurMap(pixelCount);
    std::vector<f32> blurTemp;

    for (usize i = 0; i < pixelCount; ++i) {
        colorF32[i * 4 + 0] = halfToFloat(halfPixels[i * 4 + 0]);
        colorF32[i * 4 + 1] = halfToFloat(halfPixels[i * 4 + 1]);
        colorF32[i * 4 + 2] = halfToFloat(halfPixels[i * 4 + 2]);
        colorF32[i * 4 + 3] = halfToFloat(halfPixels[i * 4 + 3]);
    }

    // Compute tilt-shift blur map based on screen-space Y
    f32 focusY = m_Settings.tiltShiftFocusY;
    f32 bandWidth = m_Settings.tiltShiftBandWidth;
    f32 halfBand = bandWidth * 0.5f;

    for (u32 y = 0; y < m_Height; ++y) {
        f32 normY = static_cast<f32>(y) / static_cast<f32>(m_Height);
        f32 distFromFocus = std::abs(normY - focusY);
        f32 blurAmount = 0.0f;
        if (distFromFocus > halfBand) {
            blurAmount = (distFromFocus - halfBand) / (1.0f - halfBand + 0.001f);
            blurAmount = std::min(blurAmount, 1.0f);
        }
        for (u32 x = 0; x < m_Width; ++x) {
            blurMap[static_cast<usize>(y) * m_Width + x] = blurAmount;
        }
    }

    // Apply weighted blur
    f32 maxBlur = m_Settings.tiltShiftBlurAmount;
    SeparableWeightedBlur(colorF32, blurMap, m_Width, m_Height, 4, maxBlur, blurTemp);

    // Convert back and write to upload buffer
    void* uploadData = nullptr;
    vkMapMemory(device, m_DofUploadMemory, 0, m_DofColorStagingSize, 0, &uploadData);
    if (uploadData) {
        u16* outHalf = static_cast<u16*>(uploadData);
        for (usize i = 0; i < pixelCount * 4; ++i) {
            outHalf[i] = floatToHalf(colorF32[i]);
        }
        vkUnmapMemory(device, m_DofUploadMemory);
    }

    vkUnmapMemory(device, m_DofColorStagingMemory);

    // Copy blurred result back to scene image
    VkBufferImageCopy uploadCopy{};
    uploadCopy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    uploadCopy.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyBufferToImage(cmd, m_DofUploadBuffer, m_SceneImage,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &uploadCopy);

    // Transition back to COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier finalBarrier{};
    finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    finalBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    finalBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    finalBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    finalBarrier.image = m_SceneImage;
    finalBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    finalBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &finalBarrier);
}

// ============================================================================
// TAA (Temporal Anti-Aliasing) — Compute Resolve
// ============================================================================

// Push constants matching the shader layout
struct TAAPushConstants {
    f32 resolutionX;
    f32 resolutionY;
    f32 invResolutionX;
    f32 invResolutionY;
    f32 feedbackMin;
    f32 feedbackMax;
    f32 sharpness;
    u32 frameIndex;
};

bool PostProcessing::CreateTAAResources() {
    if (!m_Context || m_Width == 0 || m_Height == 0) return false;

    VkDevice device = m_Context->GetDevice();

    // --- Create two history images (RGBA16F, ping-pong) ---
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageInfo.extent = { m_Width, m_Height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &imageInfo, nullptr, &m_TAAHistoryImages[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "TAA: Failed to create history image %d", i);
            DestroyTAAResources();
            return false;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, m_TAAHistoryImages[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_TAAHistoryMemory[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "TAA: Failed to allocate history memory %d", i);
            DestroyTAAResources();
            return false;
        }

        if (vkBindImageMemory(device, m_TAAHistoryImages[i], m_TAAHistoryMemory[i], 0) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "TAA: Failed to bind history image memory %d", i);
            DestroyTAAResources();
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_TAAHistoryImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        if (vkCreateImageView(device, &viewInfo, nullptr, &m_TAAHistoryViews[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "TAA: Failed to create history view %d", i);
            DestroyTAAResources();
            return false;
        }
    }

    // --- Create sampler for TAA inputs ---
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_TAASampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "TAA: Failed to create sampler");
        DestroyTAAResources();
        return false;
    }

    // --- Create compute pipeline ---
    if (!CreateTAAComputePipeline()) {
        ENJIN_LOG_WARN(Renderer, "TAA: Compute pipeline creation failed (shader may need compilation)");
        DestroyTAAResources();
        return false;
    }

    m_TAACurrentIndex = 0;
    m_TAAFrameIndex = 0;
    m_TAAReady = true;

    ENJIN_LOG_INFO(Renderer, "TAA resources created (%ux%u)", m_Width, m_Height);
    return true;
}

void PostProcessing::DestroyTAAResources() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();

    m_TAAReady = false;

    // Compute pipeline
    if (m_TAAComputePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_TAAComputePipeline, nullptr);
        m_TAAComputePipeline = VK_NULL_HANDLE;
    }
    if (m_TAAComputePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_TAAComputePipelineLayout, nullptr);
        m_TAAComputePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_TAADescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_TAADescriptorPool, nullptr);
        m_TAADescriptorPool = VK_NULL_HANDLE;
        m_TAADescriptorSets[0] = VK_NULL_HANDLE;
        m_TAADescriptorSets[1] = VK_NULL_HANDLE;
    }
    if (m_TAADescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_TAADescriptorSetLayout, nullptr);
        m_TAADescriptorSetLayout = VK_NULL_HANDLE;
    }

    // Sampler
    if (m_TAASampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_TAASampler, nullptr);
        m_TAASampler = VK_NULL_HANDLE;
    }

    // History buffers
    for (int i = 0; i < 2; ++i) {
        if (m_TAAHistoryViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_TAAHistoryViews[i], nullptr);
            m_TAAHistoryViews[i] = VK_NULL_HANDLE;
        }
        if (m_TAAHistoryImages[i] != VK_NULL_HANDLE) {
            vkDestroyImage(device, m_TAAHistoryImages[i], nullptr);
            m_TAAHistoryImages[i] = VK_NULL_HANDLE;
        }
        if (m_TAAHistoryMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device, m_TAAHistoryMemory[i], nullptr);
            m_TAAHistoryMemory[i] = VK_NULL_HANDLE;
        }
    }
}

bool PostProcessing::CreateTAAComputePipeline() {
    if (!m_Context) return false;

    VkDevice device = m_Context->GetDevice();

    // --- Descriptor set layout ---
    // binding 0: sampler2D currentColor
    // binding 1: sampler2D historyColor
    // binding 2: sampler2D velocityBuffer
    // binding 3: sampler2D depthBuffer
    // binding 4: image2D  outputColor (storage image, writeonly)
    VkDescriptorSetLayoutBinding bindings[5]{};
    for (int i = 0; i < 4; ++i) {
        bindings[i].binding = static_cast<u32>(i);
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_TAADescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "TAA: Failed to create descriptor set layout");
        return false;
    }

    // --- Push constant range ---
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(TAAPushConstants);

    // --- Pipeline layout ---
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_TAADescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_TAAComputePipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "TAA: Failed to create pipeline layout");
        return false;
    }

    // --- Load TAA compute shader from embedded SPIR-V ---
    VulkanShader taaShader(m_Context);
    if (!taaShader.LoadFromSPIRV(ShaderData::TAAResolveComputeShaderData,
                                ShaderData::TAAResolveComputeShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "TAA: Failed to load embedded taa_resolve.comp shader");
        return false;
    }

    // --- Compute pipeline ---
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = taaShader.GetModule();
    stage.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_TAAComputePipelineLayout;
    pipelineInfo.stage = stage;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_TAAComputePipeline) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "TAA: Failed to create compute pipeline");
        return false;
    }

    // --- Descriptor pool (2 sets for ping-pong, 4 samplers + 1 storage image each) ---
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 8;   // 4 per set * 2 sets
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 2;   // 1 per set * 2 sets

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 2;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_TAADescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "TAA: Failed to create descriptor pool");
        return false;
    }

    // --- Allocate 2 descriptor sets ---
    VkDescriptorSetLayout layouts[2] = { m_TAADescriptorSetLayout, m_TAADescriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_TAADescriptorPool;
    allocInfo.descriptorSetCount = 2;
    allocInfo.pSetLayouts = layouts;

    if (vkAllocateDescriptorSets(device, &allocInfo, m_TAADescriptorSets) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "TAA: Failed to allocate descriptor sets");
        return false;
    }

    // Descriptor sets are written dynamically each frame in ApplyTAA() because the
    // velocity/depth views and ping-pong indices change every frame.

    ENJIN_LOG_INFO(Renderer, "TAA compute pipeline created");
    return true;
}

VkImageView PostProcessing::GetTAAOutputImageView() const {
    if (!m_TAAReady) return VK_NULL_HANDLE;
    // After ApplyTAA(), m_TAACurrentIndex points to the buffer that will be written
    // *next* frame.  The most recent output is in the other buffer.
    return m_TAAHistoryViews[1 - m_TAACurrentIndex];
}

void PostProcessing::ApplyTAA(VkCommandBuffer cmd) {
    if (!m_TAAReady || m_Settings.aaMode != 2) return;
    if (m_TAAComputePipeline == VK_NULL_HANDLE) return;
    if (m_SceneImageView == VK_NULL_HANDLE) return;
    if (m_TAAVelocityView == VK_NULL_HANDLE || m_TAADepthView == VK_NULL_HANDLE) return;

    VkDevice device = m_Context->GetDevice();

    // Determine ping-pong indices
    u32 outputIdx = m_TAACurrentIndex;         // Write to this one
    u32 historyIdx = 1 - m_TAACurrentIndex;    // Read from this one

    // --- Transition images for compute ---
    // Scene image: COLOR_ATTACHMENT_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    // History (read): GENERAL or UNDEFINED -> SHADER_READ_ONLY_OPTIMAL
    // Output: GENERAL or UNDEFINED -> GENERAL (for storage image writes)

    VkImageMemoryBarrier preBarriers[3]{};
    u32 barrierCount = 0;

    // Scene image -> shader read
    preBarriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    preBarriers[barrierCount].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    preBarriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    preBarriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[barrierCount].image = m_SceneImage;
    preBarriers[barrierCount].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    preBarriers[barrierCount].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    preBarriers[barrierCount].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrierCount++;

    // History read buffer -> shader read (might be UNDEFINED on first frame)
    preBarriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    preBarriers[barrierCount].oldLayout = (m_TAAFrameIndex == 0) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    preBarriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    preBarriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[barrierCount].image = m_TAAHistoryImages[historyIdx];
    preBarriers[barrierCount].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    preBarriers[barrierCount].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    preBarriers[barrierCount].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrierCount++;

    // Output buffer -> general for storage writes
    preBarriers[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    preBarriers[barrierCount].oldLayout = (m_TAAFrameIndex == 0) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    preBarriers[barrierCount].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    preBarriers[barrierCount].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[barrierCount].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    preBarriers[barrierCount].image = m_TAAHistoryImages[outputIdx];
    preBarriers[barrierCount].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    preBarriers[barrierCount].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarriers[barrierCount].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrierCount++;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, barrierCount, preBarriers);

    // --- Update descriptor set for this frame ---
    VkDescriptorSet activeSet = m_TAADescriptorSets[outputIdx];

    VkDescriptorImageInfo currentColorInfo{};
    currentColorInfo.sampler = m_TAASampler;
    currentColorInfo.imageView = m_SceneImageView;
    currentColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo historyColorInfo{};
    historyColorInfo.sampler = m_TAASampler;
    historyColorInfo.imageView = m_TAAHistoryViews[historyIdx];
    historyColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo velocityInfo{};
    velocityInfo.sampler = m_TAASampler;
    velocityInfo.imageView = m_TAAVelocityView;
    velocityInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler = m_TAASampler;
    depthInfo.imageView = m_TAADepthView;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = m_TAAHistoryViews[outputIdx];
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[5]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = activeSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &currentColorInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = activeSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &historyColorInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = activeSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &velocityInfo;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = activeSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &depthInfo;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = activeSet;
    writes[4].dstBinding = 4;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);

    // --- Push constants ---
    TAAPushConstants pc{};
    pc.resolutionX = static_cast<f32>(m_Width);
    pc.resolutionY = static_cast<f32>(m_Height);
    pc.invResolutionX = 1.0f / pc.resolutionX;
    pc.invResolutionY = 1.0f / pc.resolutionY;
    pc.feedbackMin = m_Settings.taaFeedbackMin;
    pc.feedbackMax = m_Settings.taaFeedbackMax;
    pc.sharpness = m_Settings.taaSharpness;
    pc.frameIndex = m_TAAFrameIndex;

    // --- Bind pipeline and dispatch ---
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_TAAComputePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_TAAComputePipelineLayout,
                             0, 1, &activeSet, 0, nullptr);
    vkCmdPushConstants(cmd, m_TAAComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(TAAPushConstants), &pc);

    // Dispatch: workgroup size is 8x8 in the shader
    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // --- Post-dispatch barriers ---
    // Output image: GENERAL -> SHADER_READ_ONLY_OPTIMAL (for post-process fragment shader to sample)
    // Scene image: SHADER_READ_ONLY_OPTIMAL -> COLOR_ATTACHMENT_OPTIMAL (restore for next frame)
    VkImageMemoryBarrier postBarriers[2]{};

    postBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    postBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    postBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    postBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[0].image = m_TAAHistoryImages[outputIdx];
    postBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    postBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    postBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    postBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    postBarriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    postBarriers[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    postBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    postBarriers[1].image = m_SceneImage;
    postBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    postBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarriers[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 2, postBarriers);

    // Advance frame counter and swap ping-pong for next frame.
    // After the swap, m_TAACurrentIndex points to the buffer that will be *written*
    // next frame.  The buffer we just wrote (outputIdx) is the "previous" history
    // buffer that GetTAAOutputImageView() should return.
    m_TAAFrameIndex++;
    m_TAACurrentIndex = 1 - outputIdx;  // Next frame writes to the other buffer
}

} // namespace Renderer
} // namespace Enjin
