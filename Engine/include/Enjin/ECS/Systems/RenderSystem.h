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
#include "Enjin/Renderer/TextRasterizer.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/WeatherRenderer.h"
#include "Enjin/Effects/GrassRenderer.h"
#include "Enjin/Effects/ShrubRenderer.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Assets/FileWatcher.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Enjin {
namespace ECS {

// Splitscreen viewport camera — associates an entity (with CameraComponent + TransformComponent)
// with a normalized viewport rectangle within the render target.
struct ViewportCamera {
    Entity entity;       // Camera entity
    f32 viewportX;       // Normalized X offset (0-1)
    f32 viewportY;       // Normalized Y offset (0-1)
    f32 viewportWidth;   // Normalized width (0-1)
    f32 viewportHeight;  // Normalized height (0-1)
};

// Per-entity rendering data
struct EntityRenderData {
    std::unique_ptr<Renderer::VulkanBuffer> vertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> indexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> boneBuffer;
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

    // Render multiple cameras to a single render target using viewport subdivision (splitscreen)
    // Each ViewportCamera defines a normalized rect within the target.
    // The render pass must already be started by the caller.
    void RenderSplitscreen(Renderer::RenderTarget* target, const std::vector<ViewportCamera>& viewports);

    static constexpr u32 MAX_SPLITSCREEN_VIEWPORTS = 4;

    // Set splitscreen viewports for the main render pass (used by Player).
    // When non-empty, Update() renders each viewport instead of a single full-screen camera.
    // Call with empty vector to disable splitscreen.
    void SetMainPassSplitscreen(const std::vector<ViewportCamera>& viewports) {
        m_MainPassViewports = viewports;
    }

    // Runtime rendering settings
    bool IsShadowsEnabled() const { return m_ShadowsEnabled; }
    void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }

    bool IsBackfaceCullingEnabled() const { return m_BackfaceCulling; }
    void SetBackfaceCullingEnabled(bool enabled);

    bool IsWireframeEnabled() const { return m_WireframeMode; }
    void SetWireframeEnabled(bool enabled);

