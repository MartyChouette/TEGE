#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Built-in FSR 2-style temporal upscaler (Lanczos + CAS).
//
// Provides the same IUpscaler interface as the SDK-based implementation,
// but uses engine-native compute shaders instead of the AMD FidelityFX SDK.
// Pipeline: render at lower resolution -> TAA resolve at lower res ->
//           Lanczos-2 upscale to display res -> CAS sharpening.
//
// Always available (no SDK dependency).  Quality modes control the render
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

    // Get the intermediate (post-Lanczos) image view (for debugging)
    VkImageView GetIntermediateImageView() const { return m_IntermediateImageView; }

private:
    bool CreateComputePipelines();
    bool CreateImages();
    void DestroyImages();
    void DestroyPipelines();

    // Lanczos upscale dispatch (low-res -> display-res)
    void DispatchLanczos(VkCommandBuffer cmd, VkImageView inputView);

    // CAS sharpening dispatch (display-res -> display-res)
    void DispatchCAS(VkCommandBuffer cmd, f32 sharpness);

    VulkanContext* m_Context = nullptr;
    u32 m_RenderWidth = 0;
    u32 m_RenderHeight = 0;
    u32 m_DisplayWidth = 0;
    u32 m_DisplayHeight = 0;
    bool m_Initialized = false;
    bool m_HistoryReset = false;

    // --- Intermediate image (Lanczos output, display resolution) ---
    VkImage m_IntermediateImage = VK_NULL_HANDLE;
    VkDeviceMemory m_IntermediateMemory = VK_NULL_HANDLE;
    VkImageView m_IntermediateImageView = VK_NULL_HANDLE;

    // --- Output image (CAS output or Lanczos output if sharpness=0) ---
    VkImage m_OutputImage = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputMemory = VK_NULL_HANDLE;
    VkImageView m_OutputImageView = VK_NULL_HANDLE;

    // --- Sampler ---
    VkSampler m_LinearSampler = VK_NULL_HANDLE;

    // --- Lanczos compute pipeline ---
    VkPipeline m_LanczosPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_LanczosPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_LanczosDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_LanczosDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_LanczosDescSet = VK_NULL_HANDLE;

    // --- CAS compute pipeline ---
    VkPipeline m_CASPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_CASPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_CASDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_CASDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_CASDescSet = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Enjin
