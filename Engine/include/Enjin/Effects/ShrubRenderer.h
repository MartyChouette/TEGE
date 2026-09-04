#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <memory>

namespace Enjin {
namespace Effects {

// GPU-instanced shrub renderer
class ENJIN_API ShrubRenderer {
public:
    ShrubRenderer() = default;
    ~ShrubRenderer();

    // bindlessLayout: set-1 bindless texture layout for custom shrub textures
    // (VK_NULL_HANDLE = procedural-only, textures disabled)
    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout,
                    VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE);
    void Shutdown();

    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // mode2D: scatter along X on the XY plane (2D scenes); bindlessSet: set-1
    // textures for volumes with a resolved custom texture
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0,
                bool mode2D = false,
                VkDescriptorSet bindlessSet = VK_NULL_HANDLE);

private:
    void CreateShrubMesh();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    // The render pass the pipeline was last built for. VK_NULL_HANDLE means the
    // swapchain pass. ReloadShaders always rebuilt against the swapchain, so
    // hot-reloading a shader while the editor had retargeted this renderer at
    // its offscreen pass produced a pipeline with the wrong attachment count
    // (VUID-07609) instead of the reload you asked for.
    VkRenderPass m_LastRenderPass = VK_NULL_HANDLE;
    u32 m_LastColorAttachmentCount = 2;

    Renderer::VulkanRenderer* m_Renderer = nullptr;
    VkDescriptorSetLayout m_BindlessLayout = VK_NULL_HANDLE;

    std::unique_ptr<Renderer::VulkanBuffer> m_VertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_IndexBuffer;
    u32 m_IndexCount = 0;

    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
