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

// Cubemap array shadow map for up to MAX_SHADOW_POINT_LIGHTS point lights.
// Each light uses 6 cubemap faces. Total layers = MAX_SHADOW_POINT_LIGHTS * 6 = 24.
class ENJIN_API PointLightShadowMap {
public:
    PointLightShadowMap(VulkanContext* context);
    ~PointLightShadowMap();

    bool Initialize(u32 resolution = 512);
    void Shutdown();

    // Begin/end a single face render pass
    void BeginFacePass(VkCommandBuffer commandBuffer, u32 lightIndex, u32 faceIndex);
    void EndFacePass(VkCommandBuffer commandBuffer);

    // Compute the view-projection matrix for a given face of a point light
    static Math::Matrix4 ComputeFaceViewProj(const Math::Vector3& lightPos, f32 range,
                                              u32 faceIndex);

    // Getters for descriptor binding
    VkImageView GetCubeArrayView() const { return m_CubeArrayView; }
    VkImage GetDepthImage() const { return m_DepthImage; }
    VkSampler GetShadowSampler() const { return m_ShadowSampler; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    u32 GetResolution() const { return m_Resolution; }

    static constexpr u32 FACE_COUNT = 6;
    static constexpr u32 MAX_LIGHTS = ECS::MAX_SHADOW_POINT_LIGHTS;
    static constexpr u32 TOTAL_LAYERS = MAX_LIGHTS * FACE_COUNT; // 24

private:
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateSampler();
    void DestroyResources();

    VulkanContext* m_Context = nullptr;
    u32 m_Resolution = 512;

    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_CubeArrayView = VK_NULL_HANDLE;             // CUBE_ARRAY view for shader sampling
    VkImageView m_FaceViews[TOTAL_LAYERS] = {};                // Per-face 2D views for framebuffers
    VkFramebuffer m_FaceFramebuffers[TOTAL_LAYERS] = {};       // Per-face framebuffers
    VkSampler m_ShadowSampler = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
