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
    struct ClothComponent;
    struct RopeComponent;
    struct VegetationComponent;
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
#include "Enjin/Renderer/AdaptiveQuality.h"
#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/Texture.h"
#endif
#include "Enjin/Renderer/TextRasterizer.h"
#include "Enjin/Renderer/FontAtlas.h"
#include "Enjin/Renderer/VectorTessellator.h"
#include "Enjin/ECS/Components/DisplayGraphic.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Viewmodel.h"
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
    class GPUParticleSystem;
    class SplatRenderer;
    enum class GPUParticlePreset : unsigned char; // defined in Effects/GPUParticleTypes.h
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
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include "Enjin/Effects/ParticleColliders.h"
#include "Enjin/Renderer/Skybox.h"   // SkyboxConfig is backend-agnostic (class below is guarded)

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
    class RTPipeline;
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
    class DDGIProbeSystem;
    class VolumetricFogSystem;
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
    u32 boneBase;                    // skinning arena: base matrix offset (slot*256); 0 = per-entity path
    f32 _pad[1];                     // pad to 192 total
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
    // GPU compute skinning (ADR-0002 Phase 1): deformed (world-space) vertices written once
    // per frame by skinning.comp. When present and compute skinning is active, raster passes
    // bind this instead of vertexBuffer and clear FLAG_SKINNED so no pass re-skins. Created
    // with VERTEX | STORAGE usage. u32 vertexCount tracks the bind-pose vertex count for the
    // dispatch and the output allocation.
    std::unique_ptr<Renderer::VulkanBuffer> skinnedVertexBuffer;
    u32 vertexCount = 0;
    bool skinnedThisFrame = false;  // true if the compute pass deformed skinnedVertexBuffer this frame
#else
    Renderer::GPUBufferHandle vertexBuffer;
    Renderer::GPUBufferHandle indexBuffer;
    Renderer::GPUBufferHandle boneBuffer;
    Renderer::GPUBufferHandle morphBuffer;
    Renderer::GPUBindGroupHandle texBindGroup;  // per-entity texture bind group (group 2)
    bool texBindGroupValid = false;             // true if textures loaded for this entity
    bool hasMatcap = false;                     // matcap texture bound (drives ObjectData.matcapBlend)
    bool hasScrollRefl = false;                 // scrolling-reflection texture bound
    // Skinned meshes cannot share the frame's object bind group, because
    // binding 1 is their own bone buffer. Cached here and rebuilt only when the
    // shared object buffer is reallocated, rather than created and destroyed
    // every frame.
    Renderer::GPUBindGroupHandle objBoneBindGroup;
    u32 objBoneBindGroupGen = 0;
    // The same idea against the OUTLINE buffer: a skinned entity's hull needs
    // its bones bound alongside the outline records, not the main ones.
    Renderer::GPUBindGroupHandle outlineBoneBindGroup;
    u32 outlineBoneBindGroupGen = 0;
#endif
    u32 indexCount = 0;
    bool valid = false;  // true if this slot is occupied
    // Full generational handle this slot was built for. Entity handles pack the slot index in
    // the low 32 bits and a generation in the high 32, and this dense array is indexed by
    // EntityIndex(entity) — so after a destroy+create the new entity lands on the SAME slot.
    // valid alone can't tell the difference; owner must match the full handle or the cached
    // buffers belong to the dead predecessor (its mesh would render instead of the new one).
    Entity owner = INVALID_ENTITY;
#if !ENJIN_RENDERER_WEBGPU
    Renderer::MeshAllocation poolAlloc;
#endif

    void Invalidate() {
#if !ENJIN_RENDERER_WEBGPU
        vertexBuffer.reset();
        indexBuffer.reset();
        boneBuffer.reset();
        morphBuffer.reset();
        skinnedVertexBuffer.reset();
        vertexCount = 0;
        skinnedThisFrame = false;
#else
        vertexBuffer = {};
        indexBuffer = {};
        boneBuffer = {};
        morphBuffer = {};
        texBindGroup = {};
        texBindGroupValid = false;
        objBoneBindGroup = {};
        objBoneBindGroupGen = 0;
#endif
        indexCount = 0;
        valid = false;
        owner = INVALID_ENTITY;
#if !ENJIN_RENDERER_WEBGPU
        poolAlloc = {};
#endif
    }
};

// Render system - renders entities with Transform and Mesh components
class ENJIN_API RenderSystem : public ISystem {
    // Clock for hover-highlight styles, ticked from Update in BOTH renderer
    // halves. m_WebTime cannot serve: it is incremented only in the WebGPU
    // block, so a desktop highlight would never animate.
    f32 m_HighlightTimeValue = 0.0f;
public:
    f32 GetHighlightTime() const { return m_HighlightTimeValue; }
    // Settled snow, 0..1, the same number the lighting UBO carries. The web
    // vegetation system is owned by the player and cannot read that buffer,
    // so it asks for the value instead.
    f32 GetSnowAccumulation() const;

    // Load (or fetch from cache) a texture by project-relative path, for code
    // outside RenderSystem that needs to resolve an authored path. The web
    // vegetation system is owned by the player, so it has no other way to turn
    // a TreeVolume's barkTexturePath into something it can bind.
    Renderer::GPUTextureHandle ResolveWebTexture(const std::string& path);
    void TickHighlightTime(f32 dt) { m_HighlightTimeValue += dt; }
private:

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

    // RT camera override: when set, ray tracing dispatch traces from this camera
    // instead of m_Camera. The editor uses it so RT/path tracing renders the game
    // view (game camera) while the fly cam keeps driving the scene viewport.
#if !ENJIN_RENDERER_WEBGPU
    void SetRTCameraOverride(Renderer::Camera* camera) { m_RTCameraOverride = camera; }
#else
    void SetRTCameraOverride(Renderer::Camera*) {} // no RT on web
#endif

    // Asset reader for loading textures from .enjpak on web
    void SetAssetReader(Build::AssetReader* reader) { m_AssetReader = reader; }

#if !ENJIN_RENDERER_WEBGPU
    Renderer::VulkanSwapchain* GetSwapchain() const;

    // HDR output — delegates to VulkanRenderer which handles swapchain + render pass + pipeline recreation.
    // The recreation is unsafe mid-frame (same crash class as MSAA), so SetHDREnabled only records
    // the request; ApplyPendingHDRChange performs it at the start of the next frame.
    void SetHDREnabled(bool enabled);
    bool IsHDREnabled() const;
    u32 GetHDROutputMode() const;
    bool IsHDRChangePending() const { return m_PendingHDRChange; }
    void ApplyPendingHDRChange(); // Deferred HDR application (safe between frames)
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

    // ── Script render targets (FR-4: live camera→texture from script) ──
    // Create is DEFERRED: the Vulkan resources are built at the next
    // FlushPendingChanges (the pre-recording safe point); the handle is valid
    // immediately for SetCamera/Bind calls. One target renders per frame
    // (round-robin) into offscreen viewport slot 2, so script targets never
    // fight the editor viewport (slot 0) or game view (slot 1).
    u64  CreateScriptRenderTarget(u32 width, u32 height);
    void DestroyScriptRenderTarget(u64 handle);
    void SetScriptRenderTargetCamera(u64 handle, u64 cameraEntity);
    // Point an entity's material base-color at the target's live image.
    bool BindScriptRenderTargetToEntity(u64 handle, Entity entity);
    // Record this frame's script-target render (round-robin). Must be called
    // OUTSIDE any render pass. Safe to call more than once per frame (no-ops).
    void RenderScriptTargets(VkCommandBuffer commandBuffer);
    bool HasScriptRenderTargets() const { return !m_ScriptRenderTargets.empty(); }
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

    // Where a character of an entity's text sits, in the text block's own local
    // space (top-left anchored, +x right, lines descending -y). Returns the
    // origin when the entity has no text or the font atlas is unavailable.
    // A caret, a highlight box and a click target all ride on this instead of
    // each caller re-deriving font metrics by hand.
    Math::Vector3 MeasureTextTo(Entity entity, i32 codepointIndex);

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
#else
    // Web: same script-facing surface (ScriptBindings_Render compiles on both
    // platforms — every script symbol must exist everywhere). The web shadow
    // pass has no runtime toggles yet, so these are fixed-value accessors that
    // let scripts degrade cleanly rather than fail to compile.
    bool IsShadowsEnabled() const { return true; }
    void SetShadowsEnabled(bool) {}
    f32 GetShadowStrength() const;
    void SetShadowStrength(f32 s);
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

    // Shadow distance is used by BOTH backends (web: single-cascade frustum fit)
    f32 GetShadowDistance() const { return m_ShadowDistance; }
    void SetShadowDistance(f32 d);

#if !ENJIN_RENDERER_WEBGPU
    // --- Adaptive quality (120-FPS-No-Matter-What pillar) ------------------------
    // Dynamically scales rendering quality to HOLD a target frame rate. Default OFF
    // (the editor must not change quality while you author) — enable it in the game
    // runtime. Ticked from Update() with 1/deltaTime; a quality change applies the
    // frame-safe shadow levers (resolution defers to FlushPendingChanges). Target FPS
    // defaults to 60; raise it for high-refresh displays. NOTE: with vsync the measured
    // FPS is capped at the display refresh, so set the target at or below it.
    // (Vulkan only for now — the shadow levers are Vulkan-side; WebGPU is a follow-up.)
    void SetAdaptiveQualityEnabled(bool enabled);
    bool IsAdaptiveQualityEnabled() const { return m_AdaptiveQualityEnabled; }
    void SetAdaptiveQualityTargetFPS(f32 fps) { m_AdaptiveQuality.SetTargetFPS(fps); }
    Renderer::QualityLevel GetAdaptiveQualityLevel() const { return m_AdaptiveQuality.GetCurrentLevel(); }
#endif

    bool IsBackfaceCullingEnabled() const { return m_BackfaceCulling; }
    void SetBackfaceCullingEnabled(bool enabled);

    bool IsWireframeEnabled() const { return m_WireframeMode; }
    void SetWireframeEnabled(bool enabled);

    // Global texture filtering: filter 0=Point/1=Bilinear/2=Trilinear,
    // anisotropy 0/2/4/8/16, wrap 0=Repeat/1=Clamp/2=Mirror. Reconfigures the
    // shared bindless sampler (no-op on WebGPU / when unchanged).
    void SetTextureFilterConfig(u32 filter, u32 anisotropy, bool mipmaps, u32 wrap);
    // Read back the live global texture filter config (for UI display + scene capture).
    u32  GetTextureFilter() const;
    u32  GetTextureAnisotropy() const;
    bool GetTextureMipmaps() const;
    u32  GetTextureWrap() const;

#if ENJIN_RENDERER_WEBGPU
    // Web: register a callback that draws into the scene pass (arg = the scene
    // WGPURenderPassEncoder as void*) right before it ends — used for GPU particles.
    void SetWebScenePassHook(std::function<void(void*)> hook) { m_WebScenePassHook = std::move(hook); }

    /**
     * @brief Draw into the directional SHADOW pass.
     *
     * The shadow pass walks entities with a MeshComponent, so anything drawn
     * procedurally from its own shader is invisible to it -- grass, shrubs and
     * trees exist only as a volume plus a scatter hash, so a whole grove cast
     * no shadow at all while every crate beside it did.
     *
     * The callback receives the pass encoder and the light's view-projection,
     * and should render depth only.
     */
    void SetWebShadowPassHook(std::function<void(void*, const Math::Matrix4&)> hook) {
        m_WebShadowPassHook = std::move(hook);
    }
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Compile GLSL (from the Shader Graph or hand-written) into a pipeline and bind it
    // for this entity's draw instead of the default geometry pipeline. Pipelines are
    // shared by source hash, so assigning the same shader to many entities costs ONE
    // pipeline. Returns false + err on compile failure (entity keeps its old shader).
    // Safe to call between frames from the editor; pipeline creation does not touch
    // in-flight command buffers. Vulkan-only for now (web needs WGSL codegen).
    bool SetEntityCustomShader(Entity entity, const std::string& vertGLSL,
                               const std::string& fragGLSL, std::string& err);
    void ClearEntityCustomShader(Entity entity);   // revert entity to the default pipeline
    bool HasEntityCustomShader(Entity entity) const;
#endif

    // Request a full deferred pipeline + descriptor-set recreation (processed at the
    // next FlushPendingChanges, before command recording). Same heal path as the
    // wireframe toggle. The editor calls this when a render target's render pass is
    // recreated (RT resize on scene/template load destroys the old pass); pipelines
    // built against the dead pass render undefined (black geometry, VUID-07609).
    void RequestPipelineRecreation();

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
    // A GPU particle strike on a world collider, with the emitter resolved.
    struct ParticleImpact {
        Math::Vector3 position;
        f32 speed;             // impact speed along the surface normal (m/s)
        ECS::Entity emitter;   // INVALID_ENTITY if the emitter no longer exists
    };
    // Entities whose animation fired a "footstep" event since the last take
    // (consumed by SurfaceResponseSystem for the anim-event step override).
    std::vector<ECS::Entity> TakeAnimFootsteps() {
        std::vector<ECS::Entity> out = std::move(m_AnimFootsteps);
        m_AnimFootsteps.clear();
        return out;
    }

