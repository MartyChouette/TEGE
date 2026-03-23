#include "Enjin/Renderer/Upscaling/FSR2Upscaler.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

// ============================================================================
// PUSH CONSTANT STRUCTS (must match shader layouts)
// ============================================================================

struct LanczosPushConstants {
    u32 outputWidth;
    u32 outputHeight;
    u32 inputWidth;
    u32 inputHeight;
    f32 sharpness;
    f32 _pad0;
    f32 _pad1;
    f32 _pad2;
};
static_assert(sizeof(LanczosPushConstants) == 32, "Lanczos push constants must be 32 bytes");

struct CASPushConstants {
    u32 imageWidth;
    u32 imageHeight;
    f32 sharpness;
    f32 _pad0;
};
static_assert(sizeof(CASPushConstants) == 16, "CAS push constants must be 16 bytes");

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

FSR2Upscaler::FSR2Upscaler(VulkanContext* context)
    : m_Context(context) {}

FSR2Upscaler::~FSR2Upscaler() {
    Shutdown();
}

// ============================================================================
// INITIALIZE
// ============================================================================

bool FSR2Upscaler::Initialize(u32 renderWidth, u32 renderHeight,
                              u32 displayWidth, u32 displayHeight,
                              UpscalerQuality quality) {
    if (m_Initialized) return true;
    if (!m_Context) return false;

    m_RenderWidth = renderWidth;
    m_RenderHeight = renderHeight;
    m_DisplayWidth = displayWidth;
    m_DisplayHeight = displayHeight;

    // Create intermediate and output images at display resolution
    if (!CreateImages()) {
        ENJIN_LOG_WARN(Renderer, "FSR 2 (Built-in): Failed to create images");
        Shutdown();
        return false;
    }

    // Create Lanczos and CAS compute pipelines
    if (!CreateComputePipelines()) {
        ENJIN_LOG_WARN(Renderer, "FSR 2 (Built-in): Failed to create compute pipelines");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    m_HistoryReset = true;
    ENJIN_LOG_INFO(Renderer, "FSR 2 (Built-in) upscaler initialized (%ux%u -> %ux%u)",
                   renderWidth, renderHeight, displayWidth, displayHeight);
    return true;
}

// ============================================================================
// RESIZE
// ============================================================================

void FSR2Upscaler::Resize(u32 renderWidth, u32 renderHeight,
                          u32 displayWidth, u32 displayHeight) {
    if (renderWidth == m_RenderWidth && renderHeight == m_RenderHeight &&
        displayWidth == m_DisplayWidth && displayHeight == m_DisplayHeight) return;

    UpscalerQuality quality = UpscalerQuality::Quality;  // Preserve current quality
    Shutdown();
    Initialize(renderWidth, renderHeight, displayWidth, displayHeight, quality);
}

// ============================================================================
// DISPATCH
// ============================================================================

void FSR2Upscaler::Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) {
    if (!m_Initialized) return;

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;

    // Step 1: Spatial upscale (low-res input -> display-res intermediate)
    // Prefer EASU (edge-adaptive) over Lanczos when available
    if (m_EASUPipeline) {
        DispatchEASU(cmd, input.colorInput);
    } else {
        DispatchLanczos(cmd, input.colorInput);
    }

    // Barrier: spatial upscale write -> temporal/sharpening read
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Step 2: Temporal accumulation (intermediate + history -> intermediate)
    // Only if temporal pipeline is available, velocity input is provided,
    // and this is not a camera cut / first frame.
    bool doTemporal = (m_TemporalPipeline != VK_NULL_HANDLE &&
                       input.velocityInput != VK_NULL_HANDLE &&
                       !input.cameraCut && !m_HistoryReset);

    if (doTemporal) {
        DispatchTemporal(cmd, m_IntermediateImageView,
                         input.velocityInput, input.depthInput);

        // Barrier: temporal write -> sharpening read
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    // Step 3: Sharpening (intermediate -> output)
    // Prefer RCAS (robust) over CAS (basic) when available
    f32 sharpness = input.sharpness > 0.0f ? input.sharpness : 0.2f;
    if (m_RCASPipeline) {
        DispatchRCAS(cmd, sharpness);
    } else {
        DispatchCAS(cmd, sharpness);
    }

    // Barrier: sharpening write -> subsequent reads (post-processing)
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    m_HistoryReset = false;
    m_HasHistory = true;
}

// ============================================================================
// DISPATCH LANCZOS
// ============================================================================

void FSR2Upscaler::DispatchLanczos(VkCommandBuffer cmd, VkImageView inputView) {
    // Update descriptor set: bind input image to binding 0
    VkDescriptorImageInfo inputInfo{};
    inputInfo.sampler = m_LinearSampler;
    inputInfo.imageView = inputView;
    inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = m_IntermediateImageView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};

    // Binding 0: input (sampled image)
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_LanczosDescSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &inputInfo;

    // Binding 1: output (storage image)
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_LanczosDescSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 2, writes, 0, nullptr);

    // Bind pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_LanczosPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_LanczosPipelineLayout, 0, 1, &m_LanczosDescSet, 0, nullptr);

    // Push constants
    LanczosPushConstants pc{};
    pc.outputWidth = m_DisplayWidth;
    pc.outputHeight = m_DisplayHeight;
    pc.inputWidth = m_RenderWidth;
    pc.inputHeight = m_RenderHeight;
    pc.sharpness = 0.0f;  // Pure Lanczos for the upscale pass; CAS handles sharpening
    vkCmdPushConstants(cmd, m_LanczosPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(LanczosPushConstants), &pc);

    // Dispatch
    u32 groupsX = (m_DisplayWidth + 7) / 8;
    u32 groupsY = (m_DisplayHeight + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);
}

