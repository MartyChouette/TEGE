#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Techniques/RenderingTechnique.h"
#include "Enjin/Renderer/Vulkan/VulkanImage.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanPipeline;
class VulkanShader;

/**
 * @brief Deferred Rendering Technique
 * 
 * Deferred rendering pipeline:
 * 1. Geometry Pass: Render to G-Buffer (position, normal, albedo, etc.)
 * 2. Lighting Pass: Calculate lighting from G-Buffer
 * 3. Forward Pass: Render transparent objects
 * 
 * Advantages:
 * - Supports many lights efficiently
 * - No overdraw for opaque geometry
 * - Better performance for complex scenes
 */
class ENJIN_API DeferredRendering : public RenderingTechnique {
public:
    DeferredRendering();
    ~DeferredRendering() override;

    bool Initialize(VulkanRenderer* renderer, RenderPipeline* pipeline) override;
    void Shutdown() override;
    void Render(f32 deltaTime) override;
    std::string GetDescription() const override { return "Deferred rendering - G-Buffer + lighting pass"; }

private:
    // G-Buffer creation
    bool CreateGBuffer();
    void DestroyGBuffer();
    
    // Render passes
    bool CreateGeometryPass();
    bool CreateLightingPass();
    
    // G-Buffer resources
    struct GBuffer {
        std::unique_ptr<VulkanImage> position;
        std::unique_ptr<VulkanImage> normal;
        std::unique_ptr<VulkanImage> albedo;
        std::unique_ptr<VulkanImage> depth;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkRenderPass renderPass = VK_NULL_HANDLE;
    } m_GBuffer;
    
    // Lighting pass resources
    std::unique_ptr<VulkanPipeline> m_LightingPipeline;
    std::unique_ptr<VulkanShader> m_LightingVertexShader;
    std::unique_ptr<VulkanShader> m_LightingFragmentShader;
    VkDescriptorSetLayout m_LightingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_LightingDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool m_LightingDescriptorPool = VK_NULL_HANDLE;
    
    u32 m_Width = 0;
    u32 m_Height = 0;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