    // Drain the impacts recorded since the last take (consumer clears).
    std::vector<ParticleImpact> TakeParticleImpacts() {
        std::vector<ParticleImpact> out = std::move(m_ParticleImpacts);
        m_ParticleImpacts.clear();
        return out;
    }

#if ENJIN_RENDERER_WEBGPU
    // Web variant: store the scene's sky config; the lighting-UBO fill reads it.
    void SetSkybox(const Renderer::SkyboxConfig& config) { m_WebSkyConfig = config; m_WebSkyConfigured = true; }
#endif

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

    // Elemental particles (fire/water/etc) for the MAIN pass — the direct
    // swapchain path used when the camera has post-processing OFF (the offscreen
    // PP path draws them separately via RenderElementalParticles). Set once by
    // the host; without it the main pass never draws fire/smoke and campfires
    // render as lights only.
    void SetMainPassElemental(const Effects::ElementalSystem* elemental) { m_MainPassElemental = elemental; }

    // Set fluid simulation (for FluidRenderer to read grid data)
    void SetFluidSimulation(Effects::FluidSimulation* sim);

    // --- Transient point lights ---
    // Extra point lights injected each frame by gameplay systems (fire, muzzle
    // flashes, spells). The producer clears and re-adds them every frame. These
    // feed both the surface PBR pass and clustered lighting, so they light
    // surfaces AND participating media (volumetric fog) exactly like a
    // LightComponent point light. The engine renderer stays decoupled from
    // gameplay: producers push generic lights, the renderer never names them.
    //
    // Example (per frame, gameplay side):
    //   renderer.ClearTransientPointLights();
    //   elemental.BuildFireLights(time, fireLights);
    //   for (const auto& fl : fireLights)
    //       renderer.AddTransientPointLight(fl.position, fl.range, fl.color, fl.intensity);
    struct TransientPointLight {
        Math::Vector3 position;
        f32 range = 0.0f;
        Math::Vector3 color;
        f32 intensity = 0.0f;
    };
    static constexpr u32 MAX_TRANSIENT_POINT_LIGHTS = 32;

    void ClearTransientPointLights() { m_TransientPointLights.clear(); }
    void AddTransientPointLight(const Math::Vector3& position, f32 range,
                                const Math::Vector3& color, f32 intensity) {
        if (m_TransientPointLights.size() >= MAX_TRANSIENT_POINT_LIGHTS) return;
        m_TransientPointLights.push_back({position, range, color, intensity});
    }

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
    // useOffscreenSets: bind the offscreen (game camera) uniforms — required when
    // drawing into a game-view render target after RenderToTarget has restored the
    // main sets, otherwise particles project through the editor camera and vanish
    void RenderWeatherParticles(const Effects::WeatherSystem& weather, bool isRain,
                                u32 viewportWidth = 0, u32 viewportHeight = 0,
                                bool useOffscreenSets = false, u32 offscreenViewportIndex = 0);
    // GPU-compute particles: draw the sim's particle buffer (dormant until spawned).
    // `pass` = the render pass currently being recorded (VK_NULL_HANDLE = swapchain
    // main pass, 2 attachments). The draw pipeline must be compatible with the pass
    // it records into, so every call site states where it is.
#if !ENJIN_RENDERER_WEBGPU
    void RenderGPUParticles(VkRenderPass pass = VK_NULL_HANDLE, u32 colorAttachments = 2);
    // Gaussian splat cloud (first GaussianSplatComponent in the scene). Same
    // pass-compatibility contract as particles; w/h = the viewport being
    // recorded (the vertex shader's covariance projection works in pixels).
    void RenderSplats(VkRenderPass pass = VK_NULL_HANDLE, u32 colorAttachments = 2,
                      f32 viewportW = 0.0f, f32 viewportH = 0.0f);
#else
    void RenderGPUParticles();
    void RenderSplats();
#endif
    // Seed GPU particles (wakes the compute sim; Debug Workstation has a burst button)
    void SpawnGPUParticles(u32 count, const Math::Vector3& position, const Math::Vector3& direction);
    // Fire-and-forget one-shot styled by a preset (hits, sparks, pickups). No entity needed.
    void SpawnGPUParticlePreset(u32 count, const Math::Vector3& position,
                                const Math::Vector3& direction, Effects::GPUParticlePreset preset);
    // Material surface burst: maps the MaterialComponent surfaceParticle id
    // (1=Dust,2=Grass,3=Spark,4=Splash,5=Smoke,6=Snow) to a particle preset so
    // footsteps and impacts look like the surface instead of a generic puff.
    void SpawnSurfaceBurst(u32 count, const Math::Vector3& position,
                           const Math::Vector3& direction, u8 surfaceParticle);
    // Spawn every frame from GPUParticleEmitterComponent entities (continuous + burst)
    void TickGPUEmitters(f32 deltaTime);
    void RenderParticles(u32 viewportWidth = 0, u32 viewportHeight = 0,
                         bool useOffscreenSets = false, u32 offscreenViewportIndex = 0);
    // useOffscreenSets: bind the offscreen (game camera) uniforms — required when
    // drawing into a game-view render target after RenderToTarget restored the
    // main sets (same wrong-camera failure the weather draw had)
    void RenderElementalParticles(const Effects::ElementalSystem& elementalSystem,
                                  u32 viewportWidth = 0, u32 viewportHeight = 0,
                                  bool useOffscreenSets = false, u32 offscreenViewportIndex = 0);
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
    // Skinned meshes compute-skinned this frame — each is currently one descriptor
    // alloc + 3 writes + one dispatch (the per-entity cost the arena work removes).
    u32 GetSkinnedMeshCount() const { return m_LastSkinnedMeshCount; }
    void ResetFrameCounters() {
        m_LastDrawCallCount = m_DrawCallCount;
        m_LastTriangleCount = m_TriangleCount;
        m_LastDescriptorCacheHits = m_DescriptorCacheHits;
        m_LastDescriptorCacheWrites = m_DescriptorCacheWrites;
        m_LastSkinnedMeshCount = m_SkinnedMeshCount;
        m_DrawCallCount = 0; m_TriangleCount = 0;
        m_DescriptorCacheHits = 0; m_DescriptorCacheWrites = 0;
        m_SkinnedMeshCount = 0;
    }

    // Fog and snow parameters (set by editor, uploaded to LightingUBO)
    void SetFogParams(f32 density, f32 start, f32 end, f32 heightFalloff) {
        m_FogDensity = density; m_FogStart = start; m_FogEnd = end; m_FogHeightFalloff = heightFalloff;
    }
    void SetFogColor(const Math::Vector3& color) { m_FogColor = color; }
    // LIVE snow, rewritten every frame by the weather-zone updater in the
    // editor and the player. Do not author into this: with no weather zone the
    // updater writes 0 here on every single frame.
    void SetSnowIntensity(f32 intensity) { m_SnowIntensity = intensity; }

    // AUTHORED snow, from the scene's render settings. It needs to be a
    // separate member from the live one: both used to share m_SnowIntensity,
    // and since the per-frame weather updater runs after the scene loads, a
    // scene that authored snowIntensity had it overwritten with 0 before the
    // first frame was ever drawn. One value, two writers, and the frequent
    // writer always won.
    void SetAuthoredSnowIntensity(f32 intensity) { m_AuthoredSnowIntensity = intensity; }