// ============================================================================
// DISPATCH CAS
// ============================================================================

void FSR2Upscaler::DispatchCAS(VkCommandBuffer cmd, f32 sharpness) {
    // Update descriptor set: bind intermediate image as input, output image as output
    VkDescriptorImageInfo inputInfo{};
    inputInfo.sampler = m_LinearSampler;
    inputInfo.imageView = m_IntermediateImageView;
    inputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = m_OutputImageView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_CASDescSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &inputInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_CASDescSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 2, writes, 0, nullptr);

    // Bind pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_CASPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_CASPipelineLayout, 0, 1, &m_CASDescSet, 0, nullptr);

    // Push constants
    CASPushConstants pc{};
    pc.imageWidth = m_DisplayWidth;
    pc.imageHeight = m_DisplayHeight;
    pc.sharpness = sharpness;
    vkCmdPushConstants(cmd, m_CASPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(CASPushConstants), &pc);

    // Dispatch
    u32 groupsX = (m_DisplayWidth + 7) / 8;
    u32 groupsY = (m_DisplayHeight + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);
}

// ============================================================================
// DISPATCH EASU (Edge Adaptive Spatial Upscale)
// ============================================================================

void FSR2Upscaler::DispatchEASU(VkCommandBuffer cmd, VkImageView inputView) {
    // Update descriptor set: bind input image to binding 0
    VkDescriptorImageInfo inputInfo{};
    inputInfo.sampler = m_LinearSampler;
    inputInfo.imageView = inputView;
    inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = m_IntermediateImageView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_EASUDescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &inputInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_EASUDescSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 2, writes, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_EASUPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_EASUPipelineLayout, 0, 1, &m_EASUDescSet, 0, nullptr);

    // Same push constants as Lanczos (same layout)
    LanczosPushConstants pc{};
    pc.outputWidth = m_DisplayWidth;
    pc.outputHeight = m_DisplayHeight;
    pc.inputWidth = m_RenderWidth;
    pc.inputHeight = m_RenderHeight;
    pc.sharpness = 0.0f;
    vkCmdPushConstants(cmd, m_EASUPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(LanczosPushConstants), &pc);

    u32 groupsX = (m_DisplayWidth + 7) / 8;
    u32 groupsY = (m_DisplayHeight + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);
}

// ============================================================================
// DISPATCH RCAS (Robust Contrast Adaptive Sharpening)
// ============================================================================

