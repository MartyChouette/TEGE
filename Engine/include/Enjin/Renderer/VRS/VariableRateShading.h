#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// VRS shading rate modes
enum class VRSMode : u8 {
    Off = 0,            // No VRS — full rate everywhere
    Peripheral,         // Reduce rate at screen edges (foveated-style)
    ContentAdaptive,    // Reduce rate in low-detail regions (low luminance variance)
    MotionBased,        // Reduce rate for fast-moving pixels
    Full                // Combine peripheral + content + motion
};

// Variable Rate Shading system
// Uses VK_KHR_fragment_shading_rate to dynamically adjust per-tile shading rates.
// Generates a shading rate image each frame based on the selected mode.
class ENJIN_API VariableRateShading {
public:
    VariableRateShading(VulkanContext* context);
    ~VariableRateShading();

    // Probe hardware support and initialize
    bool Initialize(u32 screenWidth, u32 screenHeight);
    void Shutdown();

    // Check hardware VRS support
    bool IsSupported() const { return m_Supported; }
    bool IsEnabled() const { return m_Enabled && m_Supported; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

    VRSMode GetMode() const { return m_Mode; }
    void SetMode(VRSMode mode) { m_Mode = mode; }

    // Generate the shading rate image for this frame.
    // motionBuffer: optional velocity buffer for motion-based mode.
    // colorBuffer: optional previous frame color for content-adaptive mode.
    void GenerateShadingRateImage(
        VkCommandBuffer commandBuffer,
        VkImageView motionBufferView = VK_NULL_HANDLE,
        VkImageView colorBufferView = VK_NULL_HANDLE
    );

    // Get the shading rate image for render pass attachment
    VkImageView GetShadingRateImageView() const { return m_ShadingRateImageView; }
    VkImage GetShadingRateImage() const { return m_ShadingRateImage; }

    // Get the attachment info for render pass
    VkRenderingFragmentShadingRateAttachmentInfoKHR GetAttachmentInfo() const;

    // Resize when swapchain changes
    void Resize(u32 screenWidth, u32 screenHeight);

    // Hardware properties
    u32 GetMinTexelSize() const { return m_MinTexelSize; }
    u32 GetMaxTexelSize() const { return m_MaxTexelSize; }
    u32 GetTileWidth() const { return m_ShadingRateTileWidth; }
    u32 GetTileHeight() const { return m_ShadingRateTileHeight; }

private:
    bool ProbeHardwareSupport();
    bool CreateShadingRateImage();
    bool CreateComputePipeline();
    void DestroyShadingRateImage();

    VulkanContext* m_Context = nullptr;
    bool m_Supported = false;
    bool m_Enabled = false;
    VRSMode m_Mode = VRSMode::Peripheral;

    // Screen dimensions
    u32 m_ScreenWidth = 0;
    u32 m_ScreenHeight = 0;

    // Hardware-reported properties
    u32 m_MinTexelSize = 1;
    u32 m_MaxTexelSize = 1;
    u32 m_ShadingRateTileWidth = 16;   // Typical: 16x16 or 8x8
    u32 m_ShadingRateTileHeight = 16;

    // Shading rate image (one texel per tile)
    VkImage m_ShadingRateImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ShadingRateImageMemory = VK_NULL_HANDLE;
    VkImageView m_ShadingRateImageView = VK_NULL_HANDLE;
    u32 m_RateImageWidth = 0;
    u32 m_RateImageHeight = 0;

    // Compute pipeline for generating shading rate image
    VkPipeline m_GeneratePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Enjin