    // Fog has the same two-writer problem snow had. A weather zone drives fog
    // while it is active, and when none is the updater used to write HARDCODED
    // defaults (density 0, 20..100, colour 0.5/0.5/0.6) over whatever the scene
    // authored - every frame, so an authored fog never survived to be drawn.
    // Remembering the authored values lets the updater put them back instead of
    // inventing numbers.
    void SetAuthoredFog(f32 density, f32 start, f32 end, f32 heightFalloff,
                        const Math::Vector3& color) {
        m_AuthoredFogDensity = density;
        m_AuthoredFogStart = start;
        m_AuthoredFogEnd = end;
        m_AuthoredFogHeightFalloff = heightFalloff;
        m_AuthoredFogColor = color;
    }
    void RestoreAuthoredFog() {
        SetFogParams(m_AuthoredFogDensity, m_AuthoredFogStart,
                     m_AuthoredFogEnd, m_AuthoredFogHeightFalloff);
        SetFogColor(m_AuthoredFogColor);
    }
    f32 GetAuthoredSnowIntensity() const { return m_AuthoredSnowIntensity; }

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
        // Options preview split (web parity with the desktop menus): left of
        // the divider renders WITHOUT the previewed effect. 0 = off,
        // 5 = colorblind, 6 = brightness/contrast (the effects this pass owns).
        u32 previewEffect = 0;
        f32 previewDivider = 0.5f;
        // Post-process effects ported to web (was flat/accessibility-only). Layout
        // must stay in lockstep with PostProcessParams in POSTPROCESS_WGSL. All
        // scalars (no vec3) to avoid std140 alignment traps. 16 f32 = 64 bytes.
        f32 saturation = 1.0f;
        f32 colorFilterR = 1.0f;
        f32 colorFilterG = 1.0f;
        f32 colorFilterB = 1.0f;
        f32 vignetteIntensity = 0.0f;
        f32 vignetteSmoothness = 0.5f;
        f32 chromaticAberration = 0.0f;
        f32 colorQuantLevels = 0.0f;   // 0 = off
        f32 screenW = 1280.0f;
        f32 screenH = 720.0f;
        f32 filmGrain = 0.0f;          // 0 = off
        f32 crtScanline = 0.0f;        // 0 = off
        f32 timeSec = 0.0f;            // set per-frame for animated grain
        f32 dither = 0.0f;             // ordered-dither strength, 0 = off
        f32 stipple = 0.0f;            // 0 = off, 1 = mono, 2 = duotone, 3 = full-colour dither
        f32 stippleScale = 1.0f;
        f32 ssao = 0.0f;              // screen-space AO strength, 0 = off (color-space approx)
        f32 ssaoRadius = 0.5f;
        // Contrast-adaptive sharpening, the RCAS half of FSR-style upscaling.
        // 0 = off. Pads keep the block a 16-byte multiple and must match the
        // WGSL struct exactly: 28 f32 = 112 bytes.
        f32 sharpness = 0.0f;
        // Anti-aliasing: 1 = run FXAA, 0 = do not. Takes the first of the three
        // pads, so the block stays 112 bytes and in lockstep with the WGSL
        // struct. The shader used to run FXAA unconditionally, which made the
        // options menu's AA setting inert in both directions: "None" still paid
        // for the pass, and nothing else changed anything.
        f32 fxaaEnabled = 1.0f;
        // Depth-driven effects. These were impossible while the web scene depth
        // was 4x MSAA and could not be bound; MSAA came off on 2026-09-03 and
        // the depth buffer is now a plain single-sample texture the pass can
        // read, so the roadmap's "needs a depth pre-pass or abstraction work"
        // reduced to one usage flag and a binding.
        f32 celOutline = 0.0f;         // Sobel-on-depth outline thickness, 0 = off
        f32 celOutlineThreshold = 0.1f;
        f32 celOutlineR = 0.0f;
        f32 celOutlineG = 0.0f;
        f32 celOutlineB = 0.0f;
        // Needed to linearise the depth sample. A reversed or wrong pair makes
        // every depth comparison meaningless rather than merely inaccurate.
        f32 nearPlane = 0.1f;
        f32 farPlane = 1000.0f;
        f32 ppPad1 = 0.0f;
        f32 ppPad2 = 0.0f;
        f32 ppPad3 = 0.0f;            // 36 f32 = 144 bytes (16-multiple)
    };
    WebPPAccessibilityParams m_WebPPAccessibility;
    void SetWebAccessibility(u32 colorblindMode, f32 strength, f32 brightness, f32 contrast) {
        m_WebPPAccessibility.colorblindMode = colorblindMode;
        m_WebPPAccessibility.colorblindStrength = strength;
        m_WebPPAccessibility.brightness = brightness;
        m_WebPPAccessibility.contrast = contrast;
    }
    void SetWebAccessibilityPreview(u32 effect, f32 divider) {
        m_WebPPAccessibility.previewEffect = effect;
        m_WebPPAccessibility.previewDivider = divider;
    }
    // Ported post-process effects for the web PP pass. Fed from the scene's
    // SceneRenderSettings (ApplyToRuntime passes null pp on web, so this is the path).
    void SetWebPostProcess(f32 saturation, f32 cfR, f32 cfG, f32 cfB,
                           f32 vignetteIntensity, f32 vignetteSmoothness,
                           f32 chromaticAberration, f32 colorQuantLevels,
                           f32 filmGrain, f32 crtScanline,
                           f32 dither, f32 stipple, f32 stippleScale,
                           f32 ssao = 0.0f, f32 ssaoRadius = 0.5f,
                           f32 celOutline = 0.0f, f32 celOutlineThreshold = 0.1f,
                           const Math::Vector3& celOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f)) {
        m_WebPPAccessibility.ssao = ssao;
        m_WebPPAccessibility.ssaoRadius = ssaoRadius;
        m_WebPPAccessibility.celOutline = celOutline;
        m_WebPPAccessibility.celOutlineThreshold = celOutlineThreshold;
        m_WebPPAccessibility.celOutlineR = celOutlineColor.x;
        m_WebPPAccessibility.celOutlineG = celOutlineColor.y;
        m_WebPPAccessibility.celOutlineB = celOutlineColor.z;
        m_WebPPAccessibility.dither = dither;
        m_WebPPAccessibility.stipple = stipple;
        m_WebPPAccessibility.stippleScale = stippleScale;
        m_WebPPAccessibility.saturation = saturation;
        m_WebPPAccessibility.colorFilterR = cfR;
        m_WebPPAccessibility.colorFilterG = cfG;
        m_WebPPAccessibility.colorFilterB = cfB;
        m_WebPPAccessibility.vignetteIntensity = vignetteIntensity;
        m_WebPPAccessibility.vignetteSmoothness = vignetteSmoothness;
        m_WebPPAccessibility.chromaticAberration = chromaticAberration;
        m_WebPPAccessibility.colorQuantLevels = colorQuantLevels;
        m_WebPPAccessibility.filmGrain = filmGrain;
        m_WebPPAccessibility.crtScanline = crtScanline;
        // Sharpening is driven by SetWebSharpness rather than this call, so a
        // scene's post-process settings never silently switch off the pass that
        // makes upscaled rendering look right. The scale/sharpness members are
        // WebGPU-only, so the guard has to match theirs.
#if ENJIN_RENDERER_WEBGPU
        m_WebPPAccessibility.sharpness = m_WebSharpness;
        // AA mode 0 is None; everything else falls back to FXAA, which is the
        // only anti-aliasing the web path actually has. MSAA needs a
        // multi-sample swapchain the web target fixed at one sample, TAA needs
        // motion vectors this path does not produce, and SMAA is not
        // implemented anywhere.
        m_WebPPAccessibility.fxaaEnabled = (m_AAMode == 0) ? 0.0f : 1.0f;
#endif
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

    // Editor selection highlight: the entities to draw a bright outline around
    // (the selected entity plus its descendants, so all parts of a picked FBX
    // light up). Set each frame by the editor; empty = nothing highlighted.
    void SetHighlightEntities(std::unordered_set<Entity> entities) { m_HighlightEntities = std::move(entities); }
    void SetHighlightColor(const Math::Vector3& c) { m_HighlightColor = c; }

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
    // One player-facing Render Scale, honoured by whichever path this build
    // has. The menus set a single number; how it is reached (FSR 2 presets on
    // desktop, the scaled scene target on web) is the renderer's business, not
    // the menu's. Returns the scale actually adopted, which may be snapped to
    // the nearest preset the desktop upscaler can express.
    f32 ApplyRenderScale(f32 scale);

    f32 GetUpscalerSharpness() const { return m_UpscalerSharpness; }
    void SetUpscalerSharpness(f32 s) { m_UpscalerSharpness = s; }
#if !ENJIN_RENDERER_WEBGPU
    bool IsUpscalerActive() const { return m_UpscalerType > 0 && m_Upscaler != nullptr; }
    Renderer::IUpscaler* GetUpscaler() const { return m_Upscaler.get(); }
#else
    bool IsUpscalerActive() const { return false; }
#endif

    // Weather-driven sky: rain greys the gradient, snow pales it. Runtimes
    // feed the live weather intensities each frame; every sky read site goes
    // through WeatherSky() so the AUTHORED config never mutates (the editor
    // UI keeps showing the authored colors). Shared across backends — the
    // web path applies it to m_WebSkyConfig.
    void SetWeatherSkyBlend(f32 rain, f32 snow) {
        m_WeatherSkyRain = std::clamp(rain, 0.0f, 1.0f);
        m_WeatherSkySnow = std::clamp(snow, 0.0f, 1.0f);
    }
    Renderer::SkyboxConfig WeatherSky(const Renderer::SkyboxConfig& cfg) const;

#if !ENJIN_RENDERER_WEBGPU
    // Skybox
    // (impl differs per backend: Vulkan bakes/applies deferred; web feeds the
    // lighting UBO's sky block)
    void SetSkybox(const Renderer::SkyboxConfig& config);
    const Renderer::SkyboxConfig& GetSkyboxConfig() const { return m_Skybox.GetConfig(); }
    void SetWater2D(const Renderer::Water2DConfig& config);
    const Renderer::Water2DConfig& GetWater2DConfig() const { return m_Water2DConfig; }
    Renderer::Skybox* GetSkybox() { return &m_Skybox; }

    // Ray tracing
    bool IsRayTracingSupported() const;
    bool IsRayTracingEnabled() const { return m_RTEnabled; }
    // Enabling after boot defers initialization to the next pre-recording
    // flush. InitializeRayTracing used to run ONCE at RenderSystem::Initialize
    // — before any scene could set rtEnabled — so the whole RT stack was
    // unreachable at runtime (every rtEnabled scene rendered pure raster).
    void SetRayTracingEnabled(bool enabled) {
        if (enabled == m_RTEnabled) return;
        m_RTEnabled = enabled;
        if (enabled && !m_RTInitialized) m_PendingRTInit = true;
    }

    // GPU compute skinning (ADR-0002 Phase 1, Vulkan-only). Default OFF: when disabled the
    // engine uses the existing vertex-shader skinning path unchanged. When on, skinned meshes
    // are skinned once per frame by a compute pass and all raster passes read the result.
    void SetComputeSkinningEnabled(bool enabled) { m_ComputeSkinningEnabled = enabled; }
    bool IsComputeSkinningEnabled() const { return m_ComputeSkinningEnabled; }

    // Free a mesh's CPU vertices/indices after they're uploaded to the GPU, reclaiming RAM
    // for source-reproducible meshes (task #3). Reload is on-demand via
    // MeshAssetCache::EnsureCpuData (baked/in-memory cache). OFF by default until every CPU
    // consumer (physics colliders, picking, effects) is routed through the reload.
    void SetFreeMeshCpuData(bool enabled) { m_FreeMeshCpuData = enabled; }
    bool IsFreeMeshCpuData() const { return m_FreeMeshCpuData; }
    // Animation LOD: distant skeletal animators refresh their pose at a reduced
    // rate (dt is banked so time never drifts). The single biggest CPU lever for
    // hundreds of animated entities. On by default; off = every animator full-rate.
    void SetAnimationLODEnabled(bool enabled) { m_AnimationLODEnabled = enabled; }
    bool IsAnimationLODEnabled() const { return m_AnimationLODEnabled; }
    // Player mode: skip GPU compute shaders (culling, HiZ, clustered lighting)
    // that use disk-loaded SPIR-V not available in built games.
    void SetPlayerMode(bool enabled) { m_PlayerMode = enabled; }
    bool IsPlayerMode() const { return m_PlayerMode; }
    u32 GetRTMode() const { return m_RTMode; }
    void SetRTMode(u32 mode) { m_RTMode = mode; }

    // Record the per-frame RT chain (TLAS rebuild + dispatch + denoise + composite)
    // into the current command buffer, outside a render pass. Update() calls this on
    // the main-pass path; the editor calls it from RenderOffscreen because Update()
    // early-returns on m_SkipMainPassRendering before reaching RT. Idempotent per frame.
    void RecordRTFrame(bool allowAsync);

    // Record the per-frame compute pre-pass (clustered light assign, compute
    // skinning, DDGI, volumetric fog froxels, GPU particles) into the current
    // command buffer, outside a render pass. Update() calls this on the
    // main-pass path; the player's offscreen post-process path calls it
    // explicitly because Update() early-returns on m_SkipMainPassRendering
    // before reaching it — without it the fog volume never gets its one-shot
    // neutral clear and the PBR fog composite multiplies the scene toward
    // black. Idempotent per frame (flag reset in FlushPendingChanges).
    // Per-frame CPU phase: runs EXACTLY ONCE per frame, before any view
    // renders. Emitter spawn ticks, particle collider gather, and GPU impact
    // readback/translation live here; the per-view render paths only read.
    // Idempotent (guarded until FlushPendingChanges opens the next frame), and
    // RecordComputePrePass self-calls it, so legacy callers stay correct.
    void BeginFrame(f32 deltaTime);

    void RecordComputePrePass(f32 deltaTime);

    // Did this light win a shadow-map slot in the last selection pass? Only
    // the strongest MAX_SHADOW_POINT/SPOT_LIGHTS (intensity over distance
    // squared) get maps each frame; a shadow-casting light without a slot
    // bleeds through walls. Directional lights use CSM and never compete.
    bool LightHasShadowSlot(ECS::Entity e) const {
        for (const auto& l : m_ShadowPointLights) if (l.entity == e) return true;
        for (const auto& l : m_ShadowSpotLights)  if (l.entity == e) return true;
        return false;
    }

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

    // Load (or fetch cached) a texture and return its bindless array index
    // (set 1 binding 0), or -1 on failure. Used by the shader graph so texture
    // nodes can bake their index at compile time.
    i32 ResolveBindlessTextureIndex(const std::string& path);

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
#if !ENJIN_RENDERER_WEBGPU
    // Switch the bound geometry pipeline to the depth-write-OFF `transparent` variant
    // for AlphaMode::Blend entities (and back to `opaque` otherwise), only on change.
    // `transparentBound` tracks the current state across a draw loop. No-op if
    // `transparent` is null (creation failed) — stays on the opaque pipeline.
    void BindGeometryPipelineForMaterial(VkCommandBuffer cmd, Entity entity,
        Renderer::VulkanPipeline* opaque, Renderer::VulkanPipeline* transparent, bool& transparentBound);
#endif
    void RenderSprites();  // Sorted 2D sprite pass (after 3D geometry)
    void ClassifySceneComposition();  // Update m_SceneComposition if dirty
    void CreateDefaultMesh();
    void CreatePipeline();

#if !ENJIN_RENDERER_WEBGPU
    // poolBound tracks whether the merged geometry pool's VB/IB are bound in
    // THIS command buffer. Must be per-command-buffer state: the parallel
    // shadow path records into per-thread secondaries, and the old shared
    // member flag meant threads skipped the bind after another thread's
    // secondary bound it (indexed draws with no index buffer = driver crash
    // the moment a scene crossed the 32-shadow-caster parallel threshold).
    // maskPipeline: alpha-cutout variant chosen per draw for Mask-mode materials
    // with a base color texture (shaped shadows for foliage/hair cards).
    // boundPipeline: per-command-buffer bind state (like poolBound) — callers
    // initialize it to the pipeline they bound before the entity loop.
    void RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer, bool& poolBound,
                            VkPipeline normalPipeline, VkPipeline maskPipeline,
                            VkPipeline& boundPipeline);
    // First-person viewmodel depth remap: entities tagged ViewmodelComponent
    // draw with the viewport depth range compressed to [0, kViewmodelDepthMax]
    // so they render in front of all world geometry and never visually clip
    // into walls. State-tracked to avoid redundant viewport sets; the flag and
    // pass dimensions reset wherever a pass (re)sets its viewport. Splitscreen
    // viewports are not remapped (viewmodels draw normally there).
    static constexpr f32 kViewmodelDepthMax = 0.05f;
    void SetViewmodelDepth(VkCommandBuffer cmd, bool viewmodel);
    bool m_ViewmodelDepthActive = false;
    f32 m_PassViewportW = 0.0f;
    f32 m_PassViewportH = 0.0f;
    // Draw an entity's mesh with a model-matrix override, tint, and opacity.
    // useRealMaterial=false: flat-tint silhouette (onion-skin ghosts).
    // useRealMaterial=true: the entity's real material + textures, tinted by `tint`
    // and alpha-blended at `opacity` — used for full-colour planar reflections.
    void RenderEntityGhost(Entity entity, const Math::Matrix4& modelMatrix,
                           const Math::Vector3& tint, f32 opacity,
                           const std::vector<Math::Matrix4>* skinningMatrices = nullptr,
                           bool useRealMaterial = false);
    void RenderOnionSkinGhosts();
    // Hand-crafted planar floor reflection (PS2/GameCube wet-floor look): for each
    // active ReflectivePlaneComponent, re-draw the scene geometry mirrored across the
    // plane, tinted and dimmed, so it reads as a reflection below the surface. Drawn
    // into the main pass after opaque geometry; no offscreen target.
    void RenderPlanarReflections();
    // Mirror every mesh above a horizontal plane at planeY and re-draw it as a
    // tinted, dimmed reflection. Shared by reflective floors and reflective water.
    void MirrorSceneAcrossPlane(f32 planeY, const Math::Vector3& tint, f32 strength, Entity skipEntity);
    void CreateShadowPipeline();
    // Recreate all pipelines. If gpuAlreadyIdle is true, skips vkDeviceWaitIdle (caller guarantees GPU is idle).
    void RecreatePipelines(bool gpuAlreadyIdle = false);
    void CreateUniformBuffers();
    void CreateDescriptorSets();
    void UpdateUniformBuffer(Entity entity);
    // Sets up GPU buffers for an entity's mesh. Returns pointer to EntityRenderData,
    // or nullptr if the entity has no valid mesh.
    EntityRenderData* SetupEntityBuffers(Entity entity);

    // Cache lookup for the dense render-data array. Indexed by EntityIndex (slot) but validated
    // against the FULL generational handle — a recycled slot whose cache was built for a dead
    // predecessor is treated as a miss (see EntityRenderData::owner).
    EntityRenderData* GetRenderData(Entity entity) {
        usize idx = static_cast<usize>(EntityIndex(entity));
        if (idx >= m_EntityRenderData.size()) return nullptr;
        EntityRenderData& rd = m_EntityRenderData[idx];
        return (rd.valid && rd.owner == entity) ? &rd : nullptr;
    }
    EntityRenderData* GetOrCreateRenderData(Entity entity) {
        EntityRenderData* rd = GetRenderData(entity);
        return rd ? rd : SetupEntityBuffers(entity);
    }
    void RenderShadowPass();
