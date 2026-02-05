#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Enjin {
namespace Effects {

// GPU-instanced grass blade renderer
// Iterates GrassVolumeComponent entities and renders procedurally-placed blades
class ENJIN_API GrassRenderer {
public:
    GrassRenderer() = default;
    ~GrassRenderer();

    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout);
    void Shutdown();

    // Recreate pipeline for a different render pass (e.g. render target vs swapchain)
    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // Render all grass volumes in the scene
    // viewportWidth/Height: 0 = use swapchain extent, >0 = override (for render targets)
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0);

private:
    void CreateBladeMesh();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

    Renderer::VulkanRenderer* m_Renderer = nullptr;

    // Blade mesh (7 verts, tapered triangle strip)
    std::unique_ptr<Renderer::VulkanBuffer> m_BladeVertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_BladeIndexBuffer;
    u32 m_BladeIndexCount = 0;

    // Pipeline
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
