#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/System.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/MorphTarget.h"

// Forward declarations for cached component storage (avoids header includes)
namespace Enjin::ECS {
    struct ArtStyleComponent;
    struct Sprite2DComponent;
    struct WaterVolumeComponent;
    struct Water3DComponent;
}
namespace Enjin::Build { class AssetReader; }

// Cross-platform abstract backend interface
#include "Enjin/Renderer/RenderBackend.h"
#include "Enjin/Renderer/RenderStructs.h"
#include "Enjin/Renderer/GPUTypes.h"
#include "Enjin/Renderer/GPURenderEncoder.h"

#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#endif

#include "Enjin/Renderer/Camera.h"
#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/Texture.h"
#endif
#include "Enjin/Renderer/TextRasterizer.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Text.h"

#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/ShadowMap.h"
#include "Enjin/Renderer/PointLightShadowMap.h"
#include "Enjin/Renderer/SpotLightShadowMap.h"
#endif

// Forward declarations for effect renderers and systems (stored as unique_ptr/raw pointer)
namespace Enjin { namespace Effects {
    class WindSystem;
    class WeatherSystem;
    class WeatherRenderer;
    class ParticleRenderer;
    class FluidRenderer;
    class FluidSimulation;
    class ElementalSystem;
    class SpriteBatchRenderer;
    class SpriteTextureAtlas;
    class GrassRenderer;
    class ShrubRenderer;
    class TreeRenderer;
}}

#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/GPUDriven/GPUCulling.h"
#include "Enjin/Renderer/GPUDriven/MergedGeometryBuffer.h"
#include "Enjin/Renderer/GPUDriven/HiZPyramid.h"
#include "Enjin/Renderer/GPUDriven/IndirectDrawBatcher.h"
#include "Enjin/Renderer/GPUDriven/DeviceGeneratedCommands.h"
#include "Enjin/Renderer/AsyncComputeScheduler.h"
#include "Enjin/Renderer/Vulkan/ThreadPool.h"
#include "Enjin/Renderer/Vulkan/CommandBufferPool.h"
#endif

#include "Enjin/Assets/FileWatcher.h"

#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/RayTracing/RTCapabilities.h"
#include "Enjin/Editor/FlashTimeline.h"  // Editor::OnionSkinGhost used in m_OnionSkinGhosts
#include <vulkan/vulkan.h>
#endif

#include "Enjin/Memory/FrameAllocator.h"
#include "Enjin/Renderer/HaltonSequence.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>

#if !ENJIN_RENDERER_WEBGPU
// Forward declarations for RT subsystems and rendering infrastructure
namespace Enjin { namespace Renderer {
    class AccelerationStructureManager;
    class RTShadows;
    class RTReflections;
    class RTAmbientOcclusion;
    class RTGlobalIllumination;
    class RTTranslucency;
    class RTCaustics;
    class PathTracer;
    class SVGFDenoiser;
    class OIDNDenoiser;
    class OptiXDenoiser;
    class RTCompositor;
    class RTTemporalReuse;
    class ReSTIR;
    class LightBVH;
    class RadianceCache;
    class SurfelRadianceCache;
    class AdaptiveRayBudget;
    class BindlessResourceManager;
    class IUpscaler;
    class OITManager;
    class SHLightingSystem;
    class ReflectionProbeSystem;
    class SDFScene;
    class ClusteredLightingSystem;
    struct ClusterLight;
    class VisibilityBufferRenderer;
    class VariableRateShading;
}}
#endif

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
    u32 teleported;                  // 1 = network snap/spawn (zero velocity), 0 = normal
    f32 _pad[2];                     // 8 bytes (pad to 192 total)
    Math::Matrix4 prevModel;         // 64 bytes — previous frame model matrix for velocity
};
static_assert(sizeof(ObjectDataGPU) == 192, "ObjectDataGPU must be 192 bytes for std430");

// Per-entity rendering data (stored in dense vector indexed by entity ID)
struct EntityRenderData {
#if !ENJIN_RENDERER_WEBGPU
    std::unique_ptr<Renderer::VulkanBuffer> vertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> indexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> boneBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> morphBuffer;
#else
    Renderer::GPUBufferHandle vertexBuffer;
    Renderer::GPUBufferHandle indexBuffer;
    Renderer::GPUBufferHandle boneBuffer;
    Renderer::GPUBufferHandle morphBuffer;
    Renderer::GPUBindGroupHandle texBindGroup;  // per-entity texture bind group (group 2)
    bool texBindGroupValid = false;             // true if textures loaded for this entity
#endif
    u32 indexCount = 0;
    bool valid = false;  // true if this slot is occupied
#if !ENJIN_RENDERER_WEBGPU
    Renderer::MeshAllocation poolAlloc;
#endif

    void Invalidate() {
#if !ENJIN_RENDERER_WEBGPU
        vertexBuffer.reset();
        indexBuffer.reset();
        boneBuffer.reset();
        morphBuffer.reset();
#else
        vertexBuffer = {};
        indexBuffer = {};
        boneBuffer = {};
        morphBuffer = {};
        texBindGroup = {};
        texBindGroupValid = false;
#endif
        indexCount = 0;
        valid = false;
#if !ENJIN_RENDERER_WEBGPU
        poolAlloc = {};
#endif
    }
};

// Render system - renders entities with Transform and Mesh components
class ENJIN_API RenderSystem : public ISystem {
public:
    RenderSystem(World* world, Renderer::IRenderBackend* renderer);
    ~RenderSystem();

    void Initialize();
    void Shutdown();

    // Reset all per-entity caches (render data, material indices, sorted lists).
    // Must be called after World::Clear() and before the next render frame.
    void OnSceneClear();
    void FlushSceneClear();

    // Process deferred changes (skybox config, pipeline recreation) — call at frame start,
    // BEFORE any command buffer recording (RenderOffscreen, Update, etc.)
    void FlushPendingChanges();

    void Update(f32 deltaTime) override;
    void OnEntityAdded(Entity entity) override;
    void OnEntityRemoved(Entity entity) override;

    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }
    Renderer::Camera* GetCamera() const { return m_Camera; }

    // Asset reader for loading textures from .enjpak on web
    void SetAssetReader(Build::AssetReader* reader) { m_AssetReader = reader; }

#if !ENJIN_RENDERER_WEBGPU
    Renderer::VulkanSwapchain* GetSwapchain() const;

    // HDR output — delegates to VulkanRenderer which handles swapchain + render pass + pipeline recreation
    void SetHDREnabled(bool enabled);
    bool IsHDREnabled() const;
    u32 GetHDROutputMode() const;
#endif

    // Editor mode: when true, GPU frustum culling is disabled so all entities
    // are visible in the scene view for editing. The Player leaves this false
    // so the game camera frustum culls normally.
    void SetEditorMode(bool editor) { m_IsEditorMode = editor; }

    // When true, skip shadow/point/spot shadow passes in the main Update() pass.
    // Used during play mode — the game view already runs its own shadow pass via
    // RenderShadowPassForCamera(), so the editor viewport shadows are redundant.
    void SetSkipMainPassShadows(bool skip) { m_SkipMainPassShadows = skip; }
    bool IsSkipMainPassShadows() const { return m_SkipMainPassShadows; }
    void SetSkipMainPassRendering(bool skip) { m_SkipMainPassRendering = skip; }
    bool IsSkipMainPassRendering() const { return m_SkipMainPassRendering; }
    bool IsEditorMode() const { return m_IsEditorMode; }
    bool IsGameViewReady() { if (m_SceneClearCooldown > 0) { --m_SceneClearCooldown; return false; } return true; }
    bool IsSceneClearActive() const { return m_SceneClearCooldown > 0; }

    Renderer::IRenderBackend* GetRenderer() const { return m_Renderer; }

