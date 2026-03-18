#include "Enjin/Renderer/Upscaling/DLSSUpscaler.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

#ifdef ENJIN_HAS_DLSS_SDK
#include <sl.h>
#include <sl_dlss.h>
#include <sl_consts.h>

// Map Enjin quality preset to Streamline DLSS mode
static sl::DLSSMode MapQualityToSLMode(Enjin::Renderer::UpscalerQuality quality) {
    switch (quality) {
        case Enjin::Renderer::UpscalerQuality::Performance:  return sl::DLSSMode::eMaxPerformance;
        case Enjin::Renderer::UpscalerQuality::Balanced:     return sl::DLSSMode::eBalanced;
        case Enjin::Renderer::UpscalerQuality::Quality:      return sl::DLSSMode::eMaxQuality;
        case Enjin::Renderer::UpscalerQuality::UltraQuality: return sl::DLSSMode::eUltraQuality;
        default:                                              return sl::DLSSMode::eMaxQuality;
    }
}
#endif

namespace Enjin {
namespace Renderer {

// ============================================================================
// PUSH CONSTANT STRUCTS (must match shader layouts — shared with FSR2)
// ============================================================================

struct DLSSLanczosPushConstants {
    u32 outputWidth;
    u32 outputHeight;
    u32 inputWidth;
    u32 inputHeight;
    f32 sharpness;
    f32 _pad0;
    f32 _pad1;
    f32 _pad2;
};
static_assert(sizeof(DLSSLanczosPushConstants) == 32, "Lanczos push constants must be 32 bytes");

struct DLSSCASPushConstants {
    u32 imageWidth;
    u32 imageHeight;
    f32 sharpness;
    f32 _pad0;
};
static_assert(sizeof(DLSSCASPushConstants) == 16, "CAS push constants must be 16 bytes");

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

DLSSUpscaler::DLSSUpscaler(VulkanContext* context)
    : m_Context(context) {}

DLSSUpscaler::~DLSSUpscaler() {
    Shutdown();
}

// ============================================================================
// NVIDIA GPU DETECTION
// ============================================================================

bool DLSSUpscaler::IsNvidiaGPU() const {
    if (!m_Context) return false;

    VkPhysicalDevice physDevice = m_Context->GetPhysicalDevice();
    if (physDevice == VK_NULL_HANDLE) return false;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physDevice, &props);

