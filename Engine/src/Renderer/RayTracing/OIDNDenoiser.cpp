#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
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
// DENOISE IMPLEMENTATION
// ============================================================================

void OIDNDenoiser::DenoiseImpl(VkCommandBuffer cmd, VkImageView input, VkImageView output,
                                VkImageView normalView, u32 channels) {
#ifdef ENJIN_RAYTRACING_OIDN
    // Note: In a production implementation, this would:
    // 1. Insert a pipeline barrier (image → transfer src)
    // 2. vkCmdCopyImageToBuffer to staging buffer
    // 3. Submit + wait (or use timeline semaphore for async)
    // 4. Map staging buffer → CPU
    // 5. Run OIDN filter
    // 6. Unmap + vkCmdCopyBufferToImage back
    // 7. Insert pipeline barrier (transfer dst → shader read)
    //
    // For now, this is structured to compile and integrate cleanly.
    // The actual GPU↔CPU copy requires the VkImage handle (not just VkImageView),
    // which would need a small refactor of the RT effect output accessors.
    // This placeholder runs OIDN on zero-initialized buffers as a proof of integration.

    usize pixelCount = static_cast<usize>(m_Width) * m_Height;
    usize bufferChannels = (channels == 1) ? 3 : 3;  // OIDN always uses 3-channel (RGB)

    // Zero output (placeholder until GPU↔CPU copy is wired)
    std::fill(m_InputBuffer.begin(), m_InputBuffer.begin() + pixelCount * bufferChannels, 0.0f);
    std::fill(m_OutputBuffer.begin(), m_OutputBuffer.begin() + pixelCount * bufferChannels, 0.0f);

    OIDNFilter filter = (channels == 1) ? m_SingleFilter : m_ColorFilter;

    // Configure filter
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

    // Check for errors
    const char* errorMsg = nullptr;
    if (oidnGetDeviceError(m_Device, &errorMsg) != OIDN_ERROR_NONE) {
        ENJIN_LOG_WARN(Renderer, "OIDN denoise error: %s", errorMsg ? errorMsg : "unknown");
    }
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