void FSR2Upscaler::DispatchRCAS(VkCommandBuffer cmd, f32 sharpness) {
    VkDescriptorImageInfo inputInfo{};
    inputInfo.sampler = m_LinearSampler;
    inputInfo.imageView = m_IntermediateImageView;
    inputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = m_OutputImageView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_RCASDescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &inputInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_RCASDescSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 2, writes, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RCASPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_RCASPipelineLayout, 0, 1, &m_RCASDescSet, 0, nullptr);

    // RCAS push constants: imageSize(uvec2) + sharpness(f32) + denoise(f32)
    struct RCASPushConstants {
        u32 imageWidth;
        u32 imageHeight;
        f32 sharpness;
        f32 denoise;
    };

    RCASPushConstants pc{};
    pc.imageWidth = m_DisplayWidth;
    pc.imageHeight = m_DisplayHeight;
    pc.sharpness = sharpness;
    pc.denoise = 0.0f;  // No denoising by default
    vkCmdPushConstants(cmd, m_RCASPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(RCASPushConstants), &pc);

    u32 groupsX = (m_DisplayWidth + 7) / 8;
    u32 groupsY = (m_DisplayHeight + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);
}

// ============================================================================
// DISPATCH TEMPORAL (Motion-Vector-Aware Temporal Accumulation)
// ============================================================================

void FSR2Upscaler::DispatchTemporal(VkCommandBuffer cmd, VkImageView currentView,
                                     VkImageView velocityView, VkImageView depthView) {
    if (!m_TemporalPipeline || !m_HasHistory) return;

    // Previous history is the buffer we read from; current is the buffer we write to.
    // After dispatch, swap for next frame.
    u32 readIdx = 1 - m_CurrentHistory;
    u32 writeIdx = m_CurrentHistory;

    VkDescriptorImageInfo inputInfos[4]{};
    // Binding 0: current upscaled color (intermediate image)
    inputInfos[0].sampler = m_LinearSampler;
    inputInfos[0].imageView = currentView;
    inputInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Binding 1: history (previous frame's accumulated result)
    inputInfos[1].sampler = m_LinearSampler;
    inputInfos[1].imageView = m_HistoryViews[readIdx];
    inputInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Binding 2: velocity texture
    inputInfos[2].sampler = m_LinearSampler;
    inputInfos[2].imageView = velocityView;
    inputInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Binding 3: depth texture
    inputInfos[3].sampler = m_LinearSampler;
    inputInfos[3].imageView = depthView;
    inputInfos[3].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = m_HistoryViews[writeIdx];
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[5]{};
    for (u32 i = 0; i < 4; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_TemporalDescSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &inputInfos[i];
    }

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = m_TemporalDescSet;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[4].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 5, writes, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_TemporalPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_TemporalPipelineLayout, 0, 1, &m_TemporalDescSet, 0, nullptr);

    // Push constants for temporal accumulation
    struct TemporalPushConstants {
        u32 displayWidth;
        u32 displayHeight;
        u32 renderWidth;
        u32 renderHeight;
        f32 feedbackMin;
        f32 feedbackMax;
        f32 sharpness;
        u32 flags;
    };

    TemporalPushConstants pc{};
    pc.displayWidth = m_DisplayWidth;
    pc.displayHeight = m_DisplayHeight;
    pc.renderWidth = m_RenderWidth;
    pc.renderHeight = m_RenderHeight;
    pc.feedbackMin = 0.88f;
    pc.feedbackMax = 0.97f;
    pc.sharpness = 0.1f;
    pc.flags = (m_HasHistory ? 1u : 0u);  // bit 0 = hasHistory

    vkCmdPushConstants(cmd, m_TemporalPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(TemporalPushConstants), &pc);

    u32 groupsX = (m_DisplayWidth + 7) / 8;
    u32 groupsY = (m_DisplayHeight + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // After temporal pass, copy the accumulated result back to the intermediate
    // image so the sharpening pass reads the temporally accumulated data.
    // Use a memory barrier + blit.
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Copy history write buffer -> intermediate for sharpening
    VkImageCopy region{};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;
    region.extent = { m_DisplayWidth, m_DisplayHeight, 1 };

    vkCmdCopyImage(cmd,
        m_HistoryImages[writeIdx], VK_IMAGE_LAYOUT_GENERAL,
        m_IntermediateImage, VK_IMAGE_LAYOUT_GENERAL,
        1, &region);

    // Barrier: copy write -> compute read
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Swap history buffers for next frame
    m_CurrentHistory = 1 - m_CurrentHistory;
}

// ============================================================================
// RESET HISTORY
// ============================================================================

void FSR2Upscaler::ResetHistory() {
    m_HistoryReset = true;
    m_HasHistory = false;
}

// ============================================================================
// SHUTDOWN
// ============================================================================

void FSR2Upscaler::Shutdown() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    DestroyPipelines();
    DestroyImages();

    m_Initialized = false;
    m_RenderWidth = 0;
    m_RenderHeight = 0;
    m_DisplayWidth = 0;
    m_DisplayHeight = 0;
}

