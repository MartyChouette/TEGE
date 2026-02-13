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
#include "Enjin/Renderer/PointLightShadowMap.h"
#include "Enjin/Renderer/SpotLightShadowMap.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/Renderer/TextRasterizer.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/WeatherRenderer.h"
#include "Enjin/Effects/ParticleRenderer.h"
#include "Enjin/Effects/FluidRenderer.h"
#include "Enjin/Effects/SpriteBatchRenderer.h"
#include "Enjin/Effects/SpriteTextureAtlas.h"
#include "Enjin/Effects/GrassRenderer.h"
#include "Enjin/Effects/ShrubRenderer.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/GPUDriven/GPUCulling.h"
#include "Enjin/Renderer/GPUDriven/MergedGeometryBuffer.h"
#include "Enjin/Renderer/GPUDriven/HiZPyramid.h"
#include "Enjin/Renderer/Vulkan/ThreadPool.h"
#include "Enjin/Renderer/Vulkan/CommandBufferPool.h"
#include "Enjin/Assets/FileWatcher.h"
#include "Enjin/Renderer/RayTracing/RTCapabilities.h"
#include "Enjin/Editor/FlashTimeline.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>

// Forward declarations for RT subsystems and rendering infrastructure
namespace Enjin { namespace Renderer {
    class AccelerationStructureManager;
    class RTShadows;
    class RTReflections;
    class RTAmbientOcclusion;
    class RTGlobalIllumination;
    class PathTracer;
    class SVGFDenoiser;
    class RTCompositor;
    class OITManager;
    class SHLightingSystem;
    class SDFScene;
}}

namespace Enjin {
namespace ECS {

// Scene rendering mode — auto-detected per frame from entity composition.
// Controls which rendering features are active to avoid unnecessary GPU work.
enum class SceneRenderMode : u8 {
    Scene2D,    // Only sprites/tilemaps, no 3D meshes — skip shadows, skip 3D lighting
    Scene2_5D,  // Sprites + lights but no 3D meshes — skip shadows, keep lighting
    Scene3D     // 3D meshes present — full pipeline
};

// Cached scene composition data for per-frame rendering decisions.
// Invalidated on entity add/remove, recomputed lazily before shadow pass.
struct SceneComposition {
    SceneRenderMode mode = SceneRenderMode::Scene3D;
    u32 spriteCount = 0;
    u32 tilemapCount = 0;
    u32 mesh3DCount = 0;
    bool hasShadowCastingLights = false;
    bool dirty = true;
};

// Splitscreen viewport camera — associates an entity (with CameraComponent + TransformComponent)
// with a normalized viewport rectangle within the render target.
struct ViewportCamera {
    Entity entity;       // Camera entity
    f32 viewportX;       // Normalized X offset (0-1)
    f32 viewportY;       // Normalized Y offset (0-1)
    f32 viewportWidth;   // Normalized width (0-1)
    f32 viewportHeight;  // Normalized height (0-1)
};

// Per-object GPU data for indirect draws (std430, matches GLSL ObjectData)
struct ObjectDataGPU {
    Math::Matrix4 model;             // 64 bytes
    Math::Vector3 baseColor;         // 12 bytes
    f32 metallic;                    // 4 bytes
    Math::Vector3 emissiveColor;     // 12 bytes
    f32 roughness;                   // 4 bytes
    f32 emissiveStrength;            // 4 bytes
    f32 opacity;                     // 4 bytes
    f32 alphaCutoff;                 // 4 bytes
    i32 flags;                       // 4 bytes
    f32 parallaxScale;               // 4 bytes
    f32 _pad[3];                     // 12 bytes (pad to 128 total)
};
static_assert(sizeof(ObjectDataGPU) == 128, "ObjectDataGPU must be 128 bytes for std430");

// Per-entity rendering data (stored in dense vector indexed by entity ID)
struct EntityRenderData {
    std::unique_ptr<Renderer::VulkanBuffer> vertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> indexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> boneBuffer;
    u32 indexCount = 0;
    bool valid = false;  // true if this slot is occupied
    // Merged geometry pool allocation (valid when entity uses the shared pool)
    Renderer::MeshAllocation poolAlloc;

