#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;

// Per-buffer temporal reuse control flags
struct RTTemporalReuseConfig {
    bool enabled = false;
    f32 historyLength = 0.9f;         // Blend factor (0=no reuse, 0.9=90% previous frame)
    f32 disocclusionThreshold = 0.1f; // Depth ratio threshold for detecting new surfaces
    f32 normalThreshold = 0.9f;       // Normal similarity (dot product) threshold
    bool reuseShadows = true;
    bool reuseReflections = true;
    bool reuseAO = true;
    bool reuseGI = true;
};

// Temporal RT result reuse — carries ray-traced outputs across frames using motion vectors.
//
// For each RT output buffer (shadow, AO, reflections, GI), the system maintains a
// ping-pong history texture pair. Each frame, a compute shader reprojects the previous
// frame's result using motion vectors, validates the reprojection against depth/normal
// consistency, and blends valid history with the current frame's noisy RT output.
//
// Invalid reprojections (disocclusions, new surfaces) fall back to the current frame only,
// while valid regions accumulate temporally to reduce noise before the denoiser runs.
//
// The system outputs a confidence map (R8) where 0=freshly traced (disoccluded) and
// 1=high temporal confidence, which downstream passes (SVGF, OIDN) can use to modulate
// their spatial filter strength.
//
// Current implementation: shadow buffer temporal reuse (R16F, single-channel).
// Designed for expansion to reflections (RGBA16F), AO (R16F), and GI (RGBA16F).
class ENJIN_API RTTemporalReuse {
public:
    RTTemporalReuse(VulkanContext* context);
    ~RTTemporalReuse();

    bool Initialize(u32 width, u32 height);
    void Resize(u32 width, u32 height);
    void Shutdown();

    // Dispatch temporal reuse for the shadow buffer.
    // Must be called after RTShadows::Dispatch() and before DenoiseRTOutputs().
    // Reads the current shadow output, reprojects history, writes blended result back.
    void ReuseShadows(VkCommandBuffer cmd, VkImageView currentShadowView,
                      VkImageView depthView, VkImageView normalView,
                      VkImageView motionView, VkImageView outputView);

    // Dispatch temporal reuse for the AO buffer (R16F, single-channel).
    void ReuseAO(VkCommandBuffer cmd, VkImageView currentAOView,
                 VkImageView depthView, VkImageView normalView,
                 VkImageView motionView, VkImageView outputView);

    // Dispatch temporal reuse for the reflection buffer (RGBA16F, multi-channel).
    void ReuseReflections(VkCommandBuffer cmd, VkImageView currentReflectView,
                          VkImageView depthView, VkImageView normalView,
                          VkImageView motionView, VkImageView outputView);

    // Dispatch temporal reuse for the GI buffer (RGBA16F, multi-channel).
    void ReuseGI(VkCommandBuffer cmd, VkImageView currentGIView,
                 VkImageView depthView, VkImageView normalView,
                 VkImageView motionView, VkImageView outputView);

    // Reset temporal history (call on camera cut, scene load, etc.)
    void ResetHistory();

    // Confidence map output — 0=freshly traced, 1=high temporal confidence
    VkImageView GetConfidenceView() const { return m_ConfidenceView; }

    RTTemporalReuseConfig& GetConfig() { return m_Config; }
    const RTTemporalReuseConfig& GetConfig() const { return m_Config; }

private:
    // Internal dispatch for both single-channel (shadow/AO) and multi-channel (reflect/GI)
    void DispatchReuse(VkCommandBuffer cmd, VkImageView currentView,
                       VkImageView depthView, VkImageView normalView,
                       VkImageView motionView, VkImageView outputView,
                       VkImage historyImage, VkImageView historyView,
                       bool singleChannel, bool& hasHistory);

    void CreateComputePipeline();
    void CreateHistoryImages();
    void DestroyHistoryImages();
    void CreateDescriptorResources();
    void DestroyDescriptorResources();

    VulkanContext* m_Context = nullptr;
    RTTemporalReuseConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

    // Compute pipeline (single shader handles both single-channel and multi-channel)
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

    // Shadow history (R16F ping-pong)
    VkImage m_ShadowHistoryImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ShadowHistoryMemory = VK_NULL_HANDLE;
    VkImageView m_ShadowHistoryView = VK_NULL_HANDLE;

    // AO history (R16F)
    VkImage m_AOHistoryImage = VK_NULL_HANDLE;
    VkDeviceMemory m_AOHistoryMemory = VK_NULL_HANDLE;
    VkImageView m_AOHistoryView = VK_NULL_HANDLE;

    // Reflection history (RGBA16F)
    VkImage m_ReflectionHistoryImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ReflectionHistoryMemory = VK_NULL_HANDLE;
    VkImageView m_ReflectionHistoryView = VK_NULL_HANDLE;

    // GI history (RGBA16F)
    VkImage m_GIHistoryImage = VK_NULL_HANDLE;
    VkDeviceMemory m_GIHistoryMemory = VK_NULL_HANDLE;
    VkImageView m_GIHistoryView = VK_NULL_HANDLE;

    // Confidence map (R8 UNORM) — written by shader, read by denoiser
    VkImage m_ConfidenceImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ConfidenceMemory = VK_NULL_HANDLE;
    VkImageView m_ConfidenceView = VK_NULL_HANDLE;

    // Sampler for reading history/input textures
    VkSampler m_Sampler = VK_NULL_HANDLE;

    // Per-buffer history validity tracking
    bool m_HasShadowHistory = false;
    bool m_HasAOHistory = false;
    bool m_HasReflectionHistory = false;
    bool m_HasGIHistory = false;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