// ============================================================================
// CREATE IMAGES (intermediate + output at display resolution)
// ============================================================================

bool FSR2Upscaler::CreateImages() {
    VkDevice device = m_Context->GetDevice();

    // Helper lambda to create a RGBA16F image + memory + view
    auto createImage = [&](VkImage& image, VkDeviceMemory& memory, VkImageView& view) -> bool {
        // Image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageInfo.extent = { m_DisplayWidth, m_DisplayHeight, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                          VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyImage(device, image, nullptr);
            image = VK_NULL_HANDLE;
            return false;
        }

        vkBindImageMemory(device, image, memory, 0);
        m_Context->TrackAllocation(memReqs.size);

        // Image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
            vkFreeMemory(device, memory, nullptr);
            vkDestroyImage(device, image, nullptr);
            image = VK_NULL_HANDLE;
            memory = VK_NULL_HANDLE;
            return false;
        }

        return true;
    };

    if (!createImage(m_IntermediateImage, m_IntermediateMemory, m_IntermediateImageView)) {
        ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create intermediate image");
        return false;
    }

    if (!createImage(m_OutputImage, m_OutputMemory, m_OutputImageView)) {
        ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create output image");
        return false;
    }

    // Create temporal history ping-pong images (display resolution)
    for (u32 i = 0; i < 2; ++i) {
        if (!createImage(m_HistoryImages[i], m_HistoryMemory[i], m_HistoryViews[i])) {
            ENJIN_LOG_WARN(Renderer, "FSR 2: Failed to create history image %u — temporal disabled", i);
            // Non-fatal: temporal accumulation is optional
            break;
        }
    }

    // Create linear sampler for texture reads
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_LinearSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create sampler");
        return false;
    }

    // Transition both images to GENERAL layout for storage image use
    // Use a one-shot command buffer for the initial layout transition
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create temp command pool");
        return false;
    }

    VkCommandBufferAllocateInfo allocCmdInfo{};
    allocCmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocCmdInfo.commandPool = cmdPool;
    allocCmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocCmdInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocCmdInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Count how many images need layout transitions (2 base + 0-2 history)
    u32 barrierCount = 2;
    if (m_HistoryImages[0] != VK_NULL_HANDLE) barrierCount++;
    if (m_HistoryImages[1] != VK_NULL_HANDLE) barrierCount++;

    VkImageMemoryBarrier barriers[4]{};
    for (u32 i = 0; i < barrierCount; i++) {
        barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[i].subresourceRange.baseMipLevel = 0;
        barriers[i].subresourceRange.levelCount = 1;
        barriers[i].subresourceRange.baseArrayLayer = 0;
        barriers[i].subresourceRange.layerCount = 1;
        barriers[i].srcAccessMask = 0;
        barriers[i].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    }
    barriers[0].image = m_IntermediateImage;
    barriers[1].image = m_OutputImage;
    if (m_HistoryImages[0] != VK_NULL_HANDLE) barriers[2].image = m_HistoryImages[0];
    if (m_HistoryImages[1] != VK_NULL_HANDLE) barriers[3].image = m_HistoryImages[1];

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, barrierCount, barriers);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyCommandPool(device, cmdPool, nullptr);

    ENJIN_LOG_INFO(Renderer, "FSR 2: Created upscaler images (%ux%u)", m_DisplayWidth, m_DisplayHeight);
    return true;
}