    void Invalidate() {
        vertexBuffer.reset();
        indexBuffer.reset();
        boneBuffer.reset();
        indexCount = 0;
        valid = false;
        poolAlloc = {};
    }
};

// Render system - renders entities with Transform and Mesh components
class ENJIN_API RenderSystem : public ISystem {
public:
    RenderSystem(World* world, Renderer::VulkanRenderer* renderer);
    ~RenderSystem();

    void Initialize();
    void Shutdown();

    // Process deferred changes (skybox config, pipeline recreation) — call at frame start,
    // BEFORE any command buffer recording (RenderOffscreen, Update, etc.)
    void FlushPendingChanges();

    void Update(f32 deltaTime) override;
    void OnEntityAdded(Entity entity) override;
    void OnEntityRemoved(Entity entity) override;

    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }

    // Editor mode: when true, GPU frustum culling is disabled so all entities
    // are visible in the scene view for editing. The Player leaves this false
    // so the game camera frustum culls normally.
    void SetEditorMode(bool editor) { m_IsEditorMode = editor; }

    // When true, skip shadow/point/spot shadow passes in the main Update() pass.
    // Used during play mode — the game view already runs its own shadow pass via
    // RenderShadowPassForCamera(), so the editor viewport shadows are redundant.
    void SetSkipMainPassShadows(bool skip) { m_SkipMainPassShadows = skip; }
    void SetSkipMainPassRendering(bool skip) { m_SkipMainPassRendering = skip; }

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

    // Run the shadow pass for an offscreen camera (call BEFORE the render target's Begin()).
    // The shadow pass uses its own framebuffer, so it must not be inside another render pass.
    void RenderShadowPassForCamera(Renderer::Camera* camera);

    // Runtime rendering settings
    bool IsShadowsEnabled() const { return m_ShadowsEnabled; }
    void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }

    // Shadow quality settings
    f32 GetShadowDistance() const { return m_ShadowDistance; }
    void SetShadowDistance(f32 d);
    f32 GetShadowStrength() const;
    void SetShadowStrength(f32 s);
    f32 GetShadowSoftness() const;
    void SetShadowSoftness(f32 s);
    u32 GetShadowResolution() const;
    void SetShadowResolution(u32 r);

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

    // Weather system for main pass rendering (editor viewport)
    // When set, weather particles are rendered in the main pass
    void SetMainPassWeather(Effects::WeatherSystem* weather, bool isRain) {
        m_MainPassWeather = weather;
        m_MainPassWeatherIsRain = isRain;
    }
    void ClearMainPassWeather() { m_MainPassWeather = nullptr; }

    // Set fluid simulation (for FluidRenderer to read grid data)
    void SetFluidSimulation(Effects::FluidSimulation* sim);

    // Weather, grass, and tree renderers (initialized after main pipeline)
    Effects::WeatherRenderer* GetWeatherRenderer() { return m_WeatherRenderer.get(); }
    Effects::GrassRenderer* GetGrassRenderer() { return m_GrassRenderer.get(); }
    Effects::TreeRenderer* GetTreeRenderer() { return m_TreeRenderer.get(); }

