#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// Shadow map configuration
struct ShadowMapConfig {
    u32 resolution = 2048;       // Shadow map resolution (square)
    f32 nearPlane = 0.1f;        // Near plane for shadow projection
    f32 farPlane = 100.0f;       // Far plane for shadow projection
    f32 orthoSize = 20.0f;       // Orthographic projection size (for directional lights)
    f32 depthBias = 0.005f;      // Depth bias to reduce shadow acne
};

// Shadow map class - manages shadow depth texture and framebuffer
class ENJIN_API ShadowMap {
public:
    ShadowMap(VulkanContext* context);
    ~ShadowMap();

    bool Initialize(const ShadowMapConfig& config = ShadowMapConfig{});
    void Shutdown();

    // Begin/end shadow pass
    void BeginShadowPass(VkCommandBuffer commandBuffer);
    void EndShadowPass(VkCommandBuffer commandBuffer);

    // Set light direction for directional light shadows
    void SetLightDirection(const Math::Vector3& direction);
    void SetLightPosition(const Math::Vector3& position);

    // Adaptive shadow mapping
    void SetResolution(u32 size);  // 512/1024/2048/4096 - recreates resources
    void SetAutoFitToCamera(bool enable) { m_AutoFit = enable; }
    void UpdateFrustum(const Math::Matrix4& viewProj, const Math::Vector3& lightDir);

    // Shadow strength (0 = no shadows, 1 = full shadows)
    void SetShadowStrength(f32 strength) { m_ShadowStrength = strength; }
    f32 GetShadowStrength() const { return m_ShadowStrength; }

    // Get matrices
    Math::Matrix4 GetLightViewMatrix() const;
    Math::Matrix4 GetLightProjectionMatrix() const;
    Math::Matrix4 GetLightSpaceMatrix() const; // proj * view

    // Getters
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    VkSampler GetShadowSampler() const { return m_ShadowSampler; }
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
    u32 GetResolution() const { return m_Config.resolution; }
    f32 GetDepthBias() const { return m_Config.depthBias; }
    bool IsAutoFitEnabled() const { return m_AutoFit; }

private:
    bool CreateDepthResources();
    bool CreateRenderPass();
    bool CreateFramebuffer();
    bool CreateSampler();
    void DestroyDepthResources();

    VulkanContext* m_Context = nullptr;
    ShadowMapConfig m_Config;

    // Vulkan resources
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;
    VkSampler m_ShadowSampler = VK_NULL_HANDLE;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;

    // Light transform
    Math::Vector3 m_LightDirection = Math::Vector3(0.5f, 0.8f, 0.3f).Normalized();
    Math::Vector3 m_LightPosition = Math::Vector3(0.0f, 10.0f, 0.0f);

    // Auto-fit frustum data
    bool m_AutoFit = true;
    f32 m_FittedOrthoSize = 30.0f;
    Math::Vector3 m_FittedCenter = Math::Vector3(0.0f);

    // Shadow strength
    f32 m_ShadowStrength = 1.0f;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
