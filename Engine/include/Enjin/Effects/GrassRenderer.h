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

    // Render all grass volumes in the scene
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world);

private:
    void CreateBladeMesh();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);

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
