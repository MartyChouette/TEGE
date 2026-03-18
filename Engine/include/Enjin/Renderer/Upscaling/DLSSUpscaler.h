#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#include <vulkan/vulkan.h>

#ifdef ENJIN_HAS_DLSS_SDK
#include <sl.h>
#include <sl_dlss.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

// NVIDIA DLSS temporal upscaler backend.
//
// When ENJIN_UPSCALING_DLSS is defined and the real Streamline SDK is linked,
// this class uses DLSS Super Resolution for high-quality temporal upscaling
// on NVIDIA RTX GPUs.
//
// When the SDK is NOT linked (the default), this class provides a stub
// implementation that delegates to the same built-in Lanczos + CAS compute
// pipeline used by FSR2Upscaler.  This lets the engine exercise the full
// upscaler selection/switching code path on any hardware, and dropping in the
// real SDK later only requires replacing the Dispatch() internals.
//
// Hardware requirement: NVIDIA GPU (VkPhysicalDeviceProperties::vendorID == 0x10DE).
class ENJIN_API DLSSUpscaler : public IUpscaler {
public:
    DLSSUpscaler(VulkanContext* context);
    ~DLSSUpscaler() override;

    bool Initialize(u32 renderWidth, u32 renderHeight,
                   u32 displayWidth, u32 displayHeight,
                   UpscalerQuality quality) override;
    void Resize(u32 renderWidth, u32 renderHeight,
               u32 displayWidth, u32 displayHeight) override;
    void Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) override;
    void ResetHistory() override;
    void Shutdown() override;

    UpscalerType GetType() const override { return UpscalerType::DLSS; }

    const char* GetName() const override {
#ifdef ENJIN_HAS_DLSS_SDK
        return "DLSS 3.5 (SDK)";
#else
        return "DLSS 3.5 (Built-in)";
#endif
    }

    // Returns true when compiled against the real Streamline DLSS SDK
    static bool HasSDK() {
#ifdef ENJIN_HAS_DLSS_SDK
        return true;
#else
        return false;
#endif
    }

    // Returns true only on NVIDIA GPUs (vendorID 0x10DE).
    // When the real SDK is linked, also verifies driver/hardware DLSS support.
    bool IsAvailable() const override;

    // Compile-time check: is the DLSS SDK compiled in?
    static bool IsCompiled() {
#ifdef ENJIN_UPSCALING_DLSS
        return true;
#else
        return false;
#endif
    }

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

    // Check if the physical device is an NVIDIA GPU
    bool IsNvidiaGPU() const;

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

#ifdef ENJIN_HAS_DLSS_SDK
    // ================================================================
    // STREAMLINE SDK STATE — populated when the real SDK is linked
    // ================================================================
    sl::ViewportHandle m_Viewport{0};
    sl::DLSSOptions m_DLSSOptions{};
    bool m_SLInitialized = false;
#endif
};

} // namespace Renderer
} // namespace Enjin
