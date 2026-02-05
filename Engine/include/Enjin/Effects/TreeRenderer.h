#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Effects/WorldTime.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Enjin {
namespace Effects {

// GPU-instanced tree renderer (trunk + canopy)
class ENJIN_API TreeRenderer {
public:
    TreeRenderer() = default;
    ~TreeRenderer();

    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout);
    void Shutdown();

    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0);

    // Generate box colliders at each tree trunk position within a volume
    void GenerateColliders(ECS::World* world, ECS::Entity volumeEntity);

    // Seasonal state (driven by WorldTimeSystem)
    void SetSeasonState(Season season, f32 progress);

private:
    Season m_CurrentSeason = Season::Summer;
    f32 m_SeasonProgress = 0.0f;
    void CreateTreeMesh();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

    Renderer::VulkanRenderer* m_Renderer = nullptr;

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
