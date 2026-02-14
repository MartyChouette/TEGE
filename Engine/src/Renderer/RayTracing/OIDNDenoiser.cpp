#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Renderer {

// ============================================================================
// AVAILABILITY CHECK
// ============================================================================

bool OIDNDenoiser::IsAvailable() {
#ifdef ENJIN_RAYTRACING_OIDN
    return true;
#else
    return false;
#endif
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

OIDNDenoiser::OIDNDenoiser(VulkanContext* context)
    : m_Context(context) {}

OIDNDenoiser::~OIDNDenoiser() {
    Shutdown();
}

// ============================================================================
// IMAGE MAPPING
// ============================================================================

void OIDNDenoiser::RegisterImageMapping(VkImageView view, VkImage image, VkFormat format) {
    if (view == VK_NULL_HANDLE) return;
    m_ImageMap[view] = {image, format};
}

VkImage OIDNDenoiser::ResolveImage(VkImageView view) const {
    auto it = m_ImageMap.find(view);
    return (it != m_ImageMap.end()) ? it->second.image : VK_NULL_HANDLE;
}

VkFormat OIDNDenoiser::ResolveFormat(VkImageView view) const {
    auto it = m_ImageMap.find(view);
    return (it != m_ImageMap.end()) ? it->second.format : VK_FORMAT_UNDEFINED;
}

// ============================================================================
// INITIALIZE
// ============================================================================

bool OIDNDenoiser::Initialize(u32 width, u32 height) {
    if (m_Initialized) return true;

#ifdef ENJIN_RAYTRACING_OIDN
    m_Width = width;
    m_Height = height;

    // Create OIDN device (CPU by default — OIDN also supports SYCL/CUDA/HIP for GPU)
    m_Device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
    if (!m_Device) {
        ENJIN_LOG_WARN(Renderer, "OIDN: Failed to create device");
        return false;
    }

    // Set quality
    switch (m_Config.quality) {
        case OIDNQuality::Fast:
            oidnSetDeviceInt(m_Device, "quality", OIDN_QUALITY_BALANCED);
            break;
        case OIDNQuality::High:
            oidnSetDeviceInt(m_Device, "quality", OIDN_QUALITY_HIGH);
            break;
        default:
            oidnSetDeviceInt(m_Device, "quality", OIDN_QUALITY_DEFAULT);
            break;
    }

    oidnCommitDevice(m_Device);

    // Check for errors
    const char* errorMsg = nullptr;
    if (oidnGetDeviceError(m_Device, &errorMsg) != OIDN_ERROR_NONE) {
        ENJIN_LOG_WARN(Renderer, "OIDN: Device error: %s", errorMsg ? errorMsg : "unknown");
        oidnReleaseDevice(m_Device);
        m_Device = nullptr;
        return false;
    }

    // Create filters
    // Color filter: for RGBA (reflections, GI)
    m_ColorFilter = oidnNewFilter(m_Device, "RT");
    if (!m_ColorFilter) {
        ENJIN_LOG_WARN(Renderer, "OIDN: Failed to create color filter");
        oidnReleaseDevice(m_Device);
        m_Device = nullptr;
        return false;
    }

    // Single-channel filter: for shadows, AO
    m_SingleFilter = oidnNewFilter(m_Device, "RT");
    if (!m_SingleFilter) {
        ENJIN_LOG_WARN(Renderer, "OIDN: Failed to create single-channel filter");
        oidnReleaseFilter(m_ColorFilter);
        m_ColorFilter = nullptr;
        oidnReleaseDevice(m_Device);
        m_Device = nullptr;
        return false;
    }

    // Allocate CPU-side buffers
    m_InputBuffer.resize(static_cast<usize>(width) * height * 4);  // Max 4 channels
    m_OutputBuffer.resize(static_cast<usize>(width) * height * 4);
    m_NormalBuffer.resize(static_cast<usize>(width) * height * 3);

    // Create Vulkan staging buffers for GPU↔CPU transfer
    CreateStagingBuffers();

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "OIDN denoiser initialized (%ux%u)", width, height);
    return true;
#else
    ENJIN_LOG_WARN(Renderer, "OIDN denoiser not available (ENJIN_RAYTRACING_OIDN=OFF)");
    return false;
#endif
}