#if !ENJIN_RENDERER_WEBGPU
    Renderer::VulkanRenderer* GetVulkanRenderer() const;

    // Render all entities to an offscreen render target using a custom camera
    // Must be called outside of the main render pass (before BeginMainRenderPass)
    void RenderToTarget(Renderer::RenderTarget* target, Renderer::Camera* camera, u32 viewportIndex = 0);

    // Render multiple cameras to a single render target using viewport subdivision (splitscreen)
    // Each ViewportCamera defines a normalized rect within the target.
    // The render pass must already be started by the caller.
    void RenderSplitscreen(Renderer::RenderTarget* target, const std::vector<ViewportCamera>& viewports);
#endif

    static constexpr u32 MAX_SPLITSCREEN_VIEWPORTS = 4;

#if !ENJIN_RENDERER_WEBGPU
    // Set splitscreen viewports for the main render pass (used by Player).
    // When non-empty, Update() renders each viewport instead of a single full-screen camera.
    // Call with empty vector to disable splitscreen.
    void SetMainPassSplitscreen(const std::vector<ViewportCamera>& viewports) {
        m_MainPassViewports = viewports;
    }
    const std::vector<ViewportCamera>& GetMainPassViewports() const { return m_MainPassViewports; }

    // Run the shadow pass for an offscreen camera (call BEFORE the render target's Begin()).
    // The shadow pass uses its own framebuffer, so it must not be inside another render pass.
    void RenderShadowPassForCamera(Renderer::Camera* camera);
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Runtime rendering settings
    bool IsShadowsEnabled() const { return m_ShadowsEnabled; }
    void SetShadowsEnabled(bool enabled) {
        if (m_ShadowsEnabled != enabled) {
            m_ShadowsEnabled = enabled;
            m_ShadowDescriptorsDirty = true;
        }
    }
    // Call after shadow state changes to update offscreen descriptor bindings
    void RefreshDescriptorsIfDirty();
#endif

    // Memory profiling queries
    usize GetSortedRenderListSize() const { return m_SortedRenderList.size(); }
    usize GetEntityRenderDataSize() const { return m_EntityRenderData.size(); }

#if !ENJIN_RENDERER_WEBGPU
    // Editor viewport display modes
    void SetEditorWireframe(bool enabled) { m_EditorWireframe = enabled; }
    void SetEditorUnlit(bool enabled) { m_EditorUnlit = enabled; }
    bool GetEditorWireframe() const { return m_EditorWireframe; }
    bool GetEditorUnlit() const { return m_EditorUnlit; }

    // Shadow quality settings
    f32 GetShadowDistance() const { return m_ShadowDistance; }
    void SetShadowDistance(f32 d);
    f32 GetShadowStrength() const;
    void SetShadowStrength(f32 s);
    f32 GetShadowSoftness() const;
    void SetShadowSoftness(f32 s);
    u32 GetShadowResolution() const;
    void SetShadowResolution(u32 r);

    // Progressive cascade shadow updates — far cascades update every N frames
    bool IsCascadeProgressiveUpdate() const { return m_CascadeProgressiveUpdate; }
    void SetCascadeProgressiveUpdate(bool enabled) { m_CascadeProgressiveUpdate = enabled; }
    u32 GetCascadeFarUpdateInterval() const { return m_CascadeFarUpdateInterval; }
    void SetCascadeFarUpdateInterval(u32 interval) { m_CascadeFarUpdateInterval = std::clamp(interval, 2u, 8u); }
#endif

    bool IsBackfaceCullingEnabled() const { return m_BackfaceCulling; }
    void SetBackfaceCullingEnabled(bool enabled);

    bool IsWireframeEnabled() const { return m_WireframeMode; }
    void SetWireframeEnabled(bool enabled);

#if !ENJIN_RENDERER_WEBGPU
    // Render line-list geometry with depth testing (for editor overlays)
    void RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                         u32 firstVertex, const Math::Vector3& color, f32 opacity);

    // Render grid lines into an offscreen render target (uses offscreen descriptor sets)
    void RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                         u32 firstVertex, const Math::Vector3& color, f32 opacity,
                         u32 targetWidth, u32 targetHeight);
#endif

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
    Effects::WeatherSystem* GetMainPassWeather() const { return m_MainPassWeather; }
    bool GetMainPassWeatherIsRain() const { return m_MainPassWeatherIsRain; }

    // Set fluid simulation (for FluidRenderer to read grid data)
    void SetFluidSimulation(Effects::FluidSimulation* sim);
#if !ENJIN_RENDERER_WEBGPU
    Effects::FluidRenderer* GetFluidRenderer() const { return m_FluidRenderer.get(); }

    // Weather, grass, and tree renderers (initialized after main pipeline)
    Effects::WeatherRenderer* GetWeatherRenderer() { return m_WeatherRenderer.get(); }
    Effects::GrassRenderer* GetGrassRenderer() { return m_GrassRenderer.get(); }
    Effects::TreeRenderer* GetTreeRenderer() { return m_TreeRenderer.get(); }