    // Render line-list geometry with depth testing (for editor overlays)
    void RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                         u32 firstVertex, const Math::Vector3& color, f32 opacity);

    f32 GetAmbientIntensity() const { return m_AmbientIntensity; }
    void SetAmbientIntensity(f32 intensity) { m_AmbientIntensity = intensity; }

    Math::Vector3 GetAmbientColor() const { return m_AmbientColor; }
    void SetAmbientColor(const Math::Vector3& color) { m_AmbientColor = color; }

    // Wind system (shared, not owned)
    void SetWindSystem(Effects::WindSystem* wind) { m_WindSystem = wind; }
    Effects::WindSystem* GetWindSystem() const { return m_WindSystem; }

    // Rain active state (drives water ripple effects in shader)
    void SetRainActive(bool active) { m_RainActive = active; }
    bool IsRainActive() const { return m_RainActive; }

    // Weather, grass, and tree renderers (initialized after main pipeline)
    Effects::WeatherRenderer* GetWeatherRenderer() { return m_WeatherRenderer.get(); }
    Effects::GrassRenderer* GetGrassRenderer() { return m_GrassRenderer.get(); }
    Effects::TreeRenderer* GetTreeRenderer() { return m_TreeRenderer.get(); }

    // Render weather particles, grass, shrubs, and trees (call after scene geometry in main render pass)
    // viewportWidth/Height: 0 = swapchain, >0 = render target override
    void RenderWeatherParticles(const Effects::WeatherSystem& weather, bool isRain,
                                u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderGrass(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderShrubs(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderTrees(u32 viewportWidth = 0, u32 viewportHeight = 0);

    // Recreate effect renderer pipelines for a specific render pass (e.g. render target)
    void RecreateEffectPipelinesForRenderPass(VkRenderPass renderPass);

    // Draw call / triangle counters (reset each frame in Update)
    u32 GetDrawCallCount() const { return m_DrawCallCount; }
    u32 GetTriangleCount() const { return m_TriangleCount; }
    void ResetFrameCounters() { m_DrawCallCount = 0; m_TriangleCount = 0; }

    // Fog and snow parameters (set by editor, uploaded to LightingUBO)
    void SetFogParams(f32 density, f32 start, f32 end, f32 heightFalloff) {
        m_FogDensity = density; m_FogStart = start; m_FogEnd = end; m_FogHeightFalloff = heightFalloff;
    }
    void SetFogColor(const Math::Vector3& color) { m_FogColor = color; }
    void SetSnowIntensity(f32 intensity) { m_SnowIntensity = intensity; }

    // World curvature (vertex-shader horizon bending)
    void SetWorldCurvature(f32 strength) { m_WorldCurvature = strength; }
    f32 GetWorldCurvature() const { return m_WorldCurvature; }

    // Skybox
    void SetSkybox(const Renderer::SkyboxConfig& config);
    const Renderer::SkyboxConfig& GetSkyboxConfig() const { return m_Skybox.GetConfig(); }
    Renderer::Skybox* GetSkybox() { return &m_Skybox; }

    // Access descriptor sets for sub-renderers
    const std::vector<VkDescriptorSet>& GetDescriptorSets() const { return m_DescriptorSets; }

    // Compute offscreen buffer/descriptor set index for a given frame and viewport
    static u32 GetOffscreenBufferIndex(u32 frameIndex, u32 viewportIndex) {
        return frameIndex * MAX_SPLITSCREEN_VIEWPORTS + viewportIndex;
    }

private:
    void RenderEntity(Entity entity);
    void RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer);
    void CreateDefaultMesh();
    void CreatePipeline();
    void CreateShadowPipeline();
    void RecreatePipelines();
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

    // Line rendering (editor grid)
    std::unique_ptr<Renderer::VulkanPipeline> m_LinePipeline;
    void CreateLinePipeline();

    // Shadow mapping
    std::unique_ptr<Renderer::ShadowMap> m_ShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_ShadowPipeline;
    bool m_ShadowsEnabled = true;
    bool m_BackfaceCulling = false;
    bool m_WireframeMode = false;
    Effects::WindSystem* m_WindSystem = nullptr;
    bool m_RainActive = false;
    f32 m_AmbientIntensity = 1.0f;
    Math::Vector3 m_AmbientColor = Math::Vector3(0.1f, 0.1f, 0.15f);

    // Fog parameters
    f32 m_FogDensity = 0.0f;
    f32 m_FogStart = 20.0f;
    f32 m_FogEnd = 100.0f;
    f32 m_FogHeightFalloff = 0.1f;
    Math::Vector3 m_FogColor = Math::Vector3(0.5f, 0.5f, 0.6f);
    f32 m_SnowIntensity = 0.0f;
    f32 m_WorldCurvature = 0.0f;

    // Textures
    std::unique_ptr<Renderer::Texture> m_DefaultWhiteTexture;
    std::unordered_map<std::string, std::shared_ptr<Renderer::Texture>> m_TextureCache;

    // Text rendering
    Renderer::TextRasterizer m_TextRasterizer;
    std::unordered_map<Entity, std::shared_ptr<Renderer::Texture>> m_TextTextureCache;

    // Skeletal animation
    std::unique_ptr<Renderer::VulkanBuffer> m_DefaultBoneBuffer;
    void UpdateBoneDescriptor(Renderer::VulkanBuffer* boneBuffer);

    // Weather, grass, shrub, and tree renderers
    std::unique_ptr<Effects::WeatherRenderer> m_WeatherRenderer;
    std::unique_ptr<Effects::GrassRenderer> m_GrassRenderer;
    std::unique_ptr<Effects::ShrubRenderer> m_ShrubRenderer;
    std::unique_ptr<Effects::TreeRenderer> m_TreeRenderer;

    // Skybox
    Renderer::Skybox m_Skybox;
    VkPipeline m_SkyboxPipelineHandle = VK_NULL_HANDLE;
    VkPipelineLayout m_SkyboxPipelineLayoutHandle = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_SkyboxDescriptorSetLayoutHandle = VK_NULL_HANDLE;
    std::unique_ptr<Renderer::VulkanBuffer> m_SkyboxVertexBuffer;
    VkDescriptorPool m_SkyboxDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_SkyboxDescriptorSets;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_SkyboxUniformBuffers;
    void CreateSkyboxPipeline();
    void RenderSkybox(VkCommandBuffer commandBuffer);
    void CreateSkyboxCubeVBO();

    // Water surface mesh generation
    void EnsureWaterMeshes();

    // Helper to load or get cached texture
    std::shared_ptr<Renderer::Texture> GetOrLoadTexture(const std::string& path);
    void UpdateTextureDescriptor(Renderer::Texture* texture);
    void UpdateHeightTextureDescriptor(Renderer::Texture* texture);
    void UpdateNormalMapDescriptor(Renderer::Texture* texture);
    void UpdateMetallicRoughnessDescriptor(Renderer::Texture* texture);
    void UpdateEmissiveDescriptor(Renderer::Texture* texture);

    // Split uniform updates: frame-level (once) vs per-entity (material only)
    void UpdateFrameUniforms();
    void UpdateMaterialBuffer(Entity entity);

    // Uniform buffers (one per frame in flight)
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_UniformBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_LightingBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_MaterialBuffers;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Offscreen (game view) uniform buffers + descriptor sets
    // Separate from main pass so CPU writes don't overwrite each other
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_OffscreenUniformBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_OffscreenLightingBuffers;
    std::vector<VkDescriptorSet> m_OffscreenDescriptorSets;

    // Active rendering target pointers — swapped for offscreen passes
    std::vector<VkDescriptorSet>* m_ActiveDescriptorSets = nullptr;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>>* m_ActiveUniformBuffers = nullptr;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>>* m_ActiveLightingBuffers = nullptr;

    // When true, active buffer/descriptor indexing uses GetOffscreenBufferIndex
    bool m_OffscreenMode = false;
    u32 m_CurrentViewportIndex = 0;

    // Get the actual index into the active buffer/descriptor arrays for the current frame.
    // In main-pass mode, this returns currentFrame directly.
    // In offscreen mode, this returns frame * MAX_SPLITSCREEN_VIEWPORTS + viewportIndex.
    u32 GetActiveBufferIndex(u32 currentFrame) const {
        if (m_OffscreenMode) {
            return GetOffscreenBufferIndex(currentFrame, m_CurrentViewportIndex);
        }
        return currentFrame;
    }

    // Splitscreen viewports for the main render pass (set by Player)
    std::vector<ViewportCamera> m_MainPassViewports;

    // Per-entity render data
    std::unordered_map<Entity, EntityRenderData> m_EntityRenderData;

    // Draw call / triangle counters
    u32 m_DrawCallCount = 0;
    u32 m_TriangleCount = 0;

    // Asset hot-reload watcher (polls texture files for changes)
    Assets::FileWatcher m_TextureWatcher;
    u32 m_WatcherPollCounter = 0;

    bool m_Initialized = false;
};

} // namespace ECS
} // namespace Enjin