// ============================================================================
// RESIZE
// ============================================================================

void OIDNDenoiser::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;

#ifdef ENJIN_RAYTRACING_OIDN
    m_Width = width;
    m_Height = height;

    // Resize CPU buffers
    m_InputBuffer.resize(static_cast<usize>(width) * height * 4);
    m_OutputBuffer.resize(static_cast<usize>(width) * height * 4);
    m_NormalBuffer.resize(static_cast<usize>(width) * height * 3);

    // Recreate staging buffers
    DestroyStagingBuffers();
    CreateStagingBuffers();
#endif
}

// ============================================================================
// DENOISE SINGLE CHANNEL (Shadows, AO — R16F input)
// ============================================================================

void OIDNDenoiser::DenoiseSingleChannel(VkCommandBuffer cmd, VkImageView noisyInput,
                                         VkImageView depthView, VkImageView normalView,
                                         VkImageView motionView, VkImageView output) {
#ifdef ENJIN_RAYTRACING_OIDN
    if (!m_Initialized || !m_SingleFilter) return;
    DenoiseImpl(cmd, noisyInput, output, normalView, 1);
#endif
}

// ============================================================================
// DENOISE COLOR (Reflections, GI — RGBA16F input)
// ============================================================================

void OIDNDenoiser::DenoiseColor(VkCommandBuffer cmd, VkImageView noisyInput,
                                 VkImageView depthView, VkImageView normalView,
                                 VkImageView motionView, VkImageView output) {
#ifdef ENJIN_RAYTRACING_OIDN
    if (!m_Initialized || !m_ColorFilter) return;
    DenoiseImpl(cmd, noisyInput, output, normalView, 4);
#endif
}

// ============================================================================
// HALF-FLOAT CONVERSION HELPERS
// ============================================================================

static f32 HalfToFloat(u16 h) {
    u32 sign = (h >> 15) & 0x1;
    u32 expo = (h >> 10) & 0x1F;
    u32 mant = h & 0x3FF;
    if (expo == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        f32 m = static_cast<f32>(mant) / 1024.0f;
        f32 val = m * (1.0f / 16384.0f);  // 2^(-14)
        return sign ? -val : val;
    }
    if (expo == 31) {
        return (mant == 0) ? (sign ? -INFINITY : INFINITY) : NAN;
    }
    f32 val = std::ldexp(1.0f + static_cast<f32>(mant) / 1024.0f, static_cast<int>(expo) - 15);
    return sign ? -val : val;
}

static u16 FloatToHalf(f32 f) {
    u32 fi;
    std::memcpy(&fi, &f, sizeof(f32));
    u32 sign = (fi >> 31) & 0x1;
    i32 expo = static_cast<i32>((fi >> 23) & 0xFF) - 127 + 15;
    u32 mant = fi & 0x7FFFFF;
    if (expo <= 0) return static_cast<u16>(sign << 15);
    if (expo >= 31) return static_cast<u16>((sign << 15) | (31 << 10));
    return static_cast<u16>((sign << 15) | (expo << 10) | (mant >> 13));
}

// ============================================================================
// GPU → CPU STAGING CONVERSION
// ============================================================================
// Converts the staging buffer content (in GPU pixel format) to the f32 array
// that OIDN expects. Handles R16F (single-channel) and RGBA16F (4-channel).

