#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Matrix.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;
class VulkanPipeline;
class VulkanShader;

// Visibility buffer renderer — alternative to forward/deferred rendering.
// Pass 1 (Visibility): Ultra-thin geometry pass writes only triangle ID + instance ID
//                       to an R32G32_UINT render target. No material evaluation.
// Pass 2 (Resolve):    Full-screen compute shader fetches vertex data, computes barycentrics,
//                       evaluates materials, and performs lighting. Output = final color.
//
// Advantages: Decouples geometry complexity from shading cost, handles many small triangles
//             efficiently, naturally eliminates overdraw (every pixel shaded exactly once).
class ENJIN_API VisibilityBufferRenderer {
public:
    VisibilityBufferRenderer(VulkanContext* context);
    ~VisibilityBufferRenderer();

    bool Initialize(u32 width, u32 height, VkRenderPass compatibleRenderPass);
    void Shutdown();

    // Pass 1: Render visibility buffer (triangle ID + instance ID)
    // Should be called with a simple depth-only + ID-write pipeline.
    void BeginVisibilityPass(VkCommandBuffer commandBuffer);
    void EndVisibilityPass(VkCommandBuffer commandBuffer);

    // Pass 2: Material resolve (full-screen compute)
    void ResolvePass(
        VkCommandBuffer commandBuffer,
        VkImageView outputColor,           // Output color image (storage)
        VkBuffer vertexBuffer,             // Scene vertex data
        VkBuffer indexBuffer,              // Scene index data
        VkBuffer objectDataBuffer,         // Per-object transform/material
        const Math::Matrix4& viewProj,
        const Math::Matrix4& invViewProj
    );

    // Get the visibility buffer for debug visualization
    VkImageView GetVisibilityBufferView() const { return m_VisBufferView; }
    VkImageView GetDepthBufferView() const { return m_DepthView; }

    void Resize(u32 width, u32 height);

    // Get the render pass for the visibility pass
    VkRenderPass GetVisibilityRenderPass() const { return m_VisRenderPass; }
    VkFramebuffer GetVisibilityFramebuffer() const { return m_VisFramebuffer; }

private:
    bool CreateVisibilityBuffer();
    bool CreateVisibilityRenderPass();
    bool CreateVisibilityFramebuffer();
    bool CreateVisibilityPipeline();
    bool CreateResolvePipeline();
    void DestroyResources();

    VulkanContext* m_Context = nullptr;
    u32 m_Width = 0;
    u32 m_Height = 0;

    // Visibility buffer: R32G32_UINT (triangleID, instanceID)
    VkImage m_VisBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VisBufferMemory = VK_NULL_HANDLE;
    VkImageView m_VisBufferView = VK_NULL_HANDLE;

    // Depth buffer for visibility pass
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthView = VK_NULL_HANDLE;

    // Visibility pass resources
    VkRenderPass m_VisRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_VisFramebuffer = VK_NULL_HANDLE;

    // Visibility pass pipeline (writes IDs only)
    VkPipeline m_VisPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_VisPipelineLayout = VK_NULL_HANDLE;

    // Material resolve compute pipeline
    VkPipeline m_ResolvePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_ResolvePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ResolveDescSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_ResolveDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_ResolveDescSet = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
