#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

static constexpr u32 MAX_SHADOW_CASCADES = 4;

// Shadow map configuration
struct ShadowMapConfig {
    u32 resolution = 2048;       // Shadow map resolution (square)
    u32 cascadeCount = 4;        // Number of cascaded shadow maps
    f32 shadowDistance = 100.0f;  // Maximum shadow distance from camera
    f32 splitLambda = 0.75f;     // Log/linear split blend (0=linear, 1=logarithmic)
    f32 depthBias = 0.001f;      // Depth bias to reduce shadow acne
};

// Shadow map class - manages cascaded shadow depth textures and framebuffers
class ENJIN_API ShadowMap {
public:
    ShadowMap(VulkanContext* context);
    ~ShadowMap();

    bool Initialize(const ShadowMapConfig& config = ShadowMapConfig{});
    void Shutdown();

    // Cascade frustum computation
    void UpdateCascades(const Math::Matrix4& cameraView, const Math::Matrix4& cameraProj,
                        f32 cameraNear, f32 cameraFar, const Math::Vector3& lightDir);

    // Begin/end per-cascade shadow pass
    void BeginCascadePass(VkCommandBuffer commandBuffer, u32 cascadeIndex, bool useSecondary = false);
    VkFramebuffer GetCurrentFramebuffer() const { return m_CurrentFramebuffer; }
    void EndCascadePass(VkCommandBuffer commandBuffer);
    // Sets the cascade-sized dynamic viewport/scissor on a command buffer. Called on
    // the primary in inline mode, and on each secondary in parallel mode (dynamic
    // state does not cross the primary/secondary boundary).
    void ApplyCascadeViewportScissor(VkCommandBuffer commandBuffer) const;

    // Adaptive shadow mapping
    void SetResolution(u32 size);  // 512/1024/2048/4096 - recreates resources
    void SetShadowDistance(f32 distance) { m_Config.shadowDistance = distance; }
    f32 GetShadowDistance() const { return m_Config.shadowDistance; }

    // Shadow strength (0 = no shadows, 1 = full shadows)
    void SetShadowStrength(f32 strength) { m_ShadowStrength = strength; }
    f32 GetShadowStrength() const { return m_ShadowStrength; }

    // Shadow softness (0 = hard 3x3 PCF, >0 = soft Poisson disk radius in texels)
    void SetShadowSoftness(f32 softness) { m_ShadowSoftness = softness; }
    f32 GetShadowSoftness() const { return m_ShadowSoftness; }

    // Per-cascade data access
    const Math::Matrix4& GetCascadeViewProj(u32 index) const;
    f32 GetCascadeSplit(u32 index) const;
    u32 GetCascadeCount() const { return m_Config.cascadeCount; }

    // Getters
    VkImageView GetDepthArrayView() const { return m_DepthArrayView; }
    VkImage GetDepthImage() const { return m_DepthImage; }
    VkSampler GetShadowSampler() const { return m_ShadowSampler; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    u32 GetResolution() const { return m_Config.resolution; }
    f32 GetDepthBias() const { return m_Config.depthBias; }

private:
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateSampler();
    void DestroyDepthResources();

    VulkanContext* m_Context = nullptr;
    ShadowMapConfig m_Config;

    // Vulkan resources - layered depth image for all cascades
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthArrayView = VK_NULL_HANDLE;                   // 2D_ARRAY view for shader sampling
    VkImageView m_CascadeViews[MAX_SHADOW_CASCADES] = {};            // Per-layer views for framebuffer
    VkFramebuffer m_CascadeFramebuffers[MAX_SHADOW_CASCADES] = {};   // Per-cascade framebuffer
    VkSampler m_ShadowSampler = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_CurrentFramebuffer = VK_NULL_HANDLE;

    // Per-cascade computed data
    Math::Matrix4 m_CascadeViewProj[MAX_SHADOW_CASCADES];
    f32 m_CascadeSplits[MAX_SHADOW_CASCADES] = {};  // View-space far distance of each cascade

    // Shadow strength
    f32 m_ShadowStrength = 1.0f;

    // Shadow softness (0 = hard 3x3 PCF, >0 = Poisson disk radius in texels)
    f32 m_ShadowSoftness = 0.0f;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
