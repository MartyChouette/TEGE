#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Effects/WeatherRenderer.h"  // Reuse ParticleInstanceData
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

// Forward declaration
namespace Enjin { namespace Effects { class ElementalSystem; } }

namespace Enjin {
namespace Effects {

// GPU instanced billboard renderer for ParticleEmitterComponent particles.
// Follows the same architecture as WeatherRenderer: shared quad mesh,
// per-instance buffer, alpha-blended depth-tested pipeline.
class ENJIN_API ParticleRenderer {
public:
    ParticleRenderer() = default;
    ~ParticleRenderer();

    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout);
    void Shutdown();

    // Recreate pipeline for a different render pass (e.g. render target vs swapchain)
    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // Gather all emitter pools into instance cache and render with a single instanced draw call.
    // viewportWidth/Height: 0 = use swapchain extent, >0 = override (for render targets)
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0);

    // Render elemental particles from the ElementalSystem using the same pipeline.
    // Called after Render() to batch elemental particles alongside regular ones.
    void RenderElementalParticles(VkCommandBuffer commandBuffer,
                                  const std::vector<VkDescriptorSet>& descriptorSets,
                                  u32 currentFrame,
                                  const ElementalSystem& elementalSystem,
                                  u32 viewportWidth = 0,
                                  u32 viewportHeight = 0);

private:
    void CreateQuadBuffers();
    void CreateInstanceBuffer();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    Renderer::VulkanRenderer* m_Renderer = nullptr;

    // Shared quad mesh (4 vertices, 6 indices)
    std::unique_ptr<Renderer::VulkanBuffer> m_QuadVertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_QuadIndexBuffer;

    // Per-instance buffer (updated each frame)
    std::unique_ptr<Renderer::VulkanBuffer> m_InstanceBuffer;
    static constexpr u32 MAX_PARTICLES = 16384;

    // Pipeline
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    // Reusable instance data cache to avoid per-frame allocation
    std::vector<ParticleInstanceData> m_InstanceDataCache;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
