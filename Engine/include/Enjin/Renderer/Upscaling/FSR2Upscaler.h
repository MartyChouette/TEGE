#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Built-in FSR 2-style temporal upscaler.
//
// Provides the same IUpscaler interface as the SDK-based implementation,
// but uses engine-native compute shaders instead of the AMD FidelityFX SDK.
//
// Full pipeline (when all stages are available):
//   1. EASU (Edge Adaptive Spatial Upscale) — low-res -> display-res
//      Falls back to Lanczos-2 if EASU shader not compiled.
//   2. Temporal Accumulation — motion-vector-aware history blend at display-res
//      Skipped if velocity input is unavailable or on first frame.
//   3. RCAS (Robust Contrast Adaptive Sharpening) — detail recovery
//      Falls back to basic CAS if RCAS shader not compiled.
//
// Always available (no SDK dependency). Quality modes control the render
// scale: Performance=50%, Balanced=58%, Quality=67%, UltraQuality=77%.
class ENJIN_API FSR2Upscaler : public IUpscaler {
public:
    FSR2Upscaler(VulkanContext* context);
    ~FSR2Upscaler() override;

    bool Initialize(u32 renderWidth, u32 renderHeight,
                   u32 displayWidth, u32 displayHeight,
                   UpscalerQuality quality) override;
    void Resize(u32 renderWidth, u32 renderHeight,
               u32 displayWidth, u32 displayHeight) override;
    void Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) override;
    void ResetHistory() override;
    void Shutdown() override;

    UpscalerType GetType() const override { return UpscalerType::FSR2; }
    const char* GetName() const override { return "FSR 2 (Built-in)"; }
    bool IsAvailable() const override { return true; }  // Always available (no SDK)

    // Always compiled in (no SDK dependency)
    static bool IsCompiled() { return true; }

    // Get the upscaled output image view (display resolution, RGBA16F)
    VkImageView GetOutputImageView() const { return m_OutputImageView; }

    // Get the intermediate (post-EASU/Lanczos) image view (for debugging)
    VkImageView GetIntermediateImageView() const { return m_IntermediateImageView; }

    // Query which sub-pipelines are active (for debug display)
    bool IsEASUActive() const { return m_EASUPipeline != VK_NULL_HANDLE; }
    bool IsRCASActive() const { return m_RCASPipeline != VK_NULL_HANDLE; }
    bool IsTemporalActive() const { return m_TemporalPipeline != VK_NULL_HANDLE; }

private:
    bool CreateComputePipelines();
    bool CreateImages();
    void DestroyImages();
    void DestroyPipelines();

    // Spatial upscale: EASU (preferred) or Lanczos (fallback)
    void DispatchLanczos(VkCommandBuffer cmd, VkImageView inputView);
    void DispatchEASU(VkCommandBuffer cmd, VkImageView inputView);

    // Temporal accumulation (motion-vector-aware history blend)
    void DispatchTemporal(VkCommandBuffer cmd, VkImageView currentView,
                          VkImageView velocityView, VkImageView depthView);

    // Sharpening: RCAS (preferred) or CAS (fallback)
    void DispatchCAS(VkCommandBuffer cmd, f32 sharpness);
    void DispatchRCAS(VkCommandBuffer cmd, f32 sharpness);

    VulkanContext* m_Context = nullptr;
    u32 m_RenderWidth = 0;
    u32 m_RenderHeight = 0;
    u32 m_DisplayWidth = 0;
    u32 m_DisplayHeight = 0;
    bool m_Initialized = false;
    bool m_HistoryReset = false;
    bool m_HasHistory = false;   // True after at least one frame has been accumulated

    // --- Intermediate image (EASU/Lanczos output, display resolution) ---
    VkImage m_IntermediateImage = VK_NULL_HANDLE;
    VkDeviceMemory m_IntermediateMemory = VK_NULL_HANDLE;
    VkImageView m_IntermediateImageView = VK_NULL_HANDLE;

    // --- Output image (RCAS/CAS output, display resolution) ---
    VkImage m_OutputImage = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputMemory = VK_NULL_HANDLE;
    VkImageView m_OutputImageView = VK_NULL_HANDLE;

    // --- Temporal history images (ping-pong pair, display resolution) ---
    VkImage m_HistoryImages[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDeviceMemory m_HistoryMemory[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageView m_HistoryViews[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
    u32 m_CurrentHistory = 0;  // Ping-pong index (0 or 1)

    // --- Sampler ---
    VkSampler m_LinearSampler = VK_NULL_HANDLE;

    // --- Lanczos compute pipeline (fallback spatial upscale) ---
    VkPipeline m_LanczosPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_LanczosPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_LanczosDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_LanczosDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_LanczosDescSet = VK_NULL_HANDLE;

    // --- CAS compute pipeline (fallback sharpening) ---
    VkPipeline m_CASPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_CASPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_CASDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_CASDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_CASDescSet = VK_NULL_HANDLE;

    // --- EASU compute pipeline (edge-adaptive spatial upscale) ---
    VkPipeline m_EASUPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_EASUPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_EASUDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_EASUDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_EASUDescSet = VK_NULL_HANDLE;

    // --- RCAS compute pipeline (robust contrast adaptive sharpening) ---
    VkPipeline m_RCASPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_RCASPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_RCASDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_RCASDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_RCASDescSet = VK_NULL_HANDLE;

    // --- Temporal accumulation compute pipeline ---
    VkPipeline m_TemporalPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_TemporalPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_TemporalDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_TemporalDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_TemporalDescSet = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
