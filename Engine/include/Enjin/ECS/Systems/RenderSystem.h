#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/System.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Camera.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>

namespace Enjin {
namespace ECS {

// Per-entity rendering data
struct EntityRenderData {
    std::unique_ptr<Renderer::VulkanBuffer> vertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> indexBuffer;
    u32 indexCount = 0;
};

// Render system - renders entities with Transform and Mesh components
class ENJIN_API RenderSystem : public ISystem {
public:
    RenderSystem(World* world, Renderer::VulkanRenderer* renderer);
    ~RenderSystem();

    void Initialize();
    void Shutdown();

    void Update(f32 deltaTime) override;
    void OnEntityAdded(Entity entity) override;
    void OnEntityRemoved(Entity entity) override;

    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }

private:
    void RenderEntity(Entity entity);
    void CreateTriangleMesh();
    void CreatePipeline();
    void CreateUniformBuffers();
    void CreateDescriptorSets();
    void UpdateUniformBuffer(Entity entity);
    void SetupEntityBuffers(Entity entity);

    World* m_World = nullptr;
    Renderer::VulkanRenderer* m_Renderer = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Entity m_TriangleEntity = INVALID_ENTITY;

    // Rendering resources
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;
    
    // Uniform buffers (one per frame in flight)
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_UniformBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_LightingBuffers;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    
    // Per-entity render data
    std::unordered_map<Entity, EntityRenderData> m_EntityRenderData;
    
    bool m_Initialized = false;
};

} // namespace ECS
} // namespace Enjin