void OIDNDenoiser::ConvertStagingToFloat(const void* staging, u32 channels) {
    usize pixelCount = static_cast<usize>(m_Width) * m_Height;

    if (channels == 1) {
        // R16F → expand to RGB (OIDN requires 3-channel minimum)
        const u16* halfPixels = static_cast<const u16*>(staging);
        for (usize i = 0; i < pixelCount; ++i) {
            f32 val = HalfToFloat(halfPixels[i]);
            m_InputBuffer[i * 3 + 0] = val;
            m_InputBuffer[i * 3 + 1] = val;
            m_InputBuffer[i * 3 + 2] = val;
        }
    } else {
        // RGBA16F → RGB (OIDN uses 3-channel)
        const u16* halfPixels = static_cast<const u16*>(staging);
        for (usize i = 0; i < pixelCount; ++i) {
            m_InputBuffer[i * 3 + 0] = HalfToFloat(halfPixels[i * 4 + 0]);
            m_InputBuffer[i * 3 + 1] = HalfToFloat(halfPixels[i * 4 + 1]);
            m_InputBuffer[i * 3 + 2] = HalfToFloat(halfPixels[i * 4 + 2]);
        }
    }
}

// ============================================================================
// CPU → GPU STAGING CONVERSION
// ============================================================================
// Converts the OIDN f32 output back to the GPU pixel format for upload.

void OIDNDenoiser::ConvertFloatToStaging(void* staging, u32 channels) {
    usize pixelCount = static_cast<usize>(m_Width) * m_Height;

    if (channels == 1) {
        // RGB → R16F (take red channel, which equals the denoised single-channel value)
        u16* halfPixels = static_cast<u16*>(staging);
        for (usize i = 0; i < pixelCount; ++i) {
            halfPixels[i] = FloatToHalf(m_OutputBuffer[i * 3 + 0]);
        }
    } else {
        // RGB → RGBA16F (alpha = 1.0)
        u16* halfPixels = static_cast<u16*>(staging);
        for (usize i = 0; i < pixelCount; ++i) {
            halfPixels[i * 4 + 0] = FloatToHalf(m_OutputBuffer[i * 3 + 0]);
            halfPixels[i * 4 + 1] = FloatToHalf(m_OutputBuffer[i * 3 + 1]);
            halfPixels[i * 4 + 2] = FloatToHalf(m_OutputBuffer[i * 3 + 2]);
            halfPixels[i * 4 + 3] = FloatToHalf(1.0f);
        }
    }
}

// ============================================================================
// GPU IMAGE <-> STAGING BUFFER COPY
// ============================================================================

void OIDNDenoiser::CopyImageToStaging(VkCommandBuffer cmd, VkImage image, VkFormat format, u32 channels) {
    if (image == VK_NULL_HANDLE || m_StagingBuffer == VK_NULL_HANDLE) return;

    // Transition image to TRANSFER_SRC
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy image to staging buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            m_StagingBuffer, 1, &region);

    // Barrier: make staging buffer readable by host
    VkBufferMemoryBarrier bufBarrier{};
    bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.buffer = m_StagingBuffer;
    bufBarrier.offset = 0;
    bufBarrier.size = m_StagingSize;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        0, 0, nullptr, 1, &bufBarrier, 0, nullptr);
}

void OIDNDenoiser::CopyStagingToImage(VkCommandBuffer cmd, VkImage image, VkFormat format, u32 channels) {
    if (image == VK_NULL_HANDLE || m_OutputStagingBuffer == VK_NULL_HANDLE) return;

    // Transition image to TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Host write barrier for upload staging buffer
    VkBufferMemoryBarrier bufBarrier{};
    bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.buffer = m_OutputStagingBuffer;
    bufBarrier.offset = 0;
    bufBarrier.size = m_StagingSize;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &bufBarrier, 0, nullptr);

    // Copy staging buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyBufferToImage(cmd, m_OutputStagingBuffer, image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition image back to GENERAL for shader access
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ============================================================================
// DENOISE IMPLEMENTATION (with GPU↔CPU copy)
// ============================================================================