#endif

    // Per-frame cached component storage pointers — refreshed once at the start of
    // Update() to avoid repeated type-ID hash map lookups in hot render loops.
    // Each GetComponent<T>(entity) does hash(typeId)->storage then hash(entity)->index;
    // caching the storage pointer eliminates the first lookup for every entity.
public:
    void RefreshStorageCache();
    // Compare against World::GetStorageEpoch() before using cached storage
    // pointers — on mismatch (World::Clear ran) every cached pointer is
    // dangling; refetches and drops derived raw pointers. One int compare
    // in the common case.
    void EnsureStorageCacheFresh();
private:
    u32 m_CachedStorageEpoch = 0;  // epoch captured at last RefreshStorageCache
    // Entities whose GPU buffers need (re)building — processed in
    // FlushPendingChanges (pre-recording). OnEntityAdded queues here instead of
    // building inline, which destroyed live buffers under the recording frame.
    std::vector<Entity> m_PendingBufferSetups;
#if !ENJIN_RENDERER_WEBGPU
    // Deferred GPU-buffer destruction. VulkanBuffer::Destroy is an immediate
    // vkDestroyBuffer and frames stay in flight for MAX_FRAMES_IN_FLIGHT —
    // destroying a buffer a still-executing frame references is a device loss
    // (deleting an imported skinned mesh, 2026-08-08). Entity buffers being
    // freed or rebuilt are parked here and destroyed a few flushes later.
    struct RetiredBufferSet {
        u64 flushTick = 0;
        std::vector<std::unique_ptr<Renderer::VulkanBuffer>> buffers;
    };
    std::vector<RetiredBufferSet> m_BufferGraveyard;
    // Replaced text textures parked until no in-flight frame references them
    // (see the drain in FlushPendingChanges).
    struct RetiredTexture {
        u64 flushTick = 0;
        std::shared_ptr<Renderer::Texture> texture;
    };
    std::vector<RetiredTexture> m_TextTextureGraveyard;
    // Bindless SLOTS also have to outlive the frames that sample them. Parking
    // the texture but freeing its descriptor slot immediately still hands a
    // recycled or dead slot to an in-flight frame, which is a device loss. The
    // text path gets away with it because a rasterize is rare; a texture
    // regenerating at 30 Hz does not.
    struct RetiredBindless {
        u64 flushTick = 0;
        u32 handle = 0;
    };
    std::vector<RetiredBindless> m_BindlessGraveyard;
    u64 m_FlushTick = 0;
    // Move rd's GPU buffers into the graveyard, then Invalidate() it. Use this
    // instead of calling Invalidate() directly anywhere the GPU might still be
    // reading the buffers (i.e., everywhere except device-idle paths).
    void RetireEntityBuffers(EntityRenderData& rd);
#endif
    ComponentStorage<TransformComponent>* m_CachedTransformStorage = nullptr;
    ComponentStorage<MeshComponent>* m_CachedMeshStorage = nullptr;
    ComponentStorage<MaterialComponent>* m_CachedMaterialStorage = nullptr;
    ComponentStorage<MaterialSlotsComponent>* m_CachedMaterialSlotsStorage = nullptr;
    ComponentStorage<AnimatorComponent>* m_CachedAnimatorStorage = nullptr;
    ComponentStorage<ViewmodelComponent>* m_CachedViewmodelStorage = nullptr;
    // First animator-with-skeleton entity (orphan skinned mesh fallback). Stored as an
    // ENTITY, not an AnimatorComponent*: AddComponent<AnimatorComponent> mid-frame (FBX
    // import dialog runs between Update and RenderToTarget) reallocates the component
    // storage and dangles every cached pointer — 2026-08-08 multi-FBX import crash.
    Entity m_FallbackAnimatorEntity = INVALID_ENTITY;
    // Maps a shared Skeleton to the entity whose AnimatorComponent drives it. Lets follower
    // skinned meshes (no animator of their own) resolve the leader's animator by shared
    // skeleton identity, so every mesh in one model skins from ONE clock (no pause desync /
    // drift). Rebuilt each frame in the animator update loop. Keyed by raw Skeleton pointer.
    // Values are ENTITIES for the same dangling-pointer reason as m_FallbackAnimatorEntity.
    std::unordered_map<const Animation::Skeleton*, Entity> m_SkeletonToAnimator;
    // Per-frame work list for the parallel animation pass. Pass 1 (serial) collects the
    // animators to sample this frame plus their banked dt; Pass 2 fans comp->Update()
    // across m_ThreadPool (pose sampling touches only per-animator state, no World access;
    // clip events are COLLECTED, not fired, so nothing calls into gameplay/scripts on a
    // worker thread); Pass 3 (serial) applies IK and fires the deferred events. Reused
    // across frames to avoid per-frame allocation.
    struct AnimUpdateJob { Entity entity; AnimatorComponent* comp; f32 stepDt; };
    std::vector<AnimUpdateJob> m_AnimJobs;
    // Resolve the animator that should skin this entity: its own, else the leader driving
    // its shared skeleton, else the per-frame fallback. Returns null for non-skinned entities.
    AnimatorComponent* ResolveAnimator(Entity entity);
    // Fetch the AnimatorComponent on an entity at USE time (never cache the result across
    // anything that can AddComponent).
    AnimatorComponent* AnimatorFromEntity(Entity e);
    ComponentStorage<TextComponent>* m_CachedTextStorage = nullptr;
    ComponentStorage<ArtStyleComponent>* m_CachedArtStyleStorage = nullptr;
    ComponentStorage<Sprite2DComponent>* m_CachedSpriteStorage = nullptr;
    ComponentStorage<WaterVolumeComponent>* m_CachedWaterVolumeStorage = nullptr;
    // Probed for every mesh entity every frame by the draw-collection loop.
    // Without these each probe went through GetStorage<T>() and a typeid hash.
    ComponentStorage<ClothComponent>* m_CachedClothStorage = nullptr;
    ComponentStorage<RopeComponent>* m_CachedRopeStorage = nullptr;
    ComponentStorage<VegetationComponent>* m_CachedVegetationStorage = nullptr;
    ComponentStorage<Water3DComponent>* m_CachedWater3DStorage = nullptr;

    World* m_World = nullptr;
    Renderer::IRenderBackend* m_Renderer = nullptr;
#if !ENJIN_RENDERER_WEBGPU
    Renderer::VulkanRenderer* m_VulkanRenderer = nullptr;  // Cached cast for Vulkan-specific API calls
#endif
    Renderer::Camera* m_Camera = nullptr;

    // Transient point lights pushed by gameplay each frame (see AddTransientPointLight).
    std::vector<TransientPointLight> m_TransientPointLights;
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

    // Web SDF text generation (unified display P1 on web): the CPU atlas build
    // is shared (FontAtlas/stb_truetype); the texture is a WebGPU texture keyed
    // into m_WebTextureCache so the normal per-entity bind resolves it by path.
    struct WebSDFFont {
        std::unique_ptr<Renderer::FontAtlas> atlas;
        Renderer::GPUTextureHandle texture;
        std::string cacheKey;
    };
    std::unordered_map<std::string, WebSDFFont> m_WebSDFFonts;
    std::unordered_set<Entity> m_WebSDFTextMeshes;   // entities whose mesh the SDF path owns
    std::unordered_set<Entity> m_WebTextTextures;    // text-on-surface entities with a raster text texture
    Renderer::FontAtlas* WebGetOrBuildFontAtlas(const std::string& fontPath, std::string& outCacheKey);
    void WebEnsureTextMeshes();

    // Web scene-pass hook: invoked with the scene WGPURenderPassEncoder (as void*)
    // right before the scene pass ends. The web player uses it to draw GPU particles
    // with real scene depth. Public setter below.
    std::function<void(void*)> m_WebScenePassHook;
    std::function<void(void*, const Math::Matrix4&)> m_WebShadowPassHook;

    // Default bone buffer (single identity matrix for non-skinned meshes)
    Renderer::GPUBufferHandle m_WebDefaultBoneBuffer;

    // Shadow mapping (1-cascade directional)
    static constexpr u32 WEB_SHADOW_MAP_SIZE = 2048;
    Renderer::GPUPipelineHandle m_WebShadowPipeline;
    Renderer::GPUShaderHandle m_WebShadowShader;
    Renderer::GPUTextureHandle m_WebShadowMapTex;
    // Directional shadow cache. The web shadow fit is built from the CASTER
    // AABB, not the camera frustum, so for a scene of static geometry under a
    // static sun the depth map is bit-identical every frame -- and redrawing it
    // meant 100+ draw calls, each with its own uniform write and bind group,
    // for a texture that never changes. m_WebShadowSignature folds the light
    // direction and every caster's transform into one value during the fit loop
    // that already walks them, so detecting "nothing moved" is free.
    u64 m_WebShadowSignature = 0;
    bool m_WebShadowValid = false;      // false = must redraw (boot, resize, scene change)
    // World size of one shadow-map texel, from the last fit. The change
    // signature quantises to it: a caster movement smaller than one texel
    // cannot alter the rendered depth map.
    f32 m_WebShadowTexelWorld = 0.0f;

    // Spatial upscaling. The scene renders at renderScale x the swapchain and
    // the post-process pass resolves it back up, so cost falls with the square
    // of the scale while the UI and the final image stay at native resolution.
    // sharpness feeds the contrast-adaptive pass that restores the edge
    // definition the upscale softens. 1.0 = render native (no upscale).
    f32 m_WebRenderScale = 1.0f;     // 0.5 - 1.0
    f32 m_WebSharpness = 0.0f;       // 0 = off
public:
    void SetWebRenderScale(f32 scale) { m_WebRenderScale = (scale < 0.5f) ? 0.5f : (scale > 1.0f ? 1.0f : scale); }
    f32 GetWebRenderScale() const { return m_WebRenderScale; }
    void SetWebSharpness(f32 s) { m_WebSharpness = (s < 0.0f) ? 0.0f : (s > 1.0f ? 1.0f : s); }
    f32 GetWebSharpness() const { return m_WebSharpness; }
