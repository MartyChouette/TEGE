#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Components/Light.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

// 2D array shadow map for up to MAX_SHADOW_SPOT_LIGHTS spot lights.
// One layer per spot light.
class ENJIN_API SpotLightShadowMap {
public:
    SpotLightShadowMap(VulkanContext* context);
    ~SpotLightShadowMap();

    bool Initialize(u32 resolution = 1024);
    void Shutdown();

    // Begin/end a single spot light render pass
    void BeginPass(VkCommandBuffer commandBuffer, u32 spotIndex);
    void EndPass(VkCommandBuffer commandBuffer);

    // Compute view-projection for a spot light
    static Math::Matrix4 ComputeViewProj(const Math::Vector3& lightPos,
                                          const Math::Vector3& lightDir,
                                          f32 outerConeAngle, f32 range);

    // Getters for descriptor binding
    VkImageView GetArrayView() const { return m_ArrayView; }
    VkImage GetDepthImage() const { return m_DepthImage; }
    VkSampler GetShadowSampler() const { return m_ShadowSampler; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    u32 GetResolution() const { return m_Resolution; }

    static constexpr u32 MAX_LIGHTS = ECS::MAX_SHADOW_SPOT_LIGHTS;

private:
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateSampler();
    void DestroyResources();

    VulkanContext* m_Context = nullptr;
    u32 m_Resolution = 1024;

    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_ArrayView = VK_NULL_HANDLE;              // 2D_ARRAY view for shader sampling
    VkImageView m_LayerViews[MAX_LIGHTS] = {};             // Per-light 2D views for framebuffers
    VkFramebuffer m_Framebuffers[MAX_LIGHTS] = {};         // Per-light framebuffers
    VkSampler m_ShadowSampler = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