void OIDNDenoiser::DenoiseImpl(VkCommandBuffer cmd, VkImageView input, VkImageView output,
                                VkImageView normalView, u32 channels) {
#ifdef ENJIN_RAYTRACING_OIDN
    // Resolve VkImageView to VkImage via registered mappings
    VkImage inputImage = ResolveImage(input);
    VkImage outputImage = ResolveImage(output);
    VkFormat inputFormat = ResolveFormat(input);

    if (inputImage == VK_NULL_HANDLE || outputImage == VK_NULL_HANDLE) {
        // No image mapping registered — fall back to zero-buffer denoising
        // This happens when RegisterImageMapping() hasn't been called yet
        usize pixelCount = static_cast<usize>(m_Width) * m_Height;
        usize bufferChannels = 3;  // OIDN always uses 3-channel (RGB)

        std::fill(m_InputBuffer.begin(), m_InputBuffer.begin() + pixelCount * bufferChannels, 0.0f);
        std::fill(m_OutputBuffer.begin(), m_OutputBuffer.begin() + pixelCount * bufferChannels, 0.0f);

        OIDNFilter filter = (channels == 1) ? m_SingleFilter : m_ColorFilter;
        oidnSetFilterImage(filter, "color", m_InputBuffer.data(), OIDN_FORMAT_FLOAT3,
                           m_Width, m_Height, 0, 0, 0);
        oidnSetFilterImage(filter, "output", m_OutputBuffer.data(), OIDN_FORMAT_FLOAT3,
                           m_Width, m_Height, 0, 0, 0);
        if (m_Config.inputScale > 0.0f) {
            oidnSetFilterFloat(filter, "inputScale", m_Config.inputScale);
        }
        oidnSetFilterBool(filter, "hdr", true);
        oidnCommitFilter(filter);
        oidnExecuteFilter(filter);

        const char* errorMsg = nullptr;
        if (oidnGetDeviceError(m_Device, &errorMsg) != OIDN_ERROR_NONE) {
            ENJIN_LOG_WARN(Renderer, "OIDN denoise error: %s", errorMsg ? errorMsg : "unknown");
        }
        return;
    }

    // === STEP 1: Copy RT output image to CPU staging buffer ===
    CopyImageToStaging(cmd, inputImage, inputFormat, channels);

    // === STEP 2: Map staging buffer, convert to OIDN float format ===
    VkDevice device = m_Context->GetDevice();

    void* mappedInput = nullptr;
    if (vkMapMemory(device, m_StagingMemory, 0, m_StagingSize, 0, &mappedInput) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OIDN: Failed to map input staging buffer");
        return;
    }

    ConvertStagingToFloat(mappedInput, channels);
    vkUnmapMemory(device, m_StagingMemory);

    // === STEP 3: Run OIDN filter on CPU ===
    usize pixelCount = static_cast<usize>(m_Width) * m_Height;
    std::fill(m_OutputBuffer.begin(), m_OutputBuffer.begin() + pixelCount * 3, 0.0f);

    OIDNFilter filter = (channels == 1) ? m_SingleFilter : m_ColorFilter;

    oidnSetFilterImage(filter, "color", m_InputBuffer.data(), OIDN_FORMAT_FLOAT3,
                       m_Width, m_Height, 0, 0, 0);
    oidnSetFilterImage(filter, "output", m_OutputBuffer.data(), OIDN_FORMAT_FLOAT3,
                       m_Width, m_Height, 0, 0, 0);

    // Optionally bind normals auxiliary image
    if (m_Config.useNormals && normalView != VK_NULL_HANDLE) {
        VkImage normalImage = ResolveImage(normalView);
        if (normalImage != VK_NULL_HANDLE) {
            // Normal data would need its own staging copy — for now, skip
            // (normal-guided denoising is an optional quality enhancement)
        }
    }

    if (m_Config.inputScale > 0.0f) {
        oidnSetFilterFloat(filter, "inputScale", m_Config.inputScale);
    }
    oidnSetFilterBool(filter, "hdr", true);

    oidnCommitFilter(filter);
    oidnExecuteFilter(filter);

    // Check for OIDN errors
    const char* errorMsg = nullptr;
    if (oidnGetDeviceError(m_Device, &errorMsg) != OIDN_ERROR_NONE) {
        ENJIN_LOG_WARN(Renderer, "OIDN denoise error: %s", errorMsg ? errorMsg : "unknown");
    }

    // === STEP 4: Convert denoised output back to GPU format, write to upload buffer ===
    void* mappedOutput = nullptr;
    if (vkMapMemory(device, m_OutputStagingMemory, 0, m_StagingSize, 0, &mappedOutput) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OIDN: Failed to map output staging buffer");
        return;
    }

    ConvertFloatToStaging(mappedOutput, channels);
    vkUnmapMemory(device, m_OutputStagingMemory);

    // === STEP 5: Copy denoised result back to GPU image ===
    CopyStagingToImage(cmd, outputImage, inputFormat, channels);

#endif
}