void FSR2Upscaler::DestroyImages() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_LinearSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_LinearSampler, nullptr);
        m_LinearSampler = VK_NULL_HANDLE;
    }

    auto destroyImage = [&](VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
        if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); view = VK_NULL_HANDLE; }
        if (image != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr); image = VK_NULL_HANDLE; }
        if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); memory = VK_NULL_HANDLE; }
    };

    destroyImage(m_IntermediateImage, m_IntermediateMemory, m_IntermediateImageView);
    destroyImage(m_OutputImage, m_OutputMemory, m_OutputImageView);

    // Destroy temporal history images
    for (u32 i = 0; i < 2; ++i) {
        destroyImage(m_HistoryImages[i], m_HistoryMemory[i], m_HistoryViews[i]);
    }
}

// ============================================================================
// CREATE COMPUTE PIPELINES (Lanczos + CAS)
// ============================================================================

bool FSR2Upscaler::CreateComputePipelines() {
    VkDevice device = m_Context->GetDevice();

    // --- Helper: create a compute pipeline with 2 bindings (sampler + storage image) ---
    auto createPipeline = [&](
        const char* shaderName,
        const char* shaderPaths[],
        u32 pathCount,
        u32 pushConstantSize,
        VkDescriptorSetLayout& descSetLayout,
        VkDescriptorPool& descPool,
        VkDescriptorSet& descSet,
        VkPipelineLayout& pipelineLayout,
        VkPipeline& pipeline
    ) -> bool {
        // Descriptor set layout: binding 0 = sampler, binding 1 = storage image
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create %s descriptor set layout", shaderName);
            return false;
        }

        // Descriptor pool
        VkDescriptorPoolSize poolSizes[2]{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.maxSets = 1;
        poolCreateInfo.poolSizeCount = 2;
        poolCreateInfo.pPoolSizes = poolSizes;

        if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descPool) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create %s descriptor pool", shaderName);
            return false;
        }

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descSet) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to allocate %s descriptor set", shaderName);
            return false;
        }

        // Pipeline layout with push constants
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = pushConstantSize;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create %s pipeline layout", shaderName);
            return false;
        }

        // Load compute shader from SPIR-V
        VulkanShader shader(m_Context);
        bool loaded = false;
        for (u32 i = 0; i < pathCount; i++) {
            if (shader.LoadFromFile(shaderPaths[i]) && shader.GetModule() != VK_NULL_HANDLE) {
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            ENJIN_LOG_WARN(Renderer, "FSR 2: %s shader not found — compile with: "
                           "glslangValidator -V Engine/shaders/%s -o Engine/shaders/%s.spv",
                           shaderName, shaderName, shaderName);
            return false;
        }

        // Compute pipeline
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = shader.GetModule();
        stage.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stage;
        pipelineInfo.layout = pipelineLayout;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "FSR 2: Failed to create %s compute pipeline", shaderName);
            return false;
        }

        return true;
    };

    // --- Lanczos pipeline ---
    const char* lanczosPaths[] = {
        "shaders/upscale_lanczos.comp.spv",
        "Engine/shaders/upscale_lanczos.comp.spv",
        "../Engine/shaders/upscale_lanczos.comp.spv",
        "../../Engine/shaders/upscale_lanczos.comp.spv"
    };

    if (!createPipeline("upscale_lanczos.comp", lanczosPaths, 4,
                         sizeof(LanczosPushConstants),
                         m_LanczosDescSetLayout, m_LanczosDescPool,
                         m_LanczosDescSet, m_LanczosPipelineLayout, m_LanczosPipeline)) {
        return false;
    }

    // --- CAS pipeline ---
    const char* casPaths[] = {
        "shaders/upscale_cas.comp.spv",
        "Engine/shaders/upscale_cas.comp.spv",
        "../Engine/shaders/upscale_cas.comp.spv",
        "../../Engine/shaders/upscale_cas.comp.spv"
    };

    if (!createPipeline("upscale_cas.comp", casPaths, 4,
                         sizeof(CASPushConstants),
                         m_CASDescSetLayout, m_CASDescPool,
                         m_CASDescSet, m_CASPipelineLayout, m_CASPipeline)) {
        return false;
    }

    // --- EASU pipeline (optional — edge-adaptive spatial upscale) ---
    const char* easuPaths[] = {
        "shaders/upscale_easu.comp.spv",
        "Engine/shaders/upscale_easu.comp.spv",
        "../Engine/shaders/upscale_easu.comp.spv",
        "../../Engine/shaders/upscale_easu.comp.spv"
    };

    if (createPipeline("upscale_easu.comp", easuPaths, 4,
                         sizeof(LanczosPushConstants),
                         m_EASUDescSetLayout, m_EASUDescPool,
                         m_EASUDescSet, m_EASUPipelineLayout, m_EASUPipeline)) {
        ENJIN_LOG_INFO(Renderer, "FSR 2: EASU (edge-adaptive upscale) pipeline created");
    } else {
        ENJIN_LOG_INFO(Renderer, "FSR 2: EASU shader not found, falling back to Lanczos");
    }

    // --- RCAS pipeline (optional — robust contrast adaptive sharpening) ---
    const char* rcasPaths[] = {
        "shaders/upscale_rcas.comp.spv",
        "Engine/shaders/upscale_rcas.comp.spv",
        "../Engine/shaders/upscale_rcas.comp.spv",
        "../../Engine/shaders/upscale_rcas.comp.spv"
    };

    if (createPipeline("upscale_rcas.comp", rcasPaths, 4,
                         sizeof(CASPushConstants),
                         m_RCASDescSetLayout, m_RCASDescPool,
                         m_RCASDescSet, m_RCASPipelineLayout, m_RCASPipeline)) {
        ENJIN_LOG_INFO(Renderer, "FSR 2: RCAS (robust sharpening) pipeline created");
    } else {
        ENJIN_LOG_INFO(Renderer, "FSR 2: RCAS shader not found, falling back to CAS");
    }

    // --- Temporal accumulation pipeline (optional — motion-vector history blend) ---
    // This uses a 5-binding descriptor set (4 samplers + 1 storage image) and
    // 32 bytes of push constants, so we create it manually (not via createPipeline).
    {
        const char* temporalPaths[] = {
            "shaders/upscale_temporal_accumulate.comp.spv",
            "Engine/shaders/upscale_temporal_accumulate.comp.spv",
            "../Engine/shaders/upscale_temporal_accumulate.comp.spv",
            "../../Engine/shaders/upscale_temporal_accumulate.comp.spv"
        };

        // 5-binding descriptor set: 4 combined image samplers + 1 storage image
        VkDescriptorSetLayoutBinding tempBindings[5]{};
        for (u32 i = 0; i < 4; ++i) {
            tempBindings[i].binding = i;
            tempBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            tempBindings[i].descriptorCount = 1;
            tempBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        tempBindings[4].binding = 4;
        tempBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        tempBindings[4].descriptorCount = 1;
        tempBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo tempLayoutInfo{};
        tempLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        tempLayoutInfo.bindingCount = 5;
        tempLayoutInfo.pBindings = tempBindings;

        bool tempOk = (vkCreateDescriptorSetLayout(device, &tempLayoutInfo, nullptr, &m_TemporalDescSetLayout) == VK_SUCCESS);

        if (tempOk) {
            VkDescriptorPoolSize tempPoolSizes[2]{};
            tempPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            tempPoolSizes[0].descriptorCount = 4;
            tempPoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            tempPoolSizes[1].descriptorCount = 1;

            VkDescriptorPoolCreateInfo tempPoolInfo{};
            tempPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            tempPoolInfo.maxSets = 1;
            tempPoolInfo.poolSizeCount = 2;
            tempPoolInfo.pPoolSizes = tempPoolSizes;
            tempOk = (vkCreateDescriptorPool(device, &tempPoolInfo, nullptr, &m_TemporalDescPool) == VK_SUCCESS);
        }

        if (tempOk) {
            VkDescriptorSetAllocateInfo tempAllocInfo{};
            tempAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            tempAllocInfo.descriptorPool = m_TemporalDescPool;
            tempAllocInfo.descriptorSetCount = 1;
            tempAllocInfo.pSetLayouts = &m_TemporalDescSetLayout;
            tempOk = (vkAllocateDescriptorSets(device, &tempAllocInfo, &m_TemporalDescSet) == VK_SUCCESS);
        }

        if (tempOk) {
            VkPushConstantRange tempPCRange{};
            tempPCRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            tempPCRange.offset = 0;
            tempPCRange.size = 32;  // 4 u32s + 3 f32s + 1 u32

            VkPipelineLayoutCreateInfo tempPipeLayoutInfo{};
            tempPipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            tempPipeLayoutInfo.setLayoutCount = 1;
            tempPipeLayoutInfo.pSetLayouts = &m_TemporalDescSetLayout;
            tempPipeLayoutInfo.pushConstantRangeCount = 1;
            tempPipeLayoutInfo.pPushConstantRanges = &tempPCRange;
            tempOk = (vkCreatePipelineLayout(device, &tempPipeLayoutInfo, nullptr, &m_TemporalPipelineLayout) == VK_SUCCESS);
        }

        if (tempOk) {
            VulkanShader shader(m_Context);
            bool loaded = false;
            for (u32 i = 0; i < 4; i++) {
                if (shader.LoadFromFile(temporalPaths[i]) && shader.GetModule() != VK_NULL_HANDLE) {
                    loaded = true;
                    break;
                }
            }

            if (loaded) {
                VkPipelineShaderStageCreateInfo stage{};
                stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                stage.module = shader.GetModule();
                stage.pName = "main";

                VkComputePipelineCreateInfo pipeInfo{};
                pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                pipeInfo.stage = stage;
                pipeInfo.layout = m_TemporalPipelineLayout;

                tempOk = (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_TemporalPipeline) == VK_SUCCESS);
            } else {
                tempOk = false;
            }
        }

        if (tempOk) {
            ENJIN_LOG_INFO(Renderer, "FSR 2: Temporal accumulation pipeline created");
        } else {
            ENJIN_LOG_INFO(Renderer, "FSR 2: Temporal accumulation shader not found, temporal reuse disabled");
            // Clean up partial temporal resources
            if (m_TemporalDescPool) { vkDestroyDescriptorPool(device, m_TemporalDescPool, nullptr); m_TemporalDescPool = VK_NULL_HANDLE; }
            if (m_TemporalDescSetLayout) { vkDestroyDescriptorSetLayout(device, m_TemporalDescSetLayout, nullptr); m_TemporalDescSetLayout = VK_NULL_HANDLE; }
            if (m_TemporalPipelineLayout) { vkDestroyPipelineLayout(device, m_TemporalPipelineLayout, nullptr); m_TemporalPipelineLayout = VK_NULL_HANDLE; }
            m_TemporalDescSet = VK_NULL_HANDLE;
            m_TemporalPipeline = VK_NULL_HANDLE;
        }
    }

    ENJIN_LOG_INFO(Renderer, "FSR 2: Compute pipelines created (Lanczos + CAS%s%s%s)",
                   m_EASUPipeline ? " + EASU" : "",
                   m_RCASPipeline ? " + RCAS" : "",
                   m_TemporalPipeline ? " + Temporal" : "");
    return true;
}

