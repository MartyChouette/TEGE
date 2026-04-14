#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/RayTracing/IDenoiser.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <unordered_map>

#ifdef ENJIN_RAYTRACING_OPTIX
#include <optix.h>
#include <optix_stubs.h>
#include <cuda.h>
#include <cuda_runtime.h>
#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#endif
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

// OptiX denoiser quality/mode
enum class OptiXDenoiserMode : u32 {
    LDR = 0,       // Low dynamic range (after tonemapping)
    HDR = 1,        // High dynamic range (before tonemapping) — recommended for RT
    Temporal = 2    // Temporal mode with motion vectors
};

// OptiX denoiser configuration
struct OptiXDenoiserConfig {
    OptiXDenoiserMode mode = OptiXDenoiserMode::HDR;
    f32 blendFactor = 0.0f;     // 0 = fully denoised, 1 = original (for progressive fade)
    bool useAlbedo = false;     // Use albedo auxiliary guide
    bool useNormals = false;    // Use normal auxiliary guide
    bool useTemporalData = false; // Use motion vectors for temporal stability
};

// NVIDIA OptiX AI Denoiser backend
// GPU-based ML denoiser — superior quality to OIDN for real-time RT (1-4 spp).
// Requires NVIDIA GPU with OptiX 7+ and CUDA. Uses Vulkan-CUDA interop for
// zero-copy GPU buffer sharing.
class ENJIN_API OptiXDenoiser : public IDenoiser {
public:
    OptiXDenoiser(VulkanContext* context);
    ~OptiXDenoiser() override;

    bool Initialize(u32 width, u32 height) override;
    void Resize(u32 width, u32 height) override;

    void DenoiseSingleChannel(VkCommandBuffer cmd, VkImageView noisyInput,
                               VkImageView depthView, VkImageView normalView,
                               VkImageView motionView, VkImageView output) override;

    void DenoiseColor(VkCommandBuffer cmd, VkImageView noisyInput,
                       VkImageView depthView, VkImageView normalView,
                       VkImageView motionView, VkImageView output) override;

    void ResetHistory() override;
    void Shutdown() override;

    OptiXDenoiserConfig& GetConfig() { return m_Config; }
    const OptiXDenoiserConfig& GetConfig() const { return m_Config; }

    // Query availability at compile time and runtime
    static bool IsAvailable();

    // Register VkImageView -> VkImage mapping for GPU interop
    void RegisterImageMapping(VkImageView view, VkImage image, VkFormat format);

private:
    // Resolve VkImageView to its backing VkImage
    VkImage ResolveImage(VkImageView view) const;
    VkFormat ResolveFormat(VkImageView view) const;

    // Vulkan-CUDA interop helpers
    bool SetupCudaInterop();
    void CleanupCudaInterop();

#ifdef ENJIN_RAYTRACING_OPTIX
    // Create a VkBuffer with exportable external memory and import into CUDA
    bool CreateSharedBuffer(VkBuffer& buffer, VkDeviceMemory& memory,
                            CUexternalMemory& extMem, CUdeviceptr& devPtr,
                            usize size);
    void DestroySharedBuffer(VkBuffer& buffer, VkDeviceMemory& memory,
                             CUexternalMemory& extMem, CUdeviceptr& devPtr);

    // Vulkan image <-> shared buffer GPU-side copy
    void CopyImageToSharedBuffer(VkCommandBuffer cmd, VkImage image, VkFormat format,
                                 VkBuffer sharedBuf, u32 channels);
    void CopySharedBufferToImage(VkCommandBuffer cmd, VkImage image, VkFormat format,
                                 VkBuffer sharedBuf, u32 channels);

    // Vulkan-CUDA semaphore synchronization
    bool CreateInteropSemaphores();
    void DestroyInteropSemaphores();
    void SignalVulkanSemaphore(VkCommandBuffer cmd);
    void WaitCudaOnVulkan();
    void SignalCudaForVulkan();
    void WaitVulkanOnCuda(VkCommandBuffer cmd);
#endif

    // Image view -> image mapping
    struct ImageInfo {
        VkImage image = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
    };
    std::unordered_map<VkImageView, ImageInfo> m_ImageMap;

    // Core denoise implementation
    void DenoiseImpl(VkCommandBuffer cmd, VkImageView input, VkImageView output,
                     VkImageView normalView, u32 channels);

    VulkanContext* m_Context = nullptr;
    OptiXDenoiserConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

#ifdef ENJIN_RAYTRACING_OPTIX
    // OptiX objects
    OptixDeviceContext m_OptixContext = nullptr;
    OptixDenoiser m_Denoiser = nullptr;

    // CUDA resources
    CUdeviceptr m_DenoiserState = 0;
    CUdeviceptr m_Scratch = 0;
    CUdeviceptr m_InputBuffer = 0;
    CUdeviceptr m_OutputBuffer = 0;
    CUdeviceptr m_NormalBuffer = 0;
    CUdeviceptr m_AlbedoBuffer = 0;
    CUdeviceptr m_FlowBuffer = 0;       // Motion vectors for temporal mode
    CUdeviceptr m_PrevOutputBuffer = 0;  // Previous frame for temporal mode
    usize m_DenoiserStateSize = 0;
    usize m_ScratchSize = 0;
    CUstream m_CudaStream = nullptr;

    // Vulkan-CUDA shared input buffer (VkImage → this → CUDA reads)
    VkBuffer m_SharedInputBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_SharedInputMemory = VK_NULL_HANDLE;
    CUexternalMemory m_CudaExtInputMem = nullptr;
    CUdeviceptr m_CudaInputPtr = 0;

    // Vulkan-CUDA shared output buffer (CUDA writes → this → VkImage)
    VkBuffer m_SharedOutputBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_SharedOutputMemory = VK_NULL_HANDLE;
    CUexternalMemory m_CudaExtOutputMem = nullptr;
    CUdeviceptr m_CudaOutputPtr = 0;

    usize m_SharedBufferSize = 0;

    // Vulkan-CUDA semaphore interop for synchronization
    VkSemaphore m_VkReadySemaphore = VK_NULL_HANDLE;   // Vulkan signals when copy-to-shared is done
    VkSemaphore m_CudaDoneSemaphore = VK_NULL_HANDLE;  // CUDA signals when denoise is done
    CUexternalSemaphore m_CudaExtReadySem = nullptr;
    CUexternalSemaphore m_CudaExtDoneSem = nullptr;

    // Timeline semaphore values (monotonically increasing)
    u64 m_SemaphoreValue = 0;
#endif

    // Vulkan staging buffers for fallback path (when CUDA interop unavailable)
    VkBuffer m_StagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_StagingMemory = VK_NULL_HANDLE;
    VkBuffer m_OutputStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputStagingMemory = VK_NULL_HANDLE;
    usize m_StagingSize = 0;

    bool m_CudaInteropAvailable = false;
    bool m_Initialized = false;
    u32 m_FrameIndex = 0;  // For temporal denoising
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