private:
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
    // Per-slot cache, same idea as m_WebShadowSignature for the directional map.
    // The light's own entity is part of the signature, so a slot that changes
    // hands redraws rather than serving the previous light's depth.
    u64 m_WebSpotShadowSig[WEB_MAX_SPOT_SHADOWS] = {};
    bool m_WebSpotShadowValid[WEB_MAX_SPOT_SHADOWS] = {};

    // Point light shadows (max 1, cubemap)
    static constexpr u32 WEB_POINT_SHADOW_SIZE = 512;
    static constexpr u32 WEB_MAX_POINT_SHADOWS = 1;
    Renderer::GPUTextureHandle m_WebPointShadowCubemap;  // managed by WebGPURenderer
    void* m_WebPointShadowFaceViews[6] = {};              // WGPUTextureView per face (cast at use)
    Renderer::GPUBufferHandle m_WebPointShadowVPBuffer;   // 6 face VPs
    u64 m_WebPointShadowSig[WEB_MAX_POINT_SHADOWS] = {};
    bool m_WebPointShadowValid[WEB_MAX_POINT_SHADOWS] = {};

    // WebGPU post-process accessibility uniform buffer (uploads from m_WebPPAccessibility)
    Renderer::GPUBufferHandle m_WebPPAccessibilityBuffer;

    // Post-processing (offscreen scene → ACES tonemap → swapchain)
    Renderer::GPUShaderHandle m_WebPostProcessShader;
    Renderer::GPUPipelineHandle m_WebPostProcessPipeline;
    Renderer::GPUBindGroupLayoutHandle m_WebPostProcessLayout;
    Renderer::GPUBindGroupHandle m_WebPostProcessBG;
    Renderer::GPUTextureHandle m_WebSceneColorTex;           // offscreen RGBA16Float (resolve target)
    void* m_WebSceneColorView = nullptr;                      // WGPUTextureView (for resolve / post-process read)
    void* m_WebSceneDepthView = nullptr;
    // A SECOND view of the same depth texture, depth aspect only, registered so
    // it can be bound as texture_depth_2d. The attachment view above cannot be
    // used for sampling: a depth-stencil format has to be viewed one aspect at
    // a time to be read in a shader.
    Renderer::GPUTextureHandle m_WebSceneDepthSampleTex;                      // WGPUTextureView (offscreen depth, 4x MSAA)
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

    // Size the offscreen chain was (re)created at. 0 = never created. When the
    // swapchain size diverges (canvas resize), the frame path calls
    // RecreateWebSizedTargets before encoding any pass.
    u32 m_WebSceneTargetW = 0;
    u32 m_WebSceneTargetH = 0;
    // (Re)create every swapchain-sized offscreen resource — scene color, MSAA
    // color, depth, bloom chain, bloom scratch — plus the bind groups that
    // reference them. Shaders/layouts/pipelines are size-independent and are
    // created once in Initialize.
    u32 WebScaledDim(u32 v) const;
    void RecreateWebSizedTargets(u32 sceneW, u32 sceneH);

    // Particle rendering (instanced billboard quads)
    Renderer::GPUShaderHandle m_WebParticleShader;
    Renderer::GPUPipelineHandle m_WebParticlePipeline;
    Renderer::GPUBufferHandle m_WebParticleQuadVB;
    Renderer::GPUBufferHandle m_WebParticleQuadIB;
    static constexpr u32 WEB_MAX_PARTICLES = 8192;

    // Sprite rendering (instanced textured billboards)
    Renderer::GPUShaderHandle m_WebSpriteShader;
    Renderer::GPUPipelineHandle m_WebSpritePipeline;
    Renderer::GPUBindGroupLayoutHandle m_WebSpriteTexLayout;

    // Persistent instance buffer for the weather draw, and the bind group for
    // the untextured white sprite it uses.
    //
    // Both used to be created and destroyed EVERY frame -- a GPU allocation and
    // a bind group, for a texture pair that never changes, at 8000 particles
    // about 450 KB of buffer churn per frame. The buffer grows when a denser
    // scene needs more and is never shrunk; the bind group is built once.
    Renderer::GPUBufferHandle m_WebWeatherInstBuf;
    usize m_WebWeatherInstCapacity = 0;

    // The whole frame's ObjectData, in draw order. One buffer, uploaded once,
    // bound once, indexed by the draw's firstInstance. This replaced a GPU
    // buffer and bind group created per BATCH and per non-batchable ENTITY,
    // every frame. The generation bumps whenever the buffer is reallocated, so
    // cached per-entity bind groups referencing it know to rebuild.
    // Per-frame instance data for the sprite, particle and elemental draws.
    // Each had its own CreateBufferWithData/DestroyBuffer pair every frame; one
    // persistent buffer per slot, grown on demand, replaces all of them. Slots
    // are separate rather than one shared buffer because all three record into
    // the same encoder before it is submitted, so a shared buffer would have
    // each draw overwrite the last.
    enum class WebInstanceSlot : u32 { Sprite = 0, Particle, Elemental, Count };
    struct WebInstanceBuffer {
        Renderer::GPUBufferHandle buffer;
        usize capacity = 0;
    };
    WebInstanceBuffer m_WebInstanceBuffers[static_cast<u32>(WebInstanceSlot::Count)];

    // Upload `bytes` of instance data into the slot's buffer, growing it if
    // needed, and return it ready to bind as a vertex buffer.
    Renderer::GPUBufferHandle UploadWebInstances(WebInstanceSlot slot,
                                                 const void* data, usize bytes);

    // Texture bind groups for multi-material sub-meshes, keyed by the textures
    // they bind. One was created and destroyed for EVERY sub-mesh of every
    // multi-material mesh, every frame, even though the textures rarely change.
    std::unordered_map<u64, Renderer::GPUBindGroupHandle> m_WebSubMeshTexCache;

    Renderer::GPUBufferHandle m_WebObjectArrayBuf;
    Renderer::GPUBindGroupHandle m_WebObjectArrayBG;
    usize m_WebObjectArrayCapacity = 0;
    u32 m_WebObjectArrayGen = 1;
    Renderer::GPUBindGroupHandle m_WebWhiteSpriteBindGroup;

    // Inverted-hull geometry outlines (the web half of RenderOutlinePass).
    // Reuses the main pass's frame + object bind group layouts: an outline draw
    // is the same ObjectData with baseColor/metallic read as outline colour and
    // width, so it needs a shader and a pipeline and nothing else.
    // Byte size of the packed shadow-caster matrix array, so the frame can
    // tell whether the existing buffer is still big enough.
    usize m_WebShadowObjectCapacity = 0;

    // One view-projection buffer and bind group per spot slot and per cube
    // face, built once. These were created and destroyed EVERY FRAME - one pair
    // per spot light, six per point light - for a uniform holding two matrices
    // that only changes value, never size. Same grow-and-keep rule the rest of
    // the frame now follows; here nothing even has to grow.
    Renderer::GPUBufferHandle m_WebSpotVPBuffer[WEB_MAX_SPOT_SHADOWS];
    Renderer::GPUBindGroupHandle m_WebSpotVPBindGroup[WEB_MAX_SPOT_SHADOWS];
    Renderer::GPUBufferHandle m_WebPointFaceVPBuffer[6];
    Renderer::GPUBindGroupHandle m_WebPointFaceVPBindGroup[6];

    // Weighted-blended OIT. Two scene-sized targets: accum sums premultiplied
    // colour times a depth weight, reveal multiplies down by (1 - alpha). The
    // composite divides one by the other back over the opaque scene, so
    // transparency never has to be sorted and therefore cannot pop.
    //
    // Allocated lazily, the first frame a scene actually has a blended entity.
    // A fully opaque scene pays nothing and renders exactly as it did before.
    Renderer::GPUTextureHandle m_WebOITAccumTex;
    void* m_WebOITAccumView = nullptr;          // WGPUTextureView
    Renderer::GPUTextureHandle m_WebOITRevealTex;
    void* m_WebOITRevealView = nullptr;         // WGPUTextureView
    Renderer::GPUPipelineHandle m_WebOITPipeline;          // accumulate (fs_oit)
    Renderer::GPUShaderHandle m_WebOITCompositeShader;
    Renderer::GPUPipelineHandle m_WebOITCompositePipeline;
    Renderer::GPUBindGroupLayoutHandle m_WebOITCompositeLayout;
    Renderer::GPUBindGroupHandle m_WebOITCompositeBG;
    u32 m_WebOITTargetW = 0;
    u32 m_WebOITTargetH = 0;

    // Build (or rebuild at a new size) the OIT targets and their bind group.
    // Returns false if anything failed, in which case the caller falls back to
    // the sorted-blend path rather than dropping transparency on the floor.
    bool EnsureWebOITTargets(u32 w, u32 h);


    Renderer::GPUShaderHandle m_WebOutlineShader;
    Renderer::GPUPipelineHandle m_WebOutlinePipeline;
    // The outline pass's own packed ObjectData, same grow-and-keep rule as the
    // main pass. It cannot share the main buffer because every record differs:
    // baseColor and metallic carry the outline colour and width instead.
    Renderer::GPUBufferHandle m_WebOutlineObjectBuf;
    Renderer::GPUBindGroupHandle m_WebOutlineObjectBG;
    usize m_WebOutlineObjectCapacity = 0;
    u32 m_WebOutlineObjectGen = 1;

    // Procedural sky
    Renderer::GPUShaderHandle m_WebSkyShader;
    Renderer::GPUPipelineHandle m_WebSkyPipeline;

    f32 m_WebTime = 0.0f;  // Accumulated time for shader animations
#else
    // Vulkan-specific rendering resources (advanced pipelines)
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;               // Vulkan main pipeline (kept for compatibility)
    std::unique_ptr<Renderer::VulkanPipeline> m_OffscreenPipeline;
    // Depth-write-OFF variants of the two geometry pipelines. The sorted draw loop
    // switches to these for AlphaMode::Blend entities (drawn after opaque) so glass
    // tests depth but doesn't WRITE it — otherwise a transparent surface culls
    // whatever is behind it. Null if creation failed (falls back to the opaque
    // pipeline = pre-transparency behavior).
    std::unique_ptr<Renderer::VulkanPipeline> m_TransparentPipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_OffscreenTransparentPipeline;
    Renderer::MaterialSpecKey m_BoundSpecKey{0xFFFFFFFF}; // Currently bound variant key (invalid = force rebind)

#if !ENJIN_RENDERER_WEBGPU
    // Custom per-material shaders (Shader Graph / hand-written GLSL). Pipelines are
    // shared by GLSL source hash so N entities with the same shader reuse ONE pipeline
    // (avoids per-entity pipeline thrash). Each shares the main geometry pipeline layout
    // so it binds as a drop-in in the sorted draw loop. See SetEntityCustomShader.
    struct CustomShaderPipeline {
        std::unique_ptr<Renderer::VulkanShader> vs;
        std::unique_ptr<Renderer::VulkanShader> fs;
        std::unique_ptr<Renderer::VulkanPipeline> pipeline;            // swapchain main pass (MRT, MSAA)
        std::unique_ptr<Renderer::VulkanPipeline> offscreenPipeline;   // editor game view (1 attachment, no MSAA), lazy
    };
    std::unordered_map<u64, CustomShaderPipeline> m_CustomShaderPipelines; // key = source hash
    std::unordered_map<u32, u64> m_EntityCustomShader;                     // EntityIndex -> source hash
    bool m_LastPipelineWasCustom = false;
    Renderer::VulkanPipeline* GetEntityCustomPipeline(Entity entity, bool offscreenPass);
#endif
    VkRenderPass m_OffscreenRenderPass = VK_NULL_HANDLE;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;
    std::unique_ptr<Renderer::VulkanShader> m_ShadowVertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_ShadowMaskVertexShader;    // UV passthrough variant
    std::unique_ptr<Renderer::VulkanShader> m_ShadowMaskFragmentShader;  // alpha-cutout discard

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
    void RenderSelectionHighlight();    // Editor: bright outline on selected entities + descendants

    // Per-entity wireframe overlay (VK_POLYGON_MODE_LINE over solid geometry)
    std::unique_ptr<Renderer::VulkanPipeline> m_WireframeOverlayPipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_OffscreenWireframeOverlayPipeline;
    std::unique_ptr<Renderer::VulkanShader> m_WireframeFragmentShader;  // flat push-constant color; no vertex inputs (interface-safe with triangle.vert)
    void CreateWireframeOverlayPipeline();
    void RenderWireframeOverlayPass();
#endif

#if !ENJIN_RENDERER_WEBGPU
    // Shadow mapping
    std::unique_ptr<Renderer::ShadowMap> m_ShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_ShadowPipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_ShadowMaskPipeline;  // alpha-cutout variant (same pass/config + frag)
    Math::Matrix4 m_CurrentCascadeVP;  // Set per-cascade in RenderShadowPass, read by RenderEntityShadow
    bool m_ShadowsEnabled = true;
    bool m_ShadowDescriptorsDirty = false;
    bool m_EditorWireframe = false;
    bool m_EditorUnlit = false;
    bool m_PlayerMode = false;
    bool m_PendingMSAAChange = false;
    bool m_PendingHDRChange = false;   // Deferred HDR toggle requested mid-frame
    bool m_PendingHDREnabled = false;  // Target HDR state for the pending change
    u32 m_PendingShadowResolution = 0; // 0 = no change pending
    bool m_CascadeProgressiveUpdate = false;
    u32 m_CascadeFarUpdateInterval = 2;   // Far cascades update every N frames (2-8)

    // Point light shadow mapping (cubemap array, up to 4 lights)
    std::unique_ptr<Renderer::PointLightShadowMap> m_PointShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_PointShadowPipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_PointShadowMaskPipeline;

    // Spot light shadow mapping (2D array, up to 4 lights)
    std::unique_ptr<Renderer::SpotLightShadowMap> m_SpotShadowMap;
    std::unique_ptr<Renderer::VulkanPipeline> m_SpotShadowPipeline;
    std::unique_ptr<Renderer::VulkanPipeline> m_SpotShadowMaskPipeline;

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

    // Shared by both backends: Vulkan CSM range + web single-cascade frustum fit
    f32 m_ShadowDistance = 100.0f;

#if !ENJIN_RENDERER_WEBGPU
    // Adaptive quality (120-FPS pillar). Default OFF; enabled by the game runtime.
    // Ticked in Update(); a level change applies frame-safe shadow levers.
    Renderer::AdaptiveQualitySystem m_AdaptiveQuality;
    bool m_AdaptiveQualityEnabled = false;
    void ApplyAdaptiveQualityLevel(Renderer::QualityLevel level);
