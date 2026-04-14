#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/RayTracing/IDenoiser.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// SVGF denoiser configuration
struct SVGFConfig {
    f32 temporalAlpha = 0.05f;        // Temporal blend factor (lower = more history)
    u32 atrousIterations = 5;         // A-trous wavelet filter iterations
    f32 sigmaDepth = 1.0f;            // Depth edge-stopping sensitivity
    f32 sigmaNormal = 128.0f;         // Normal edge-stopping sensitivity
    f32 sigmaLuminance = 4.0f;        // Luminance edge-stopping sensitivity
};

// 3-pass SVGF (Spatiotemporal Variance-Guided Filtering) compute denoiser
// Pass 1: Temporal accumulation with motion vectors + moment tracking
// Pass 2: Variance estimation from accumulated moments
// Pass 3: Edge-aware a-trous wavelet filter (iterated 5x with doubling step)
class ENJIN_API SVGFDenoiser : public IDenoiser {
public:
    SVGFDenoiser(VulkanContext* context);
    ~SVGFDenoiser() override;

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

    SVGFConfig& GetConfig() { return m_Config; }
    const SVGFConfig& GetConfig() const { return m_Config; }

private:
    void CreateComputePipelines();
    void CreateIntermediateImages();
    void DestroyIntermediateImages();
    void CreateDescriptorSets();

    // Run the 3-pass denoiser on given input
    void RunDenoiserPasses(VkCommandBuffer cmd, VkImageView noisyInput,
                           VkImageView depthView, VkImageView normalView,
                           VkImageView motionView, VkImageView output,
                           bool singleChannel);

    VulkanContext* m_Context = nullptr;
    SVGFConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    // Compute pipelines (one for each pass)
    VkPipeline m_TemporalPipeline = VK_NULL_HANDLE;
    VkPipeline m_VariancePipeline = VK_NULL_HANDLE;
    VkPipeline m_AtrousPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

    // Intermediate images
    // Temporal history (previous frame's denoised output)
    VkImage m_HistoryImage = VK_NULL_HANDLE;
    VkDeviceMemory m_HistoryMemory = VK_NULL_HANDLE;
    VkImageView m_HistoryView = VK_NULL_HANDLE;

    // Moments buffer (RG16F: first moment, second moment)
    VkImage m_MomentsImage = VK_NULL_HANDLE;
    VkDeviceMemory m_MomentsMemory = VK_NULL_HANDLE;
    VkImageView m_MomentsView = VK_NULL_HANDLE;

    // Variance image (R16F)
    VkImage m_VarianceImage = VK_NULL_HANDLE;
    VkDeviceMemory m_VarianceMemory = VK_NULL_HANDLE;
    VkImageView m_VarianceView = VK_NULL_HANDLE;

    // Ping-pong images for a-trous iterations
    VkImage m_PingPongImages[2] = {};
    VkDeviceMemory m_PingPongMemory[2] = {};
    VkImageView m_PingPongViews[2] = {};

    bool m_HasHistory = false;  // False on first frame or after reset
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
