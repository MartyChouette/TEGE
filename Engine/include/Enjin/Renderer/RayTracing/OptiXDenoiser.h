#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/RayTracing/IDenoiser.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

#ifdef ENJIN_RAYTRACING_OPTIX
#include <optix.h>
#include <optix_stubs.h>
#include <cuda_runtime.h>
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

    // Vulkan-CUDA external memory handles
    VkBuffer m_SharedBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_SharedMemory = VK_NULL_HANDLE;
    usize m_SharedBufferSize = 0;
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
