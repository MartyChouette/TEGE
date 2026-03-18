#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Intel XeSS temporal upscaler backend.
//
// When ENJIN_UPSCALING_XESS is defined and the real XeSS SDK is linked,
// this class uses Intel XeSS for temporal upscaling.  XeSS uses DP4a
// (dot-product-accumulate) instructions and works on all modern GPUs,
// but runs fastest on Intel Arc hardware with XMX acceleration.
//
// When the SDK is NOT linked (the default), this class provides a stub
// implementation that delegates to the same built-in Lanczos + CAS compute
// pipeline used by FSR2Upscaler.  This lets the engine exercise the full
// upscaler selection/switching code path on any hardware, and dropping in the
// real SDK later only requires replacing the Dispatch() internals.
//
// Hardware requirement: XeSS works on all GPUs that support DP4a (i.e., most
// modern GPUs).  IsAvailable() returns true on Intel GPUs (vendorID 0x8086)
// unconditionally, and true on other vendors when DP4a support is detected.
// In stub mode, IsAvailable() always returns true since the fallback pipeline
// has no hardware requirement.
class ENJIN_API XeSSUpscaler : public IUpscaler {
public:
    XeSSUpscaler(VulkanContext* context);
    ~XeSSUpscaler() override;

    bool Initialize(u32 renderWidth, u32 renderHeight,
                   u32 displayWidth, u32 displayHeight,
                   UpscalerQuality quality) override;
    void Resize(u32 renderWidth, u32 renderHeight,
               u32 displayWidth, u32 displayHeight) override;
    void Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) override;
    void ResetHistory() override;
    void Shutdown() override;

    UpscalerType GetType() const override { return UpscalerType::XeSS; }

    const char* GetName() const override {
#ifdef ENJIN_UPSCALING_XESS
        return "XeSS";
#else
        return "XeSS (Stub \xe2\x80\x94 SDK not linked)";
#endif
    }

    // In stub mode, always returns true (Lanczos+CAS runs on any GPU).
    // When the real SDK is linked, checks for DP4a support and XeSS runtime.
    bool IsAvailable() const override;

    // Compile-time check: is the XeSS SDK compiled in?
    static bool IsCompiled() {
#ifdef ENJIN_UPSCALING_XESS
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

    // Check if the physical device is an Intel GPU
    bool IsIntelGPU() const;

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

#ifdef ENJIN_UPSCALING_XESS
    // ================================================================
    // REAL SDK INTEGRATION POINT
    // ================================================================
    // When the XeSS SDK is linked, add XeSS-specific state here:
    //
    //   xess_context_handle_t m_XeSSContext = nullptr;
    //   xess_2d_t m_InputResolution;
    //   xess_2d_t m_OutputResolution;
    //
    // The Initialize() method should call:
    //   xessCreateContext(vulkanDevice, &m_XeSSContext)
    //   xessSetVelocityScale(m_XeSSContext, ...)
    //   xessD3D12Init(m_XeSSContext, &initParams)   // or Vulkan equivalent
    //
    // The Dispatch() method should call:
    //   xessD3D12Execute(m_XeSSContext, ...)  // or Vulkan equivalent
    // instead of the Lanczos + CAS fallback.
    //
    // Quality mode mapping:
    //   UpscalerQuality::Performance  -> XESS_QUALITY_SETTING_PERFORMANCE
    //   UpscalerQuality::Balanced     -> XESS_QUALITY_SETTING_BALANCED
    //   UpscalerQuality::Quality      -> XESS_QUALITY_SETTING_QUALITY
    //   UpscalerQuality::UltraQuality -> XESS_QUALITY_SETTING_ULTRA_QUALITY
    // ================================================================
#endif
};

} // namespace Renderer
} // namespace Enjin