#endif

    bool m_BackfaceCulling = false;
    bool m_WireframeMode = false;
    Effects::WindSystem* m_WindSystem = nullptr;
    Renderer::SkyboxConfig m_WebSkyConfig;   // web: scene sky (desktop uses m_Skybox)
    f32 m_WeatherSkyRain = 0.0f, m_WeatherSkySnow = 0.0f;  // live weather sky blend
    bool m_WebSkyConfigured = false;
    // Vulkan stores this on the ShadowMap object; the web path has no such
    // object, so the slider needs somewhere to live.
    f32 m_WebShadowStrength = 1.0f;
    std::vector<ParticleImpact> m_ParticleImpacts;
    std::vector<ECS::Entity> m_AnimFootsteps;
    bool m_RainActive = false;
    f32 m_AmbientIntensity = 1.0f;
    Math::Vector3 m_AmbientColor = Math::Vector3(0.1f, 0.1f, 0.15f);

    // Fog parameters
    f32 m_FogDensity = 0.0f;
    f32 m_FogStart = 20.0f;
    f32 m_FogEnd = 100.0f;
    f32 m_FogHeightFalloff = 0.1f;
    Math::Vector3 m_FogColor = Math::Vector3(0.5f, 0.5f, 0.6f);
    f32 m_SnowIntensity = 0.0f;          // live, per-frame from weather zones
    f32 m_AuthoredSnowIntensity = 0.0f;  // from the scene's render settings
    // Authored fog, so the weather updater can restore it rather than guess.
    f32 m_AuthoredFogDensity = 0.0f;
    f32 m_AuthoredFogStart = 20.0f;
    f32 m_AuthoredFogEnd = 100.0f;
    f32 m_AuthoredFogHeightFalloff = 0.1f;
    Math::Vector3 m_AuthoredFogColor = Math::Vector3(0.5f, 0.5f, 0.6f);
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
    std::unordered_set<Entity> m_HighlightEntities;
    Math::Vector3 m_HighlightColor = Math::Vector3(1.0f, 0.55f, 0.0f);  // editor selection: bright orange
    f32 m_HighlightWidth = 0.05f;   // world-units rim; visible without swamping the model
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
    // CPU-generated textures (ProceduralTextureComponent): reaction-diffusion,
    // Physarum, script-authored pixels. Same lifetime protocol as the text
    // cache above - a replacement frees the old bindless slot and parks the old
    // texture in the graveyard, never destroys it inline.
    std::unordered_map<Entity, std::shared_ptr<Renderer::Texture>> m_ProcTextureCache;

    // SDF text (unified display P1): one shared glyph atlas per font, uploaded
    // once; its bindless handle rides m_TextureBindlessHandles like any other
    // texture. A null atlas entry = the font failed to build (no per-frame retry).
    struct SDFFont {
        std::unique_ptr<Renderer::FontAtlas> atlas;
        std::shared_ptr<Renderer::Texture> texture;
    };
    std::unordered_map<std::string, SDFFont> m_SDFFonts;
    // Bare text entities whose MeshComponent the SDF path owns: rebuilt when the
    // text dirties, and skipped by EnsureTextTextures (no rasterize, no churn).
    std::unordered_set<Entity> m_SDFTextMeshes;
    Renderer::FontAtlas* GetOrBuildFontAtlas(const std::string& fontPath, Renderer::Texture** outTexture);

    // Vector graphics (unified display P2): SVG tessellations shared across
    // every DisplayGraphic entity using the same source (the UICanvas
    // VectorGraphic element keeps its own cache on the UI side). Entities in
    // the owned set get their MeshComponent rebuilt when the component dirties.
    std::unordered_map<std::string, Renderer::TessellatedGraphic> m_VectorGraphicCache;
    std::unordered_set<Entity> m_VectorGraphicMeshes;
    const Renderer::TessellatedGraphic* GetOrTessellateGraphic(const std::string& path, f32 tolerance);
#endif
    Renderer::TextRasterizer m_TextRasterizer;

#if !ENJIN_RENDERER_WEBGPU
    // Skeletal animation
    std::unique_ptr<Renderer::VulkanBuffer> m_DefaultBoneBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_DefaultMorphBuffer;
    void UpdateBoneDescriptor(Renderer::VulkanBuffer* boneBuffer);
    void UpdateMorphDescriptor(Renderer::VulkanBuffer* morphBuffer);
    void UploadMorphTargetSSBO(Entity entity, ECS::MorphTargetComponent& morph, EntityRenderData& rd);

    // GPU compute skinning (ADR-0002 Phase 1). Pipeline + a per-frame-reset descriptor pool.
    // Created lazily on first use; torn down in Shutdown.
    VkPipeline            m_SkinningPipeline       = VK_NULL_HANDLE;
    VkPipelineLayout      m_SkinningPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_SkinningDescSetLayout  = VK_NULL_HANDLE;
    // One descriptor pool per frame-in-flight; the current frame's pool is reset each frame
    // (safe after the frame fence wait) and per-dispatch sets are allocated from it.
    std::vector<VkDescriptorPool> m_SkinningDescPools;
    bool InitComputeSkinning();      // create pipeline/layout/pools from embedded SPIR-V (idempotent)
    void ShutdownComputeSkinning();
    void BeginComputeSkinningFrame();  // reset the descriptor pool for this frame's dispatches
    // Skin one mesh into renderData.skinnedVertexBuffer (allocated on first use). Records a
    // compute dispatch on cmd. The caller issues one barrier after all dispatches before raster.
    // Returns true if a dispatch was recorded (output buffer holds this frame's deformed verts).
    bool DispatchComputeSkinning(VkCommandBuffer cmd, EntityRenderData& renderData,
                                 Renderer::VulkanBuffer* boneBuffer);

    // --- #1 Shared skinning arena (step 1: foundation only) --------------------------
    // A single SSBO holding EVERY skinned entity's bone matrices this frame, entity's
    // slot i at matrix offset i*kBonesPerSlot. Replaces the 200 per-entity bone buffers
    // and is what lets steps 2-3 draw all instances of one mesh in a single instanced
    // draw (vertex-shader skinning reads its slot via gl_InstanceIndex). Populated by
    // UpdateBoneArena only when m_UseBoneArena is set — OFF by default, so this is
    // additive scaffolding with zero behavior change until the draw path consumes it.
    static constexpr u32 kBonesPerSlot = 256;   // matches the per-entity 256-bone headroom
    std::unique_ptr<Renderer::VulkanBuffer> m_BoneArena;
    std::unordered_map<Entity, u32> m_BoneArenaSlot;   // entity -> slot, rebuilt each frame
    u32 m_BoneArenaSlotCount = 0;                       // slots populated this frame
    bool m_UseBoneArena = true;   // default ON since 2026-08-18 (verified at ~1000 skinned chars)
    bool m_ArenaAccumActive = false;   // raised only around loops that flush afterwards
    void AccumulateArenaInstance(Entity entity, MeshComponent* arenaMesh, u64 arenaHash);
    void UpdateBoneArena();                             // pack all skinned bones into m_BoneArena
    u32  GetBoneArenaSlot(Entity e) const;             // slot for an entity (0 if none)
    bool HasBoneArenaSlot(Entity e) const { return m_BoneArenaSlot.find(e) != m_BoneArenaSlot.end(); }

    // --- #1 Shared skinning arena (step 2: the instanced draw path) ------------------
    // All identical skinned meshes (e.g. a grid of the same imported FBX) share ONE
    // bind-pose VB/IB keyed by mesh content hash, and are drawn with a single instanced
    // vkCmdDrawIndexed: per-instance bone offset comes from the bone arena (binding 7),
    // and per-instance model/material from an ObjectData SSBO (binding 13) exactly like
    // the static textured-indirect path. Collapses N per-entity skinned draws into 1.
    // Gated by m_UseBoneArena (default OFF); shadows stay on the per-entity path (they
    // upload their own bone matrices), so this only replaces the main color draw.
    struct ArenaSharedMesh {
        std::unique_ptr<Renderer::VulkanBuffer> vertexBuffer;   // shared bind-pose (bone-local) verts
        std::unique_ptr<Renderer::VulkanBuffer> indexBuffer;
        u32 indexCount = 0;
        u32 vertexCount = 0;   // for pose-dedup compute skinning (input vertex count)
    };
    std::unordered_map<u64, ArenaSharedMesh> m_ArenaSharedMeshes;   // contentHash -> shared buffers
    std::unique_ptr<Renderer::VulkanBuffer> m_ArenaObjectData;      // per-frame ObjectData SSBO
    u32 m_ArenaObjectDataCapacity = 0;                             // element capacity of m_ArenaObjectData
    // Per-frame batch accumulation, grouped by (meshHash, material signature). Instances
    // sharing a mesh + materials collapse together; each instance stores only its transform +
    // bone offset (material comes from the representative's sub-meshes at flush time, so
    // multi-material skinned characters are supported — one instanced draw per sub-mesh range).
    struct ArenaInstance {
        Math::Matrix4 model;
        Math::Matrix4 prevModel;
        u32 boneBase = 0;
        u32 teleported = 0;
        u64 poseKey = 0;   // pose-dedup: instances sharing (meshHash,clip,quantized phase) skin once
    };
    struct ArenaBatch {
        u64 meshHash = 0;
        u64 poseKey = 0;   // pose-dedup: this batch's shared deformed buffer (0 = VS-skinned path)
        Entity representative = INVALID_ENTITY;   // supplies mesh sub-mesh ranges + materials
        std::vector<ArenaInstance> instances;
    };
    std::vector<ArenaBatch> m_ArenaBatches;
    std::unordered_map<u64, u32> m_ArenaBatchKeyToIndex;   // (meshHash ^ materialSig) -> batch index
    bool m_ArenaEngageLogged = false;                     // one-shot per toggle-on engagement report
    void EnsureArenaSharedMeshes();                        // build shared VB/IB (FlushPendingChanges only)
    bool ArenaEligible(Entity e, MeshComponent* mesh, u64& outHash) const;
    void FlushArenaBatches(VkCommandBuffer cmd, VkPipelineLayout layout);
    void UpdateArenaObjectDataDescriptor(Renderer::VulkanBuffer* buf);   // rebind binding 13

    // --- #1 step 3: pose-dedup (skin each unique pose ONCE, reuse across instances+passes) ----
    // A pose = (meshHash, clip, quantized normalized time). All instances with the same pose key
    // share ONE compute-skinned deformed buffer, computed once per frame in the pre-pass, then
    // drawn instanced with FLAG_SKINNED cleared. Gated by m_UsePoseDedup (needs m_UseBoneArena).
    struct PoseDeformed {
        std::unique_ptr<Renderer::VulkanBuffer> buffer;   // deformed verts (VERTEX|STORAGE), one mesh's worth
        u32 vertexCount = 0;
        u64 lastFrameSkinned = 0;   // dedup within a frame: skin a given pose key only once
    };
    bool m_UsePoseDedup = true;   // default ON since 2026-08-18 (no-op when compute skinning is off)
    std::unordered_map<u64, PoseDeformed> m_PoseDeformed;      // (meshHash ^ poseKey) -> deformed buffer
    std::unordered_map<Entity, u64> m_EntityPoseKey;          // entity -> pose key this frame
    u32 m_PoseUniqueCount = 0;                                // unique poses skinned this frame (stat)
    u64 m_PoseFrameCounter = 0;                               // increments per SkinUniquePoses call
    u64 ComputePoseKey(Entity e, u64 meshHash);              // (clip, quantized phase) hash
    void SkinUniquePoses(VkCommandBuffer cmd);               // pre-pass: compute-skin each unique pose once
    // Compute-skin an explicit (in, bones@offset, out) triple. Returns true if a dispatch recorded.
    bool DispatchComputeSkinningExplicit(VkCommandBuffer cmd, Renderer::VulkanBuffer* inVerts,
                                         Renderer::VulkanBuffer* boneBuf, VkDeviceSize boneOffset,
                                         Renderer::VulkanBuffer* outVerts, u32 vertexCount);
public:
    void SetUsePoseDedup(bool b) { m_UsePoseDedup = b; }
    bool IsUsePoseDedup() const { return m_UsePoseDedup; }
    u32  GetPoseUniqueCount() const { return m_PoseUniqueCount; }
private:
public:
    void SetUseBoneArena(bool b) { m_UseBoneArena = b; m_ArenaEngageLogged = false; }
    bool IsUseBoneArena() const { return m_UseBoneArena; }
    u32  GetBoneArenaSlotCount() const { return m_BoneArenaSlotCount; }
    // Live arena draw stats (populated each frame by FlushArenaBatches) for the Debug Workstation.
    struct ArenaDebugStats {
        u32 batches = 0;
        u32 draws = 0;
        u32 instanceSubmeshes = 0;
    };
    const ArenaDebugStats& GetArenaDebugStats() const { return m_ArenaDebugStats; }
private:
    ArenaDebugStats m_ArenaDebugStats;                    // live stats for the Debug Workstation
public:
    // Run the once-per-frame compute skinning pass. MUST be called OUTSIDE any render pass
    // (compute cannot run inside one), before the passes that draw skinned meshes. No-op unless
    // compute skinning is enabled. Records dispatches + one compute->vertex barrier on cmd.
    void RunComputeSkinningPass(VkCommandBuffer cmd);
private:
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
    const Effects::ElementalSystem* m_MainPassElemental = nullptr;  // Elemental for main pass (PP-off path)
    bool m_MainPassWeatherIsRain = false;

    // Scene composition cache (auto-detected per frame, drives rendering decisions)
    SceneComposition m_SceneComposition;
    u32 m_DiagnosticFrameCounter = 0;