void FSR2Upscaler::DestroyPipelines() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    auto destroyPipeline = [&](VkPipeline& pipeline, VkPipelineLayout& layout,
                                VkDescriptorSetLayout& descLayout, VkDescriptorPool& pool,
                                VkDescriptorSet& descSet) {
        if (pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, pipeline, nullptr); pipeline = VK_NULL_HANDLE; }
        if (layout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, layout, nullptr); layout = VK_NULL_HANDLE; }
        if (pool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, pool, nullptr); pool = VK_NULL_HANDLE; }
        if (descLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, descLayout, nullptr); descLayout = VK_NULL_HANDLE; }
        descSet = VK_NULL_HANDLE;
    };

    destroyPipeline(m_LanczosPipeline, m_LanczosPipelineLayout,
                     m_LanczosDescSetLayout, m_LanczosDescPool, m_LanczosDescSet);
    destroyPipeline(m_CASPipeline, m_CASPipelineLayout,
                     m_CASDescSetLayout, m_CASDescPool, m_CASDescSet);
    destroyPipeline(m_EASUPipeline, m_EASUPipelineLayout,
                     m_EASUDescSetLayout, m_EASUDescPool, m_EASUDescSet);
    destroyPipeline(m_RCASPipeline, m_RCASPipelineLayout,
                     m_RCASDescSetLayout, m_RCASDescPool, m_RCASDescSet);
    destroyPipeline(m_TemporalPipeline, m_TemporalPipelineLayout,
                     m_TemporalDescSetLayout, m_TemporalDescPool, m_TemporalDescSet);
}

} // namespace Renderer
} // namespace Enjin