// ============================================================================
// RESET HISTORY
// ============================================================================

void OIDNDenoiser::ResetHistory() {
    // OIDN filters are stateless (no temporal accumulation) — nothing to reset
    // If using the temporal denoising mode (RTLightmap filter), we would reset here
}

// ============================================================================
// SHUTDOWN
// ============================================================================

void OIDNDenoiser::Shutdown() {
    if (!m_Initialized) return;

#ifdef ENJIN_RAYTRACING_OIDN
    if (m_SingleFilter) {
        oidnReleaseFilter(m_SingleFilter);
        m_SingleFilter = nullptr;
    }
    if (m_ColorFilter) {
        oidnReleaseFilter(m_ColorFilter);
        m_ColorFilter = nullptr;
    }
    if (m_Device) {
        oidnReleaseDevice(m_Device);
        m_Device = nullptr;
    }
#endif

    DestroyStagingBuffers();

    m_InputBuffer.clear();
    m_OutputBuffer.clear();
    m_NormalBuffer.clear();
    m_ImageMap.clear();
    m_Initialized = false;
}

// ============================================================================
// STAGING BUFFERS
// ============================================================================

void OIDNDenoiser::CreateStagingBuffers() {
    if (!m_Context) return;

    // Size for RGBA f32 at full resolution (largest possible transfer)
    m_StagingSize = static_cast<usize>(m_Width) * m_Height * 4 * sizeof(f32);
    if (m_StagingSize == 0) return;

    VkDevice device = m_Context->GetDevice();

    // Input staging buffer (GPU → CPU)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = m_StagingSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_StagingBuffer) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OIDN: Failed to create input staging buffer");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_StagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_StagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_StagingBuffer, nullptr);
        m_StagingBuffer = VK_NULL_HANDLE;
        return;
    }
    vkBindBufferMemory(device, m_StagingBuffer, m_StagingMemory, 0);

    // Output staging buffer (CPU → GPU)
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_OutputStagingBuffer) != VK_SUCCESS) {
        return;
    }

    vkGetBufferMemoryRequirements(device, m_OutputStagingBuffer, &memReqs);
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_OutputStagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, m_OutputStagingBuffer, nullptr);
        m_OutputStagingBuffer = VK_NULL_HANDLE;
        return;
    }
    vkBindBufferMemory(device, m_OutputStagingBuffer, m_OutputStagingMemory, 0);
}

void OIDNDenoiser::DestroyStagingBuffers() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_OutputStagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_OutputStagingBuffer, nullptr);
        m_OutputStagingBuffer = VK_NULL_HANDLE;
    }
    if (m_OutputStagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_OutputStagingMemory, nullptr);
        m_OutputStagingMemory = VK_NULL_HANDLE;
    }
    if (m_StagingBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_StagingBuffer, nullptr);
        m_StagingBuffer = VK_NULL_HANDLE;
    }
    if (m_StagingMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_StagingMemory, nullptr);
        m_StagingMemory = VK_NULL_HANDLE;
    }
}

} // namespace Renderer
} // namespace Enjin
