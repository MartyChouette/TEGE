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
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/ShadowMap.h"
#include "Enjin/Renderer/Texture.h"
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

    // Render all entities to an offscreen render target using a custom camera
    // Must be called outside of the main render pass (before BeginMainRenderPass)
    void RenderToTarget(Renderer::RenderTarget* target, Renderer::Camera* camera);

private:
    void RenderEntity(Entity entity);
    void RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer);
    void CreateDefaultMesh();
    void CreatePipeline();
    void CreateShadowPipeline();
    void CreateUniformBuffers();
    void CreateDescriptorSets();
    void UpdateUniformBuffer(Entity entity);
    void SetupEntityBuffers(Entity entity);
    void RenderShadowPass();

    World* m_World = nullptr;
    Renderer::VulkanRenderer* m_Renderer = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Entity m_DefaultEntity = INVALID_ENTITY;

    // Rendering resources
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    // Shadow mapping
    std::unique_ptr<Renderer::ShadowMap> m_ShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_ShadowPipeline;
    bool m_ShadowsEnabled = true;

    // Textures
    std::unique_ptr<Renderer::Texture> m_DefaultWhiteTexture;
    std::unordered_map<std::string, std::shared_ptr<Renderer::Texture>> m_TextureCache;

    // Helper to load or get cached texture
    std::shared_ptr<Renderer::Texture> GetOrLoadTexture(const std::string& path);
    void UpdateTextureDescriptor(Renderer::Texture* texture);
    
    // Uniform buffers (one per frame in flight)
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_UniformBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_LightingBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_MaterialBuffers;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    
    // Per-entity render data
    std::unordered_map<Entity, EntityRenderData> m_EntityRenderData;
    
    bool m_Initialized = false;
};

} // namespace ECS
} // namespace Enjin