#if !ENJIN_RENDERER_WEBGPU
    // Progressive cascade shadow updates — far cascades update less frequently
    u32 m_ShadowFrameCounter = 0;
    Math::Vector3 m_PrevShadowCameraPos{0, 0, 0};
    Math::Vector3 m_PrevShadowCameraForward{0, 0, -1};  // for cascade full-update on rotation
    u32 m_CascadeUpdateCooldown = 0;    // Frames until next forced cascade recalc during rotation-only
    bool ShouldUpdateCascade(u32 cascade) const;

    // Shadow caster cache — rebuilt when dirty, avoids per-cascade entity iteration
    std::vector<Entity> m_ShadowCasters;
    bool m_ShadowCastersDirty = true;
    void RebuildShadowCasterCache();
    // Per-frame filtered view of m_ShadowCasters: skinned casters beyond a camera
    // distance are dropped (skinned shadow LOD). A distant animated character
    // contributes nothing visible to a cascade but costs a full per-cascade skinned
    // draw — with hundreds of characters this is a large slice of shadow GPU time.
    // Rebuilt cheaply each shadow pass; static casters always pass through.
    std::vector<Entity> m_FrameShadowCasters;
    void BuildFrameShadowCasterList();
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
    bool m_ComputePrePassDone = false;    // Per-frame guard for RecordComputePrePass (reset in FlushPendingChanges)
    bool m_FramePrepDone = false;         // Per-frame guard for BeginFrame (reset in FlushPendingChanges)
    f32 m_FrameEffectDt = 0.0f;           // dt for GPU effect sims (wall-clock fallback applied)
    std::vector<Effects::ParticleColliderShape> m_FrameParticleColliders;  // gathered in BeginFrame
    std::vector<Math::Vector3> m_RecentStains;   // dedup ring: no stains stacked on one spot
    usize m_RecentStainCursor = 0;

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
    VkPipeline m_SkyboxPipelineOffscreen = VK_NULL_HANDLE;  // offscreen UNORM 1-attachment pass variant (VUID-02684)
    VkPipelineLayout m_SkyboxPipelineLayoutHandle = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_SkyboxDescriptorSetLayoutHandle = VK_NULL_HANDLE;
    std::unique_ptr<Renderer::VulkanBuffer> m_SkyboxVertexBuffer;
    VkDescriptorPool m_SkyboxDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_SkyboxDescriptorSets;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_SkyboxUniformBuffers;
    // Last contents written to each per-frame skybox set — skip redundant
    // vkUpdateDescriptorSets (updating a set bound in the recording command buffer
    // invalidates the whole buffer: the '-recording' validation storm)
    std::vector<VkImageView> m_SkyboxSetWrittenView;
    std::vector<VkBuffer> m_SkyboxSetWrittenBuffer;
    void CreateSkyboxPipeline(VkRenderPass renderPass = VK_NULL_HANDLE);
    bool CreateSkyboxPipelineVariant(VkRenderPass renderPass, u32 colorAttachmentCount,
                                     VkSampleCountFlagBits samples, VkPipeline& outPipeline);
    void RenderSkybox(VkCommandBuffer commandBuffer,
                      const VkViewport* viewportOverride = nullptr,
                      const VkRect2D* scissorOverride = nullptr,
                      bool offscreenPass = false);
    void CreateSkyboxCubeVBO();

    // 2D scene sky: full-screen authored backdrop behind sprites in Scene2D
    // (the 3D skybox needs a perspective camera; this is screen-space). Shares
    // the scene SkyboxConfig; only draws when the scene has a non-solid sky.
    VkPipeline m_Sky2DPipeline = VK_NULL_HANDLE;
    VkPipeline m_Sky2DPipelineOffscreen = VK_NULL_HANDLE;
    VkPipelineLayout m_Sky2DPipelineLayout = VK_NULL_HANDLE;
    // Custom cloud texture cache (SkyboxConfig is const via GetConfig): re-resolve
    // the bindless index only when the authored path changes.
    std::string m_CloudTexPath;
    i32 m_CloudTexIndex = -1;
    void CreateSky2DPipeline(VkRenderPass renderPass = VK_NULL_HANDLE);
    bool CreateSky2DPipelineVariant(VkRenderPass renderPass, u32 colorAttachmentCount,
                                    VkSampleCountFlagBits samples, VkPipeline& outPipeline);
    void Render2DSky(VkCommandBuffer commandBuffer,
                     const VkViewport* viewportOverride = nullptr,
                     const VkRect2D* scissorOverride = nullptr,
                     bool offscreenPass = false);

    // 2D scene water: a full-screen translucent overlay drawn AFTER the 2D
    // sprites, submerging everything below a world-space waterline. Cousin of
    // the 2D sky; alpha-blended (the sky is opaque), so its pipeline differs
    // only in the blend state.
    Renderer::Water2DConfig m_Water2DConfig;
    bool m_PendingWater2DConfig = false;
    Renderer::Water2DConfig m_PendingWater2D;
    VkPipeline m_Water2DPipeline = VK_NULL_HANDLE;
    VkPipeline m_Water2DPipelineOffscreen = VK_NULL_HANDLE;
    VkPipelineLayout m_Water2DPipelineLayout = VK_NULL_HANDLE;
    void CreateWater2DPipeline(VkRenderPass renderPass = VK_NULL_HANDLE);
    bool CreateWater2DPipelineVariant(VkRenderPass renderPass, u32 colorAttachmentCount,
                                      VkSampleCountFlagBits samples, VkPipeline& outPipeline);
    void Render2DWater(VkCommandBuffer commandBuffer,
                       const VkViewport* viewportOverride = nullptr,
                       const VkRect2D* scissorOverride = nullptr,
                       bool offscreenPass = false);

public:
    // Water surface mesh generation. PUBLIC so the editor can build the water mesh at a
    // safe pre-render point: the editor renders the game view via RenderToTarget and
    // never calls Update() (where these run for the player), so without this 3D water
    // never gets its mesh in the editor game view (reported 2026-09-02).
private:
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
    void EnsureTextTextures();  // rasterize authored text + route its texture onto the material
    // Swap an entity's cached text texture SAFELY: frees the old one's
    // bindless slot and parks the old texture in the graveyard (in-flight
    // frames still reference it in bound descriptor sets - immediate
    // destruction is the mid-frame GPU crash class). Every Text_SetContent
    // re-rasterize (typewriter, caret) goes through here.
    void CacheTextTexture(Entity entity, std::shared_ptr<Renderer::Texture> tex);
    // Same swap for CPU-generated textures. Reaction-diffusion at 30 Hz means a
    // replacement every other frame, so going through the graveyard is not
    // optional here - it is the difference between running and a device loss.
    void CacheProcTexture(Entity entity, std::shared_ptr<Renderer::Texture> tex);
    // Upload any ProceduralTextureComponent whose pixels changed this frame.
    void EnsureProceduralTextures();
    // Grow the per-frame material SSBOs + rebind descriptor binding 2 when the entity
    // count outgrows capacity. MUST run pre-recording (FlushPendingChanges) — never from
    // BuildMaterialSSBO, which records mid-frame and would invalidate the bound command buffer.
    void EnsureMaterialSSBOCapacity();
    u32 GetMaterialIndex(Entity entity) const;

    // A multi-material mesh needs ONE SSBO ENTRY PER SLOT, not one per entity.
    // The shader reads its material through the entry selected by the draw's
    // firstInstance (adr-0003), and that entry carries the BINDLESS TEXTURE
    // INDEX. Every sub-mesh used to be drawn with the entity's own index, so
    // every sub-mesh sampled the entity's base texture: an imported tree drew
    // its leaves with its bark. Falls back to the entity's entry when the slot
    // has none.
    u32 GetSlotMaterialIndex(Entity entity, i32 slot) const;

    // Total SSBO entries: one per mesh entity, plus one per material slot.
    // Capacity and the rebuild check both need it and must agree.
    u32 CountMaterialSSBOEntries() const;

    // Uniform buffers (one per frame in flight)
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_UniformBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_LightingBuffers;
    LightingUBO m_CachedLightingData{};  // Cached copy of the latest lighting UBO for RT path tracer NEE
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_MaterialBuffers;  // Now SSBO (one per frame)
    std::vector<VkDescriptorSet> m_DescriptorSets;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Material SSBO batching state
    std::vector<u8> m_MaterialSSBOData;                 // CPU-side buffer (aligned MaterialGPU entries)
    std::unordered_map<u64, u32> m_EntityMaterialIndex;
    // entity -> index of its FIRST slot entry; slot n is base + n.
    std::unordered_map<u64, u32> m_EntitySlotMaterialBase;  // Entity -> index into SSBO
    u32 m_MaterialSSBOCount = 0;                         // Number of materials this frame
    u32 m_MaterialSSBOStride = 0;                        // Bytes per material entry (aligned to device minimum)
    u32 m_MaterialSSBOCapacity = 0;                      // Max materials the GPU buffer can hold
    bool m_MaterialSSBOBuilt = false;                    // Set after BuildMaterialSSBO(), reset at frame start

    // Offscreen (game view) uniform buffers + descriptor sets
    // Separate from main pass so CPU writes don't overwrite each other
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_OffscreenUniformBuffers;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>> m_OffscreenLightingBuffers;
    std::vector<VkDescriptorSet> m_OffscreenDescriptorSets;

    // Script render targets (FR-4). Resources are created in FlushPendingChanges
    // (pre-recording safe point) and rendered round-robin, one per frame, into
    // offscreen viewport slot 2. The external Texture aliases the target's color
    // view/sampler so the normal material/bindless machinery samples it.
    struct ScriptRenderTargetSlot {
        std::unique_ptr<Renderer::RenderTarget> target;
        std::unique_ptr<Renderer::Texture> aliasTexture;  // non-owning external wrap
        u64 cameraEntity = 0;
        u32 pendingWidth = 0, pendingHeight = 0;          // nonzero → create pending
        std::vector<Entity> pendingBinds;                 // entities awaiting material bind
    };
    std::unordered_map<u64, ScriptRenderTargetSlot> m_ScriptRenderTargets;
    u64 m_NextScriptRenderTargetHandle = 1;
    u64 m_ScriptTargetRoundRobin = 0;                     // rotates which target renders this frame
    bool m_ScriptTargetsRenderedThisFrame = false;        // reset in FlushPendingChanges
    void ProcessPendingScriptRenderTargets();             // create + bind at the safe point

    // Active rendering target pointers — swapped for offscreen passes
    std::vector<VkDescriptorSet>* m_ActiveDescriptorSets = nullptr;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>>* m_ActiveUniformBuffers = nullptr;
    std::vector<std::unique_ptr<Renderer::VulkanBuffer>>* m_ActiveLightingBuffers = nullptr;

    // Set-0 bind cache: RenderEntity skips its per-entity descriptor bind when
    // the same set is already bound on the same command buffer. The cache is
    // trusted ONLY inside a contiguous run of RenderEntity draws (the entity
    // loops bracket themselves with InvalidateBoundSet0) — pipeline switches
    // in the loop are safe because every geometry pipeline shares the main
    // layout (see BindGeometryPipelineForMaterial). A stale cache means wrong
    // descriptors; when in doubt, invalidate — a redundant rebind is free.
    VkCommandBuffer m_Set0BoundCB = VK_NULL_HANDLE;
    VkDescriptorSet m_Set0BoundSet = VK_NULL_HANDLE;
    void InvalidateBoundSet0() { m_Set0BoundCB = VK_NULL_HANDLE; m_Set0BoundSet = VK_NULL_HANDLE; }

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

    // Sort gating: the main-pass sort is skipped when the list's membership +
    // static sort-key bits (pipeline/material/texture — everything but depth)
    // are unchanged AND the camera moved less than the depth-order threshold.
    // Hash 0 = cache invalid, always sort. ANY other writer of
    // m_SortedRenderList (the splitscreen build) must zero the hash, or a
    // later frame could keep its foreign order.
    std::vector<Entity> m_RenderListScratch;
    u64 m_RenderListStaticHash = 0;
    Math::Vector3 m_LastSortCamPos{0.0f, 0.0f, 0.0f};
    bool m_LastSortHadCam = false;
    bool m_RenderListDirty = true;            // Set when entities/materials/visibility change; cleared after sort
    Math::Vector3 m_PrevCameraPos{0, 0, 0};   // Track camera movement for sort key recalculation
    u32 m_PrevEntityCount = 0;                // Detect entity count changes
    std::vector<Math::Vector3> m_IKChainCache; // Reused per frame for FABRIK IK solving

    // Draw call / triangle counters (current frame, accumulating)
    u32 m_DrawCallCount = 0;
    u32 m_TriangleCount = 0;
    u32 m_DescriptorCacheHits = 0;
    u32 m_DescriptorCacheWrites = 0;
    u32 m_SkinnedMeshCount = 0;   // compute-skinned meshes this frame
    u32 m_LastSkinnedMeshCount = 0;

    // Last completed frame's counters (snapshot taken in ResetFrameCounters before zeroing)
    u32 m_LastDrawCallCount = 0;
    u32 m_LastTriangleCount = 0;
    u32 m_LastDescriptorCacheHits = 0;
    u32 m_LastDescriptorCacheWrites = 0;

    // Cached player entity (any entity with a CharacterController) for per-frame position lookup.
    // Updated in OnEntityAdded/OnEntityRemoved to avoid linear search each frame.
    Entity m_CachedPlayerEntity = INVALID_ENTITY;

    // Asset hot-reload watcher (polls texture files for changes). Polled from
    // FlushPendingChanges, not Update - see the comment at the poll site.
    Assets::FileWatcher m_TextureWatcher;

    // Textures whose file changed on disk, by CACHE KEY (the path the material
    // stores), queued by the watcher callback. The watcher polls from Update(),
    // and swapping a live texture destroys a GPU resource that frames in flight
    // still reference, so the actual reload runs in FlushPendingChanges - the
    // only safe home for it - and the replaced texture goes to the same
    // graveyard the text-texture path uses.
    std::vector<std::string> m_PendingTextureReloads;
    void ProcessPendingTextureReloads();
    // Shared gate for the texture AND shader watchers (both polled from
    // FlushPendingChanges, which has no delta time).
    std::chrono::steady_clock::time_point m_LastAssetPoll{};

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