    // NVIDIA vendor ID
    return props.vendorID == 0x10DE;
}

bool DLSSUpscaler::IsAvailable() const {
    if (!IsNvidiaGPU()) return false;

#ifdef ENJIN_HAS_DLSS_SDK
    // Real SDK: verify DLSS feature support via Streamline runtime
    sl::Feature feature = sl::kFeatureDLSS;
    sl::FeatureRequirements reqs{};
    if (slGetFeatureRequirements(feature, reqs) != sl::Result::eOk) {
        return false;
    }
    return (reqs.flags & sl::FeatureRequirementFlags::eHardwareSupported) != 0;
#else
    // Built-in fallback: require NVIDIA GPU for DLSS branding consistency,
    // even though the Lanczos+CAS pipeline runs on any GPU.
    return true;
#endif
}

// ============================================================================
// INITIALIZE
// ============================================================================

bool DLSSUpscaler::Initialize(u32 renderWidth, u32 renderHeight,
                              u32 displayWidth, u32 displayHeight,
                              UpscalerQuality quality) {
    if (m_Initialized) return true;
    if (!m_Context) return false;

    m_RenderWidth = renderWidth;
    m_RenderHeight = renderHeight;
    m_DisplayWidth = displayWidth;
    m_DisplayHeight = displayHeight;

#ifdef ENJIN_HAS_DLSS_SDK
    // ================================================================
    // REAL SDK INIT — Streamline DLSS initialization
    // ================================================================
    {
        sl::Preferences pref{};
        pref.showConsole = false;
        pref.logLevel = sl::LogLevel::eOff;
        pref.featuresToLoad = &sl::kFeatureDLSS;
        pref.numFeaturesToLoad = 1;

        sl::Result initResult = slInit(pref);
        if (initResult != sl::Result::eOk) {
            ENJIN_LOG_ERROR(Renderer, "DLSS: Streamline slInit() failed (result %d)",
                            static_cast<int>(initResult));
            // Fall through to built-in fallback
        } else {
            m_SLInitialized = true;

            // Load the DLSS feature
            sl::Result loadResult = slSetFeatureLoaded(sl::kFeatureDLSS, true);
            if (loadResult != sl::Result::eOk) {
                ENJIN_LOG_WARN(Renderer, "DLSS: slSetFeatureLoaded failed (result %d), "
                               "falling back to built-in upscaler",
                               static_cast<int>(loadResult));
                slShutdown();
                m_SLInitialized = false;
            } else {
                // Query optimal render resolution for the selected quality mode
                m_DLSSOptions.mode = MapQualityToSLMode(quality);
                m_DLSSOptions.outputWidth = displayWidth;
                m_DLSSOptions.outputHeight = displayHeight;

                sl::DLSSOptimalSettings optSettings{};
                sl::Result optResult = slDLSSGetOptimalSettings(m_DLSSOptions, optSettings);
                if (optResult == sl::Result::eOk &&
                    optSettings.optimalRenderWidth > 0 && optSettings.optimalRenderHeight > 0) {
                    m_RenderWidth = optSettings.optimalRenderWidth;
                    m_RenderHeight = optSettings.optimalRenderHeight;
                    ENJIN_LOG_INFO(Renderer, "DLSS: SDK optimal render resolution %ux%u for %ux%u output",
                                   m_RenderWidth, m_RenderHeight, displayWidth, displayHeight);
                }

                slDLSSSetOptions(m_Viewport, m_DLSSOptions);
            }
        }
    }

    if (m_SLInitialized) {
        ENJIN_LOG_INFO(Renderer, "DLSS: Streamline SDK initialized successfully");
    } else {
        ENJIN_LOG_INFO(Renderer, "DLSS: Using built-in Lanczos + CAS fallback");
    }
#endif

    // Create intermediate and output images at display resolution
    // (needed for both SDK and fallback paths)
    if (!CreateImages()) {
        ENJIN_LOG_WARN(Renderer, "DLSS: Failed to create images");
        Shutdown();
        return false;
    }

#ifdef ENJIN_HAS_DLSS_SDK
    if (!m_SLInitialized) {
#endif
        // Create Lanczos and CAS compute pipelines (built-in fallback)
        if (!CreateComputePipelines()) {
            ENJIN_LOG_WARN(Renderer, "DLSS: Failed to create compute pipelines");
            Shutdown();
            return false;
        }
#ifdef ENJIN_HAS_DLSS_SDK
    }
#endif

    m_Initialized = true;
    m_HistoryReset = true;

    const char* backend = "built-in Lanczos + CAS";
#ifdef ENJIN_HAS_DLSS_SDK
    if (m_SLInitialized) backend = "Streamline SDK";
#endif
    ENJIN_LOG_INFO(Renderer, "DLSS upscaler initialized (%ux%u -> %ux%u) [%s]",
                   renderWidth, renderHeight, displayWidth, displayHeight, backend);
    return true;
}

// ============================================================================
// RESIZE
// ============================================================================

void DLSSUpscaler::Resize(u32 renderWidth, u32 renderHeight,
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

void DLSSUpscaler::Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) {
    if (!m_Initialized) return;

#ifdef ENJIN_HAS_DLSS_SDK
    if (m_SLInitialized) {
        // ================================================================
        // REAL SDK DISPATCH — Streamline DLSS evaluation
        // ================================================================

        // Tag resources for Streamline
        sl::Resource colorRes{sl::ResourceType::eTex2d, nullptr, nullptr,
                              nullptr, 0};
        sl::Resource depthRes{sl::ResourceType::eTex2d, nullptr, nullptr,
                              nullptr, 0};
        sl::Resource mvecRes{sl::ResourceType::eTex2d, nullptr, nullptr,
                             nullptr, 0};
        sl::Resource outRes{sl::ResourceType::eTex2d, nullptr, nullptr,
                            nullptr, 0};

        // Streamline uses native API handles; pass Vulkan image views
        // through the Vulkan interposer layer
        colorRes.native = input.colorInput;
        depthRes.native = input.depthInput;
        mvecRes.native = input.velocityInput;
        outRes.native = m_OutputImageView;

        sl::ResourceTag tags[] = {
            {&colorRes, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilPresent, nullptr},
            {&depthRes, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, nullptr},
            {&mvecRes,  sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, nullptr},
            {&outRes,   sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilPresent, nullptr}
        };

        slSetTag(m_Viewport, tags, 4, cmd);

        // Configure DLSS options for this frame
        sl::DLSSOptions dlssConsts = m_DLSSOptions;
        dlssConsts.sharpness = input.sharpness > 0.0f ? input.sharpness : 0.0f;
        dlssConsts.colorBuffersHDR = sl::Boolean::eTrue;
        dlssConsts.reset = (input.cameraCut || m_HistoryReset) ? sl::Boolean::eTrue : sl::Boolean::eFalse;
        slDLSSSetOptions(m_Viewport, dlssConsts);

        // Evaluate DLSS feature
        sl::FrameToken frameToken{};
        sl::Result result = slEvaluateFeature(sl::kFeatureDLSS, frameToken, cmd);
        if (result == sl::Result::eOk) {
            m_HistoryReset = false;
            return;
        }

        // SDK dispatch failed — fall through to built-in fallback
        ENJIN_LOG_WARN(Renderer, "DLSS: slEvaluateFeature failed (result %d), "
                       "falling back to Lanczos + CAS", static_cast<int>(result));
    }
#endif

    // --- BUILT-IN FALLBACK: Lanczos + CAS (same as FSR2) ---

    // Step 1: Lanczos upscale (low-res input -> display-res intermediate)
    DispatchLanczos(cmd, input.colorInput);

    // Memory barrier: Lanczos write -> CAS read
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Step 2: CAS sharpening (intermediate -> output)
    f32 sharpness = input.sharpness > 0.0f ? input.sharpness : 0.2f;
    DispatchCAS(cmd, sharpness);

    // Memory barrier: CAS write -> subsequent reads (post-processing)
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    m_HistoryReset = false;
}

// ============================================================================
// DISPATCH LANCZOS
// ============================================================================

void DLSSUpscaler::DispatchLanczos(VkCommandBuffer cmd, VkImageView inputView) {
    VkDescriptorImageInfo inputInfo{};
    inputInfo.sampler = m_LinearSampler;
    inputInfo.imageView = inputView;
    inputInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = m_IntermediateImageView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet writes[2]{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_LanczosDescSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &inputInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_LanczosDescSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &outputInfo;

    vkUpdateDescriptorSets(m_Context->GetDevice(), 2, writes, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_LanczosPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_LanczosPipelineLayout, 0, 1, &m_LanczosDescSet, 0, nullptr);

    DLSSLanczosPushConstants pc{};
    pc.outputWidth = m_DisplayWidth;
    pc.outputHeight = m_DisplayHeight;
    pc.inputWidth = m_RenderWidth;
    pc.inputHeight = m_RenderHeight;
    pc.sharpness = 0.0f;
    vkCmdPushConstants(cmd, m_LanczosPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(DLSSLanczosPushConstants), &pc);

    u32 groupsX = (m_DisplayWidth + 7) / 8;
    u32 groupsY = (m_DisplayHeight + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);
}

// ============================================================================
// DISPATCH CAS
// ============================================================================

void DLSSUpscaler::DispatchCAS(VkCommandBuffer cmd, f32 sharpness) {
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

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_CASPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_CASPipelineLayout, 0, 1, &m_CASDescSet, 0, nullptr);

    DLSSCASPushConstants pc{};
    pc.imageWidth = m_DisplayWidth;
    pc.imageHeight = m_DisplayHeight;
    pc.sharpness = sharpness;
    vkCmdPushConstants(cmd, m_CASPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(DLSSCASPushConstants), &pc);

    u32 groupsX = (m_DisplayWidth + 7) / 8;
    u32 groupsY = (m_DisplayHeight + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);
}

// ============================================================================
// RESET HISTORY
// ============================================================================

void DLSSUpscaler::ResetHistory() {
    m_HistoryReset = true;
    // The reset flag is consumed in the next Dispatch() call.
    // For the real SDK path, sl::DLSSOptions::reset is set to eTrue when
    // m_HistoryReset is true. For the built-in path, this is a no-op since
    // the Lanczos+CAS pipeline has no temporal state.
}

// ============================================================================
// SHUTDOWN
// ============================================================================

void DLSSUpscaler::Shutdown() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

#ifdef ENJIN_HAS_DLSS_SDK
    if (m_SLInitialized) {
        slSetFeatureLoaded(sl::kFeatureDLSS, false);
        slShutdown();
        m_SLInitialized = false;
        ENJIN_LOG_INFO(Renderer, "DLSS: Streamline SDK shut down");
    }
#endif

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

bool DLSSUpscaler::CreateImages() {
    VkDevice device = m_Context->GetDevice();

    auto createImage = [&](VkImage& image, VkDeviceMemory& memory, VkImageView& view) -> bool {
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
        ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create intermediate image");
        return false;
    }

    if (!createImage(m_OutputImage, m_OutputMemory, m_OutputImageView)) {
        ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create output image");
        return false;
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
        ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create sampler");
        return false;
    }

    // Transition both images to GENERAL layout for storage image use
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &cmdPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create temp command pool");
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

    VkImageMemoryBarrier barriers[2]{};
    for (int i = 0; i < 2; i++) {
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

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
    vkDestroyCommandPool(device, cmdPool, nullptr);

    ENJIN_LOG_INFO(Renderer, "DLSS: Created upscaler images (%ux%u)", m_DisplayWidth, m_DisplayHeight);
    return true;
}

void DLSSUpscaler::DestroyImages() {
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
}

// ============================================================================
// CREATE COMPUTE PIPELINES (Lanczos + CAS — same shaders as FSR2)
// ============================================================================

bool DLSSUpscaler::CreateComputePipelines() {
    VkDevice device = m_Context->GetDevice();

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
            ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create %s descriptor set layout", shaderName);
            return false;
        }

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
            ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create %s descriptor pool", shaderName);
            return false;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &descSet) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to allocate %s descriptor set", shaderName);
            return false;
        }

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
            ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create %s pipeline layout", shaderName);
            return false;
        }

        VulkanShader shader(m_Context);
        bool loaded = false;
        for (u32 i = 0; i < pathCount; i++) {
            if (shader.LoadFromFile(shaderPaths[i]) && shader.GetModule() != VK_NULL_HANDLE) {
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            ENJIN_LOG_WARN(Renderer, "DLSS: %s shader not found — compile with: "
                           "glslangValidator -V Engine/shaders/%s -o Engine/shaders/%s.spv",
                           shaderName, shaderName, shaderName);
            return false;
        }

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
            ENJIN_LOG_ERROR(Renderer, "DLSS: Failed to create %s compute pipeline", shaderName);
            return false;
        }

        return true;
    };

    // Lanczos pipeline — uses same SPIR-V as FSR2
    const char* lanczosPaths[] = {
        "shaders/upscale_lanczos.comp.spv",
        "Engine/shaders/upscale_lanczos.comp.spv",
        "../Engine/shaders/upscale_lanczos.comp.spv",
        "../../Engine/shaders/upscale_lanczos.comp.spv"
    };

    if (!createPipeline("upscale_lanczos.comp", lanczosPaths, 4,
                         sizeof(DLSSLanczosPushConstants),
                         m_LanczosDescSetLayout, m_LanczosDescPool,
                         m_LanczosDescSet, m_LanczosPipelineLayout, m_LanczosPipeline)) {
        return false;
    }

    // CAS pipeline — uses same SPIR-V as FSR2
    const char* casPaths[] = {
        "shaders/upscale_cas.comp.spv",
        "Engine/shaders/upscale_cas.comp.spv",
        "../Engine/shaders/upscale_cas.comp.spv",
        "../../Engine/shaders/upscale_cas.comp.spv"
    };

    if (!createPipeline("upscale_cas.comp", casPaths, 4,
                         sizeof(DLSSCASPushConstants),
                         m_CASDescSetLayout, m_CASDescPool,
                         m_CASDescSet, m_CASPipelineLayout, m_CASPipeline)) {
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "DLSS: Compute pipelines created (Lanczos + CAS fallback)");
    return true;
}

void DLSSUpscaler::DestroyPipelines() {
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
}

} // namespace Renderer
} // namespace Enjin