    // Render weather particles, game particles, grass, shrubs, and trees
    // (call after scene geometry in main render pass)
    // viewportWidth/Height: 0 = swapchain, >0 = render target override
    void RenderWeatherParticles(const Effects::WeatherSystem& weather, bool isRain,
                                u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderParticles(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderFluid(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderGrass(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderShrubs(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderTrees(u32 viewportWidth = 0, u32 viewportHeight = 0);

    // Recreate effect renderer pipelines for a specific render pass (e.g. render target)
    void RecreateEffectPipelinesForRenderPass(VkRenderPass renderPass);

    // Scene composition (auto-detected rendering mode)
    SceneRenderMode GetSceneRenderMode() const { return m_SceneComposition.mode; }
    const SceneComposition& GetSceneComposition() const { return m_SceneComposition; }

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

    // Fog and snow getters
    f32 GetFogDensity() const { return m_FogDensity; }
    f32 GetFogStart() const { return m_FogStart; }
    f32 GetFogEnd() const { return m_FogEnd; }
    f32 GetFogHeightFalloff() const { return m_FogHeightFalloff; }
    Math::Vector3 GetFogColor() const { return m_FogColor; }
    f32 GetSnowIntensity() const { return m_SnowIntensity; }

    // World curvature (vertex-shader horizon bending)
    void SetWorldCurvature(f32 strength) { m_WorldCurvature = strength; }
    f32 GetWorldCurvature() const { return m_WorldCurvature; }

    // Global retro shader overrides (forced on all entities when true)
    bool GetGlobalFlatShading() const { return m_GlobalFlatShading; }
    void SetGlobalFlatShading(bool v) { m_GlobalFlatShading = v; }
    bool GetGlobalAffineTexturing() const { return m_GlobalAffineTexturing; }
    void SetGlobalAffineTexturing(bool v) { m_GlobalAffineTexturing = v; }
    bool GetGlobalVertexSnapping() const { return m_GlobalVertexSnapping; }
    void SetGlobalVertexSnapping(bool v) { m_GlobalVertexSnapping = v; }
    bool GetGlobalStippleTransparency() const { return m_GlobalStippleTransparency; }
    void SetGlobalStippleTransparency(bool v) { m_GlobalStippleTransparency = v; }
    bool GetGlobalUVQuantize() const { return m_GlobalUVQuantize; }
    void SetGlobalUVQuantize(bool v) { m_GlobalUVQuantize = v; }
    bool GetGlobalGouraudOnly() const { return m_GlobalGouraudOnly; }
    void SetGlobalGouraudOnly(bool v) { m_GlobalGouraudOnly = v; }
    u8 GetGlobalVertexSnapResolution() const { return m_GlobalVertexSnapResolution; }
    void SetGlobalVertexSnapResolution(u8 v) { m_GlobalVertexSnapResolution = v; }

    // Cel shading (lighting quantization)
    bool IsCelShadingEnabled() const { return m_CelShadingEnabled; }
    void SetCelShadingEnabled(bool enabled) { m_CelShadingEnabled = enabled; }
    f32 GetCelDiffuseBands() const { return m_CelDiffuseBands; }
    void SetCelDiffuseBands(f32 bands) { m_CelDiffuseBands = bands; }
    f32 GetCelSpecularCutoff() const { return m_CelSpecularCutoff; }
    void SetCelSpecularCutoff(f32 cutoff) { m_CelSpecularCutoff = cutoff; }

    // Skybox
    void SetSkybox(const Renderer::SkyboxConfig& config);
    const Renderer::SkyboxConfig& GetSkyboxConfig() const { return m_Skybox.GetConfig(); }
    Renderer::Skybox* GetSkybox() { return &m_Skybox; }

    // Ray tracing
    bool IsRayTracingSupported() const;
    bool IsRayTracingEnabled() const { return m_RTEnabled; }
    void SetRayTracingEnabled(bool enabled) { m_RTEnabled = enabled; }
    u32 GetRTMode() const { return m_RTMode; }
    void SetRTMode(u32 mode) { m_RTMode = mode; }

    // RT subsystem accessors
    Renderer::AccelerationStructureManager* GetASManager() { return m_ASManager.get(); }
    Renderer::RTShadows* GetRTShadows() { return m_RTShadows.get(); }
    Renderer::RTReflections* GetRTReflections() { return m_RTReflections.get(); }
    Renderer::RTAmbientOcclusion* GetRTAO() { return m_RTAO.get(); }
    Renderer::RTGlobalIllumination* GetRTGI() { return m_RTGI.get(); }
    Renderer::PathTracer* GetPathTracer() { return m_PathTracer.get(); }
    Renderer::SVGFDenoiser* GetSVGFDenoiser() { return m_SVGFDenoiser.get(); }
    Renderer::RTCompositor* GetRTCompositor() { return m_RTCompositor.get(); }

    // Order-Independent Transparency
    bool IsOITEnabled() const { return m_OITEnabled; }
    void SetOITEnabled(bool enabled) { m_OITEnabled = enabled; }
    Renderer::OITManager* GetOITManager() { return m_OITManager.get(); }

    // SH Light Probes
    Renderer::SHLightingSystem* GetSHLighting() { return m_SHLighting.get(); }

    // SDF Scene
    Renderer::SDFScene* GetSDFScene() { return m_SDFScene.get(); }

    // Onion skin ghost rendering (editor viewport only)
    void SetOnionSkinGhosts(const std::vector<Editor::OnionSkinGhost>& ghosts) { m_OnionSkinGhosts = ghosts; }
    void ClearOnionSkinGhosts() { m_OnionSkinGhosts.clear(); }

    // Load or retrieve a cached texture (public wrapper for editor/tool use)
    std::shared_ptr<Renderer::Texture> LoadTexture(const std::string& path) { return GetOrLoadTexture(path); }

    // Clear a path from the failed texture cache so it will be retried on next load
    void ClearFailedTexture(const std::string& path) { m_FailedTextures.erase(path); }

    // Access descriptor sets for sub-renderers
    const std::vector<VkDescriptorSet>& GetDescriptorSets() const { return m_DescriptorSets; }

    // Compute offscreen buffer/descriptor set index for a given frame and viewport
    static u32 GetOffscreenBufferIndex(u32 frameIndex, u32 viewportIndex) {
        return frameIndex * MAX_SPLITSCREEN_VIEWPORTS + viewportIndex;
    }

private:
    void RenderEntity(Entity entity);
    void RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer);
    void RenderEntityGhost(Entity entity, const Math::Matrix4& modelMatrix,
                           const Math::Vector3& tint, f32 opacity);
    void RenderOnionSkinGhosts();
    void RenderSprites();  // Sorted 2D sprite pass (after 3D geometry)
    void ClassifySceneComposition();  // Update m_SceneComposition if dirty
    void CreateDefaultMesh();
    void CreatePipeline();
    void CreateShadowPipeline();
    // Recreate all pipelines. If gpuAlreadyIdle is true, skips vkDeviceWaitIdle (caller guarantees GPU is idle).
    void RecreatePipelines(bool gpuAlreadyIdle = false);
    void CreateUniformBuffers();
    void CreateDescriptorSets();
    void UpdateUniformBuffer(Entity entity);
    // Sets up GPU buffers for an entity's mesh. Returns pointer to EntityRenderData,
    // or nullptr if the entity has no valid mesh.
    EntityRenderData* SetupEntityBuffers(Entity entity);
    void RenderShadowPass();

    World* m_World = nullptr;
    Renderer::VulkanRenderer* m_Renderer = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Entity m_DefaultEntity = INVALID_ENTITY;

    // Rendering resources
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;
    std::unique_ptr<Renderer::VulkanShader> m_ShadowVertexShader;

    // Line rendering (editor grid)
    std::unique_ptr<Renderer::VulkanPipeline> m_LinePipeline;
    void CreateLinePipeline();

    // Shadow mapping
    std::unique_ptr<Renderer::ShadowMap> m_ShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_ShadowPipeline;
    Math::Matrix4 m_CurrentCascadeVP;  // Set per-cascade in RenderShadowPass, read by RenderEntityShadow
    bool m_ShadowsEnabled = true;
    f32 m_ShadowDistance = 100.0f;
    u32 m_PendingShadowResolution = 0; // 0 = no change pending

    // Point light shadow mapping (cubemap array, up to 4 lights)
    std::unique_ptr<Renderer::PointLightShadowMap> m_PointShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_PointShadowPipeline;

    // Spot light shadow mapping (2D array, up to 4 lights)
    std::unique_ptr<Renderer::SpotLightShadowMap> m_SpotShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_SpotShadowPipeline;

    // Shadow data SSBO (uploaded per-frame with point/spot view-proj matrices)
    std::unique_ptr<Renderer::VulkanBuffer> m_ShadowDataBuffer;

    // Per-frame selected shadow-casting point/spot lights (sorted to front of UBO arrays)
    struct ShadowPointLight {
        Entity entity;
        Math::Vector3 position;
        f32 range;
        f32 score;
    };
    struct ShadowSpotLight {
        Entity entity;
        Math::Vector3 position;
        Math::Vector3 direction;
        f32 outerConeAngle;
        f32 range;
        f32 score;
    };
    std::vector<ShadowPointLight> m_ShadowPointLights;
    std::vector<ShadowSpotLight> m_ShadowSpotLights;
    u32 m_ActivePointShadowCount = 0;
    u32 m_ActiveSpotShadowCount = 0;

    void CreatePointShadowPipeline();
    void CreateSpotShadowPipeline();
    void RenderPointShadowPass();
    void RenderSpotShadowPass();
    void SelectShadowLights();
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

    // Cel shading parameters
    bool m_CelShadingEnabled = false;
    f32 m_CelDiffuseBands = 3.0f;     // Number of quantized bands (2-8)
    f32 m_CelSpecularCutoff = 0.5f;   // Hard cutoff threshold for specular highlights

    // Global retro shader overrides (forced on all entities when true)
    bool m_GlobalFlatShading = false;
    bool m_GlobalAffineTexturing = false;
    bool m_GlobalVertexSnapping = false;
    bool m_GlobalStippleTransparency = false;
    bool m_GlobalUVQuantize = false;
    bool m_GlobalGouraudOnly = false;
    u8 m_GlobalVertexSnapResolution = 160;

    // Textures — integer-keyed for O(1) lookup after initial load
    std::unique_ptr<Renderer::Texture> m_DefaultWhiteTexture;
    std::unordered_map<std::string, u32> m_TexturePathToId;          // path → ID (only hit on first load)
    std::vector<std::shared_ptr<Renderer::Texture>> m_TextureById;   // dense: ID → texture
    std::vector<std::string> m_TextureIdToPath;                       // reverse: ID → path (for hot-reload)
    std::unordered_set<std::string> m_FailedTextures; // Paths that failed to load (avoid per-frame retry)

    // Text rendering
    Renderer::TextRasterizer m_TextRasterizer;
    std::unordered_map<Entity, std::shared_ptr<Renderer::Texture>> m_TextTextureCache;

    // Skeletal animation
    std::unique_ptr<Renderer::VulkanBuffer> m_DefaultBoneBuffer;
    void UpdateBoneDescriptor(Renderer::VulkanBuffer* boneBuffer);

    // Weather, particle, grass, shrub, tree, and sprite batch renderers
    std::unique_ptr<Effects::WeatherRenderer> m_WeatherRenderer;
    Effects::WeatherSystem* m_MainPassWeather = nullptr;  // Weather for main pass (editor viewport)
    bool m_MainPassWeatherIsRain = false;
    std::unique_ptr<Effects::ParticleRenderer> m_ParticleRenderer;
    std::unique_ptr<Effects::FluidRenderer> m_FluidRenderer;
    std::unique_ptr<Effects::GrassRenderer> m_GrassRenderer;
    std::unique_ptr<Effects::ShrubRenderer> m_ShrubRenderer;
    std::unique_ptr<Effects::TreeRenderer> m_TreeRenderer;
    std::unique_ptr<Effects::SpriteBatchRenderer> m_SpriteBatchRenderer;
    std::unique_ptr<Effects::SpriteTextureAtlas> m_SpriteAtlas;

    // Scene composition cache (auto-detected per frame, drives rendering decisions)
    SceneComposition m_SceneComposition;
    u32 m_DiagnosticFrameCounter = 0;

    // Shadow caster cache — rebuilt when dirty, avoids per-cascade entity iteration
    std::vector<Entity> m_ShadowCasters;
    bool m_ShadowCastersDirty = true;
    void RebuildShadowCasterCache();

    // Per-frame cached light entity list (populated once at start of Update, reused by all sub-functions)
    std::vector<Entity> m_CachedLightEntities;

    // Merged geometry buffer (single VB+IB for all static 3D meshes)
    std::unique_ptr<Renderer::MergedGeometryBuffer> m_GeometryPool;
    bool IsPoolEligible(Entity entity) const;  // Check if entity should use merged pool

    // GPU frustum culling system
    std::unique_ptr<Renderer::GPUCullingSystem> m_GPUCulling;
    std::vector<Renderer::CullableObject> m_CullableObjects;
    std::vector<u32> m_EntityToCullIndex; // Maps entity index to cullable object index
    bool m_GPUCullingEnabled = true;  // Enabled: GPU-driven indirect draws (no readback stall)
    bool m_IsEditorMode = false;      // When true, skip frustum culling (show all entities)
    bool m_SkipMainPassShadows = false; // When true, skip shadow passes in Update() (play mode)
    bool m_SkipMainPassRendering = false; // When true, skip geometry+effects in Update() (play mode — game view handles rendering)
    void BuildCullableObjectList();
    void PerformGPUCulling();
    void PerformGPUCullingAsync(); // Record to compute command buffer

    // Indirect draw: ObjectData upload + vkCmdDrawIndexedIndirectCount
    std::vector<ObjectDataGPU> m_ObjectDataCPU;
    void UploadObjectData();
    void DrawIndirect(VkCommandBuffer commandBuffer);

    // Hi-Z occlusion culling (previous-frame depth pyramid)
    std::unique_ptr<Renderer::HiZPyramid> m_HiZPyramid;

    // Multi-threaded command buffer recording
    Renderer::ThreadPool m_ThreadPool;
    std::unique_ptr<Renderer::CommandBufferPool> m_CmdBufferPool;

    // Skybox
    Renderer::Skybox m_Skybox;
    bool m_PendingSkyboxConfig = false;
    Renderer::SkyboxConfig m_PendingSkybox;
    VkPipeline m_SkyboxPipelineHandle = VK_NULL_HANDLE;
    VkPipelineLayout m_SkyboxPipelineLayoutHandle = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_SkyboxDescriptorSetLayoutHandle = VK_NULL_HANDLE;
    std::unique_ptr<Renderer::VulkanBuffer> m_SkyboxVertexBuffer;
    VkDescriptorPool m_SkyboxDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_SkyboxDescriptorSets;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_SkyboxUniformBuffers;
    void CreateSkyboxPipeline(VkRenderPass renderPass = VK_NULL_HANDLE);
    void RenderSkybox(VkCommandBuffer commandBuffer,
                      const VkViewport* viewportOverride = nullptr,
                      const VkRect2D* scissorOverride = nullptr);
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

    // Batched texture descriptor update — calls vkUpdateDescriptorSets once for all textures
    // Pass nullptr for any texture slot that should use the default white texture
    void UpdateEntityTextureDescriptors(
        Renderer::Texture* baseColor,
        Renderer::Texture* height,
        Renderer::Texture* normal,
        Renderer::Texture* metallicRoughness,
        Renderer::Texture* emissive);

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

    // Per-entity render data — dense vector indexed by entity ID for cache-friendly O(1) lookup
    std::vector<EntityRenderData> m_EntityRenderData;

    // Descriptor set caching — tracks what was last written to the shared descriptor set.
    // When the next entity's textures/bones match, vkUpdateDescriptorSets is skipped.
    struct LastBoundState {
        MaterialComponent::TextureKey textureKey;
        Renderer::VulkanBuffer* boneBuffer = nullptr;
        void Reset() { textureKey = {}; boneBuffer = nullptr; }
    };
    LastBoundState m_LastBound;
    std::vector<Entity> m_SortedRenderList;  // Reused per frame to avoid allocation

    // Draw call / triangle counters
    u32 m_DrawCallCount = 0;
    u32 m_TriangleCount = 0;

    // Cached player entity (any entity with a CharacterController) for per-frame position lookup.
    // Updated in OnEntityAdded/OnEntityRemoved to avoid linear search each frame.
    Entity m_CachedPlayerEntity = INVALID_ENTITY;

    // Asset hot-reload watcher (polls texture files for changes)
    Assets::FileWatcher m_TextureWatcher;
    u32 m_WatcherPollCounter = 0;

    // Shader hot-reload (editor-only)
    Assets::FileWatcher m_ShaderWatcher;
    std::string m_ShaderDir;       // Path to Engine/shaders/ (empty = not found, hot-reload disabled)
    bool m_ShaderHotReloadEnabled = true;
    void FindShaderDirectory();
    void SetupShaderWatchers();
    void ReloadMainShaders(const std::string& changedFile);
    void ReloadSkyboxShaders();
    void ReloadShadowShaders();

    // Deferred pipeline recreation — avoids GPU stalls mid-frame by deferring work
    // to the start of the next Update() call, where WaitForAllFrames() is safe.
    enum class PendingRecreationType : u8 {
        None, PipelineOnly, MainShader, SkyboxShader, ShadowShader
    };
    PendingRecreationType m_PendingRecreation = PendingRecreationType::None;
    std::unique_ptr<Renderer::VulkanShader> m_PendingVertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_PendingFragmentShader;
    void ProcessPendingRecreation();

    bool m_Initialized = false;

    // --- Ray Tracing subsystems (null when unsupported) ---
    void InitializeRayTracing();
    void ShutdownRayTracing();
    void RebuildTLAS(VkCommandBuffer cmd);
    void DispatchRTEffects(VkCommandBuffer cmd);
    void DenoiseRTOutputs(VkCommandBuffer cmd);
    void CompositeRTResults(VkCommandBuffer cmd);

    bool m_RTEnabled = false;
    u32 m_RTMode = 0;  // 0=Hybrid, 1=PathTrace
    u32 m_RTFrameCount = 0;
    Math::Matrix4 m_PrevViewProj;  // Previous frame's VP for path tracer camera change detection

    std::unique_ptr<Renderer::AccelerationStructureManager> m_ASManager;
    std::unique_ptr<Renderer::RTShadows> m_RTShadows;
    std::unique_ptr<Renderer::RTReflections> m_RTReflections;
    std::unique_ptr<Renderer::RTAmbientOcclusion> m_RTAO;
    std::unique_ptr<Renderer::RTGlobalIllumination> m_RTGI;
    std::unique_ptr<Renderer::PathTracer> m_PathTracer;
    std::unique_ptr<Renderer::SVGFDenoiser> m_SVGFDenoiser;
    std::unique_ptr<Renderer::RTCompositor> m_RTCompositor;

    // RT descriptor set layout and pool
    VkDescriptorSetLayout m_RTDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_RTDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_RTDescriptorSet = VK_NULL_HANDLE;

    // RT dummy/placeholder resources for unwritten descriptor bindings
    VkImage m_RTDummyImage = VK_NULL_HANDLE;
    VkDeviceMemory m_RTDummyImageMemory = VK_NULL_HANDLE;
    VkImageView m_RTDummyImageView = VK_NULL_HANDLE;
    VkSampler m_RTDummySampler = VK_NULL_HANDLE;
    VkBuffer m_RTDummyBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_RTDummyBufferMemory = VK_NULL_HANDLE;

    // RT light data UBO (one per frame in flight)
    static constexpr u32 RT_FRAMES_IN_FLIGHT = 2;
    VkBuffer m_RTLightUBO[RT_FRAMES_IN_FLIGHT] = {};
    VkDeviceMemory m_RTLightUBOMemory[RT_FRAMES_IN_FLIGHT] = {};
    void* m_RTLightUBOMapped[RT_FRAMES_IN_FLIGHT] = {};

    bool m_RTDescriptorsWritten = false;

    // --- Order-Independent Transparency (Weighted Blended OIT) ---
    std::unique_ptr<Renderer::OITManager> m_OITManager;
    bool m_OITEnabled = false;

    // --- SH Light Probes ---
    std::unique_ptr<Renderer::SHLightingSystem> m_SHLighting;

    // --- SDF Scene (ray-marched primitives) ---
    std::unique_ptr<Renderer::SDFScene> m_SDFScene;

    // Onion skin ghosts (set by editor, rendered in main pass only)
    std::vector<Editor::OnionSkinGhost> m_OnionSkinGhosts;

    void CreateRTDummyResources();
    void DestroyRTDummyResources();
    void WriteRTDescriptors();
    void TransitionRTOutputImages(VkCommandBuffer cmd);
    void UpdateRTLightUBO(const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                          f32 lightIntensity, f32 shadowDistance, f32 shadowRadius, u32 frameCount);
};

} // namespace ECS
} // namespace Enjin