public:
    // Phase 2/5/6 systems (public for editor settings UI access)
    std::unique_ptr<Renderer::DDGIProbeSystem> m_DDGISystem;
    std::unique_ptr<Renderer::VolumetricFogSystem> m_VolumetricFog;
    std::unique_ptr<Effects::GPUParticleSystem> m_GPUParticleSystem;
private:
    // Gaussian splat cloud renderer (flagship #9). One cloud at a time (the
    // first GaussianSplatComponent found); loading + sorting happen in
    // FlushPendingChanges (frame safety), drawing rides the same passes as
    // particles.
    std::unique_ptr<Effects::SplatRenderer> m_SplatRenderer;
    ECS::Entity m_SplatEntity = ECS::INVALID_ENTITY;
    // DDGI geometry feed: a MeshInstance SSBO (transform + pool offsets per
    // pool-eligible static mesh) built from m_EntityRenderData, handed to the
    // DDGI voxelizer alongside the merged vertex/index buffers.
    std::unique_ptr<Renderer::VulkanBuffer> m_DDGIInstanceBuffer;
    bool m_DDGIGeometryDirty = true;   // rebuild the instance buffer + re-feed DDGI
    bool m_DDGIAtlasBound = false;      // probe atlas written to main-pass binding 22 once
    void BuildDDGIGeometry();

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

    // Shared: the web render path builds water3D surfaces too, so this cannot
    // live inside the !WEBGPU block or the web definition has no declaration.
    //
    // Public because the EDITOR calls it: the game view builds water meshes
    // from UpdateGameViewSims, since Update() is never called there and the
    // surface would otherwise be invisible in the game view (0adaa966).
public:
    void EnsureWaterMeshes();
    void EnsureWater3DMeshes();
private:

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
    bool m_RTInitialized = false;   // InitializeRayTracing completed successfully
    bool m_PendingRTInit = false;   // RT enabled post-boot; init at next flush
    // GPU compute skinning feature flag (ADR-0002 Phase 1). Default OFF. Declared outside the
    // Vulkan-only block so the inline setter/getter compile on every backend; only the Vulkan
    // path acts on it.
    // ADR-0002 compute skinning — default ON. Deforms each skinned mesh once into a
    // shared buffer that BOTH the main and shadow passes read, so their skinned
    // positions are guaranteed identical (no shadow-acne flicker from the two passes
    // skinning independently). Editor Rendering panel checkbox toggles it at runtime.
    bool m_ComputeSkinningEnabled = true;
    bool m_FreeMeshCpuData = false;   // task #3: free CPU verts after upload (opt-in)
    bool m_AnimationLODEnabled = true;   // distance-based animator update-rate LOD (see SetAnimationLODEnabled)
    VkCommandBuffer m_LastSkinningCmd = VK_NULL_HANDLE;  // once-per-frame guard for RunComputeSkinningPass
    u32 m_RTMode = 0;  // 0=Hybrid, 1=PathTrace
    u32 m_RTFrameCount = 0;
    Math::Matrix4 m_PrevViewProj;    // Previous frame's VP for velocity/motion vectors (UpdateFrameUniforms)
    Math::Matrix4 m_RTPrevViewProj;  // RT camera's previous VP for path tracer accumulation reset detection
    Renderer::Camera* m_RTCameraOverride = nullptr;  // RT traces from this camera when set (editor game view)
    VkCommandBuffer m_LastRTFrameCmd = VK_NULL_HANDLE;  // once-per-frame guard for RecordRTFrame
    // PT accumulation image layout state: RecordRTFrame leaves it SHADER_READ_ONLY
    // after a path-trace dispatch so display passes (editor game view PP, player PP)
    // can sample it, and restores GENERAL before the next dispatch.
    bool m_PTImageReadOnly = false;

    std::unique_ptr<Renderer::AccelerationStructureManager> m_ASManager;
    std::unique_ptr<Renderer::RTShadows> m_RTShadows;
    std::unique_ptr<Renderer::RTReflections> m_RTReflections;
    std::unique_ptr<Renderer::RTAmbientOcclusion> m_RTAO;
    std::unique_ptr<Renderer::RTGlobalIllumination> m_RTGI;
    std::unique_ptr<Renderer::RTTranslucency> m_RTTranslucency;
    std::unique_ptr<Renderer::RTCaustics> m_RTCaustics;
    std::unique_ptr<Renderer::PathTracer> m_PathTracer;

    // Hybrid RT G-buffer: a primary-ray pass fills depth (binding 29/2) + normal
    // (binding 30/3) from the TLAS so hybrid effects have real screen inputs in
    // the editor game view and the player (the forward pass has no normal buffer).
    std::unique_ptr<Renderer::RTPipeline> m_RTGBufferPipeline;
    VkPipelineLayout m_RTGBufferPipelineLayout = VK_NULL_HANDLE;
    VkImage m_RTGBufferDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_RTGBufferDepthMemory = VK_NULL_HANDLE;
    VkImageView m_RTGBufferDepthView = VK_NULL_HANDLE;
    VkImage m_RTGBufferNormalImage = VK_NULL_HANDLE;
    VkDeviceMemory m_RTGBufferNormalMemory = VK_NULL_HANDLE;
    VkImageView m_RTGBufferNormalView = VK_NULL_HANDLE;
    u32 m_RTGBufferWidth = 0, m_RTGBufferHeight = 0;
    bool m_RTGBufferLayoutInitialized = false;  // false until first GENERAL transition
    void DispatchRTGBuffer(VkCommandBuffer cmd);
    // Swapchain size the RT screen-space resources were last (re)sized to. All RT
    // images are created at init-time swapchain size; without tracking this they
    // never follow a window resize, and the hybrid overlay then samples stale-sized
    // textures and paints an offset ghost. ResizeRayTracing rebuilds everything when
    // this goes stale (driven lazily from RecordRTFrame).
    VkExtent2D m_RTOutputExtent{};
    void ResizeRayTracing(u32 width, u32 height);
    void RecreateRTGBufferImages(u32 width, u32 height);
    // Hybrid shadow/AO outputs get flipped GENERAL<->SHADER_READ_ONLY each frame so
    // the post-process overlay can sample them. Accessors expose them to the editor
    // and player, which bind them to PostProcessing::SetRTHybridInputs.
    bool m_RTHybridOutputsReadable = false;
public:
    VkImageView GetRTHybridShadowView() const;
    VkImageView GetRTHybridAOView() const;
    VkImageView GetRTHybridReflectView() const;
    VkImageView GetRTHybridGIView() const;
    VkSampler GetRTHybridSampler() const { return m_RTDummySampler; }
    bool IsRTHybridActive() const;  // RT on, hybrid mode, any hybrid effect enabled, TLAS valid
    // Overlay strengths from the RT compositor config, each already gated to 0 if
    // its effect is disabled. Lets callers (player) avoid the RT effect headers.
    void GetRTHybridStrengths(f32& shadow, f32& ao, f32& reflect, f32& gi) const;
private:

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
    // Per-material filter override: (Texture* ^ filterMode) -> {bindless handle, packed config
    // it was built with}. Populated frame-safely in FlushPendingChanges (EnsureOverrideTextureHandles),
    // read by BuildMaterialSSBO's lookupBindless. See MaterialComponent::textureFilterOverride.
    struct OverrideHandle { u32 handle; u32 builtConfigKey; };
    std::unordered_map<u64, OverrideHandle> m_OverrideTextureHandles;
    void EnsureOverrideTextureHandles();
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
    // 1x1 cube dummy for the probeCubemap binding (samplerCube) when no probe is baked.
    // Without this the binding falls back to a 2D view and mismatches the shader's Dim=Cube.
    VkImage m_DummyCubeImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DummyCubeImageMemory = VK_NULL_HANDLE;
    VkImageView m_DummyCubeImageView = VK_NULL_HANDLE;

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

    // RT instance geometry SSBO (binding 10) — per-entity vertex/index buffer
    // device addresses so hit shaders can read the real triangle and interpolate
    // true vertex normals (indexed by gl_InstanceCustomIndexEXT = EntityIndex,
    // same scheme as the material SSBOs; filled in RebuildTLAS alongside AddInstance)
    VkBuffer m_RTInstanceGeomBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_RTInstanceGeomMemory = VK_NULL_HANDLE;
    void* m_RTInstanceGeomMapped = nullptr;
    u32 m_RTInstanceGeomCapacity = 0;
    void EnsureRTInstanceGeomBuffer(u32 requiredCapacity);

    // RT vegetation: grass/shrub/tree are GPU-procedural instanced draws the TLAS
    // never sees. These are RT-side copies of the template meshes (device-address
    // usage) plus per-tree-volume bakes; instance transforms replicate the vertex
    // shaders' hash placement on the CPU so the traced scene matches the raster one.
    struct RTVegGeometry {
        VkBuffer vtx = VK_NULL_HANDLE; VkDeviceMemory vtxMem = VK_NULL_HANDLE;
        VkBuffer idx = VK_NULL_HANDLE; VkDeviceMemory idxMem = VK_NULL_HANDLE;
        VkDeviceAddress vtxAddr = 0; VkDeviceAddress idxAddr = 0;
        u32 vertexCount = 0; u32 indexCount = 0;
        u32 blasId = 0xFFFFFFFFu;
        f32 paramKey[4] = {0, 0, 0, 0};  // Tree bakes: trunkH/trunkW/canopyR/canopyO for staleness
    };
    RTVegGeometry m_RTGrassGeom;
    RTVegGeometry m_RTShrubGeom;
    std::unordered_map<u32, RTVegGeometry> m_RTTreeGeoms;      // key: volume EntityIndex
    std::vector<RTVegGeometry> m_RTVegRetired;                  // param-edited bakes, freed at RT shutdown
    bool m_RTVegBudgetWarned = false;
    bool CreateRTVegBuffers(RTVegGeometry& g, const void* vtxData, usize vtxBytes, u32 vertexCount,
                            const u32* idxData, u32 indexCount);
    void DestroyRTVegGeometry(RTVegGeometry& g);
    void CollectVegetationRTInstances();
    void DestroyRTVegetationResources();

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
    // Handles last written to the per-frame RT descriptor refresh (bindings 9/18/
    // 10/28). These only change on buffer realloc or scene load, so the refresh is
    // skipped when they're unchanged — rewriting an in-use descriptor set every
    // frame is VUID-03047 (the set is single, not per-frame).
    VkBuffer m_RTLastMatBuffer = VK_NULL_HANDLE;
    VkBuffer m_RTLastSimplifiedBuffer = VK_NULL_HANDLE;
    VkBuffer m_RTLastGeomBuffer = VK_NULL_HANDLE;
    VkImageView m_RTLastSkyboxView = VK_NULL_HANDLE;
    VkAccelerationStructureKHR m_RTLastTLAS = VK_NULL_HANDLE;  // binding 0; stable across in-place refits

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
    // One-shot transition of depth images UNDEFINED -> DEPTH_STENCIL_READ_ONLY_OPTIMAL
    // so shadow maps are in a sampleable layout even when their pass never runs.
    void TransitionDepthImagesToReadable(const std::vector<VkImage>& images);
    void DestroyRTDummyResources();
    void WriteRTDescriptors();
    void TransitionRTOutputImages(VkCommandBuffer cmd);
    void UploadRTMaterials();
    void EnsureRTMaterialBuffer(u32 requiredCapacity);
    void EnsureRTSimplifiedMaterialBuffer(u32 requiredCapacity);
    void UpdateRTLightUBO(VkCommandBuffer cmd, const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                          f32 lightIntensity, f32 shadowDistance, f32 shadowRadius, u32 frameCount,
                          f32 fireflyClamp, i32 enableNEE, i32 enableMIS,
                          i32 rrMinBounce, f32 rrMinProb,
                          u32 dirLightCount, u32 ptLightCount, u32 sptLightCount,
                          u32 maxBounces, u32 accumulatedSamples);
#endif // !ENJIN_RENDERER_WEBGPU (RT/OIT/SH/SDF block)
};

} // namespace ECS
} // namespace Enjin