#endif

    // Render weather particles, game particles, grass, shrubs, and trees
    // (call after scene geometry in main render pass)
    // viewportWidth/Height: 0 = swapchain, >0 = render target override
    void RenderWeatherParticles(const Effects::WeatherSystem& weather, bool isRain,
                                u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderParticles(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderElementalParticles(const Effects::ElementalSystem& elementalSystem,
                                  u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderFluid(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderGrass(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderShrubs(u32 viewportWidth = 0, u32 viewportHeight = 0);
    void RenderTrees(u32 viewportWidth = 0, u32 viewportHeight = 0);

#if !ENJIN_RENDERER_WEBGPU
    // Recreate effect renderer pipelines for a specific render pass (e.g. render target)
    void RecreateEffectPipelinesForRenderPass(VkRenderPass renderPass);
#endif

    // Scene composition (auto-detected rendering mode)
    SceneRenderMode GetSceneRenderMode() const { return m_SceneComposition.mode; }
    const SceneComposition& GetSceneComposition() const { return m_SceneComposition; }

    // Mark the material SSBO as dirty so it will be fully rebuilt next frame.
    // Call when material properties change outside of entity add/remove (e.g., inspector edits, scripts).
    void MarkMaterialsDirty() { m_MaterialSSBODirty = true; }

    // Draw call / triangle counters — getters return last completed frame's values
    // so that UI reads (which happen before the next render) see valid numbers.
    u32 GetDrawCallCount() const { return m_LastDrawCallCount; }
    u32 GetTriangleCount() const { return m_LastTriangleCount; }
    u32 GetDescriptorCacheHits() const { return m_LastDescriptorCacheHits; }
    u32 GetDescriptorCacheWrites() const { return m_LastDescriptorCacheWrites; }
    void ResetFrameCounters() {
        m_LastDrawCallCount = m_DrawCallCount;
        m_LastTriangleCount = m_TriangleCount;
        m_LastDescriptorCacheHits = m_DescriptorCacheHits;
        m_LastDescriptorCacheWrites = m_DescriptorCacheWrites;
        m_DrawCallCount = 0; m_TriangleCount = 0;
        m_DescriptorCacheHits = 0; m_DescriptorCacheWrites = 0;
    }

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
    f32 GetTexturePageSize() const { return m_TexturePageSize; }
    void SetTexturePageSize(f32 v) { m_TexturePageSize = v; }
    f32 GetDepthSortJitter() const { return m_DepthSortJitter; }
    void SetDepthSortJitter(f32 v) { m_DepthSortJitter = v; }
    f32 GetNormalQuantizeSteps() const { return m_NormalQuantizeSteps; }
    void SetNormalQuantizeSteps(f32 v) { m_NormalQuantizeSteps = v; }

    // Light ramp (art style)
    f32 GetLightRampMode() const { return m_LightRampMode; }
    void SetLightRampMode(f32 v) { m_LightRampMode = v; }
    f32 GetCelShadowMode() const { return m_CelShadowMode; }
    void SetCelShadowMode(f32 v) { m_CelShadowMode = v; }

    // Shading model
    u32 GetShadingModel() const { return m_ShadingModel; }
    void SetShadingModel(u32 model) { m_ShadingModel = model; }
    bool IsFresnelEnabled() const { return m_FresnelEnabled; }
    void SetFresnelEnabled(bool v) { m_FresnelEnabled = v; }
    bool IsEnergyConservation() const { return m_EnergyConservation; }
    void SetEnergyConservation(bool v) { m_EnergyConservation = v; }
    bool IsGeometryTerm() const { return m_GeometryTerm; }
    void SetGeometryTerm(bool v) { m_GeometryTerm = v; }
    bool IsSphereEnvMapEnabled() const { return m_SphereEnvMapEnabled; }
    void SetSphereEnvMapEnabled(bool v) { m_SphereEnvMapEnabled = v; }
    bool IsHalfLambert() const { return m_HalfLambert; }
    void SetHalfLambert(bool v) { m_HalfLambert = v; }
    f32 GetSphereEnvStrength() const { return m_SphereEnvStrength; }
    void SetSphereEnvStrength(f32 v) { m_SphereEnvStrength = v; }
    f32 GetPosterizeLevels() const { return m_PosterizeLevels; }
    void SetPosterizeLevels(f32 v) { m_PosterizeLevels = v; }

    // Accessibility: update WebGPU post-process params (colorblind, brightness, contrast)
    // No-op on Vulkan builds (post-processing handled by PostProcessing class)
    struct WebPPAccessibilityParams {
        u32 colorblindMode = 0;
        f32 colorblindStrength = 1.0f;
        f32 brightness = 0.0f;
        f32 contrast = 1.0f;
    };
    WebPPAccessibilityParams m_WebPPAccessibility;
    void SetWebAccessibility(u32 colorblindMode, f32 strength, f32 brightness, f32 contrast) {
        m_WebPPAccessibility.colorblindMode = colorblindMode;
        m_WebPPAccessibility.colorblindStrength = strength;
        m_WebPPAccessibility.brightness = brightness;
        m_WebPPAccessibility.contrast = contrast;
    }

    // Cel shading (lighting quantization)
    bool IsCelShadingEnabled() const { return m_CelShadingEnabled; }
    void SetCelShadingEnabled(bool enabled) { m_CelShadingEnabled = enabled; }
    f32 GetCelDiffuseBands() const { return m_CelDiffuseBands; }
    void SetCelDiffuseBands(f32 bands) { m_CelDiffuseBands = bands; }
    f32 GetCelSpecularCutoff() const { return m_CelSpecularCutoff; }
    void SetCelSpecularCutoff(f32 cutoff) { m_CelSpecularCutoff = cutoff; }

    // Geometry outlines (inverted-hull)
    bool IsGeometryOutlinesEnabled() const { return m_GeometryOutlinesEnabled; }
    void SetGeometryOutlinesEnabled(bool enabled) { m_GeometryOutlinesEnabled = enabled; }
    f32 GetGeometryOutlineWidth() const { return m_GeometryOutlineWidth; }
    void SetGeometryOutlineWidth(f32 w) { m_GeometryOutlineWidth = w; }
    Math::Vector3 GetGeometryOutlineColor() const { return m_GeometryOutlineColor; }
    void SetGeometryOutlineColor(const Math::Vector3& c) { m_GeometryOutlineColor = c; }

    // Anti-aliasing mode: 0=None, 1=FXAA, 2=TAA, 3=SMAA, 4=MSAA 2x, 5=MSAA 4x, 6=MSAA 8x
    u32 GetAAMode() const { return m_AAMode; }
    void SetAAMode(u32 mode);
    void ApplyPendingMSAAChange(); // Deferred MSAA application (safe between frames)

    // Maximum MSAA sample count supported by the GPU (queried at init)
    u32 GetMaxMSAASamples() const;

    // Temporal upscaling (FSR 2, DLSS, XeSS — replaces TAA when active)
    u32 GetUpscalerType() const { return m_UpscalerType; }
    void SetUpscalerType(u32 type);
    u32 GetUpscalerQuality() const { return m_UpscalerQuality; }
    void SetUpscalerQuality(u32 quality);
    f32 GetUpscalerSharpness() const { return m_UpscalerSharpness; }
    void SetUpscalerSharpness(f32 s) { m_UpscalerSharpness = s; }
#if !ENJIN_RENDERER_WEBGPU
    bool IsUpscalerActive() const { return m_UpscalerType > 0 && m_Upscaler != nullptr; }
    Renderer::IUpscaler* GetUpscaler() const { return m_Upscaler.get(); }
#else
    bool IsUpscalerActive() const { return false; }
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Skybox
    void SetSkybox(const Renderer::SkyboxConfig& config);
    const Renderer::SkyboxConfig& GetSkyboxConfig() const { return m_Skybox.GetConfig(); }
    Renderer::Skybox* GetSkybox() { return &m_Skybox; }

    // Ray tracing
    bool IsRayTracingSupported() const;
    bool IsRayTracingEnabled() const { return m_RTEnabled; }
    void SetRayTracingEnabled(bool enabled) { m_RTEnabled = enabled; }
    // Player mode: skip GPU compute shaders (culling, HiZ, clustered lighting)
    // that use disk-loaded SPIR-V not available in built games.
    void SetPlayerMode(bool enabled) { m_PlayerMode = enabled; }
    bool IsPlayerMode() const { return m_PlayerMode; }
    u32 GetRTMode() const { return m_RTMode; }
    void SetRTMode(u32 mode) { m_RTMode = mode; }

    // RT subsystem accessors
    Renderer::AccelerationStructureManager* GetASManager() { return m_ASManager.get(); }
    Renderer::RTShadows* GetRTShadows() { return m_RTShadows.get(); }
    Renderer::RTReflections* GetRTReflections() { return m_RTReflections.get(); }
    Renderer::RTAmbientOcclusion* GetRTAO() { return m_RTAO.get(); }
    Renderer::RTGlobalIllumination* GetRTGI() { return m_RTGI.get(); }
    Renderer::RTTranslucency* GetRTTranslucency() { return m_RTTranslucency.get(); }
    Renderer::RTCaustics* GetRTCaustics() { return m_RTCaustics.get(); }
    Renderer::PathTracer* GetPathTracer() { return m_PathTracer.get(); }
    Renderer::SVGFDenoiser* GetSVGFDenoiser() { return m_SVGFDenoiser.get(); }
    Renderer::OIDNDenoiser* GetOIDNDenoiser() { return m_OIDNDenoiser.get(); }
    Renderer::OptiXDenoiser* GetOptiXDenoiser() { return m_OptiXDenoiser.get(); }
    Renderer::RTCompositor* GetRTCompositor() { return m_RTCompositor.get(); }
    Renderer::RTTemporalReuse* GetRTTemporalReuse() { return m_RTTemporalReuse.get(); }
    Renderer::ReSTIR* GetReSTIR() { return m_ReSTIR.get(); }
    Renderer::LightBVH* GetLightBVH() { return m_LightBVH.get(); }
    Renderer::RadianceCache* GetRadianceCache() { return m_RadianceCache.get(); }
    Renderer::SurfelRadianceCache* GetSurfelRadianceCache() { return m_SurfelRadianceCache.get(); }
    Renderer::AdaptiveRayBudget* GetAdaptiveRayBudget() { return m_AdaptiveRayBudget.get(); }
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Denoiser type selection: 0=SVGF, 1=OIDN, 2=OptiX
    u32 GetDenoiserType() const { return m_DenoiserType; }
    void SetDenoiserType(u32 type) { m_DenoiserType = type; }

    // Order-Independent Transparency
    bool IsOITEnabled() const { return m_OITEnabled; }
    void SetOITEnabled(bool enabled) { m_OITEnabled = enabled; }
    Renderer::OITManager* GetOITManager() { return m_OITManager.get(); }

    // SH Light Probes
    Renderer::SHLightingSystem* GetSHLighting() { return m_SHLighting.get(); }

    // Reflection Probes (box-projected environment reflections)
    Renderer::ReflectionProbeSystem* GetReflectionProbes() { return m_ReflectionProbes.get(); }

    // Access the Vulkan renderer (needed by subsystems like reflection probe baking)
    // Implemented via static_cast from IRenderBackend* — safe because only called in Vulkan builds.

    // Update the reflection probe cubemap descriptor binding for all descriptor sets.
    // Called after a probe is baked to bind the cubemap to the shader.
    void UpdateProbeCubemapDescriptor();

    // SDF Scene
    Renderer::SDFScene* GetSDFScene() { return m_SDFScene.get(); }

    // Onion skin ghost rendering (editor viewport only)
    void SetOnionSkinGhosts(const std::vector<Editor::OnionSkinGhost>& ghosts) { m_OnionSkinGhosts = ghosts; }
    void ClearOnionSkinGhosts() { m_OnionSkinGhosts.clear(); }
    const std::vector<Editor::OnionSkinGhost>& GetOnionSkinGhosts() const { return m_OnionSkinGhosts; }
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Load or retrieve a cached texture (public wrapper for editor/tool use)
    std::shared_ptr<Renderer::Texture> LoadTexture(const std::string& path) { return GetOrLoadTexture(path); }

    // Clear a path from the failed texture cache so it will be retried on next load
    void ClearFailedTexture(const std::string& path) { m_FailedTextures.erase(path); }
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Access descriptor sets for sub-renderers
    const std::vector<VkDescriptorSet>& GetDescriptorSets() const { return m_DescriptorSets; }

    // Compute offscreen buffer/descriptor set index for a given frame and viewport
    static u32 GetOffscreenBufferIndex(u32 frameIndex, u32 viewportIndex) {
        return frameIndex * MAX_SPLITSCREEN_VIEWPORTS + viewportIndex;
    }
#endif

private:
    void RenderEntity(Entity entity);
    void RenderSprites();  // Sorted 2D sprite pass (after 3D geometry)
    void ClassifySceneComposition();  // Update m_SceneComposition if dirty
    void CreateDefaultMesh();
    void CreatePipeline();

#if !ENJIN_RENDERER_WEBGPU
    void RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer);
    void RenderEntityGhost(Entity entity, const Math::Matrix4& modelMatrix,
                           const Math::Vector3& tint, f32 opacity,
                           const std::vector<Math::Matrix4>* skinningMatrices = nullptr);
    void RenderOnionSkinGhosts();
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
#endif

    // Per-frame cached component storage pointers — refreshed once at the start of
    // Update() to avoid repeated type-ID hash map lookups in hot render loops.
    // Each GetComponent<T>(entity) does hash(typeId)->storage then hash(entity)->index;
    // caching the storage pointer eliminates the first lookup for every entity.
public:
    void RefreshStorageCache();
private:
    ComponentStorage<TransformComponent>* m_CachedTransformStorage = nullptr;
    ComponentStorage<MeshComponent>* m_CachedMeshStorage = nullptr;
    ComponentStorage<MaterialComponent>* m_CachedMaterialStorage = nullptr;
    ComponentStorage<MaterialSlotsComponent>* m_CachedMaterialSlotsStorage = nullptr;
    ComponentStorage<AnimatorComponent>* m_CachedAnimatorStorage = nullptr;
    AnimatorComponent* m_CachedFallbackAnimator = nullptr; // First animator with skeleton (for orphan skinned meshes)
    ComponentStorage<TextComponent>* m_CachedTextStorage = nullptr;
    ComponentStorage<ArtStyleComponent>* m_CachedArtStyleStorage = nullptr;
    ComponentStorage<Sprite2DComponent>* m_CachedSpriteStorage = nullptr;
    ComponentStorage<WaterVolumeComponent>* m_CachedWaterVolumeStorage = nullptr;
    ComponentStorage<Water3DComponent>* m_CachedWater3DStorage = nullptr;

    World* m_World = nullptr;
    Renderer::IRenderBackend* m_Renderer = nullptr;
#if !ENJIN_RENDERER_WEBGPU
    Renderer::VulkanRenderer* m_VulkanRenderer = nullptr;  // Cached cast for Vulkan-specific API calls
#endif
    Renderer::Camera* m_Camera = nullptr;
    Build::AssetReader* m_AssetReader = nullptr;
    Entity m_DefaultEntity = INVALID_ENTITY;

    // --- Cross-platform rendering resources (abstract handles) ---
    Renderer::GPUPipelineHandle m_MainPipeline;
    Renderer::GPUShaderHandle m_MainVertexShader;
    Renderer::GPUShaderHandle m_MainFragmentShader;
    Renderer::GPUBindGroupLayoutHandle m_MainBindGroupLayout;
    Renderer::IRenderEncoder* m_ActiveEncoder = nullptr;  // Valid between BeginRenderPass/EndRenderPass

#if ENJIN_RENDERER_WEBGPU
    // WebGPU-specific rendering resources
    // Bind group layouts (3 groups for PBR: frame, object, textures)
    Renderer::GPUBindGroupLayoutHandle m_WebFrameLayout;     // group 0: ViewProj + Lighting
    Renderer::GPUBindGroupLayoutHandle m_WebObjectLayout;    // group 1: ObjectData
    Renderer::GPUBindGroupLayoutHandle m_WebTextureLayout;   // group 2: 3 tex + 3 sampler

    // Uniform buffers
    Renderer::GPUBufferHandle m_WebViewProjBuffer;           // 144 bytes
    Renderer::GPUBufferHandle m_WebLightingBuffer;           // 464 bytes
    Renderer::GPUBufferHandle m_WebObjectBuffer;             // 128 bytes

    // Bind groups
    Renderer::GPUBindGroupHandle m_WebFrameBindGroup;        // group 0
    Renderer::GPUBindGroupHandle m_WebObjectBindGroup;       // group 1
    Renderer::GPUBindGroupHandle m_WebDefaultTexBindGroup;   // group 2 (default textures)

    // Default textures
    Renderer::GPUTextureHandle m_WebDefaultWhiteTex;
    Renderer::GPUTextureHandle m_WebDefaultNormalTex;
    Renderer::GPUTextureHandle m_WebDefaultBlackTex;

    // Texture cache: path → loaded GPU texture handle
    std::unordered_map<std::string, Renderer::GPUTextureHandle> m_WebTextureCache;
    std::unordered_set<std::string> m_WebFailedTextures;  // don't retry failed loads
    Renderer::GPUTextureHandle WebGetOrLoadTexture(const std::string& path);

    // Default bone buffer (single identity matrix for non-skinned meshes)
    Renderer::GPUBufferHandle m_WebDefaultBoneBuffer;

    // Shadow mapping (1-cascade directional)
    static constexpr u32 WEB_SHADOW_MAP_SIZE = 2048;
    Renderer::GPUPipelineHandle m_WebShadowPipeline;
    Renderer::GPUShaderHandle m_WebShadowShader;
    Renderer::GPUTextureHandle m_WebShadowMapTex;
    Renderer::GPUBindGroupLayoutHandle m_WebShadowFrameLayout;
    Renderer::GPUBindGroupLayoutHandle m_WebShadowObjectLayout;
    Renderer::GPUBufferHandle m_WebShadowVPBuffer;       // light VP UBO
    Renderer::GPUBufferHandle m_WebShadowObjectBuffer;   // per-entity model UBO
    Renderer::GPUBindGroupHandle m_WebShadowFrameBG;
    Renderer::GPUBindGroupHandle m_WebShadowObjectBG;

    // Shadow sampling in main PBR pass (bind group 3)
    Renderer::GPUBindGroupLayoutHandle m_WebShadowSampleLayout;
    Renderer::GPUBindGroupHandle m_WebShadowSampleBG;

    // Spot light shadows (max 2)
    static constexpr u32 WEB_SPOT_SHADOW_SIZE = 512;
    static constexpr u32 WEB_MAX_SPOT_SHADOWS = 2;
    Renderer::GPUTextureHandle m_WebSpotShadowTex[WEB_MAX_SPOT_SHADOWS];
    Renderer::GPUBufferHandle m_WebSpotShadowVPBuffer;   // 2 lights worth of VP pairs

    // Point light shadows (max 1, cubemap)
    static constexpr u32 WEB_POINT_SHADOW_SIZE = 512;
    static constexpr u32 WEB_MAX_POINT_SHADOWS = 1;
    Renderer::GPUTextureHandle m_WebPointShadowCubemap;  // managed by WebGPURenderer
    void* m_WebPointShadowFaceViews[6] = {};              // WGPUTextureView per face (cast at use)
    Renderer::GPUBufferHandle m_WebPointShadowVPBuffer;   // 6 face VPs

    // WebGPU post-process accessibility uniform buffer (uploads from m_WebPPAccessibility)
    Renderer::GPUBufferHandle m_WebPPAccessibilityBuffer;

    // Post-processing (offscreen scene → ACES tonemap → swapchain)
    Renderer::GPUShaderHandle m_WebPostProcessShader;
    Renderer::GPUPipelineHandle m_WebPostProcessPipeline;
    Renderer::GPUBindGroupLayoutHandle m_WebPostProcessLayout;
    Renderer::GPUBindGroupHandle m_WebPostProcessBG;
    Renderer::GPUTextureHandle m_WebSceneColorTex;           // offscreen RGBA16Float (resolve target)
    void* m_WebSceneColorView = nullptr;                      // WGPUTextureView (for resolve / post-process read)
    void* m_WebSceneDepthView = nullptr;                      // WGPUTextureView (offscreen depth, 4x MSAA)
    void* m_WebSceneDepthTex = nullptr;                       // WGPUTexture (offscreen depth)

    // MSAA 4x intermediate textures
    void* m_WebMSAAColorView = nullptr;                       // WGPUTextureView (4x MSAA render target)
    void* m_WebMSAAColorTex = nullptr;                        // WGPUTexture (4x MSAA)

    // Bloom (Dual Kawase downsample/upsample chain)
    static constexpr u32 WEB_BLOOM_LEVELS = 4;
    Renderer::GPUShaderHandle m_WebBloomThresholdShader;
    Renderer::GPUShaderHandle m_WebBloomDownShader;
    Renderer::GPUShaderHandle m_WebBloomUpShader;
    Renderer::GPUShaderHandle m_WebBloomCompositeShader;
    Renderer::GPUPipelineHandle m_WebBloomThresholdPipeline;
    Renderer::GPUPipelineHandle m_WebBloomDownPipeline;
    Renderer::GPUPipelineHandle m_WebBloomUpPipeline;
    Renderer::GPUPipelineHandle m_WebBloomCompositePipeline;
    Renderer::GPUBindGroupLayoutHandle m_WebBloomSingleTexLayout;  // 1 texture + 1 sampler
    Renderer::GPUBindGroupLayoutHandle m_WebBloomCompositeLayout;  // 2 textures + 2 samplers
    // Per-level bloom textures (half-res chain)
    Renderer::GPUTextureHandle m_WebBloomTex[WEB_BLOOM_LEVELS];
    void* m_WebBloomView[WEB_BLOOM_LEVELS] = {};                   // WGPUTextureView
    Renderer::GPUBindGroupHandle m_WebBloomDownBG[WEB_BLOOM_LEVELS]; // downsample bind groups
    Renderer::GPUBindGroupHandle m_WebBloomUpBG[WEB_BLOOM_LEVELS];   // upsample bind groups
    Renderer::GPUBindGroupHandle m_WebBloomThresholdBG;              // scene → bloom[0]
    Renderer::GPUBindGroupHandle m_WebBloomCompositeBG;              // scene + bloom → scene
    // Scratch texture for composite output (same size as scene)
    Renderer::GPUTextureHandle m_WebBloomScratchTex;
    void* m_WebBloomScratchView = nullptr;

    // Particle rendering (instanced billboard quads)
    Renderer::GPUShaderHandle m_WebParticleShader;
    Renderer::GPUPipelineHandle m_WebParticlePipeline;
    Renderer::GPUBufferHandle m_WebParticleQuadVB;
    Renderer::GPUBufferHandle m_WebParticleQuadIB;
    static constexpr u32 WEB_MAX_PARTICLES = 8192;

    // Grass rendering (instanced blades)
    Renderer::GPUShaderHandle m_WebGrassShader;
    Renderer::GPUPipelineHandle m_WebGrassPipeline;
    Renderer::GPUBufferHandle m_WebGrassBladeVB;
    Renderer::GPUBufferHandle m_WebGrassBladeIB;
    Renderer::GPUBindGroupLayoutHandle m_WebVolumeParamsLayout;
    u32 m_WebGrassBladeIndexCount = 0;

    // Tree rendering (instanced trunk+canopy)
    Renderer::GPUShaderHandle m_WebTreeShader;
    Renderer::GPUPipelineHandle m_WebTreePipeline;
    Renderer::GPUBufferHandle m_WebTreeMeshVB;
    Renderer::GPUBufferHandle m_WebTreeMeshIB;
    u32 m_WebTreeIndexCount = 0;

    // Sprite rendering (instanced textured billboards)
    Renderer::GPUShaderHandle m_WebSpriteShader;
    Renderer::GPUPipelineHandle m_WebSpritePipeline;
    Renderer::GPUBindGroupLayoutHandle m_WebSpriteTexLayout;

    // Procedural sky
    Renderer::GPUShaderHandle m_WebSkyShader;
    Renderer::GPUPipelineHandle m_WebSkyPipeline;

    f32 m_WebTime = 0.0f;  // Accumulated time for shader animations
#else
    // Vulkan-specific rendering resources (advanced pipelines)
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;               // Vulkan main pipeline (kept for compatibility)
    std::unique_ptr<Renderer::VulkanPipeline> m_OffscreenPipeline;
    Renderer::MaterialSpecKey m_BoundSpecKey{0xFFFFFFFF}; // Currently bound variant key (invalid = force rebind)
    VkRenderPass m_OffscreenRenderPass = VK_NULL_HANDLE;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;
    std::unique_ptr<Renderer::VulkanShader> m_ShadowVertexShader;

    // Line rendering (editor grid)
    std::unique_ptr<Renderer::VulkanPipeline> m_LinePipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_OffscreenLinePipeline;
    void CreateLinePipeline();

    // Geometry outline (inverted-hull backface extrusion)
    std::unique_ptr<Renderer::VulkanPipeline> m_OutlinePipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_OffscreenOutlinePipeline;
    std::unique_ptr<Renderer::VulkanShader> m_OutlineVertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_OutlineFragmentShader;
    void CreateOutlinePipeline();
    void RenderOutlinePass();
    void RenderOutlinePassForTarget();  // Offscreen render target variant

    // Per-entity wireframe overlay (VK_POLYGON_MODE_LINE over solid geometry)
    std::unique_ptr<Renderer::VulkanPipeline> m_WireframeOverlayPipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_OffscreenWireframeOverlayPipeline;
    void CreateWireframeOverlayPipeline();
    void RenderWireframeOverlayPass();
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Shadow mapping
    std::unique_ptr<Renderer::ShadowMap> m_ShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_ShadowPipeline;
    Math::Matrix4 m_CurrentCascadeVP;  // Set per-cascade in RenderShadowPass, read by RenderEntityShadow
    bool m_ShadowsEnabled = true;
    bool m_ShadowDescriptorsDirty = false;
    bool m_EditorWireframe = false;
    bool m_EditorUnlit = false;
    bool m_PlayerMode = false;
    bool m_PendingMSAAChange = false;
    f32 m_ShadowDistance = 100.0f;
    u32 m_PendingShadowResolution = 0; // 0 = no change pending
    bool m_CascadeProgressiveUpdate = false;
    u32 m_CascadeFarUpdateInterval = 2;   // Far cascades update every N frames (2-8)

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
#endif // !ENJIN_RENDERER_WEBGPU (shadow mapping block)

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

    // Shading model
    u32 m_ShadingModel = 0;            // 0=Blinn-Phong, 1=PBR (GGX)
    bool m_FresnelEnabled = false;
    bool m_EnergyConservation = false;
    bool m_GeometryTerm = false;
    bool m_SphereEnvMapEnabled = false;
    bool m_HalfLambert = false;        // Half-Lambert soft falloff (pre-PBR/hand-painted)
    f32 m_SphereEnvStrength = 0.5f;
    f32 m_PosterizeLevels = 0.0f;      // 0=disabled

    // Cel shading parameters
    bool m_CelShadingEnabled = false;
    f32 m_CelDiffuseBands = 3.0f;     // Number of quantized bands (2-8)
    f32 m_CelSpecularCutoff = 0.5f;   // Hard cutoff threshold for specular highlights

    // Geometry outlines (inverted-hull backface extrusion)
    bool m_GeometryOutlinesEnabled = false;
    f32 m_GeometryOutlineWidth = 0.02f;   // Global outline width (world units)
    Math::Vector3 m_GeometryOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f);

    // Global retro shader overrides (forced on all entities when true)
    bool m_GlobalFlatShading = false;
    bool m_GlobalAffineTexturing = false;
    bool m_GlobalVertexSnapping = false;
    bool m_GlobalStippleTransparency = false;
    bool m_GlobalUVQuantize = false;
    bool m_GlobalGouraudOnly = false;
    u8 m_GlobalVertexSnapResolution = 160;
    f32 m_TexturePageSize = 0.0f;    // PS1 VRAM texture page size (0=off, 64/128 typical)
    f32 m_DepthSortJitter = 0.0f;    // PS1 ordering table depth jitter (0=off, 0.001-0.01)
    f32 m_NormalQuantizeSteps = 0.0f; // Snap normals to N directions (0=off, 4-16)
    f32 m_LightRampMode = 0.0f;       // 0=off, 1=smooth step, 2=warm, 3=cool, 4=anime
    f32 m_CelShadowMode = 0.0f;       // 0=off, 1=purple, 2=blue, 3=warm, 4=neutral cool

#if !ENJIN_RENDERER_WEBGPU
    // Textures — integer-keyed for O(1) lookup after initial load
    std::unique_ptr<Renderer::Texture> m_DefaultWhiteTexture;
    std::unordered_map<std::string, u32> m_TexturePathToId;          // path → ID (only hit on first load)
    std::vector<std::shared_ptr<Renderer::Texture>> m_TextureById;   // dense: ID → texture
    std::vector<std::string> m_TextureIdToPath;                       // reverse: ID → path (for hot-reload)
    std::unordered_set<std::string> m_FailedTextures; // Paths that failed to load (avoid per-frame retry)

    // Text rendering (TextRasterizer is platform-agnostic, but texture cache needs Vulkan Texture)
    std::unordered_map<Entity, std::shared_ptr<Renderer::Texture>> m_TextTextureCache;
#endif
    Renderer::TextRasterizer m_TextRasterizer;

#if !ENJIN_RENDERER_WEBGPU
    // Skeletal animation
    std::unique_ptr<Renderer::VulkanBuffer> m_DefaultBoneBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_DefaultMorphBuffer;
    void UpdateBoneDescriptor(Renderer::VulkanBuffer* boneBuffer);
    void UpdateMorphDescriptor(Renderer::VulkanBuffer* morphBuffer);
    void UploadMorphTargetSSBO(Entity entity, ECS::MorphTargetComponent& morph, EntityRenderData& rd);
#endif

    // Weather, particle, grass, shrub, tree, and sprite batch renderers
#if !ENJIN_RENDERER_WEBGPU
    std::unique_ptr<Effects::WeatherRenderer> m_WeatherRenderer;
    std::unique_ptr<Effects::ParticleRenderer> m_ParticleRenderer;
    std::unique_ptr<Effects::FluidRenderer> m_FluidRenderer;
    std::unique_ptr<Effects::GrassRenderer> m_GrassRenderer;
    std::unique_ptr<Effects::ShrubRenderer> m_ShrubRenderer;
    std::unique_ptr<Effects::TreeRenderer> m_TreeRenderer;
    std::unique_ptr<Effects::SpriteBatchRenderer> m_SpriteBatchRenderer;
    std::unique_ptr<Effects::SpriteTextureAtlas> m_SpriteAtlas;
#endif
    Effects::WeatherSystem* m_MainPassWeather = nullptr;  // Weather for main pass (editor viewport)
    bool m_MainPassWeatherIsRain = false;

    // Scene composition cache (auto-detected per frame, drives rendering decisions)
    SceneComposition m_SceneComposition;
    u32 m_DiagnosticFrameCounter = 0;

#if !ENJIN_RENDERER_WEBGPU
    // Progressive cascade shadow updates — far cascades update less frequently
    u32 m_ShadowFrameCounter = 0;
    Math::Vector3 m_PrevShadowCameraPos{0, 0, 0};
    u32 m_CascadeUpdateCooldown = 0;    // Frames until next forced cascade recalc during rotation-only
    bool ShouldUpdateCascade(u32 cascade) const;

    // Shadow caster cache — rebuilt when dirty, avoids per-cascade entity iteration
    std::vector<Entity> m_ShadowCasters;
    bool m_ShadowCastersDirty = true;
    void RebuildShadowCasterCache();
#endif

    // Cached light entity list — rebuilt only when dirty (entity add/remove or light count change)
    std::vector<Entity> m_CachedLightEntities;
    bool m_LightListDirty = true;

#if !ENJIN_RENDERER_WEBGPU
    // Merged geometry buffer (single VB+IB for all static 3D meshes)
    std::unique_ptr<Renderer::MergedGeometryBuffer> m_GeometryPool;
    bool IsPoolEligible(Entity entity) const;  // Check if entity should use merged pool

    // GPU frustum culling system
    std::unique_ptr<Renderer::GPUCullingSystem> m_GPUCulling;
    std::vector<Renderer::CullableObject> m_CullableObjects;
    std::vector<u32> m_EntityToCullIndex; // Maps entity index to cullable object index
    bool m_GPUCullingEnabled = true;  // Enabled: GPU-driven indirect draws (no readback stall)
#endif

    bool m_IsEditorMode = false;      // When true, skip frustum culling (show all entities)
    bool m_SkipMainPassShadows = false; // When true, skip shadow passes in Update() (play mode)
    bool m_SkipMainPassRendering = false; // When true, skip geometry+effects in Update() (play mode — game view handles rendering)

#if !ENJIN_RENDERER_WEBGPU
    void BuildCullableObjectList();
    void PerformGPUCulling();
    void PerformGPUCullingAsync(); // Record to compute command buffer

    // Indirect draw: ObjectData upload + vkCmdDrawIndexedIndirectCount
    std::vector<ObjectDataGPU> m_ObjectDataCPU;
    void UploadObjectData();
    void DrawIndirect(VkCommandBuffer commandBuffer);
    // Entities drawn by DrawIndirect (non-textured pool entities) — skip in per-entity loop
    std::vector<bool> m_IndirectDrawn;

    // Texture-grouped indirect draws: batch textured pool entities by texture set
    std::unique_ptr<Renderer::IndirectDrawBatcher> m_IndirectDrawBatcher;
    void BuildTexturedIndirectBatches();
    void DrawTexturedIndirect(VkCommandBuffer commandBuffer);

    // Device Generated Commands: GPU generates entire command stream (push constants + draws)
    std::unique_ptr<Renderer::DeviceGeneratedCommands> m_DGC;
    void DrawDGC(VkCommandBuffer commandBuffer);

    // Async compute scheduler for RT/denoise overlap
    std::unique_ptr<Renderer::AsyncComputeScheduler> m_AsyncComputeScheduler;
    void DispatchRTEffectsAsync(u32 frameIndex);    // RT effects on async compute queue
    void DenoiseRTOutputsAsync(u32 frameIndex);     // Denoiser on async compute queue

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
    void EnsureWater3DMeshes();
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Helper to load or get cached texture
    std::shared_ptr<Renderer::Texture> GetOrLoadTexture(const std::string& path);
#endif

#if !ENJIN_RENDERER_WEBGPU
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
        Renderer::Texture* emissive,
        Renderer::Texture* matcap = nullptr);

    // Split uniform updates: frame-level (once) vs per-entity (material only)
    void UpdateFrameUniforms();
    void UpdateMaterialBuffer(Entity entity);

    // Batched material SSBO — collects all MaterialGPU data at frame start, uploads once
    void BuildMaterialSSBO();
    u32 GetMaterialIndex(Entity entity) const;

    // Uniform buffers (one per frame in flight)
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_UniformBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_LightingBuffers;
    LightingUBO m_CachedLightingData{};  // Cached copy of the latest lighting UBO for RT path tracer NEE
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_MaterialBuffers;  // Now SSBO (one per frame)
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Material SSBO batching state
    std::vector<u8> m_MaterialSSBOData;                 // CPU-side buffer (aligned MaterialGPU entries)
    std::unordered_map<u64, u32> m_EntityMaterialIndex;  // Entity -> index into SSBO
    u32 m_MaterialSSBOCount = 0;                         // Number of materials this frame
    u32 m_MaterialSSBOStride = 0;                        // Bytes per material entry (aligned to device minimum)
    u32 m_MaterialSSBOCapacity = 0;                      // Max materials the GPU buffer can hold
    bool m_MaterialSSBOBuilt = false;                    // Set after BuildMaterialSSBO(), reset at frame start

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
#endif

    // Material and scene-clear state (platform-agnostic, used by unguarded public methods)
    bool m_MaterialSSBODirty = true;                     // True when materials need full rebuild (entity add/remove, property edits)
    bool m_SceneClearPending = false;                    // Deferred scene-clear flag (flushed at frame boundary)
    u32 m_SceneClearCooldown = 0;                        // Skip game view for N frames after scene clear

    // Per-entity render data — dense vector indexed by entity ID for cache-friendly O(1) lookup
    std::vector<EntityRenderData> m_EntityRenderData;

#if !ENJIN_RENDERER_WEBGPU
    // Descriptor set caching — tracks what was last written to the shared descriptor set.
    // When the next entity's textures/bones match, vkUpdateDescriptorSets is skipped.
    struct LastBoundState {
        MaterialComponent::TextureKey textureKey;
        Renderer::VulkanBuffer* boneBuffer = nullptr;
        Renderer::VulkanBuffer* morphBuffer = nullptr;
        void Reset() { textureKey = {}; boneBuffer = nullptr; morphBuffer = nullptr; }
    };
    LastBoundState m_LastBound;
    bool m_GeometryPoolBound = false;  // Track if geometry pool buffers are bound this pass
#endif
    std::vector<Entity> m_SortedRenderList;  // Reused per frame to avoid allocation
    bool m_RenderListDirty = true;            // Set when entities/materials/visibility change; cleared after sort
    Math::Vector3 m_PrevCameraPos{0, 0, 0};   // Track camera movement for sort key recalculation
    u32 m_PrevEntityCount = 0;                // Detect entity count changes
    std::vector<Math::Vector3> m_IKChainCache; // Reused per frame for FABRIK IK solving

    // Draw call / triangle counters (current frame, accumulating)
    u32 m_DrawCallCount = 0;
    u32 m_TriangleCount = 0;
    u32 m_DescriptorCacheHits = 0;
    u32 m_DescriptorCacheWrites = 0;

    // Last completed frame's counters (snapshot taken in ResetFrameCounters before zeroing)
    u32 m_LastDrawCallCount = 0;
    u32 m_LastTriangleCount = 0;
    u32 m_LastDescriptorCacheHits = 0;
    u32 m_LastDescriptorCacheWrites = 0;

    // Cached player entity (any entity with a CharacterController) for per-frame position lookup.
    // Updated in OnEntityAdded/OnEntityRemoved to avoid linear search each frame.
    Entity m_CachedPlayerEntity = INVALID_ENTITY;

    // Asset hot-reload watcher (polls texture files for changes)
    Assets::FileWatcher m_TextureWatcher;
    f32 m_WatcherPollTimer = 0.0f;

#if !ENJIN_RENDERER_WEBGPU
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
#endif

    // Per-frame linear allocator (8 MB) for hot-path arrays rebuilt every frame.
    // Reset at frame start, replaces std::vector clear/push for render lists.
    std::unique_ptr<FrameAllocator> m_FrameAllocator;

#if !ENJIN_RENDERER_WEBGPU
#ifdef ENJIN_CLUSTERED_LIGHTING
    std::unique_ptr<Renderer::ClusteredLightingSystem> m_ClusteredLighting;
    std::vector<Renderer::ClusterLight> m_ClusterLightsCache;  // Reused per frame to avoid heap allocation
#endif

#ifdef ENJIN_VISIBILITY_BUFFER
    std::unique_ptr<Renderer::VisibilityBufferRenderer> m_VisibilityBuffer;
#endif

#ifdef ENJIN_VRS
    std::unique_ptr<Renderer::VariableRateShading> m_VRS;
#endif
#endif // !ENJIN_RENDERER_WEBGPU

    bool m_Initialized = false;

    // --- Temporal Upscaling state ---
    u32 m_UpscalerType = 0;        // 0=None, 1=FSR2, 2=DLSS, 3=XeSS
    u32 m_UpscalerQuality = 2;     // 0=Performance, 1=Balanced, 2=Quality, 3=UltraQuality
    f32 m_UpscalerSharpness = 0.0f;
#if !ENJIN_RENDERER_WEBGPU
    std::unique_ptr<Renderer::IUpscaler> m_Upscaler;
#endif

    // --- TAA (Temporal Anti-Aliasing) state ---
    // Anti-aliasing mode: 0=None, 1=FXAA, 2=TAA, 3=SMAA, 4=MSAA 2x, 5=MSAA 4x, 6=MSAA 8x
    u32 m_AAMode = 1;
    // Frame counter for Halton jitter sequence cycling (incremented each frame)
    u32 m_TAAFrameCounter = 0;
    // Previous jitter offset (NDC) stored for velocity buffer reprojection
    Math::Vector2 m_PrevJitter = Math::Vector2(0.0f, 0.0f);
    // Per-entity previous-frame model matrices for motion vector computation.
    // Keyed by entity ID — entries are added on first sight, removed on entity destruction.
    std::unordered_map<u64, Math::Matrix4> m_PrevModelMatrices;

#if !ENJIN_RENDERER_WEBGPU
    // --- Ray Tracing subsystems (null when unsupported) ---
    void InitializeRayTracing();
    void ShutdownRayTracing();
    void RebuildTLAS(VkCommandBuffer cmd);
    void DispatchRTEffects(VkCommandBuffer cmd);
    void TemporalReuseRTOutputs(VkCommandBuffer cmd);
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
    std::unique_ptr<Renderer::RTTranslucency> m_RTTranslucency;
    std::unique_ptr<Renderer::RTCaustics> m_RTCaustics;
    std::unique_ptr<Renderer::PathTracer> m_PathTracer;
    std::unique_ptr<Renderer::SVGFDenoiser> m_SVGFDenoiser;
    std::unique_ptr<Renderer::OIDNDenoiser> m_OIDNDenoiser;
    std::unique_ptr<Renderer::OptiXDenoiser> m_OptiXDenoiser;
    std::unique_ptr<Renderer::RTCompositor> m_RTCompositor;
    std::unique_ptr<Renderer::RTTemporalReuse> m_RTTemporalReuse;
    std::unique_ptr<Renderer::ReSTIR> m_ReSTIR;
    std::unique_ptr<Renderer::LightBVH> m_LightBVH;
    std::unique_ptr<Renderer::RadianceCache> m_RadianceCache;
    std::unique_ptr<Renderer::SurfelRadianceCache> m_SurfelRadianceCache;
    std::unique_ptr<Renderer::AdaptiveRayBudget> m_AdaptiveRayBudget;
    std::unique_ptr<Renderer::BindlessResourceManager> m_BindlessManager;
    std::unordered_map<void*, u32> m_TextureBindlessHandles;  // Texture* -> bindless handle
    u32 m_DefaultBindlessHandle = UINT32_MAX;
    u32 m_DenoiserType = 0;  // 0=SVGF, 1=OIDN, 2=OptiX

    // RT descriptor set layout and pool
    VkDescriptorSetLayout m_RTDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_RTDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_RTDescriptorSet = VK_NULL_HANDLE;

    // RT dummy/placeholder resources for unwritten descriptor bindings
    VkImage m_RTDummyImage = VK_NULL_HANDLE;
    VkDeviceMemory m_RTDummyImageMemory = VK_NULL_HANDLE;
    VkImageView m_RTDummyImageView = VK_NULL_HANDLE;
    // 3D dummy image for sampler3D bindings (froxel volume, etc.)
    VkImage m_RTDummy3DImage = VK_NULL_HANDLE;
    VkDeviceMemory m_RTDummy3DImageMemory = VK_NULL_HANDLE;
    VkImageView m_RTDummy3DImageView = VK_NULL_HANDLE;
    VkSampler m_RTDummySampler = VK_NULL_HANDLE;
    VkBuffer m_RTDummyBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_RTDummyBufferMemory = VK_NULL_HANDLE;

    // RT light data UBO (one per frame in flight)
    static constexpr u32 RT_FRAMES_IN_FLIGHT = 2;
    VkBuffer m_RTLightUBO[RT_FRAMES_IN_FLIGHT] = {};
    VkDeviceMemory m_RTLightUBOMemory[RT_FRAMES_IN_FLIGHT] = {};
    void* m_RTLightUBOMapped[RT_FRAMES_IN_FLIGHT] = {};

    // RT material SSBO (binding 9) — per-entity MaterialGPU indexed by entity ID
    static constexpr u32 RT_MATERIAL_BUFFER_INITIAL_CAPACITY = 4096;
    VkBuffer m_RTMaterialBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_RTMaterialMemory = VK_NULL_HANDLE;
    void* m_RTMaterialMapped = nullptr;
    u32 m_RTMaterialBufferCapacity = 0;  // Current capacity in number of MaterialGPU entries

    // RT simplified material SSBO (binding 18) — pre-baked material properties
    // to reduce hit shader divergence and texture lookups on secondary bounces
    VkBuffer m_RTSimplifiedMaterialBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_RTSimplifiedMaterialMemory = VK_NULL_HANDLE;
    void* m_RTSimplifiedMaterialMapped = nullptr;
    u32 m_RTSimplifiedMaterialBufferCapacity = 0;

    // NEE light SSBO (binding 16) — scene lights for path tracer direct light sampling
    VkBuffer m_RTNEELightBuffer[RT_FRAMES_IN_FLIGHT] = {};
    VkDeviceMemory m_RTNEELightMemory[RT_FRAMES_IN_FLIGHT] = {};
    void* m_RTNEELightMapped[RT_FRAMES_IN_FLIGHT] = {};
    static constexpr u32 RT_NEE_LIGHT_BUFFER_SIZE = 2097152;  // 2 MB — supports up to 32K lights (64 bytes each)

    // SDF scene SSBO (binding 17) — SDF objects for reflection fallback sphere tracing
    VkBuffer m_RTSDFBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_RTSDFMemory = VK_NULL_HANDLE;
    void* m_RTSDFMapped = nullptr;
    static constexpr u32 RT_SDF_BUFFER_SIZE = 16 + 48 * 256;  // Header (16B) + up to 256 SDF objects

    bool m_RTDescriptorsWritten = false;

    // --- Order-Independent Transparency (Weighted Blended OIT) ---
    std::unique_ptr<Renderer::OITManager> m_OITManager;
    bool m_OITEnabled = false;

    // --- SH Light Probes ---
    std::unique_ptr<Renderer::SHLightingSystem> m_SHLighting;

    // --- Reflection Probes ---
    std::unique_ptr<Renderer::ReflectionProbeSystem> m_ReflectionProbes;

    // --- SDF Scene (ray-marched primitives) ---
    std::unique_ptr<Renderer::SDFScene> m_SDFScene;

    // Onion skin ghosts (set by editor, rendered in main pass only)
    std::vector<Editor::OnionSkinGhost> m_OnionSkinGhosts;

    // Reusable bone buffer for skeletal onion skin ghost rendering
    std::unique_ptr<Renderer::VulkanBuffer> m_GhostBoneBuffer;
    usize m_GhostBoneBufferCapacity = 0;  // Current capacity in bytes

    void CreateRTDummyResources();
    void DestroyRTDummyResources();
    void WriteRTDescriptors();
    void TransitionRTOutputImages(VkCommandBuffer cmd);
    void UploadRTMaterials();
    void EnsureRTMaterialBuffer(u32 requiredCapacity);
    void EnsureRTSimplifiedMaterialBuffer(u32 requiredCapacity);
    void UpdateRTLightUBO(const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                          f32 lightIntensity, f32 shadowDistance, f32 shadowRadius, u32 frameCount,
                          f32 fireflyClamp, i32 enableNEE, i32 enableMIS,
                          i32 rrMinBounce, f32 rrMinProb,
                          u32 dirLightCount, u32 ptLightCount, u32 sptLightCount,
                          u32 maxBounces, u32 accumulatedSamples);
#endif // !ENJIN_RENDERER_WEBGPU (RT/OIT/SH/SDF block)
};

} // namespace ECS
} // namespace Enjin
