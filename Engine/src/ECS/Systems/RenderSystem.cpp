#include "Enjin/ECS/Systems/RenderSystem.h"
// Includes moved from RenderSystem.h (forward-declared there, needed here for full definitions)
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/WeatherRenderer.h"
#include "Enjin/Effects/ParticleRenderer.h"
#include "Enjin/Effects/FluidRenderer.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/Effects/SpriteBatchRenderer.h"
#include "Enjin/Effects/SpriteTextureAtlas.h"
#include "Enjin/Effects/GrassRenderer.h"
#include "Enjin/Effects/ShrubRenderer.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/ArtStyle.h"
#include "Enjin/ECS/Components/MeshRenderer.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/BoneAttachment.h"
#include "Enjin/Animation/IKSolver.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/PointLightShadowMap.h"
#include "Enjin/Renderer/SpotLightShadowMap.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/SVGLoader.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/RTTranslucency.h"
#include "Enjin/Renderer/RayTracing/RTCaustics.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/OptiXDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/RTTemporalReuse.h"
#include "Enjin/Renderer/RayTracing/ReSTIR.h"
#include "Enjin/Renderer/RayTracing/LightBVH.h"
#include "Enjin/Renderer/RayTracing/AdaptiveRayBudget.h"
#include "Enjin/Renderer/Vulkan/BindlessResources.h"
#include "Enjin/Renderer/RayTracing/RadianceCache.h"
#include "Enjin/Renderer/RayTracing/SurfelRadianceCache.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/Renderer/ReflectionProbeSystem.h"
#include "Enjin/Renderer/SDFScene.h"
#include "Enjin/Renderer/OITManager.h"
#ifdef ENJIN_CLUSTERED_LIGHTING
#include "Enjin/Renderer/ClusteredLighting.h"
#endif
#ifdef ENJIN_VISIBILITY_BUFFER
#include "Enjin/Renderer/VisibilityBuffer/VisibilityBuffer.h"
#endif
#ifdef ENJIN_VRS
#include "Enjin/Renderer/VRS/VariableRateShading.h"
#endif
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#include "Enjin/Renderer/Upscaling/FSR2Upscaler.h"
#include "Enjin/Renderer/Upscaling/DLSSUpscaler.h"
#include "Enjin/Renderer/Upscaling/XeSSUpscaler.h"
#include "Enjin/ECS/View.h"
#include <cstring>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace ECS {

RenderSystem::RenderSystem(World* world, Renderer::VulkanRenderer* renderer)
    : m_World(world), m_Renderer(renderer) {
    m_Camera = nullptr;
}

RenderSystem::~RenderSystem() {
    Shutdown();
}

void RenderSystem::RefreshStorageCache() {
    if (!m_World) {
        m_CachedTransformStorage = nullptr;
        m_CachedMeshStorage = nullptr;
        m_CachedMaterialStorage = nullptr;
        m_CachedMaterialSlotsStorage = nullptr;
        m_CachedAnimatorStorage = nullptr;
        m_CachedTextStorage = nullptr;
        m_CachedArtStyleStorage = nullptr;
        m_CachedSpriteStorage = nullptr;
        m_CachedWaterVolumeStorage = nullptr;
        m_CachedWater3DStorage = nullptr;
        return;
    }
    m_CachedTransformStorage = m_World->GetComponentStorage<TransformComponent>();
    m_CachedMeshStorage = m_World->GetComponentStorage<MeshComponent>();
    m_CachedMaterialStorage = m_World->GetComponentStorage<MaterialComponent>();
    m_CachedMaterialSlotsStorage = m_World->GetComponentStorage<MaterialSlotsComponent>();
    m_CachedAnimatorStorage = m_World->GetComponentStorage<AnimatorComponent>();
    m_CachedTextStorage = m_World->GetComponentStorage<TextComponent>();
    m_CachedArtStyleStorage = m_World->GetComponentStorage<ArtStyleComponent>();
    m_CachedSpriteStorage = m_World->GetComponentStorage<Sprite2DComponent>();
    m_CachedWaterVolumeStorage = m_World->GetComponentStorage<WaterVolumeComponent>();
    m_CachedWater3DStorage = m_World->GetComponentStorage<Water3DComponent>();
}

void RenderSystem::Initialize() {
    if (m_Initialized) {
        return;
    }

    ENJIN_LOG_INFO(Renderer, "Initializing RenderSystem...");

    // Create default camera if none provided
    if (!m_Camera) {
        static Renderer::Camera defaultCamera;
        defaultCamera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        defaultCamera.SetLookAt(
            Math::Vector3(0.0f, 0.0f, 3.0f),  // Camera at z=3 looking at origin
            Math::Vector3(0.0f, 0.0f, 0.0f),
            Math::Vector3(0.0f, 1.0f, 0.0f)
        );
        m_Camera = &defaultCamera;
    }

    // Create shaders
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TriangleVertexShaderData),
        Renderer::ShaderData::TriangleVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TriangleFragmentShaderData),
        Renderer::ShaderData::TriangleFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load fragment shader");
        return;
    }

    // Create shadow vertex shader (push-constant-based, avoids HOST_COHERENT UBO race)
    m_ShadowVertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_ShadowVertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ShadowVertexShaderData),
        Renderer::ShaderData::ShadowVertexShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load shadow vertex shader");
    }

    // Create pipeline
    CreatePipeline();

    // Create line pipeline for editor grid rendering
    CreateLinePipeline();

    // Create outline shaders and pipeline (inverted-hull geometry outlines)
    m_OutlineVertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_OutlineVertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::OutlineVertexShaderData),
        Renderer::ShaderData::OutlineVertexShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load outline vertex shader");
        m_OutlineVertexShader.reset();
    }
    m_OutlineFragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_OutlineFragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::OutlineFragmentShaderData),
        Renderer::ShaderData::OutlineFragmentShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load outline fragment shader");
        m_OutlineFragmentShader.reset();
    }
    CreateOutlinePipeline();
    CreateWireframeOverlayPipeline();

    // Create cascaded shadow map
    m_ShadowMap = std::make_unique<Renderer::ShadowMap>(m_Renderer->GetContext());
    Renderer::ShadowMapConfig shadowConfig;
    shadowConfig.resolution = 2048;
    shadowConfig.cascadeCount = 4;
    shadowConfig.shadowDistance = m_ShadowDistance;
    if (!m_ShadowMap->Initialize(shadowConfig)) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize shadow map, shadows disabled");
        m_ShadowsEnabled = false;
        m_ShadowMap.reset();  // Clear the failed shadow map
    } else {
        CreateShadowPipeline();
        if (!m_ShadowPipeline) {
            m_ShadowsEnabled = false;
            m_ShadowMap.reset();
        }
    }

    // Create point light shadow map (cubemap array for up to 4 point lights)
    m_PointShadowMap = std::make_unique<Renderer::PointLightShadowMap>(m_Renderer->GetContext());
    if (!m_PointShadowMap->Initialize(1024)) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize point light shadow map");
        m_PointShadowMap.reset();
    } else {
        CreatePointShadowPipeline();
    }

    // Create spot light shadow map (2D array for up to 4 spot lights)
    m_SpotShadowMap = std::make_unique<Renderer::SpotLightShadowMap>(m_Renderer->GetContext());
    if (!m_SpotShadowMap->Initialize(1024)) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize spot light shadow map");
        m_SpotShadowMap.reset();
    } else {
        CreateSpotShadowPipeline();
    }

    // Create default white texture (used when no texture is bound).
    // This MUST succeed — without it, every texture fallback path leads to a null deref.
    m_DefaultWhiteTexture = std::make_unique<Renderer::Texture>(m_Renderer->GetContext());
    if (!m_DefaultWhiteTexture->CreateSolidColor(255, 255, 255, 255)) {
        ENJIN_LOG_FATAL(Renderer, "Failed to create default white texture — rendering will be broken");
    }

    // Create default bone buffer with 256 identity matrices — covers any bone index
    // a non-skinned mesh might reference without out-of-bounds SSBO reads.
    static constexpr usize DEFAULT_BONE_COUNT = 256;
    m_DefaultBoneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (m_DefaultBoneBuffer->Create(DEFAULT_BONE_COUNT * sizeof(Math::Matrix4), Renderer::BufferUsage::Storage, true)) {
        std::vector<Math::Matrix4> identities(DEFAULT_BONE_COUNT);
        for (auto& mat : identities) mat = Math::Matrix4::Identity();
        m_DefaultBoneBuffer->UploadData(identities.data(), identities.size() * sizeof(Math::Matrix4));
    } else {
        ENJIN_LOG_WARN(Renderer, "Failed to create default bone buffer");
        m_DefaultBoneBuffer.reset();
    }

    // Default morph target buffer (header says targetCount=0, so shader skips morph loop)
    m_DefaultMorphBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    {
        // Header: [vertexCount as uint bits, targetCount as uint bits] = [0, 0]
        f32 morphHeader[2] = {0.0f, 0.0f};
        if (m_DefaultMorphBuffer->Create(sizeof(morphHeader), Renderer::BufferUsage::Storage, true)) {
            m_DefaultMorphBuffer->UploadData(morphHeader, sizeof(morphHeader));
        } else {
            ENJIN_LOG_WARN(Renderer, "Failed to create default morph buffer");
            m_DefaultMorphBuffer.reset();
        }
    }

    // Create shadow data SSBO for point/spot light shadow matrices
    m_ShadowDataBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_ShadowDataBuffer->Create(sizeof(ShadowDataSSBO), Renderer::BufferUsage::Storage, true)) {
        ENJIN_LOG_WARN(Renderer, "Failed to create shadow data SSBO");
        m_ShadowDataBuffer.reset();
    }

    // Create uniform buffers and descriptor sets
    CreateUniformBuffers();
    CreateDescriptorSets();

    // Default active rendering target: main pass
    m_ActiveDescriptorSets = &m_DescriptorSets;
    m_ActiveUniformBuffers = &m_UniformBuffers;
    m_ActiveLightingBuffers = &m_LightingBuffers;

    // Create default sphere mesh
    CreateDefaultMesh();

    // Initialize weather particle renderer
    m_WeatherRenderer = std::make_unique<Effects::WeatherRenderer>();
    if (!m_WeatherRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "WeatherRenderer initialization failed, 3D particles disabled");
        m_WeatherRenderer.reset();
    }

    // Initialize particle emitter renderer
    m_ParticleRenderer = std::make_unique<Effects::ParticleRenderer>();
    if (!m_ParticleRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "ParticleRenderer initialization failed, emitter particles disabled");
        m_ParticleRenderer.reset();
    }

    // Initialize fluid renderer
    m_FluidRenderer = std::make_unique<Effects::FluidRenderer>();
    if (!m_FluidRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "FluidRenderer initialization failed, fluid rendering disabled");
        m_FluidRenderer.reset();
    }

    // Initialize grass renderer
    m_GrassRenderer = std::make_unique<Effects::GrassRenderer>();
    if (!m_GrassRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "GrassRenderer initialization failed, grass disabled");
        m_GrassRenderer.reset();
    }

    // Initialize shrub renderer
    m_ShrubRenderer = std::make_unique<Effects::ShrubRenderer>();
    if (!m_ShrubRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "ShrubRenderer initialization failed, shrubs disabled");
        m_ShrubRenderer.reset();
    }

    // Initialize tree renderer
    m_TreeRenderer = std::make_unique<Effects::TreeRenderer>();
    if (!m_TreeRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "TreeRenderer initialization failed, trees disabled");
        m_TreeRenderer.reset();
    }

    // Initialize sprite batch renderer
    m_SpriteBatchRenderer = std::make_unique<Effects::SpriteBatchRenderer>();
    if (!m_SpriteBatchRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "SpriteBatchRenderer initialization failed, sprite batching disabled");
        m_SpriteBatchRenderer.reset();
    }

    // Initialize sprite texture atlas (auto-packs small sprites into one GPU texture)
    m_SpriteAtlas = std::make_unique<Effects::SpriteTextureAtlas>();
    if (m_SpriteAtlas->Initialize(m_Renderer->GetContext())) {
        if (m_SpriteBatchRenderer) m_SpriteBatchRenderer->SetAtlas(m_SpriteAtlas.get());
    } else {
        ENJIN_LOG_WARN(Renderer, "SpriteTextureAtlas initialization failed, atlas packing disabled");
        m_SpriteAtlas.reset();
    }

    // Initialize skybox
    m_Skybox.Initialize(m_Renderer->GetContext());
    CreateSkyboxCubeVBO();
    CreateSkyboxPipeline();

    // Set up shader hot-reload (editor-only)
    FindShaderDirectory();
    if (!m_ShaderDir.empty() && m_ShaderHotReloadEnabled) {
        SetupShaderWatchers();
    }

    // Initialize merged geometry buffer (single VB+IB for all static 3D meshes)
    m_GeometryPool = std::make_unique<Renderer::MergedGeometryBuffer>(m_Renderer->GetContext());
    if (!m_GeometryPool->Initialize()) {
        ENJIN_LOG_WARN(Renderer, "MergedGeometryBuffer initialization failed, using per-entity buffers");
        m_GeometryPool.reset();
    }

    // Initialize GPU frustum culling system (no readback stall — visibility read from previous frame)
    if (m_GPUCullingEnabled) {
        m_GPUCulling = std::make_unique<Renderer::GPUCullingSystem>(m_Renderer->GetContext());
        if (!m_GPUCulling->Initialize()) {
            ENJIN_LOG_WARN(Renderer, "GPUCullingSystem initialization failed, using CPU culling");
            m_GPUCulling.reset();
            m_GPUCullingEnabled = false;
        } else {
            ENJIN_LOG_INFO(Renderer, "GPU frustum culling enabled");
        }
    }

    // Initialize texture-grouped indirect draw batcher
    if (m_GPUCullingEnabled && m_GPUCulling) {
        m_IndirectDrawBatcher = std::make_unique<Renderer::IndirectDrawBatcher>();
        if (!m_IndirectDrawBatcher->Initialize(m_Renderer->GetContext(), m_GPUCulling->GetMaxObjects())) {
            ENJIN_LOG_WARN(Renderer, "IndirectDrawBatcher init failed, textured entities use per-entity draws");
            m_IndirectDrawBatcher.reset();
        } else {
            ENJIN_LOG_INFO(Renderer, "Texture-grouped indirect draw batching enabled");
        }
    }

    // Initialize Device Generated Commands (DGC) — GPU generates entire command stream
    if (m_GPUCullingEnabled && m_GPUCulling && m_Renderer->GetContext()->IsDGCSupported()) {
        m_DGC = std::make_unique<Renderer::DeviceGeneratedCommands>();
        if (!m_DGC->Initialize(m_Renderer->GetContext(), m_GPUCulling->GetMaxObjects())) {
            ENJIN_LOG_INFO(Renderer, "DGC not available on this device, using multi-draw indirect");
            m_DGC.reset();
        } else {
            // Create commands layout now that the pipeline exists
            if (m_Pipeline && m_DGC->CreateCommandsLayout(m_Pipeline->GetLayout(), m_Pipeline->GetPipeline())) {
                ENJIN_LOG_INFO(Renderer, "Device Generated Commands initialized (disabled by default, toggle in editor)");
            } else {
                ENJIN_LOG_INFO(Renderer, "DGC commands layout creation deferred (pipeline not ready)");
            }
        }
    }

    // Initialize async compute scheduler for RT/denoise overlap
    {
        m_AsyncComputeScheduler = std::make_unique<Renderer::AsyncComputeScheduler>();
        if (m_AsyncComputeScheduler->Initialize(m_Renderer->GetContext(), 2)) {
            ENJIN_LOG_INFO(Renderer, "Async compute scheduler enabled (RT/denoise overlap)");
        } else {
            // Not an error — many GPUs share graphics/compute queue
            m_AsyncComputeScheduler.reset();
        }
    }

    // Initialize thread pool and per-thread command buffer pools
    m_ThreadPool.Initialize();
    u32 framesInFlight = 2; // Matches VulkanRenderer::MAX_FRAMES_IN_FLIGHT
    m_CmdBufferPool = std::make_unique<Renderer::CommandBufferPool>();
    if (!m_CmdBufferPool->Initialize(m_Renderer->GetContext(),
                                     m_ThreadPool.GetThreadCount(), framesInFlight)) {
        ENJIN_LOG_WARN(Renderer, "CommandBufferPool init failed, shadow passes will be single-threaded");
        m_CmdBufferPool.reset();
    }

    // Initialize ray tracing subsystems (if hardware supports it)
    InitializeRayTracing();

    // Initialize OIT, SH light probes, and SDF scene
    m_OITManager = std::make_unique<Renderer::OITManager>();
    auto extent = m_Renderer->GetSwapchainExtent();
    if (!m_OITManager->Initialize(m_Renderer->GetContext(), extent.width, extent.height, m_Renderer->GetRenderPass())) {
        ENJIN_LOG_WARN(Renderer, "OITManager init failed, OIT disabled");
        m_OITManager.reset();
    }

    m_SHLighting = std::make_unique<Renderer::SHLightingSystem>();
    m_ReflectionProbes = std::make_unique<Renderer::ReflectionProbeSystem>();
    m_ReflectionProbes->Initialize(m_Renderer->GetContext());
    m_SDFScene = std::make_unique<Renderer::SDFScene>();

    // Per-frame linear allocator: 8 MB supports ~100K entities x 128B each
    m_FrameAllocator = std::make_unique<FrameAllocator>(8 * 1024 * 1024);

    // Initialize clustered forward lighting system
#ifdef ENJIN_CLUSTERED_LIGHTING
    {
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        m_ClusteredLighting = std::make_unique<Renderer::ClusteredLightingSystem>(m_Renderer->GetContext());
        if (!m_ClusteredLighting->Initialize(extent.width, extent.height)) {
            ENJIN_LOG_WARN(Renderer, "Clustered lighting init failed — falling back to brute-force");
            m_ClusteredLighting.reset();
        }
    }
#endif

    // Initialize visibility buffer renderer
#ifdef ENJIN_VISIBILITY_BUFFER
    {
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        m_VisibilityBuffer = std::make_unique<Renderer::VisibilityBufferRenderer>(m_Renderer->GetContext());
        if (!m_VisibilityBuffer->Initialize(extent.width, extent.height, m_Renderer->GetRenderPass())) {
            ENJIN_LOG_WARN(Renderer, "Visibility buffer init failed — using standard forward path");
            m_VisibilityBuffer.reset();
        }
    }
#endif

    // Initialize variable rate shading
#ifdef ENJIN_VRS
    if (m_Renderer->GetContext()->IsVRSSupported()) {
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        m_VRS = std::make_unique<Renderer::VariableRateShading>(m_Renderer->GetContext());
        if (!m_VRS->Initialize(extent.width, extent.height)) {
            ENJIN_LOG_WARN(Renderer, "VRS init failed — shading rate control disabled");
            m_VRS.reset();
        }
    }
#endif

    // Initialize bindless resource manager for texture indexing
    m_BindlessManager = std::make_unique<Renderer::BindlessResourceManager>(m_Renderer->GetContext());
    if (!m_BindlessManager->Initialize()) {
        ENJIN_LOG_WARN(Renderer, "Bindless resource manager initialization failed — per-entity descriptors will be used");
        m_BindlessManager.reset();
    } else {
        // Register default white texture so empty material slots have a valid fallback
        if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
            m_DefaultBindlessHandle = m_BindlessManager->RegisterTexture(
                m_DefaultWhiteTexture->GetImageView(), m_DefaultWhiteTexture->GetSampler());
        }
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RenderSystem initialized (bindless: %s)",
                   m_BindlessManager ? "enabled" : "disabled");
}

void RenderSystem::OnSceneClear() {
    // Defer the actual clear to the next FlushPendingChanges() call at the
    // frame boundary so we never tear down GPU resources mid-frame.
    m_SceneClearPending = true;
}

void RenderSystem::FlushSceneClear() {
    // Wait for in-flight GPU work before invalidating any resources
    if (m_Renderer && m_Renderer->GetContext()) {
        m_Renderer->GetContext()->WaitForGPU();
    }

    m_EntityRenderData.clear();
    m_SortedRenderList.clear();
    m_EntityMaterialIndex.clear();
    m_EntityToCullIndex.clear();
    m_CullableObjects.clear();
    m_CachedLightEntities.clear();
    m_LastBound.Reset();
    // Null ALL cached storage pointers — World::Clear() destroyed the storages
    // they pointed to, so these are dangling. They'll be refreshed in Update()
    // via RefreshStorageCache(). Until then, render code null-checks these.
    m_CachedTransformStorage = nullptr;
    m_CachedMeshStorage = nullptr;
    m_CachedMaterialStorage = nullptr;
    m_CachedAnimatorStorage = nullptr;
    m_CachedFallbackAnimator = nullptr;
    m_CachedTextStorage = nullptr;
    m_MaterialSSBOBuilt = false;
    m_MaterialSSBODirty = true;
    m_LightListDirty = true;
    m_SceneComposition.dirty = true;
    m_SceneClearCooldown = 2;  // Skip game view for 2 frames (double-buffered)

    // Invalidate RT acceleration structures so the driver doesn't access freed geometry
    if (m_ASManager) {
        m_ASManager->InvalidateAll();
        m_ASManager->ResetInstances();
    }
}

void RenderSystem::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    // Wait for GPU to finish (fence-based when renderer is active)
    if (m_Renderer && m_Renderer->GetContext()) {
        m_Renderer->GetContext()->WaitForGPU();
    }

    // Clean up descriptor pool
    if (m_DescriptorPool != VK_NULL_HANDLE && m_Renderer->GetContext()) {
        vkDestroyDescriptorPool(m_Renderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    // Clean up entity render data
    m_EntityRenderData.clear();

    // Clean up uniform buffers
    m_UniformBuffers.clear();
    m_LightingBuffers.clear();
    m_MaterialBuffers.clear();
    m_DescriptorSets.clear();
    m_OffscreenUniformBuffers.clear();
    m_OffscreenLightingBuffers.clear();
    m_OffscreenDescriptorSets.clear();
    m_ActiveDescriptorSets = nullptr;
    m_ActiveUniformBuffers = nullptr;
    m_ActiveLightingBuffers = nullptr;

    // Clean up performance optimization subsystems
#ifdef ENJIN_CLUSTERED_LIGHTING
    if (m_ClusteredLighting) { m_ClusteredLighting->Shutdown(); m_ClusteredLighting.reset(); }
#endif
#ifdef ENJIN_VISIBILITY_BUFFER
    if (m_VisibilityBuffer) { m_VisibilityBuffer->Shutdown(); m_VisibilityBuffer.reset(); }
#endif
#ifdef ENJIN_VRS
    if (m_VRS) { m_VRS->Shutdown(); m_VRS.reset(); }
#endif

    // Clean up weather, particle, grass, shrub, tree, and sprite batch renderers
    m_WeatherRenderer.reset();
    m_ParticleRenderer.reset();
    m_GrassRenderer.reset();
    m_ShrubRenderer.reset();
    m_TreeRenderer.reset();
    m_SpriteAtlas.reset();
    m_SpriteBatchRenderer.reset();

    // Clean up merged geometry buffer (before entity render data so pool frees are valid)
    m_GeometryPool.reset();

    // Clean up thread pool and command buffer pools (before GPU culling)
    m_ThreadPool.Shutdown();
    m_CmdBufferPool.reset();

    // Clean up async compute scheduler
    if (m_AsyncComputeScheduler) {
        m_AsyncComputeScheduler->Shutdown();
        m_AsyncComputeScheduler.reset();
    }

    // Clean up Device Generated Commands
    if (m_DGC) {
        m_DGC->Shutdown();
        m_DGC.reset();
    }

    // Clean up texture-grouped indirect draw batcher
    if (m_IndirectDrawBatcher) {
        m_IndirectDrawBatcher->Shutdown();
        m_IndirectDrawBatcher.reset();
    }

    // Clean up GPU culling system
    if (m_GPUCulling) {
        m_GPUCulling->Shutdown();
        m_GPUCulling.reset();
    }
    m_CullableObjects.clear();
    m_EntityToCullIndex.clear();

    // Clean up skybox resources
    m_SkyboxVertexBuffer.reset();
    m_SkyboxUniformBuffers.clear();
    if (m_Renderer->GetContext()) {
        VkDevice device = m_Renderer->GetContext()->GetDevice();
        if (m_SkyboxPipelineHandle != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, m_SkyboxPipelineHandle, nullptr);
            m_SkyboxPipelineHandle = VK_NULL_HANDLE;
        }
        if (m_SkyboxPipelineLayoutHandle != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_SkyboxPipelineLayoutHandle, nullptr);
            m_SkyboxPipelineLayoutHandle = VK_NULL_HANDLE;
        }
        if (m_SkyboxDescriptorSetLayoutHandle != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_SkyboxDescriptorSetLayoutHandle, nullptr);
            m_SkyboxDescriptorSetLayoutHandle = VK_NULL_HANDLE;
        }
        if (m_SkyboxDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, m_SkyboxDescriptorPool, nullptr);
            m_SkyboxDescriptorPool = VK_NULL_HANDLE;
        }
    }
    m_Skybox.Shutdown();

    // Clean up OIT, SH light probes, reflection probes, and SDF scene
    if (m_OITManager) { m_OITManager->Shutdown(); m_OITManager.reset(); }
    m_SHLighting.reset();
    if (m_ReflectionProbes) { m_ReflectionProbes->Shutdown(); m_ReflectionProbes.reset(); }
    m_SDFScene.reset();

    // Clean up ray tracing subsystems
    ShutdownRayTracing();

    // Clean up line pipeline
    m_OffscreenLinePipeline.reset();
    m_LinePipeline.reset();

    // Clean up shadow resources
    m_ShadowPipeline.reset();
    m_ShadowMap.reset();
    m_PointShadowPipeline.reset();
    m_PointShadowMap.reset();
    m_SpotShadowPipeline.reset();
    m_SpotShadowMap.reset();
    m_ShadowDataBuffer.reset();

    // Clean up text texture cache and rasterizer
    m_TextTextureCache.clear();
    m_TextRasterizer.ClearFontCache();

    // Clean up textures and bone buffers
    m_DefaultWhiteTexture.reset();
    m_DefaultBoneBuffer.reset();
    m_GhostBoneBuffer.reset();
    m_GhostBoneBufferCapacity = 0;

    // Clean up pipeline
    m_OffscreenPipeline.reset();
    m_Pipeline.reset();
    m_FragmentShader.reset();
    m_VertexShader.reset();

    m_Initialized = false;
}

void RenderSystem::ProcessPendingRecreation() {
    if (m_PendingRecreation == PendingRecreationType::None) return;

    // Wait for all in-flight frames to finish (2 fences, fast)
    m_Renderer->WaitForAllFrames();

    switch (m_PendingRecreation) {
        case PendingRecreationType::PipelineOnly:
            RecreatePipelines(true);
            break;
        case PendingRecreationType::MainShader:
            m_VertexShader = std::move(m_PendingVertexShader);
            m_FragmentShader = std::move(m_PendingFragmentShader);
            RecreatePipelines(true);
            ENJIN_LOG_INFO(Renderer, "Shader hot-reload: main shaders reloaded successfully");
            break;
        case PendingRecreationType::SkyboxShader: {
            VkDevice device = m_Renderer->GetContext()->GetDevice();
            if (m_SkyboxPipelineHandle != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, m_SkyboxPipelineHandle, nullptr);
                m_SkyboxPipelineHandle = VK_NULL_HANDLE;
            }
            if (m_SkyboxPipelineLayoutHandle != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, m_SkyboxPipelineLayoutHandle, nullptr);
                m_SkyboxPipelineLayoutHandle = VK_NULL_HANDLE;
            }
            if (m_SkyboxDescriptorSetLayoutHandle != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, m_SkyboxDescriptorSetLayoutHandle, nullptr);
                m_SkyboxDescriptorSetLayoutHandle = VK_NULL_HANDLE;
            }
            if (m_SkyboxDescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, m_SkyboxDescriptorPool, nullptr);
                m_SkyboxDescriptorPool = VK_NULL_HANDLE;
            }
            m_SkyboxDescriptorSets.clear();
            m_SkyboxUniformBuffers.clear();
            CreateSkyboxPipeline();
            ENJIN_LOG_INFO(Renderer, "Shader hot-reload: skybox shaders reloaded successfully");
            break;
        }
        case PendingRecreationType::ShadowShader:
            m_ShadowVertexShader = std::move(m_PendingVertexShader);
            RecreatePipelines(true);
            ENJIN_LOG_INFO(Renderer, "Shader hot-reload: shadow vertex shader reloaded successfully");
            break;
        default: break;
    }
    m_PendingRecreation = PendingRecreationType::None;
    m_PendingVertexShader.reset();
    m_PendingFragmentShader.reset();
}

void RenderSystem::FlushPendingChanges() {
    if (!m_Renderer || !m_Initialized) return;

    // Flush deferred scene clear (set by OnSceneClear mid-frame)
    if (m_SceneClearPending) {
        m_SceneClearPending = false;
        FlushSceneClear();
    }

    // Apply deferred shadow resolution change before pipeline recreation
    // (ProcessPendingRecreation will wait for the GPU via WaitForAllFrames)
    if (m_PendingShadowResolution != 0 && m_ShadowMap) {
        m_ShadowMap->SetResolution(m_PendingShadowResolution);
        m_PendingShadowResolution = 0;
    }

    ProcessPendingRecreation();

    // Apply deferred skybox config — must happen before any rendering commands
    // reference the old cubemap (including RenderOffscreen for the Game View)
    if (m_PendingSkyboxConfig) {
        m_PendingSkyboxConfig = false;
        if (m_Skybox.IsValid()) {
            m_Renderer->WaitForAllFrames();
        }
        m_Skybox.SetConfig(m_PendingSkybox);
    }

    // Process pending reflection probe bakes — renders 6 faces per probe.
    // Safe to do here because no frame is in progress yet.
    if (m_ReflectionProbes && m_ReflectionProbes->HasPendingBake()) {
        m_Renderer->WaitForAllFrames();
        m_ReflectionProbes->ProcessPendingBakes(m_World, this);
        // Update descriptor binding 19 with the newly baked cubemap
        UpdateProbeCubemapDescriptor();
    }
}

void RenderSystem::Update(f32 deltaTime) {
    if (!m_Renderer || !m_Initialized) {
        return;
    }

    // Apply deferred MSAA change (requested mid-frame by editor settings UI).
    // Must happen before any rendering — it recreates swapchain, render pass, pipelines.
    if (m_PendingMSAAChange) {
        ApplyPendingMSAAChange();
    }

    // Process any pending changes not yet flushed (fallback if FlushPendingChanges
    // wasn't called earlier this frame, e.g. in standalone Player without editor)
    FlushPendingChanges();

    // Cache component storage pointers for this frame. Each storage pointer is looked
    // up once here (via type-ID hash) instead of once per entity in the hot loops.
    RefreshStorageCache();

    // Mark all transform world-matrix caches dirty so each entity recomputes at most
    // once this frame (across main pass, shadow pass, outline pass, etc.).
    if (m_CachedTransformStorage) {
        auto& transforms = m_CachedTransformStorage->GetComponents();
        for (auto& t : transforms) {
            t.worldMatrixDirty = true;
        }
    }

    // Reset per-frame stats
    ResetFrameCounters();

    // Begin async compute scheduler frame (reset per-frame state)
    if (m_AsyncComputeScheduler) {
        m_AsyncComputeScheduler->BeginFrame(m_Renderer->GetCurrentFrameIndex());
    }

    // Reset material SSBO flag — will be rebuilt on first use this frame
    m_MaterialSSBOBuilt = false;

    // Reset per-frame linear allocator (all FrameArray allocations from previous frame are freed)
    if (m_FrameAllocator) m_FrameAllocator->Reset();

    // Rebuild cached light entity list only when dirty (entity add/remove with LightComponent).
    // Also detect dynamic LightComponent add/remove on existing entities via count mismatch.
    if (m_World) {
        const auto& liveLights = m_World->GetEntitiesWithComponent<LightComponent>();
        if (!m_LightListDirty && liveLights.size() != m_CachedLightEntities.size()) {
            m_LightListDirty = true;
        }
        if (m_LightListDirty) {
            m_CachedLightEntities.assign(liveLights.begin(), liveLights.end());
            m_LightListDirty = false;
        }
    } else if (m_LightListDirty) {
        m_CachedLightEntities.clear();
        m_LightListDirty = false;
    }

    // Reset per-thread command buffer pools for this frame
    if (m_CmdBufferPool) {
        m_CmdBufferPool->ResetFrame(m_Renderer->GetCurrentFrameIndex());
    }

    // Poll texture and shader file watchers every 5 seconds (time-based, not frame-count).
    m_WatcherPollTimer += deltaTime;
    if (m_WatcherPollTimer >= 5.0f) {
        m_WatcherPollTimer = 0.0f;
        m_TextureWatcher.Poll();
        if (m_ShaderHotReloadEnabled && !m_ShaderDir.empty()) {
            m_ShaderWatcher.Poll();
        }
    }

    // Auto-create meshes for water volume entities that don't have one yet
    EnsureWaterMeshes();
    EnsureWater3DMeshes();

    // Regenerate terrain meshes when dirty (only iterate entities that have the component)
    {
        for (Entity entity : m_World->GetEntitiesWithComponent<TerrainComponent>()) {
            auto* terrain = m_World->GetComponent<TerrainComponent>(entity);
            if (terrain && terrain->meshDirty) {
                auto mesh = Renderer::MeshFactory::CreateTerrain(*terrain);
                if (m_World->HasComponent<MeshComponent>(entity)) {
                    *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
                } else {
                    m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
                }
                // Force re-upload of GPU buffers
                if (static_cast<usize>(entity) < m_EntityRenderData.size())
                    m_EntityRenderData[static_cast<usize>(entity)].Invalidate();
                terrain->meshDirty = false;
            }
        }
        for (Entity entity : m_World->GetEntitiesWithComponent<Terrain2DComponent>()) {
            auto* terrain2d = m_World->GetComponent<Terrain2DComponent>(entity);
            if (terrain2d && terrain2d->meshDirty) {
                auto mesh = Renderer::MeshFactory::CreateTerrain2D(*terrain2d);
                if (m_World->HasComponent<MeshComponent>(entity)) {
                    *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
                } else {
                    m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
                }
                if (static_cast<usize>(entity) < m_EntityRenderData.size())
                    m_EntityRenderData[static_cast<usize>(entity)].Invalidate();
                terrain2d->meshDirty = false;
            }
        }
        // JellyMesh dirty check (flower system vertex deformation)
        // Re-upload vertex data to existing buffer instead of erase/recreate,
        // because destroying buffers while the GPU is still reading them crashes the driver.
        for (Entity entity : m_World->GetEntitiesWithComponent<JellyMeshComponent>()) {
            auto* jelly = m_World->GetComponent<JellyMeshComponent>(entity);
            if (jelly && jelly->meshDirty) {
                EntityRenderData* rd = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
                    ? &m_EntityRenderData[static_cast<usize>(entity)] : nullptr;
                if (rd && rd->vertexBuffer) {
                    auto* mesh = m_World->GetComponent<MeshComponent>(entity);
                    if (mesh && !mesh->vertices.empty()) {
                        usize dataSize = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
                        rd->vertexBuffer->UploadData(mesh->vertices.data(), dataSize);
                    }
                } else {
                    // No existing buffer — will be created on next render via SetupEntityBuffers
                }
                jelly->meshDirty = false;
            }
        }

        // Advance animated sprite timers and mark dirty on frame change
        for (Entity entity : m_World->GetEntitiesWithComponent<AnimatedSprite2DComponent>()) {
            auto* anim = m_World->GetComponent<AnimatedSprite2DComponent>(entity);
            auto* sprite = m_World->GetComponent<Sprite2DComponent>(entity);
            if (!anim || !sprite || !anim->playing || anim->frames.empty()) continue;

            anim->frameTimer += deltaTime * anim->playbackSpeed;
            const auto& frame = anim->frames[anim->currentFrame];
            if (anim->frameTimer >= frame.duration) {
                anim->frameTimer -= frame.duration;
                anim->frameChanged = true;
                u32 nextFrame = anim->currentFrame + 1;
                if (nextFrame >= static_cast<u32>(anim->frames.size())) {
                    if (anim->loop) { nextFrame = 0; }
                    else { nextFrame = anim->currentFrame; anim->playing = false; anim->animationComplete = true; }
                }
                anim->currentFrame = nextFrame;
                const auto& newFrame = anim->frames[anim->currentFrame];
                sprite->srcX = newFrame.srcX;
                sprite->srcY = newFrame.srcY;
                sprite->spriteDirty = true;
            } else {
                anim->frameChanged = false;
            }

            // Apply per-frame collider on frame change
            if (anim->frameChanged) {
                auto* pfc = m_World->GetComponent<PerFrameColliderComponent>(entity);
                if (pfc && pfc->autoApply && anim->currentFrame < static_cast<u32>(pfc->frameColliders.size())) {
                    auto* box = m_World->GetComponent<BoxColliderComponent>(entity);
                    if (box) {
                        const auto& fc = pfc->frameColliders[anim->currentFrame];
                        if (fc.enabled) {
                            box->center = Math::Vector3(fc.offset.x, fc.offset.y, 0.0f);
                            box->size = Math::Vector3(fc.size.x, fc.size.y, 0.1f);
                        }
                    }
                }
            }
        }

        // Auto-generate sprite quad meshes when dirty
        for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
            auto* sprite = m_World->GetComponent<Sprite2DComponent>(entity);
            if (!sprite || !sprite->spriteDirty) continue;

            // Resolve texture pixel dimensions for UV normalization
            if (sprite->texPixelWidth == 0 && !sprite->texturePath.empty()) {
                auto tex = GetOrLoadTexture(sprite->texturePath);
                if (tex && tex->IsValid()) {
                    sprite->texPixelWidth = static_cast<f32>(tex->GetWidth());
                    sprite->texPixelHeight = static_cast<f32>(tex->GetHeight());
                }
            }

            // Calculate normalized UVs (srcWidth 0 = full texture)
            f32 uvL = 0.0f, uvT = 0.0f, uvR = 1.0f, uvB = 1.0f;
            if (sprite->srcWidth > 0 && sprite->srcHeight > 0 && sprite->texPixelWidth > 0) {
                uvL = sprite->srcX / sprite->texPixelWidth;
                uvT = sprite->srcY / sprite->texPixelHeight;
                uvR = (sprite->srcX + sprite->srcWidth) / sprite->texPixelWidth;
                uvB = (sprite->srcY + sprite->srcHeight) / sprite->texPixelHeight;
            }

            auto mesh = Renderer::MeshFactory::CreateSpriteQuad(
                sprite->size.x, sprite->size.y,
                sprite->pivot.x, sprite->pivot.y,
                uvL, uvT, uvR, uvB,
                sprite->flipX, sprite->flipY);

            if (m_World->HasComponent<MeshComponent>(entity)) {
                *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
            } else {
                m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
            }
            if (static_cast<usize>(entity) < m_EntityRenderData.size())
                m_EntityRenderData[static_cast<usize>(entity)].Invalidate();
            sprite->spriteDirty = false;
        }

        // Auto-generate tilemap meshes when dirty
        for (Entity entity : m_World->GetEntitiesWithComponent<TilemapComponent>()) {
            auto* tilemap = m_World->GetComponent<TilemapComponent>(entity);
            if (!tilemap || !tilemap->meshDirty) continue;

            auto mesh = Renderer::MeshFactory::CreateTilemapMesh(*tilemap);
            if (m_World->HasComponent<MeshComponent>(entity)) {
                *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
            } else {
                m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
            }
            if (static_cast<usize>(entity) < m_EntityRenderData.size())
                m_EntityRenderData[static_cast<usize>(entity)].Invalidate();
            tilemap->meshDirty = false;
        }
    }

    // Update skeletal animators and apply IK constraints (single pass over AnimatorComponent entities)
    m_CachedFallbackAnimator = nullptr;
    for (Entity entity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
        AnimatorComponent* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
        if (!animComp) continue;

        // Cache first animator with a skeleton for orphan skinned meshes (Mixamo FBX split imports)
        if (!m_CachedFallbackAnimator && animComp->animator.GetSkeleton()) {
            m_CachedFallbackAnimator = animComp;
        }

        animComp->Update(deltaTime);

        if (!animComp->animator.IsPlaying()) continue;

        auto* lookAtIK = m_World->GetComponent<LookAtIKComponent>(entity);
        if (lookAtIK && lookAtIK->lookWeight > 0.0f) {
            // Resolve target position
            Math::Vector3 targetPos = lookAtIK->targetWorldPos;
            if (lookAtIK->useEntityTarget && lookAtIK->targetEntity != INVALID_ENTITY) {
                auto* targetTransform = m_World->GetComponent<TransformComponent>(lookAtIK->targetEntity);
                if (targetTransform) {
                    targetPos = targetTransform->position;
                }
            }

            // Get head bone world position from entity transform
            auto* entityTransform = m_World->GetComponent<TransformComponent>(entity);
            if (entityTransform) {
                Math::Vector3 headWorldPos = entityTransform->position + Math::Vector3(0, 1.6f, 0);
                Math::Quaternion solved = Animation::LookAtIK::Solve(
                    headWorldPos, targetPos, lookAtIK->currentHeadRotation,
                    lookAtIK->maxRotation, lookAtIK->smoothSpeed, deltaTime);
                lookAtIK->currentHeadRotation = solved;
            }
        }

        auto* interactionIK = m_World->GetComponent<InteractionIKComponent>(entity);
        if (interactionIK && interactionIK->ikWeight > 0.0f) {
            auto* entityTransform = m_World->GetComponent<TransformComponent>(entity);
            if (entityTransform) {
                // Find nearest interactable within radius (only scan InteractableComponent entities)
                Math::Vector3 handPos = entityTransform->position + Math::Vector3(0.3f, 1.0f, 0.5f);
                Math::Vector3 nearestTarget = handPos;
                f32 nearestDist = interactionIK->interactionRadius + 1.0f;

                for (Entity other : m_World->GetEntitiesWithComponent<InteractableComponent>()) {
                    if (other == entity) continue;
                    if (!interactionIK->interactionTag.empty()) {
                        auto* otherTag = m_World->GetComponent<TagComponent>(other);
                        if (!otherTag || !otherTag->HasTag(interactionIK->interactionTag))
                            continue;
                    }
                    auto* otherTransform = m_World->GetComponent<TransformComponent>(other);
                    if (!otherTransform) continue;
                    f32 dist = (otherTransform->position - handPos).Length();
                    if (dist < nearestDist && dist <= interactionIK->interactionRadius) {
                        nearestDist = dist;
                        nearestTarget = otherTransform->position;
                    }
                }

                if (nearestDist <= interactionIK->interactionRadius) {
                    // Simple 3-bone FABRIK solve for hand chain
                    // Reuse member vector to avoid per-frame heap allocation
                    m_IKChainCache.resize(3);
                    m_IKChainCache[0] = entityTransform->position + Math::Vector3(0.2f, 1.3f, 0.0f); // shoulder
                    m_IKChainCache[1] = entityTransform->position + Math::Vector3(0.3f, 1.1f, 0.3f); // elbow
                    m_IKChainCache[2] = handPos;                                                        // hand
                    Animation::FABRIK::Solve(m_IKChainCache, nearestTarget, 5);
                }
            }
        }

        // Two-Bone IK: analytic solve for arm/leg chains
        auto* twoBoneIK = m_World->GetComponent<TwoBoneIKComponent>(entity);
        if (twoBoneIK && twoBoneIK->weight > 0.0f) {
            const auto* skeleton = animComp->animator.GetSkeleton();
            if (skeleton) {
                i32 rootIdx = skeleton->FindBoneIndex(twoBoneIK->rootBoneName);
                i32 midIdx = skeleton->FindBoneIndex(twoBoneIK->midBoneName);
                i32 endIdx = skeleton->FindBoneIndex(twoBoneIK->endBoneName);

                if (rootIdx >= 0 && midIdx >= 0 && endIdx >= 0) {
                    const auto& pose = animComp->animator.GetCurrentPose();

                    // Get entity world transform to convert bone-local to world space
                    auto* entityTransform2 = m_World->GetComponent<TransformComponent>(entity);
                    Math::Matrix4 entityWorld = entityTransform2 ? ComputeWorldMatrix(m_World, entity) : Math::Matrix4::Identity();

                    // Extract bone world positions (pose.worldTransforms are in entity-local space)
                    Math::Matrix4 rootWorld = entityWorld * pose.worldTransforms[rootIdx];
                    Math::Matrix4 midWorld = entityWorld * pose.worldTransforms[midIdx];
                    Math::Matrix4 endWorld = entityWorld * pose.worldTransforms[endIdx];

                    Math::Vector3 rootPos(rootWorld.m[12], rootWorld.m[13], rootWorld.m[14]);
                    Math::Vector3 midPos(midWorld.m[12], midWorld.m[13], midWorld.m[14]);
                    Math::Vector3 endPos(endWorld.m[12], endWorld.m[13], endWorld.m[14]);

                    // Resolve target position
                    Math::Vector3 ikTarget = twoBoneIK->targetPosition;
                    if (twoBoneIK->useEntityTarget && twoBoneIK->targetEntity != INVALID_ENTITY) {
                        auto* targetTransform = m_World->GetComponent<TransformComponent>(twoBoneIK->targetEntity);
                        if (targetTransform) {
                            ikTarget = targetTransform->position;
                        }
                    }

                    // Solve two-bone IK
                    Math::Vector3 solvedMid, solvedEnd;
                    Animation::TwoBoneIK::Solve(
                        rootPos, midPos, endPos, ikTarget,
                        twoBoneIK->poleVector, twoBoneIK->weight,
                        solvedMid, solvedEnd
                    );

                    // Apply IK result by computing rotation deltas for root and mid bones
                    auto& poseMut = const_cast<Animation::SkeletonPose&>(pose);

                    // Root bone rotation delta: rotate the upper limb toward solved mid position
                    {
                        Math::Vector3 origDir = midPos - rootPos;
                        Math::Vector3 newDir = solvedMid - rootPos;
                        f32 origLen = origDir.Length();
                        f32 newLen = newDir.Length();
                        if (origLen > 0.0001f && newLen > 0.0001f) {
                            origDir = origDir * (1.0f / origLen);
                            newDir = newDir * (1.0f / newLen);
                            Math::Vector3 axis = origDir.Cross(newDir);
                            f32 axisMag = axis.Length();
                            f32 dotP = std::clamp(origDir.Dot(newDir), -1.0f, 1.0f);
                            if (axisMag > 0.0001f) {
                                axis = axis * (1.0f / axisMag);
                                Math::Quaternion rotDelta(axis, std::acos(dotP));
                                poseMut.localRotations[rootIdx] = rotDelta * poseMut.localRotations[rootIdx];
                            }
                        }
                    }

                    // Mid bone rotation delta: rotate the lower limb toward solved end position
                    {
                        Math::Vector3 origDir = endPos - midPos;
                        Math::Vector3 newDir = solvedEnd - solvedMid;
                        f32 origLen = origDir.Length();
                        f32 newLen = newDir.Length();
                        if (origLen > 0.0001f && newLen > 0.0001f) {
                            origDir = origDir * (1.0f / origLen);
                            newDir = newDir * (1.0f / newLen);
                            Math::Vector3 axis = origDir.Cross(newDir);
                            f32 axisMag = axis.Length();
                            f32 dotP = std::clamp(origDir.Dot(newDir), -1.0f, 1.0f);
                            if (axisMag > 0.0001f) {
                                axis = axis * (1.0f / axisMag);
                                Math::Quaternion rotDelta(axis, std::acos(dotP));
                                poseMut.localRotations[midIdx] = rotDelta * poseMut.localRotations[midIdx];
                            }
                        }
                    }

                    // Write updated rotations back and mark dirty
                    animComp->animator.SetBoneLocalRotation(
                        twoBoneIK->rootBoneName, poseMut.localRotations[rootIdx]);
                    animComp->animator.SetBoneLocalRotation(
                        twoBoneIK->midBoneName, poseMut.localRotations[midIdx]);
                    animComp->matricesDirty = true;
                }
            }
        }
    }

    // Update bone attachment transforms: snap attached entities to their target bone
    for (Entity entity : m_World->GetEntitiesWithComponent<BoneAttachmentComponent>()) {
        auto* ba = m_World->GetComponent<BoneAttachmentComponent>(entity);
        if (!ba || ba->targetEntity == INVALID_ENTITY || ba->targetBoneName.empty()) continue;
        if (!m_World->IsValid(ba->targetEntity)) continue;

        auto* animComp = m_World->GetComponent<AnimatorComponent>(ba->targetEntity);
        if (!animComp) continue;

        // Get bone world transform (in skeleton/entity-local space)
        Math::Matrix4 boneLocal = animComp->animator.GetBoneWorldTransform(ba->targetBoneName);

        // Multiply by the target entity's world matrix to get the bone in world space
        Math::Matrix4 targetWorld = ComputeWorldMatrix(m_World, ba->targetEntity);
        Math::Matrix4 boneWorld = targetWorld * boneLocal;

        // Extract bone world position
        Math::Vector3 bonePos(boneWorld.m[12], boneWorld.m[13], boneWorld.m[14]);

        // Extract bone world rotation from the 3x3 portion of the matrix
        Math::Quaternion boneRot = Math::Quaternion::FromMatrix(boneWorld);

        // Apply offsets
        Math::Vector3 finalPos = bonePos + boneRot.Rotate(ba->positionOffset);
        Math::Quaternion finalRot = boneRot * ba->rotationOffset;

        // Write to this entity's transform
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (transform) {
            transform->position = finalPos;
            transform->rotation = finalRot;
            transform->worldMatrixDirty = true;
        }
    }

    // Classify scene composition (2D / 2.5D / 3D) before rendering decisions
    ClassifySceneComposition();

    // When main-pass rendering is skipped (editor viewport renders offscreen),
    // skip all pre-pass work (shadows, culling, RT, clustered lighting) but still
    // start the main render pass so ImGui has a valid pass to draw into.
    if (m_SkipMainPassRendering) {
        m_SkipMainPassRendering = false;  // Consume for this frame (RenderOffscreen sets it each frame)
        m_Renderer->BeginMainRenderPass();
        return;
    }

    // Build list of cullable objects for GPU frustum culling
    // Only done when we have 3D meshes and GPU culling is enabled.
    // In editor mode, skip culling entirely so all entities are visible for editing.
    // In player mode, skip because GPU compute shaders (cull.comp.spv) are not
    // available in built games — indirect draw buffers would be empty, causing
    // entities to be marked as "drawn" but never actually rendered.
    if (m_GPUCullingEnabled && !m_IsEditorMode && !m_PlayerMode && m_SceneComposition.mesh3DCount > 0) {
        BuildCullableObjectList();
    }

    // Select shadow-casting point/spot lights (before shadow passes)
    if (m_ShadowsEnabled && m_SceneComposition.mode == SceneRenderMode::Scene3D) {
        SelectShadowLights();
    } else {
        m_ActivePointShadowCount = 0;
        m_ActiveSpotShadowCount = 0;
    }

    // Shadow pass first (if enabled) - only run when 3D meshes AND shadow-casting lights exist.
    // Pure 2D scenes skip entirely since sprites never cast shadows.
    // Scenes with no shadow-casting lights also skip to avoid rendering 4 cascades for nothing.
    // During play mode, the game view runs its own shadow pass via RenderShadowPassForCamera(),
    // so we skip the main-pass shadows to avoid rendering 8 cascade passes per frame.
    if (!m_SkipMainPassShadows && m_ShadowsEnabled && m_ShadowMap && m_ShadowPipeline &&
        m_SceneComposition.mode == SceneRenderMode::Scene3D &&
        m_SceneComposition.hasShadowCastingLights) {
        RenderShadowPass();
    }

    // Point light shadow pass
    if (!m_SkipMainPassShadows && m_ShadowsEnabled && m_PointShadowMap && m_PointShadowPipeline && m_ActivePointShadowCount > 0) {
        RenderPointShadowPass();
    }

    // Spot light shadow pass
    if (!m_SkipMainPassShadows && m_ShadowsEnabled && m_SpotShadowMap && m_SpotShadowPipeline && m_ActiveSpotShadowCount > 0) {
        RenderSpotShadowPass();
    }

    // GPU frustum culling (compute shader dispatch before main render pass)
    // Upload per-object material/transform data, then run the compute culling shader.
    // When async compute is available, record to dedicated compute command buffer.
    // Skipped in editor mode — the editor scene view shows all entities.
    if (m_GPUCullingEnabled && !m_IsEditorMode && !m_CullableObjects.empty()) {
        UploadObjectData();
        if (m_Renderer->HasAsyncCompute() && m_Renderer->BeginComputeCommandBuffer()) {
            // Record culling on async compute queue
            PerformGPUCullingAsync();
            m_Renderer->EndComputeCommandBuffer();
            m_Renderer->SubmitCompute();
        } else {
            // Fall back to graphics queue
            PerformGPUCulling();
        }
    }

    // Ray tracing pass (after shadow passes, before main render pass)
    // When async compute is available, dispatch RT effects and denoising on the compute
    // queue so they overlap with main geometry rasterization on the graphics queue.
    // TLAS rebuild stays on graphics (needs vertex/index buffer access).
    // Compositing is deferred until after main pass when compute results are ready.
    if (m_RTEnabled && m_ASManager && m_SceneComposition.mode == SceneRenderMode::Scene3D) {
        VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
        if (commandBuffer != VK_NULL_HANDLE) {
            RebuildTLAS(commandBuffer);

            u32 frameIdx = m_Renderer->GetCurrentFrameIndex();
            bool asyncRT = m_AsyncComputeScheduler &&
                           m_AsyncComputeScheduler->ShouldUseAsync(Renderer::AsyncComputeWorkType::RTDispatch);

            if (asyncRT) {
                // Dispatch RT effects on async compute queue (overlaps with main geometry)
                DispatchRTEffectsAsync(frameIdx);
                // Denoising also runs on compute queue after RT finishes
                // (submitted as part of the same compute command buffer)
            } else {
                // Single-queue fallback: RT effects + temporal reuse + denoise + composite on graphics queue
                DispatchRTEffects(commandBuffer);
                TemporalReuseRTOutputs(commandBuffer);
                DenoiseRTOutputs(commandBuffer);
                CompositeRTResults(commandBuffer);
            }
        }
    }

    // Clustered forward lighting: build light list and assign to spatial clusters before main render pass
#ifdef ENJIN_CLUSTERED_LIGHTING
    if (m_ClusteredLighting && !m_PlayerMode && m_SceneComposition.mode != SceneRenderMode::Scene2D && m_Camera) {
        VkCommandBuffer cmdBuf = m_Renderer->GetCurrentCommandBuffer();
        if (cmdBuf != VK_NULL_HANDLE) {
            // Build ClusterLight array from cached light entities (reuse pre-allocated vector)
            std::vector<Renderer::ClusterLight> clusterLights;
            
            auto* lightStorageCL = m_World->GetComponentStorage<LightComponent>();
            for (Entity e : m_CachedLightEntities) {
                auto* light = lightStorageCL ? lightStorageCL->Get(e) : nullptr;
                auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(e) : nullptr;
                if (!light || light->type == LightType::Directional) continue;

                Renderer::ClusterLight cl{};
                cl.position = xform ? xform->position : Math::Vector3(0.0f);
                cl.range = light->range;
                cl.color = light->color;
                cl.intensity = light->intensity;
                if (light->type == LightType::Spot) {
                    Math::Vector3 fwd(0.0f, 0.0f, -1.0f);
                    cl.direction = xform ? xform->rotation.Rotate(fwd).Normalized() : Math::Vector3(0, -1, 0);
                    cl.outerConeAngle = light->outerConeAngle;
                } else {
                    cl.direction = Math::Vector3(0.0f);
                    cl.outerConeAngle = 0.0f;
                }
                clusterLights.push_back(cl);
            }
            if (!clusterLights.empty()) {
                Math::Matrix4 viewMatrix = m_Camera->GetViewMatrix();
                m_ClusteredLighting->AssignLights(cmdBuf, clusterLights.data(),
                    static_cast<u32>(clusterLights.size()), viewMatrix);
            }
        }
    }
#endif

    // Periodic diagnostic warnings (every 300 frames)
    if (++m_DiagnosticFrameCounter >= 300) {
        m_DiagnosticFrameCounter = 0;
        if (m_SceneComposition.spriteCount > 100 && !m_SpriteBatchRenderer) {
            ENJIN_LOG_WARN(Renderer, "Scene has %u sprites without batch renderer — high draw call count",
                           m_SceneComposition.spriteCount);
        }
        // Warn on ortho/perspective camera mixing
        {
            u32 perspCount = 0, orthoCount = 0;
            for (Entity e : m_World->GetEntitiesWithComponent<CameraComponent>()) {
                auto* cam = m_World->GetComponent<CameraComponent>(e);
                if (!cam || !cam->isActive) continue;
                if (cam->projectionType == ProjectionType::Perspective) perspCount++;
                else orthoCount++;
            }
            if (perspCount > 0 && orthoCount > 0) {
                ENJIN_LOG_WARN(Renderer,
                    "Mixed camera projections: %u perspective + %u orthographic — may cause unexpected rendering",
                    perspCount, orthoCount);
            }
        }
    }

    // Begin the main render pass (after any pre-passes like shadows)
    m_Renderer->BeginMainRenderPass();

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Splitscreen main pass: render each viewport separately
    if (!m_MainPassViewports.empty()) {
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
        Renderer::Camera* prevCamera = m_Camera;

        // Switch to offscreen buffers for per-viewport uniform isolation
        m_ActiveDescriptorSets = &m_OffscreenDescriptorSets;
        m_ActiveUniformBuffers = &m_OffscreenUniformBuffers;
        m_ActiveLightingBuffers = &m_OffscreenLightingBuffers;
        m_OffscreenMode = true;

        // Build sorted render list once for all viewports (maximizes descriptor cache hits).
        // Sort keys use depth=0 since splitscreen viewports have different cameras;
        // the important part is grouping by texture/material hash, not depth ordering.
        {
            m_SortedRenderList.clear();
            ECS::View<TransformComponent, MeshComponent> view(*m_World);
            auto* matStorage = m_World->GetComponentStorage<MaterialComponent>();
            m_SortedRenderList.reserve(view.UpperBound());
            for (Entity entity : view) {
                auto* xform = view.Get<TransformComponent>(entity);
                if (!xform || !xform->visible) continue;
                auto* mat = matStorage ? matStorage->Get(entity) : nullptr;
                if (mat) mat->ComputeSortKey(0.0f);
                m_SortedRenderList.push_back(entity);
            }
            std::sort(m_SortedRenderList.begin(), m_SortedRenderList.end(),
                [matStorage](Entity a, Entity b) {
                    auto* matA = matStorage ? matStorage->Get(a) : nullptr;
                    auto* matB = matStorage ? matStorage->Get(b) : nullptr;
                    u64 keyA = matA ? matA->cachedSortKey : 0;
                    u64 keyB = matB ? matB->cachedSortKey : 0;
                    return keyA < keyB;
                });
        }

        u32 viewportCount = static_cast<u32>(m_MainPassViewports.size());
        if (viewportCount > MAX_SPLITSCREEN_VIEWPORTS) viewportCount = MAX_SPLITSCREEN_VIEWPORTS;

        for (u32 v = 0; v < viewportCount; ++v) {
            const ViewportCamera& vc = m_MainPassViewports[v];
            m_CurrentViewportIndex = v;

            auto* cameraComp = m_World->GetComponent<CameraComponent>(vc.entity);
            auto* cameraTransform = m_World->GetComponent<TransformComponent>(vc.entity);
            if (!cameraComp || !cameraTransform) continue;

            Renderer::Camera viewCamera;
            f32 pixelW = vc.viewportWidth * static_cast<f32>(extent.width);
            f32 pixelH = vc.viewportHeight * static_cast<f32>(extent.height);
            f32 aspect = (pixelH > 0.0f) ? (pixelW / pixelH) : 1.0f;

            if (cameraComp->projectionType == ProjectionType::Perspective) {
                viewCamera.SetPerspective(cameraComp->fieldOfView, aspect,
                                           cameraComp->nearPlane, cameraComp->farPlane);
            } else {
                f32 halfH = cameraComp->orthoSize;
                f32 halfW = halfH * aspect;
                viewCamera.SetOrthographic(-halfW, halfW, -halfH, halfH,
                                            cameraComp->nearPlane, cameraComp->farPlane);
            }

            viewCamera.SetPosition(cameraTransform->position);
            Math::Vector3 forward = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
            Math::Vector3 up = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
            viewCamera.SetLookAt(cameraTransform->position, cameraTransform->position + forward, up);

            m_Camera = &viewCamera;
            UpdateFrameUniforms();
            BuildMaterialSSBO();

            VkViewport vkViewport{};
            vkViewport.x = vc.viewportX * static_cast<f32>(extent.width);
            vkViewport.y = vc.viewportY * static_cast<f32>(extent.height);
            vkViewport.width = pixelW;
            vkViewport.height = pixelH;
            vkViewport.minDepth = 0.0f;
            vkViewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);

            VkRect2D scissor{};
            scissor.offset = { static_cast<i32>(vkViewport.x), static_cast<i32>(vkViewport.y) };
            scissor.extent = { static_cast<u32>(pixelW), static_cast<u32>(pixelH) };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            RenderSkybox(commandBuffer, &vkViewport, &scissor);

            m_Pipeline->Bind(commandBuffer);
            {
                u32 zeroOff = 0;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_Pipeline->GetLayout(), 0, 1,
                    &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
            }
            vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Reset descriptor cache for each viewport
            m_LastBound.Reset(); m_GeometryPoolBound = false;

            {
            auto* spriteStorageVP = m_World->GetComponentStorage<Sprite2DComponent>();
            for (Entity entity : m_SortedRenderList) {
                auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                if (!xform) continue;
                if (!xform->visible) continue;
                // Skip GPU-culled entities (frustum culling — disabled in editor mode)
                if (m_GPUCullingEnabled && !m_IsEditorMode && m_GPUCulling && !m_CullableObjects.empty()) {
                    usize entityIdx = static_cast<usize>(entity);
                    if (entityIdx < m_EntityToCullIndex.size()) {
                        u32 cullIdx = m_EntityToCullIndex[entityIdx];
                        if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                            continue;
                        }
                    }
                }
                // Skip 2D sprites — rendered in sorted pass after 3D geometry
                if (spriteStorageVP && spriteStorageVP->Has(entity)) continue;
                RenderEntity(entity);
            }
            }

            // Geometry outline pass (after 3D geometry)
            RenderOutlinePass();

            // Per-entity wireframe overlay (after outlines, before sprites)
            RenderWireframeOverlayPass();

            // Sorted 2D sprite rendering pass (after 3D geometry)
            RenderSprites();

            // Render effects for this viewport
            u32 vpW = static_cast<u32>(pixelW);
            u32 vpH = static_cast<u32>(pixelH);
            RenderGrass(vpW, vpH);
            RenderShrubs(vpW, vpH);
            RenderTrees(vpW, vpH);
            RenderParticles(vpW, vpH);
            RenderFluid(vpW, vpH);
        }

        m_Camera = prevCamera;
        m_ActiveDescriptorSets = &m_DescriptorSets;
        m_ActiveUniformBuffers = &m_UniformBuffers;
        m_ActiveLightingBuffers = &m_LightingBuffers;
        m_OffscreenMode = false;
        m_CurrentViewportIndex = 0;
        return;
    }

    // Single-camera main pass (default)

    // Upload frame-level uniforms once (view/proj + lighting)
    UpdateFrameUniforms();
    BuildMaterialSSBO();

    // Update bindless descriptor set if any textures were registered/changed
    if (m_BindlessManager) m_BindlessManager->UpdateDescriptorSet();

    // Build the sorted render list every frame (needed by RenderToTarget() offscreen path too)
    {
        m_SortedRenderList.clear();

        // Multi-component view: iterates the smallest set (Transform or Mesh),
        // filters to entities that have both. Excludes 2D sprites (separate pass).
        // All storage pointers are cached in the View — no per-entity type-ID hash lookups.
        ECS::View<TransformComponent, MeshComponent> view(*m_World);
        view.Exclude(m_World->GetComponentStorage<Sprite2DComponent>());

        // Also cache material storage for sort key computation (optional component)
        auto* matStorage = m_World->GetComponentStorage<MaterialComponent>();

        // Compute camera position for depth-aware sort key
        Math::Vector3 camPos;
        bool haveCam = (m_Camera != nullptr);
        if (haveCam) camPos = m_Camera->GetPosition();

        m_SortedRenderList.reserve(view.UpperBound());

        for (Entity entity : view) {
            auto* xform = view.Get<TransformComponent>(entity);
            if (!xform->visible) continue;

            // Skip GPU-culled entities (frustum culling — disabled in editor mode)
            if (m_GPUCullingEnabled && !m_IsEditorMode && m_GPUCulling && !m_CullableObjects.empty()) {
                usize entityIdx = static_cast<usize>(entity);
                if (entityIdx < m_EntityToCullIndex.size()) {
                    u32 cullIdx = m_EntityToCullIndex[entityIdx];
                    if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                        continue;
                    }
                }
            }

            // Compute 64-bit sort key (pipeline | material/texture hash | depth)
            auto* mat = matStorage ? matStorage->Get(entity) : nullptr;
            if (mat) {
                f32 depth = haveCam ? (xform->position - camPos).Length() : 0.0f;
                mat->ComputeSortKey(depth);
            }

            m_SortedRenderList.push_back(entity);
        }

        // Sort by 64-bit material sort key: groups by pipeline (opaque→mask→blend),
        // then by material/texture hash (minimizes descriptor set updates),
        // then by depth (front-to-back for opaque, back-to-front for blend).
        std::sort(m_SortedRenderList.begin(), m_SortedRenderList.end(),
            [matStorage](Entity a, Entity b) {
                auto* matA = matStorage ? matStorage->Get(a) : nullptr;
                auto* matB = matStorage ? matStorage->Get(b) : nullptr;
                u64 keyA = matA ? matA->cachedSortKey : 0;
                u64 keyB = matB ? matB->cachedSortKey : 0;
                return keyA < keyB;
            });
    }

    // GPU timestamp: main geometry begin
    {
        VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_MAIN_BEGIN);
        }
    }

    // Render skybox first (behind all geometry)
    RenderSkybox(commandBuffer);

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Bind pipeline, descriptor set, viewport, and scissor once for all entities
    m_Pipeline->Bind(commandBuffer);
    u32 matDynOffset = 0;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &matDynOffset);

    // Bind bindless texture array at set 1 (persists for entire pass)
    if (m_BindlessManager) {
        VkDescriptorSet bindlessSet = m_BindlessManager->GetDescriptorSet();
        if (bindlessSet) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 1, 1, &bindlessSet, 0, nullptr);
        }
    }

    VkExtent2D extent = m_Renderer->GetSwapchainExtent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Single-pass LOD update + render — iterates renderable entities once
    // Reset last-bound state so no stale descriptor data carries from a previous pass
    m_LastBound.Reset(); m_GeometryPoolBound = false;

    // GPU-driven rendering: DGC (if enabled) or multi-draw indirect fallback.
    // Skipped in editor mode (editor always uses per-entity draws for full control).
    if (m_GPUCullingEnabled && !m_IsEditorMode && !m_CullableObjects.empty() && m_GeometryPool) {
        // Device Generated Commands: GPU generates push constants + draw calls directly.
        // Eliminates ALL CPU-side draw call submission for non-textured pool entities.
        if (m_DGC && m_DGC->IsEnabled()) {
            DrawDGC(commandBuffer);
        } else {
            // Fallback: multi-draw indirect with parallaxScale = -1.0 sentinel
            DrawIndirect(commandBuffer);
        }

        // Texture-grouped indirect draws: batch textured pool entities by texture set.
        // Each batch shares the same descriptor state, reducing draw calls for textured meshes.
        if (m_IndirectDrawBatcher && m_IndirectDrawBatcher->HasBatches()) {
            DrawTexturedIndirect(commandBuffer);
        }
    }

    // If RT effects were dispatched on async compute, wait for completion and composite.
    // The compute queue ran RT dispatches + denoising while the main geometry was rasterizing.
    if (m_AsyncComputeScheduler && m_AsyncComputeScheduler->WasComputeSubmitted() &&
        m_AsyncComputeScheduler->GetLastWorkType() == Renderer::AsyncComputeWorkType::RTDispatch) {
        m_AsyncComputeScheduler->WaitForCompute(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        CompositeRTResults(commandBuffer);
    }

    {
        Math::Vector3 camPos;
        bool doLOD = (m_Camera != nullptr);
        if (doLOD) camPos = m_Camera->GetPosition();

        // Cache component storage pointers for the hot loop (avoids per-entity type-ID lookups)
        auto* lodStorage = m_World->GetComponentStorage<LODComponent>();
        auto* xformStorageLoop = m_World->GetComponentStorage<TransformComponent>();
        auto* meshStorageLoop = m_World->GetComponentStorage<MeshComponent>();

        for (Entity entity : m_SortedRenderList) {
            // LOD selection with hysteresis (if camera is available)
            if (doLOD) {
                auto* lod = lodStorage ? lodStorage->Get(entity) : nullptr;
                if (lod && lod->enabled && lod->levelCount > 1) {
                    auto* transform = xformStorageLoop ? xformStorageLoop->Get(entity) : nullptr;
                    if (transform) {
                        f32 metric;
                        if (lod->useScreenSize) {
                            // Screen-space projected size: accounts for object scale.
                            // Approximation: bounding sphere diameter / distance.
                            auto* mesh = meshStorageLoop ? meshStorageLoop->Get(entity) : nullptr;
                            f32 dist = Math::Max((transform->position - camPos).Length(), 0.001f);
                            f32 scale = Math::Max(Math::Max(
                                Math::Abs(transform->scale.x),
                                Math::Abs(transform->scale.y)),
                                Math::Abs(transform->scale.z));
                            // Use cached AABB extent as size estimate (if available)
                            f32 objectSize = scale;
                            if (mesh) {
                                Math::Vector3 extent = mesh->cachedAABBMax - mesh->cachedAABBMin;
                                objectSize = scale * Math::Max(Math::Max(extent.x, extent.y), extent.z);
                            }
                            // Screen metric: larger = closer/bigger = more detail needed
                            // Invert so that larger metric means further away (matches distance thresholds)
                            metric = dist / Math::Max(objectSize, 0.01f);
                        } else {
                            metric = (transform->position - camPos).Length();
                        }

                        // LOD selection with directional hysteresis bands.
                        // When at LOD N, upgrading to N-1 (more detail) requires metric < threshold - hysteresis.
                        // Downgrading to N+1 (less detail) requires metric > threshold + hysteresis.
                        i32 newLOD = lod->activeLOD;
                        f32 hyst = lod->hysteresisRatio;
                        for (i32 l = 0; l < lod->levelCount - 1; ++l) {
                            f32 threshold = lod->levels[l].maxDistance;
                            f32 band = threshold * hyst;
                            if (l < lod->activeLOD) {
                                // Considering upgrading to higher detail (lower LOD index)
                                // Must be well within this level's range
                                if (metric < threshold - band) {
                                    newLOD = l;
                                    break;
                                }
                            } else if (l >= lod->activeLOD) {
                                // Considering downgrading to lower detail (higher LOD index)
                                if (metric > threshold + band) {
                                    newLOD = l + 1;
                                } else {
                                    break;
                                }
                            }
                        }

                        if (newLOD != lod->activeLOD && newLOD < lod->levelCount) {
                            auto* mesh = meshStorageLoop ? meshStorageLoop->Get(entity) : nullptr;
                            if (mesh && lod->levels[newLOD].mesh.IsValid()) {
                                *mesh = lod->levels[newLOD].mesh;
                                if (static_cast<usize>(entity) < m_EntityRenderData.size())
                                    m_EntityRenderData[static_cast<usize>(entity)].Invalidate();
                                lod->activeLOD = newLOD;
                            }
                        }
                    }
                }
            }

            RenderEntity(entity);
        }
    }

    // Geometry outline pass (inverted-hull backface extrusion, after main geometry)
    RenderOutlinePass();

    // Per-entity wireframe overlay
    RenderWireframeOverlayPass();

    // Render onion skin ghosts (editor viewport only, before sprites)
    RenderOnionSkinGhosts();

    // Sorted 2D sprite rendering pass (after 3D geometry)
    RenderSprites();

    // Render effect passes (grass, shrubs, trees, particles, fluid)
    RenderGrass(0, 0);
    RenderShrubs(0, 0);
    RenderTrees(0, 0);
    RenderParticles(0, 0);
    RenderFluid(0, 0);

    // Render weather particles in main pass if set (editor viewport)
    if (m_MainPassWeather) {
        RenderWeatherParticles(*m_MainPassWeather, m_MainPassWeatherIsRain, 0, 0);
    }

    // GPU timestamp: main geometry end
    {
        VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_MAIN_END);
        }
    }
}

void RenderSystem::RenderToTarget(Renderer::RenderTarget* target, Renderer::Camera* camera, u32 viewportIndex) {
    if (!target || !target->IsValid() || !camera || !m_Renderer || !m_Initialized || !m_Pipeline) {
        return;
    }

    // NOTE: The caller (EditorLayer::RenderOffscreen) has already started the render target's
    // render pass via sceneTarget->Begin(). We must NOT start another render pass here
    // (e.g. shadow pass). Water meshes and shadows are handled by Update() for the main pass.

    // Temporarily swap the camera so UpdateFrameUniforms uses the game camera
    Renderer::Camera* prevCamera = m_Camera;
    m_Camera = camera;

    // Switch to offscreen buffers and descriptor sets so the main pass
    // doesn't overwrite the game-camera uniform data
    m_ActiveDescriptorSets = &m_OffscreenDescriptorSets;
    m_ActiveUniformBuffers = &m_OffscreenUniformBuffers;
    m_ActiveLightingBuffers = &m_OffscreenLightingBuffers;
    m_OffscreenMode = true;
    m_CurrentViewportIndex = viewportIndex;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        m_Camera = prevCamera;
        m_ActiveDescriptorSets = &m_DescriptorSets;
        m_ActiveUniformBuffers = &m_UniformBuffers;
        m_ActiveLightingBuffers = &m_LightingBuffers;
        m_OffscreenMode = false;
        return;
    }

    // The render target's Begin/End are handled by the caller (EditorLayer::RenderOffscreen)
    // We just need to bind our pipeline and draw within the active render pass

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    u32 activeIdx = GetActiveBufferIndex(currentFrame);
    // Clamp shadow distance to match game camera far plane (keeps cascade splits consistent)
    f32 prevShadowDist = m_ShadowDistance;
    f32 camFar = camera->GetFarPlane();
    if (m_ShadowDistance > camFar) {
        m_ShadowDistance = camFar;
    }

    // Ensure light list is fresh (RenderOffscreen runs before Update where it's normally rebuilt)
    if (m_LightListDirty && m_World) {
        m_CachedLightEntities.clear();
        auto* lightStorage = m_World->GetComponentStorage<LightComponent>();
        if (lightStorage) {
            const auto& liveLights = m_World->GetEntitiesWithComponent<LightComponent>();
            m_CachedLightEntities.assign(liveLights.begin(), liveLights.end());
        }
        m_LightListDirty = false;
    }

    // Upload frame-level uniforms to offscreen buffers (game camera view/proj + lighting)
    UpdateFrameUniforms();
    BuildMaterialSSBO();

    m_ShadowDistance = prevShadowDist;

    // Set viewport and scissor to match render target size
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(target->GetWidth());
    viewport.height = static_cast<f32>(target->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { target->GetWidth(), target->GetHeight() };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Render skybox in game view (pass viewport/scissor for render target dimensions)
    RenderSkybox(commandBuffer, &viewport, &scissor);

    // Reset descriptor cache for this render pass
    m_LastBound.Reset(); m_GeometryPoolBound = false;

    // Use offscreen pipeline (created for offscreen UNORM render pass) to avoid
    // Vulkan spec violation from binding SRGB pipeline in UNORM render pass
    auto* targetPipeline = m_OffscreenPipeline ? m_OffscreenPipeline.get() : m_Pipeline.get();

    // Bind pipeline and descriptor set ONCE before the entity loop (not per-entity)
    targetPipeline->Bind(commandBuffer);
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            targetPipeline->GetLayout(), 0, 1, &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
    }

    // Use the sorted render list (sorted by cachedTextureKey) to maximize descriptor cache hits.
    // The main pass builds m_SortedRenderList each frame; reuse it for the offscreen path.
    // If the list is empty (e.g. first frame), fall back to unsorted iteration.
    const auto& renderList = m_SortedRenderList.empty()
        ? m_World->GetEntitiesWithComponent<MeshComponent>()
        : m_SortedRenderList;

    // Cache storage pointers for the RenderToTarget entity loop
    auto* spriteStorageRT = m_World->GetComponentStorage<Sprite2DComponent>();

    for (Entity entity : renderList) {
        {
            // Skip invisible entities or entities without transform (cached storage)
            auto* xformRT = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (!xformRT || !xformRT->visible) continue;

            // Skip GPU-culled entities (frustum culling — disabled in editor mode)
            if (m_GPUCullingEnabled && !m_IsEditorMode && m_GPUCulling && !m_CullableObjects.empty()) {
                usize entityIdx = static_cast<usize>(entity);
                if (entityIdx < m_EntityToCullIndex.size()) {
                    u32 cullIdx = m_EntityToCullIndex[entityIdx];
                    if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                        continue;
                    }
                }
            }

            // Skip 2D sprites — rendered in sorted pass after 3D geometry
            if (spriteStorageRT && spriteStorageRT->Has(entity)) continue;

            EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
                ? &m_EntityRenderData[static_cast<usize>(entity)] : SetupEntityBuffers(entity);
            if (!pRD) continue;
            EntityRenderData& renderData = *pRD;

            // Bind material SSBO at this entity's dynamic offset
            {
                u32 matIdx = GetMaterialIndex(entity);
                u32 dynOffset = matIdx * m_MaterialSSBOStride;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    targetPipeline->GetLayout(), 0, 1, &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &dynOffset);
            }

            // Push constants (world matrix includes parent chain).
            // For skinned meshes: use identity model matrix because skinning matrices
            // already transform vertices from bone-local → world space. Applying the
            // entity's parent hierarchy on top would double-transform the mesh.
            Renderer::PushConstants pushConstants{};
            AnimatorComponent* preCheckAnim = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
            pushConstants.model = ECS::ComputeWorldMatrix(m_World, entity);

            MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
            Renderer::Texture* boundTexture = nullptr;
            Renderer::Texture* texHeight = nullptr;
            Renderer::Texture* texNormal = nullptr;
            Renderer::Texture* texMR = nullptr;
            Renderer::Texture* texEmissive = nullptr;
            Renderer::Texture* texMatcap = nullptr;

            if (material) {
                pushConstants.baseColor = material->baseColor;
                pushConstants.metallic = material->metallic;
                pushConstants.emissiveColor = material->emissiveColor;
                pushConstants.roughness = material->roughness;
                pushConstants.emissiveStrength = material->emissiveStrength;
                pushConstants.opacity = material->opacity;
                pushConstants.alphaCutoff = material->alphaCutoff;

                // Resolve textures using cache (avoids per-frame string hash lookups)
                if (material->textureCacheDirty) {
                    if (!material->baseColorTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->baseColorTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedBaseColorTexture = tex.get();
                            material->baseColorTexture = 1;
                        }
                    }
                    if (!material->heightTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->heightTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedHeightTexture = tex.get();
                            material->heightTexture = 1;
                        }
                    }
                    if (!material->normalTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->normalTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedNormalTexture = tex.get();
                            material->normalTexture = 1;
                        }
                    }
                    if (!material->metallicRoughnessTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->metallicRoughnessTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    // Specular map overrides metallic-roughness slot for pre-PBR shading
                    if (!material->specularTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->specularTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    if (!material->emissiveTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedEmissiveTexture = tex.get();
                            material->emissiveTexture = 1;
                        }
                    }
                    if (!material->matcapTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->matcapTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMatcapTexture = tex.get();
                            material->matcapTexture = 1;
                        }
                    }
                    material->textureCacheDirty = false;
                    material->cachedTextureKey = { material->cachedBaseColorTexture,
                        material->cachedHeightTexture, material->cachedNormalTexture,
                        material->cachedMetallicRoughnessTexture, material->cachedEmissiveTexture,
                        material->cachedMatcapTexture };
                }

                // Use cached texture pointers
                boundTexture = material->cachedBaseColorTexture;
                texHeight = material->cachedHeightTexture;
                texNormal = material->cachedNormalTexture;
                texMR = material->cachedMetallicRoughnessTexture;
                texEmissive = material->cachedEmissiveTexture;
                texMatcap = material->cachedMatcapTexture;

                pushConstants.flags = 0;
                if (material->doubleSided) pushConstants.flags |= 1;
                if (material->castShadows) pushConstants.flags |= 2;
                if (material->receiveShadows) pushConstants.flags |= 4;
                pushConstants.flags |= (static_cast<i32>(material->alphaMode) << 8);
                if (boundTexture != nullptr) pushConstants.flags |= (1 << 16);
                if (material->normalTexture >= 0) pushConstants.flags |= (1 << 17);
                if (material->metallicRoughnessTexture >= 0) pushConstants.flags |= (1 << 18);
                if (material->emissiveTexture >= 0) pushConstants.flags |= (1 << 19);
                // Height texture flag
                if (material->heightTexture >= 0) pushConstants.flags |= (1 << 10);
                // Retro flags (per-material)
                if (material->flatShading) pushConstants.flags |= (1 << 20);
                if (material->affineTexturing) pushConstants.flags |= (1 << 21);
                if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
                if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
                if (material->uvQuantize) pushConstants.flags |= (1 << 12);
                if (material->gouraudOnly) pushConstants.flags |= (1 << 13);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
                pushConstants.flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
                pushConstants.parallaxScale = material->parallaxScale;
                // Artistic surface params (reused push constant slots)
                pushConstants.surfaceParam1 = material->reflectivity;
                pushConstants.surfaceParam2 = material->fresnelPower;
                pushConstants.surfaceParam3 = material->rimLightStrength;
                // Dithered gradient: encode bands + pattern into surfaceParam1
                if (material->ditherGradient) {
                    pushConstants.flags |= (1 << 20); // Force flat shading
                    pushConstants.surfaceParam1 = 100.0f + static_cast<f32>(material->ditherGradientBands)
                        + static_cast<f32>(material->ditherGradientPattern) * 0.1f;
                }
                // Dithered transparency: encode pattern + opacity + blend color into surfaceParams
                if (material->ditherTransparency) {
                    pushConstants.surfaceParam1 = 200.0f + static_cast<f32>(material->ditherTransPattern);
                    pushConstants.surfaceParam2 = material->ditherTransOpacity;
                    u32 r = static_cast<u32>(material->ditherTransBlendColor.x * 1023.0f) & 0x3FF;
                    u32 g = static_cast<u32>(material->ditherTransBlendColor.y * 1023.0f) & 0x3FF;
                    u32 b = static_cast<u32>(material->ditherTransBlendColor.z * 1023.0f) & 0x3FF;
                    u32 packed = (r << 20) | (g << 10) | b;
                    pushConstants.surfaceParam3 = *reinterpret_cast<f32*>(&packed);
                }
                // Elemental surface effects: encode char/wet/snow/frost into surfaceParams
                if (!material->ditherGradient && !material->ditherTransparency) {
                    auto* elemSurface = m_World->GetComponent<ECS::ElementalSurfaceComponent>(entity);
                    if (elemSurface && (elemSurface->charAmount > 0.01f || elemSurface->wetness > 0.01f ||
                                        elemSurface->snowCoverage > 0.01f || elemSurface->frostAmount > 0.01f)) {
                        pushConstants.surfaceParam1 = 300.0f + elemSurface->charAmount;
                        pushConstants.surfaceParam2 = elemSurface->wetness + std::floor(elemSurface->snowCoverage * 256.0f);
                        pushConstants.surfaceParam3 = elemSurface->frostAmount;
                    }
                }
                // Procedural surface noise: encode scale/strength into surfaceParams (range 400+)
                // Only when no other effect has claimed the surfaceParam slots
                if (!material->ditherGradient && !material->ditherTransparency &&
                    material->surfaceNoiseScale > 0.0f && pushConstants.surfaceParam1 < 100.0f) {
                    pushConstants.surfaceParam1 = 400.0f + material->surfaceNoiseScale;
                    pushConstants.surfaceParam2 = material->surfaceNoiseStrength;
                }
            } else {
                pushConstants.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
                pushConstants.metallic = 0.0f;
                pushConstants.emissiveColor = Math::Vector3(0.0f);
                pushConstants.roughness = 0.5f;
                pushConstants.emissiveStrength = 0.0f;
                pushConstants.opacity = 1.0f;
                pushConstants.alphaCutoff = 0.5f;
                pushConstants.flags = 0;
                pushConstants.parallaxScale = 0.0f;
            }

            // Global retro overrides (OR with per-material — global forces on)
            if (m_GlobalFlatShading) pushConstants.flags |= (1 << 20);
            if (m_GlobalAffineTexturing) pushConstants.flags |= (1 << 21);
            if (m_GlobalVertexSnapping) pushConstants.flags |= (1 << 22);
            if (m_GlobalStippleTransparency) pushConstants.flags |= (1 << 23);
            if (m_GlobalUVQuantize) pushConstants.flags |= (1 << 12);
            if (m_GlobalGouraudOnly) pushConstants.flags |= (1 << 13);
            if (m_GlobalVertexSnapping && m_GlobalVertexSnapResolution > 0) {
                pushConstants.flags = (pushConstants.flags & ~(0x1F << 24)) | (static_cast<i32>((m_GlobalVertexSnapResolution / 8) & 0x1F) << 24);
            }

            // Per-entity art style override (ArtStyleComponent)
            ArtStyleComponent* artStyle = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
            if (artStyle && artStyle->style != ArtStyleType::Inherit) {
                switch (artStyle->style) {
                case ArtStyleType::PrePBR:
                    if (artStyle->prePBR_flatShading) pushConstants.flags |= (1 << 20);
                    if (artStyle->prePBR_gouraudOnly) pushConstants.flags |= (1 << 13);
                    break;
                case ArtStyleType::HandPainted:
                    // Hand-painted uses light ramp (handled at scene level),
                    // per-entity just force-enables half-Lambert via gouraud mode
                    break;
                case ArtStyleType::CelToon:
                    // Per-entity cel: rim strength via push constants (outline handled in outline pass)
                    pushConstants.surfaceParam3 = artStyle->cel_rimStrength;
                    break;
                case ArtStyleType::Retro:
                    if (artStyle->retro_flatShading) pushConstants.flags |= (1 << 20);
                    if (artStyle->retro_affineTexturing) pushConstants.flags |= (1 << 21);
                    if (artStyle->retro_vertexSnapping) pushConstants.flags |= (1 << 22);
                    if (artStyle->retro_uvQuantize) pushConstants.flags |= (1 << 12);
                    if (artStyle->retro_vertexSnapping && artStyle->retro_snapResolution > 0) {
                        pushConstants.flags = (pushConstants.flags & ~(0x1F << 24))
                            | (static_cast<i32>((artStyle->retro_snapResolution / 8) & 0x1F) << 24);
                    }
                    break;
                case ArtStyleType::MaterialExpression:
                    // Override surface noise from art style component (SSS handled in SSBO build)
                    if (material && artStyle->matExpr_surfaceNoiseScale > 0.0f &&
                        pushConstants.surfaceParam1 < 100.0f) {
                        pushConstants.surfaceParam1 = 400.0f + artStyle->matExpr_surfaceNoiseScale;
                        pushConstants.surfaceParam2 = artStyle->matExpr_surfaceNoiseStrength;
                    }
                    break;
                case ArtStyleType::NPR:
                case ArtStyleType::PixelArt:
                case ArtStyleType::Analog:
                    // These styles are primarily post-process driven (outlines, palettes,
                    // film effects). The per-entity component stores parameters but the
                    // actual rendering happens in the post-process pass, which queries
                    // ArtStyleComponent on the camera entity or scene default.
                    break;
                default:
                    break;
                }
            }

            // Set wind sway flag for vegetation entities
            if (m_World->HasComponent<VegetationComponent>(entity)) {
                pushConstants.flags |= (1 << 4); // FLAG_WIND_SWAY
            }

            // Set water surface flag for water volume entities
            WaterVolumeComponent* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
            if (waterVol) {
                pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE — always set, shader handles freeze
                pushConstants.parallaxScale = waterVol->freezeProgress; // repurpose for water (POM skips water)

                if (m_RainActive && waterVol->freezeProgress < 0.5f) {
                    pushConstants.flags |= (1 << 6); // FLAG_RAIN_RIPPLES (no ripples on ice)
                }
                if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
                    pushConstants.flags |= (1 << 7); // FLAG_WATER_SHORE
                    pushConstants.surfaceParam1 = waterVol->shoreWidth;
                    pushConstants.surfaceParam2 = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
                    pushConstants.surfaceParam3 = waterVol->foamScale;
                }
                if (waterVol->waterType == WaterType::Ocean) {
                    pushConstants.flags |= (1 << 11); // FLAG_WATER_OCEAN
                }
                // Lerp base color toward ice color as water freezes
                f32 fp = waterVol->freezeProgress;
                pushConstants.baseColor = Math::Vector3(
                    pushConstants.baseColor.x * (1.0f - fp) + waterVol->iceColor.x * fp,
                    pushConstants.baseColor.y * (1.0f - fp) + waterVol->iceColor.y * fp,
                    pushConstants.baseColor.z * (1.0f - fp) + waterVol->iceColor.z * fp
                );
                pushConstants.opacity = pushConstants.opacity * (1.0f - fp) + waterVol->iceOpacity * fp;
            } else if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) {
                auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
                pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE for Water3D
                pushConstants.parallaxScale = 0.0f; // no freeze — shader reads this as freezeProgress
                if (water3d) {
                    pushConstants.baseColor = water3d->settings.shallowColor;
                    pushConstants.opacity = water3d->settings.opacity;
                }
            }

            // Rasterize text texture if entity has a TextComponent (cached storage)
            TextComponent* textComp = m_CachedTextStorage ? m_CachedTextStorage->Get(entity) : nullptr;
            if (textComp && textComp->dirty && !textComp->fontPath.empty() && !textComp->text.empty()) {
                auto pixels = m_TextRasterizer.Rasterize(*textComp);
                if (!pixels.empty()) {
                    auto textTex = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
                    if (textTex->CreateFromData(pixels.data(), textComp->textureWidth, textComp->textureHeight, 4)) {
                        m_TextTextureCache[entity] = textTex;
                    }
                }
                textComp->dirty = false;
            }

            // If entity has a text texture, override the base color texture
            auto textTexIt = m_TextTextureCache.find(entity);
            if (textComp && textTexIt != m_TextTextureCache.end() && textTexIt->second && textTexIt->second->IsValid()) {
                boundTexture = textTexIt->second.get();
                pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
            }

            // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 6)
            UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive, texMatcap);

            // Upload bone matrices for skinned meshes.
            // Set FLAG_SKINNED whenever the entity has valid skinning matrices —
            // not just when an animation is playing. Imported FBX models start in
            // bind pose (not playing), but their vertices are in bone-local space
            // and MUST be transformed by the bind pose skinning matrices to look
            // correct. Without FLAG_SKINNED, raw bone-local vertices render as a
            // distorted mess (the Mixamo import bug).
            // Find AnimatorComponent — may be on this entity or anywhere in the
            // scene (Mixamo FBX puts mesh and skeleton on different entities).
            // If this entity has bone weights, find the first AnimatorComponent
            // in the world to use its skinning matrices.
            AnimatorComponent* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
            if (!animComp && renderData.indexCount > 0) {
                // This entity has bone weights but no animator — use cached fallback
                // (computed once per frame in Update, not per-entity)
                animComp = m_CachedFallbackAnimator;
            }

            // Upload skinning matrices. If this entity doesn't have its own bone
            // buffer but we found an animator in the hierarchy, create a temporary
            // bone buffer or use the animator entity's buffer.
            if (animComp && !renderData.boneBuffer) {
                // This mesh entity has bone weights but no bone buffer — create one
                // using the animator we found in the hierarchy.
                usize boneCount = animComp->animator.GetSkeleton()
                    ? animComp->animator.GetSkeleton()->bones.size() : 0;
                if (boneCount > 0) {
                    renderData.boneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
                    if (!renderData.boneBuffer->Create(boneCount * sizeof(Math::Matrix4),
                                                        Renderer::BufferUsage::Storage, true)) {
                        renderData.boneBuffer.reset();
                    }
                }
            }

            if (animComp && renderData.boneBuffer) {
                const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
                if (!skinningMatrices.empty()) {
                    renderData.boneBuffer->UploadData(skinningMatrices.data(),
                        skinningMatrices.size() * sizeof(Math::Matrix4));
                    UpdateBoneDescriptor(renderData.boneBuffer.get());
                    pushConstants.flags |= (1 << 3); // FLAG_SKINNED
                }
            } else {
                if (m_DefaultBoneBuffer) {
                    UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
                }
            }

            // Morph target SSBO upload + descriptor binding
            auto* morphComp = m_World->GetComponent<MorphTargetComponent>(entity);
            if (morphComp && !morphComp->targets.empty()) {
                UploadMorphTargetSSBO(entity, *morphComp, renderData);
                if (renderData.morphBuffer) {
                    UpdateMorphDescriptor(renderData.morphBuffer.get());
                }
            } else {
                if (m_DefaultMorphBuffer) {
                    UpdateMorphDescriptor(m_DefaultMorphBuffer.get());
                }
            }

            // Multi-material sub-mesh rendering: if entity has MaterialSlotsComponent
            // and the mesh has sub-meshes, draw each sub-mesh with its own material.
            MeshComponent* meshForSubMesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
            MaterialSlotsComponent* matSlots = m_CachedMaterialSlotsStorage ? m_CachedMaterialSlotsStorage->Get(entity) : nullptr;
            bool useSubMeshes = meshForSubMesh && meshForSubMesh->HasSubMeshes() && matSlots && !matSlots->slots.empty();

            bool poolPath = renderData.poolAlloc.valid && m_GeometryPool;
            if (useSubMeshes) {
                // Bind vertex/index buffers once for all sub-meshes
                if (poolPath) {
                    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
                } else if (renderData.vertexBuffer && renderData.indexBuffer) {
                    VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
                    VkDeviceSize vbOffsets[] = { 0 };
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vbOffsets);
                    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    m_GeometryPoolBound = false;
                } else {
                    useSubMeshes = false;  // No valid buffers, fall through to single-material path
                }
            }

            if (useSubMeshes) {
                // Total index count for bounds validation
                u32 totalIndexCount = poolPath ? renderData.poolAlloc.indexCount : renderData.indexCount;

                for (const auto& subMesh : meshForSubMesh->subMeshes) {
                    // Validate sub-mesh bounds: skip if index range exceeds buffer
                    if (subMesh.indexCount == 0) continue;
                    if (subMesh.indexOffset + subMesh.indexCount > totalIndexCount) continue;

                    // Validate material slot index
                    if (subMesh.materialSlot < 0 || subMesh.materialSlot >= static_cast<i32>(matSlots->slots.size())) continue;

                    MaterialComponent* slotMat = matSlots->GetSlot(subMesh.materialSlot);
                    if (!slotMat) continue;

                    // Build push constants for this sub-mesh's material
                    Renderer::PushConstants subPC = pushConstants;  // Copy shared state (model, flags base)
                    subPC.baseColor = slotMat->baseColor;
                    subPC.metallic = slotMat->metallic;
                    subPC.emissiveColor = slotMat->emissiveColor;
                    subPC.roughness = slotMat->roughness;
                    subPC.emissiveStrength = slotMat->emissiveStrength;
                    subPC.opacity = slotMat->opacity;
                    subPC.alphaCutoff = slotMat->alphaCutoff;
                    subPC.parallaxScale = slotMat->parallaxScale;

                    // Rebuild flags from this slot's material
                    subPC.flags = pushConstants.flags & ((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 11)); // Preserve skinned, wind, water flags
                    if (slotMat->doubleSided) subPC.flags |= 1;
                    if (slotMat->castShadows) subPC.flags |= 2;
                    if (slotMat->receiveShadows) subPC.flags |= 4;
                    subPC.flags |= (static_cast<i32>(slotMat->alphaMode) << 8);
                    if (slotMat->flatShading) subPC.flags |= (1 << 20);
                    if (slotMat->affineTexturing) subPC.flags |= (1 << 21);
                    if (slotMat->vertexSnapping) subPC.flags |= (1 << 22);
                    if (slotMat->stippleTransparency) subPC.flags |= (1 << 23);
                    if (slotMat->uvQuantize) subPC.flags |= (1 << 12);
                    if (slotMat->gouraudOnly) subPC.flags |= (1 << 13);
                    subPC.flags |= (static_cast<i32>(slotMat->shadowDitherMode & 0x3) << 14);
                    subPC.flags |= (static_cast<i32>((slotMat->vertexSnapResolution / 8) & 0x1F) << 24);
                    subPC.flags |= (static_cast<i32>(slotMat->shadowDitherPattern & 0x7) << 29);

                    // Resolve textures for this slot's material
                    Renderer::Texture* subBoundTex = nullptr;
                    Renderer::Texture* subTexHeight = nullptr;
                    Renderer::Texture* subTexNormal = nullptr;
                    Renderer::Texture* subTexMR = nullptr;
                    Renderer::Texture* subTexEmissive = nullptr;
                    Renderer::Texture* subTexMatcap = nullptr;
                    if (slotMat->textureCacheDirty) {
                        if (!slotMat->baseColorTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->baseColorTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedBaseColorTexture = tex.get(); slotMat->baseColorTexture = 1; }
                        }
                        if (!slotMat->heightTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->heightTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedHeightTexture = tex.get(); slotMat->heightTexture = 1; }
                        }
                        if (!slotMat->normalTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->normalTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedNormalTexture = tex.get(); slotMat->normalTexture = 1; }
                        }
                        if (!slotMat->metallicRoughnessTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->metallicRoughnessTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedMetallicRoughnessTexture = tex.get(); slotMat->metallicRoughnessTexture = 1; }
                        }
                        if (!slotMat->specularTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->specularTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedMetallicRoughnessTexture = tex.get(); slotMat->metallicRoughnessTexture = 1; }
                        }
                        if (!slotMat->emissiveTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->emissiveTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedEmissiveTexture = tex.get(); slotMat->emissiveTexture = 1; }
                        }
                        if (!slotMat->matcapTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->matcapTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedMatcapTexture = tex.get(); slotMat->matcapTexture = 1; }
                        }
                        slotMat->textureCacheDirty = false;
                        slotMat->cachedTextureKey = { slotMat->cachedBaseColorTexture,
                            slotMat->cachedHeightTexture, slotMat->cachedNormalTexture,
                            slotMat->cachedMetallicRoughnessTexture, slotMat->cachedEmissiveTexture,
                            slotMat->cachedMatcapTexture };
                    }
                    subBoundTex = slotMat->cachedBaseColorTexture;
                    subTexHeight = slotMat->cachedHeightTexture;
                    subTexNormal = slotMat->cachedNormalTexture;
                    subTexMR = slotMat->cachedMetallicRoughnessTexture;
                    subTexEmissive = slotMat->cachedEmissiveTexture;
                    subTexMatcap = slotMat->cachedMatcapTexture;

                    // Set texture flags in push constants
                    if (subBoundTex) subPC.flags |= (1 << 16);
                    if (slotMat->normalTexture >= 0) subPC.flags |= (1 << 17);
                    if (slotMat->metallicRoughnessTexture >= 0) subPC.flags |= (1 << 18);
                    if (slotMat->emissiveTexture >= 0) subPC.flags |= (1 << 19);
                    if (slotMat->heightTexture >= 0) subPC.flags |= (1 << 10);

                    // Global retro overrides
                    if (m_GlobalFlatShading) subPC.flags |= (1 << 20);
                    if (m_GlobalAffineTexturing) subPC.flags |= (1 << 21);
                    if (m_GlobalVertexSnapping) subPC.flags |= (1 << 22);
                    if (m_GlobalStippleTransparency) subPC.flags |= (1 << 23);
                    if (m_GlobalUVQuantize) subPC.flags |= (1 << 12);
                    if (m_GlobalGouraudOnly) subPC.flags |= (1 << 13);

                    UpdateEntityTextureDescriptors(subBoundTex, subTexHeight, subTexNormal, subTexMR, subTexEmissive, subTexMatcap);

                    vkCmdPushConstants(commandBuffer, targetPipeline->GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof(Renderer::PushConstants), &subPC);

                    // Draw this sub-mesh range
                    if (poolPath) {
                        vkCmdDrawIndexed(commandBuffer, subMesh.indexCount, 1,
                                         renderData.poolAlloc.indexOffset + subMesh.indexOffset,
                                         renderData.poolAlloc.vertexOffset, 0);
                    } else {
                        vkCmdDrawIndexed(commandBuffer, subMesh.indexCount, 1, subMesh.indexOffset, 0, 0);
                    }
                    m_DrawCallCount++;
                    m_TriangleCount += subMesh.indexCount / 3;
                }
            } else {
                // Single-material path (original behavior)
                vkCmdPushConstants(commandBuffer, targetPipeline->GetLayout(),
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                    sizeof(Renderer::PushConstants), &pushConstants);

                if (renderData.poolAlloc.valid && m_GeometryPool) {
                    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
                    vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                                     renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
                } else if (renderData.vertexBuffer && renderData.indexBuffer) {
                    VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
                    m_GeometryPoolBound = false;
                }
                m_DrawCallCount++;
                m_TriangleCount += renderData.indexCount / 3;
            }
        }
    }

    // Geometry outline pass (inverted-hull backface extrusion, after main geometry)
    RenderOutlinePassForTarget();

    // Sorted 2D sprite rendering pass (after 3D geometry)
    RenderSprites();

    // Render effect passes (grass, shrubs, trees, particles, fluid)
    // Pass render target dimensions so vegetation renderers use the correct viewport
    u32 targetW = target->GetWidth();
    u32 targetH = target->GetHeight();
    RenderGrass(targetW, targetH);
    RenderShrubs(targetW, targetH);
    RenderTrees(targetW, targetH);
    RenderParticles(targetW, targetH);
    RenderFluid(targetW, targetH);

    // Restore main pass camera, buffers, and descriptor sets
    m_Camera = prevCamera;
    m_ActiveDescriptorSets = &m_DescriptorSets;
    m_ActiveUniformBuffers = &m_UniformBuffers;
    m_ActiveLightingBuffers = &m_LightingBuffers;
    m_OffscreenMode = false;
    m_CurrentViewportIndex = 0;
}

void RenderSystem::RenderSplitscreen(Renderer::RenderTarget* target, const std::vector<ViewportCamera>& viewports) {
    if (!target || !target->IsValid() || viewports.empty() || !m_Renderer || !m_Initialized || !m_Pipeline) {
        return;
    }

    u32 viewportCount = static_cast<u32>(viewports.size());
    if (viewportCount > MAX_SPLITSCREEN_VIEWPORTS) {
        viewportCount = MAX_SPLITSCREEN_VIEWPORTS;
    }

    Renderer::Camera* prevCamera = m_Camera;

    // Switch to offscreen buffers
    m_ActiveDescriptorSets = &m_OffscreenDescriptorSets;
    m_ActiveUniformBuffers = &m_OffscreenUniformBuffers;
    m_ActiveLightingBuffers = &m_OffscreenLightingBuffers;
    m_OffscreenMode = true;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        m_Camera = prevCamera;
        m_ActiveDescriptorSets = &m_DescriptorSets;
        m_ActiveUniformBuffers = &m_UniformBuffers;
        m_ActiveLightingBuffers = &m_LightingBuffers;
        m_OffscreenMode = false;
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    u32 targetW = target->GetWidth();
    u32 targetH = target->GetHeight();

    for (u32 v = 0; v < viewportCount; ++v) {
        const ViewportCamera& vc = viewports[v];
        m_CurrentViewportIndex = v;

        // Build a Camera from the entity's CameraComponent + TransformComponent
        auto* cameraComp = m_World->GetComponent<CameraComponent>(vc.entity);
        auto* cameraTransform = m_World->GetComponent<TransformComponent>(vc.entity);
        if (!cameraComp || !cameraTransform) continue;

        Renderer::Camera viewCamera;
        f32 pixelW = vc.viewportWidth * static_cast<f32>(targetW);
        f32 pixelH = vc.viewportHeight * static_cast<f32>(targetH);
        f32 aspect = (pixelH > 0.0f) ? (pixelW / pixelH) : 1.0f;

        if (cameraComp->projectionType == ProjectionType::Perspective) {
            viewCamera.SetPerspective(cameraComp->fieldOfView, aspect,
                                       cameraComp->nearPlane, cameraComp->farPlane);
        } else {
            f32 halfH = cameraComp->orthoSize;
            f32 halfW = halfH * aspect;
            viewCamera.SetOrthographic(-halfW, halfW, -halfH, halfH,
                                        cameraComp->nearPlane, cameraComp->farPlane);
        }

        viewCamera.SetPosition(cameraTransform->position);
        Math::Vector3 forward = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
        Math::Vector3 up = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
        Math::Vector3 lookTarget = cameraTransform->position + forward;
        viewCamera.SetLookAt(cameraTransform->position, lookTarget, up);

        m_Camera = &viewCamera;

        // Upload frame-level uniforms to this viewport's offscreen buffers
        UpdateFrameUniforms();
        BuildMaterialSSBO();

        // Compute pixel viewport rect
        VkViewport vkViewport{};
        vkViewport.x = vc.viewportX * static_cast<f32>(targetW);
        vkViewport.y = vc.viewportY * static_cast<f32>(targetH);
        vkViewport.width = pixelW;
        vkViewport.height = pixelH;
        vkViewport.minDepth = 0.0f;
        vkViewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);

        VkRect2D scissor{};
        scissor.offset = { static_cast<i32>(vkViewport.x), static_cast<i32>(vkViewport.y) };
        scissor.extent = { static_cast<u32>(pixelW), static_cast<u32>(pixelH) };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Render skybox for this viewport
        RenderSkybox(commandBuffer, &vkViewport, &scissor);

        // Reset descriptor cache for each viewport
        m_LastBound.Reset(); m_GeometryPoolBound = false;

        // Use offscreen pipeline (created for offscreen UNORM render pass)
        auto* ssPipeline = m_OffscreenPipeline ? m_OffscreenPipeline.get() : m_Pipeline.get();

        // Bind pipeline, descriptor set, viewport, and scissor once per viewport
        ssPipeline->Bind(commandBuffer);
        {
            u32 zeroOff = 0;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                ssPipeline->GetLayout(), 0, 1,
                &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
        }
        vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Render all entities using sorted render list (skip sprites — drawn in sorted pass)
        // Cache sprite storage pointer outside the loop to avoid per-entity type-ID hash
        auto* spriteStorageSS = m_World->GetComponentStorage<Sprite2DComponent>();
        for (Entity entity : m_SortedRenderList) {
            // Skip invisible entities or entities without transform (cached storage)
            {
                auto* xformSS = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                if (!xformSS || !xformSS->visible) continue;
            }

            // Skip 2D sprites — rendered in sorted pass after 3D geometry
            if (spriteStorageSS && spriteStorageSS->Has(entity)) continue;

            EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
                ? &m_EntityRenderData[static_cast<usize>(entity)] : SetupEntityBuffers(entity);
            if (!pRD) continue;
            EntityRenderData& renderData = *pRD;

            // Bind material SSBO at this entity's dynamic offset
            {
                u32 matIdx = GetMaterialIndex(entity);
                u32 dynOffset = matIdx * m_MaterialSSBOStride;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ssPipeline->GetLayout(), 0, 1,
                    &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &dynOffset);
            }

            // Build push constants — skinned meshes use identity model matrix (cached storage)
            Renderer::PushConstants pushConstants{};
            {
                AnimatorComponent* ac = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
                pushConstants.model = (ac && ac->animator.GetSkeleton())
                    ? Math::Matrix4::Identity()
                    : ECS::ComputeWorldMatrix(m_World, entity);
            }

            MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
            Renderer::Texture* boundTexture = nullptr;
            Renderer::Texture* texHeight = nullptr;
            Renderer::Texture* texNormal = nullptr;
            Renderer::Texture* texMR = nullptr;
            Renderer::Texture* texEmissive = nullptr;
            Renderer::Texture* texMatcap = nullptr;

            if (material) {
                pushConstants.baseColor = material->baseColor;
                pushConstants.metallic = material->metallic;
                pushConstants.emissiveColor = material->emissiveColor;
                pushConstants.roughness = material->roughness;
                pushConstants.emissiveStrength = material->emissiveStrength;
                pushConstants.opacity = material->opacity;
                pushConstants.alphaCutoff = material->alphaCutoff;

                // Resolve textures using cache (avoids per-frame string hash lookups)
                if (material->textureCacheDirty) {
                    if (!material->baseColorTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->baseColorTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedBaseColorTexture = tex.get();
                            material->baseColorTexture = 1;
                        }
                    }
                    if (!material->heightTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->heightTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedHeightTexture = tex.get();
                            material->heightTexture = 1;
                        }
                    }
                    if (!material->normalTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->normalTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedNormalTexture = tex.get();
                            material->normalTexture = 1;
                        }
                    }
                    if (!material->metallicRoughnessTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->metallicRoughnessTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    // Specular map overrides metallic-roughness slot for pre-PBR shading
                    if (!material->specularTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->specularTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    if (!material->emissiveTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedEmissiveTexture = tex.get();
                            material->emissiveTexture = 1;
                        }
                    }
                    if (!material->matcapTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->matcapTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMatcapTexture = tex.get();
                            material->matcapTexture = 1;
                        }
                    }
                    material->textureCacheDirty = false;
                    material->cachedTextureKey = { material->cachedBaseColorTexture,
                        material->cachedHeightTexture, material->cachedNormalTexture,
                        material->cachedMetallicRoughnessTexture, material->cachedEmissiveTexture,
                        material->cachedMatcapTexture };
                }

                // Use cached texture pointers
                boundTexture = material->cachedBaseColorTexture;
                texHeight = material->cachedHeightTexture;
                texNormal = material->cachedNormalTexture;
                texMR = material->cachedMetallicRoughnessTexture;
                texEmissive = material->cachedEmissiveTexture;
                texMatcap = material->cachedMatcapTexture;

                pushConstants.flags = 0;
                if (material->doubleSided) pushConstants.flags |= 1;
                if (material->castShadows) pushConstants.flags |= 2;
                if (material->receiveShadows) pushConstants.flags |= 4;
                pushConstants.flags |= (static_cast<i32>(material->alphaMode) << 8);
                if (boundTexture != nullptr) pushConstants.flags |= (1 << 16);
                if (material->normalTexture >= 0) pushConstants.flags |= (1 << 17);
                if (material->metallicRoughnessTexture >= 0) pushConstants.flags |= (1 << 18);
                if (material->emissiveTexture >= 0) pushConstants.flags |= (1 << 19);
                if (material->heightTexture >= 0) pushConstants.flags |= (1 << 10);
                if (material->flatShading) pushConstants.flags |= (1 << 20);
                if (material->affineTexturing) pushConstants.flags |= (1 << 21);
                if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
                if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
                if (material->uvQuantize) pushConstants.flags |= (1 << 12);
                if (material->gouraudOnly) pushConstants.flags |= (1 << 13);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
                pushConstants.flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
                pushConstants.parallaxScale = material->parallaxScale;
                // Artistic surface params (reused push constant slots)
                pushConstants.surfaceParam1 = material->reflectivity;
                pushConstants.surfaceParam2 = material->fresnelPower;
                pushConstants.surfaceParam3 = material->rimLightStrength;
                // Dithered gradient: encode bands + pattern into surfaceParam1
                if (material->ditherGradient) {
                    pushConstants.flags |= (1 << 20); // Force flat shading
                    pushConstants.surfaceParam1 = 100.0f + static_cast<f32>(material->ditherGradientBands)
                        + static_cast<f32>(material->ditherGradientPattern) * 0.1f;
                }
                // Dithered transparency: encode pattern + opacity + blend color into surfaceParams
                if (material->ditherTransparency) {
                    pushConstants.surfaceParam1 = 200.0f + static_cast<f32>(material->ditherTransPattern);
                    pushConstants.surfaceParam2 = material->ditherTransOpacity;
                    u32 r = static_cast<u32>(material->ditherTransBlendColor.x * 1023.0f) & 0x3FF;
                    u32 g = static_cast<u32>(material->ditherTransBlendColor.y * 1023.0f) & 0x3FF;
                    u32 b = static_cast<u32>(material->ditherTransBlendColor.z * 1023.0f) & 0x3FF;
                    u32 packed = (r << 20) | (g << 10) | b;
                    pushConstants.surfaceParam3 = *reinterpret_cast<f32*>(&packed);
                }
                // Elemental surface effects: encode char/wet/snow/frost into surfaceParams
                if (!material->ditherGradient && !material->ditherTransparency) {
                    auto* elemSurface = m_World->GetComponent<ECS::ElementalSurfaceComponent>(entity);
                    if (elemSurface && (elemSurface->charAmount > 0.01f || elemSurface->wetness > 0.01f ||
                                        elemSurface->snowCoverage > 0.01f || elemSurface->frostAmount > 0.01f)) {
                        pushConstants.surfaceParam1 = 300.0f + elemSurface->charAmount;
                        pushConstants.surfaceParam2 = elemSurface->wetness + std::floor(elemSurface->snowCoverage * 256.0f);
                        pushConstants.surfaceParam3 = elemSurface->frostAmount;
                    }
                }
                // Procedural surface noise: encode scale/strength into surfaceParams (range 400+)
                // Only when no other effect has claimed the surfaceParam slots
                if (!material->ditherGradient && !material->ditherTransparency &&
                    material->surfaceNoiseScale > 0.0f && pushConstants.surfaceParam1 < 100.0f) {
                    pushConstants.surfaceParam1 = 400.0f + material->surfaceNoiseScale;
                    pushConstants.surfaceParam2 = material->surfaceNoiseStrength;
                }
            } else {
                pushConstants.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
                pushConstants.metallic = 0.0f;
                pushConstants.emissiveColor = Math::Vector3(0.0f);
                pushConstants.roughness = 0.5f;
                pushConstants.emissiveStrength = 0.0f;
                pushConstants.opacity = 1.0f;
                pushConstants.alphaCutoff = 0.5f;
                pushConstants.flags = 0;
                pushConstants.parallaxScale = 0.0f;
            }

            // Global retro overrides (OR with per-material — global forces on)
            if (m_GlobalFlatShading) pushConstants.flags |= (1 << 20);
            if (m_GlobalAffineTexturing) pushConstants.flags |= (1 << 21);
            if (m_GlobalVertexSnapping) pushConstants.flags |= (1 << 22);
            if (m_GlobalStippleTransparency) pushConstants.flags |= (1 << 23);
            if (m_GlobalUVQuantize) pushConstants.flags |= (1 << 12);
            if (m_GlobalGouraudOnly) pushConstants.flags |= (1 << 13);
            if (m_GlobalVertexSnapping && m_GlobalVertexSnapResolution > 0) {
                pushConstants.flags = (pushConstants.flags & ~(0x1F << 24)) | (static_cast<i32>((m_GlobalVertexSnapResolution / 8) & 0x1F) << 24);
            }

            if (m_World->HasComponent<VegetationComponent>(entity)) {
                pushConstants.flags |= (1 << 4);
            }

            WaterVolumeComponent* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
            if (waterVol) {
                pushConstants.flags |= (1 << 5);
                pushConstants.parallaxScale = waterVol->freezeProgress;
                if (m_RainActive && waterVol->freezeProgress < 0.5f) {
                    pushConstants.flags |= (1 << 6);
                }
                if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
                    pushConstants.flags |= (1 << 7);
                    pushConstants.surfaceParam1 = waterVol->shoreWidth;
                    pushConstants.surfaceParam2 = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
                    pushConstants.surfaceParam3 = waterVol->foamScale;
                }
                if (waterVol->waterType == WaterType::Ocean) {
                    pushConstants.flags |= (1 << 11);
                }
                f32 fp = waterVol->freezeProgress;
                pushConstants.baseColor = Math::Vector3(
                    pushConstants.baseColor.x * (1.0f - fp) + waterVol->iceColor.x * fp,
                    pushConstants.baseColor.y * (1.0f - fp) + waterVol->iceColor.y * fp,
                    pushConstants.baseColor.z * (1.0f - fp) + waterVol->iceColor.z * fp
                );
                pushConstants.opacity = pushConstants.opacity * (1.0f - fp) + waterVol->iceOpacity * fp;
            } else if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) {
                auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
                pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE for Water3D
                pushConstants.parallaxScale = 0.0f; // no freeze
                if (water3d) {
                    pushConstants.baseColor = water3d->settings.shallowColor;
                    pushConstants.opacity = water3d->settings.opacity;
                }
            }

            // Text rendering (cached storage)
            TextComponent* textComp = m_CachedTextStorage ? m_CachedTextStorage->Get(entity) : nullptr;
            if (textComp && textComp->dirty && !textComp->fontPath.empty() && !textComp->text.empty()) {
                auto pixels = m_TextRasterizer.Rasterize(*textComp);
                if (!pixels.empty()) {
                    auto textTex = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
                    if (textTex->CreateFromData(pixels.data(), textComp->textureWidth, textComp->textureHeight, 4)) {
                        m_TextTextureCache[entity] = textTex;
                    }
                }
                textComp->dirty = false;
            }
            auto textTexIt = m_TextTextureCache.find(entity);
            if (textComp && textTexIt != m_TextTextureCache.end() && textTexIt->second && textTexIt->second->IsValid()) {
                boundTexture = textTexIt->second.get();
                pushConstants.flags |= (1 << 16);
            }

            // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 6)
            UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive, texMatcap);

            // Upload bone matrices for skinned meshes (bind pose or animation, cached storage)
            AnimatorComponent* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
            if (animComp && renderData.boneBuffer) {
                const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
                if (!skinningMatrices.empty()) {
                    renderData.boneBuffer->UploadData(skinningMatrices.data(),
                        skinningMatrices.size() * sizeof(Math::Matrix4));
                    UpdateBoneDescriptor(renderData.boneBuffer.get());
                    pushConstants.flags |= (1 << 3);
                }
            } else {
                if (m_DefaultBoneBuffer) {
                    UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
                }
            }

            vkCmdPushConstants(commandBuffer, ssPipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                sizeof(Renderer::PushConstants), &pushConstants);

            if (renderData.poolAlloc.valid && m_GeometryPool) {
                if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
                vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                                 renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
            } else if (renderData.vertexBuffer && renderData.indexBuffer) {
                VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
                m_GeometryPoolBound = false;
            }
            m_DrawCallCount++;
            m_TriangleCount += renderData.indexCount / 3;
        }

        // Sorted 2D sprite rendering pass (after 3D geometry)
        RenderSprites();

        // Render effects for this viewport
        RenderGrass(targetW, targetH);
        RenderShrubs(targetW, targetH);
        RenderTrees(targetW, targetH);
        RenderParticles(targetW, targetH);
        RenderFluid(targetW, targetH);
    }

    // Restore main pass state
    m_Camera = prevCamera;
    m_ActiveDescriptorSets = &m_DescriptorSets;
    m_ActiveUniformBuffers = &m_UniformBuffers;
    m_ActiveLightingBuffers = &m_LightingBuffers;
    m_OffscreenMode = false;
    m_CurrentViewportIndex = 0;
}

void RenderSystem::OnEntityAdded(Entity entity) {
    SetupEntityBuffers(entity);

    // Invalidate scene composition cache (new entity may change 2D/3D classification)
    m_SceneComposition.dirty = true;

    // Invalidate material SSBO (new entity needs to be included in the buffer)
    m_MaterialSSBODirty = true;

    // Invalidate shadow caster cache (new entity may be a shadow caster)
    m_ShadowCastersDirty = true;

    // Invalidate light entity cache (new entity may have a LightComponent)
    if (m_World && m_World->HasComponent<LightComponent>(entity)) {
        m_LightListDirty = true;
    }

    // Cache player entity (first entity with any CharacterController)
    if (m_CachedPlayerEntity == INVALID_ENTITY && m_World) {
        bool hasController = m_World->HasComponent<ThirdPersonController>(entity) ||
                             m_World->HasComponent<FirstPersonController>(entity) ||
                             m_World->HasComponent<Platformer2DController>(entity) ||
                             m_World->HasComponent<TopDown2DController>(entity) ||
                             m_World->HasComponent<TopDown3DController>(entity);
        if (hasController) {
            m_CachedPlayerEntity = entity;
        }
    }
}

void RenderSystem::OnEntityRemoved(Entity entity) {
    // Invalidate this entity's BLAS cache entry BEFORE freeing the pool allocation.
    // Without this, freeing the pool region allows a future entity to reuse the same
    // offsets, producing an identical address-based mesh hash that returns a stale BLAS
    // built for the old (now-freed) geometry — causing a GPU crash on ray trace dispatch.
    if (m_ASManager && m_RTEnabled &&
        static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid) {
        const auto& rd = m_EntityRenderData[static_cast<usize>(entity)];
        VkDeviceAddress vertAddr = 0, idxAddr = 0;
        if (rd.vertexBuffer && rd.indexBuffer) {
            vertAddr = rd.vertexBuffer->GetDeviceAddress();
            idxAddr = rd.indexBuffer->GetDeviceAddress();
        } else if (rd.poolAlloc.valid && m_GeometryPool &&
                   m_GeometryPool->GetVertexBuffer() && m_GeometryPool->GetIndexBuffer()) {
            vertAddr = m_GeometryPool->GetVertexBuffer()->GetDeviceAddress()
                     + static_cast<VkDeviceAddress>(rd.poolAlloc.vertexOffset) * sizeof(MeshComponent::Vertex);
            idxAddr = m_GeometryPool->GetIndexBuffer()->GetDeviceAddress()
                    + static_cast<VkDeviceAddress>(rd.poolAlloc.indexOffset) * sizeof(u32);
        }
        if (vertAddr != 0 && idxAddr != 0) {
            u64 meshHash = vertAddr ^ (idxAddr << 32) ^ (idxAddr >> 32);
            m_ASManager->InvalidateMesh(meshHash);
        }
    }

    // Free merged geometry pool allocation before erasing render data
    if (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid) {
        auto& rd = m_EntityRenderData[static_cast<usize>(entity)];
        if (rd.poolAlloc.valid && m_GeometryPool) {
            m_GeometryPool->Free(rd.poolAlloc);
        }
        rd.Invalidate();
    }
    m_TextTextureCache.erase(entity);
    m_PrevModelMatrices.erase(static_cast<u64>(entity));

    // Invalidate scene composition cache (removed entity may change 2D/3D classification)
    m_SceneComposition.dirty = true;

    // Invalidate material SSBO (removed entity changes the buffer layout)
    m_MaterialSSBODirty = true;

    // Invalidate shadow caster cache (removed entity may have been a shadow caster)
    m_ShadowCastersDirty = true;

    // Invalidate light entity cache (removed entity may have had a LightComponent)
    if (m_World && m_World->HasComponent<LightComponent>(entity)) {
        m_LightListDirty = true;
    }

    // Invalidate cached player entity — will be re-discovered lazily
    if (entity == m_CachedPlayerEntity) {
        m_CachedPlayerEntity = INVALID_ENTITY;
    }
}

void RenderSystem::ClassifySceneComposition() {
    if (!m_SceneComposition.dirty || !m_World) return;

    m_SceneComposition.spriteCount = 0;
    m_SceneComposition.tilemapCount = 0;
    m_SceneComposition.mesh3DCount = 0;
    m_SceneComposition.hasShadowCastingLights = false;

    // Count sprites and tilemaps using direct container size (avoids iteration)
    m_SceneComposition.spriteCount = static_cast<u32>(m_World->GetEntitiesWithComponent<Sprite2DComponent>().size());
    m_SceneComposition.tilemapCount = static_cast<u32>(m_World->GetEntitiesWithComponent<TilemapComponent>().size());

    // Count 3D meshes: total MeshComponent entities minus sprites and tilemaps
    // (sprites and tilemaps also have MeshComponent, so subtract them)
    {
        u32 totalMesh = static_cast<u32>(m_World->GetEntitiesWithComponent<MeshComponent>().size());
        m_SceneComposition.mesh3DCount = (totalMesh > m_SceneComposition.spriteCount + m_SceneComposition.tilemapCount)
            ? totalMesh - m_SceneComposition.spriteCount - m_SceneComposition.tilemapCount : 0;
    }

    // Check for shadow-casting directional lights and any lights at all
    bool hasAnyLights = !m_CachedLightEntities.empty();
    for (Entity entity : m_CachedLightEntities) {
        auto* light = m_World->GetComponent<LightComponent>(entity);
        if (light && light->type == LightType::Directional && light->castShadows) {
            m_SceneComposition.hasShadowCastingLights = true;
            break;
        }
    }

    // Classify scene mode
    // Scene3D: 3D meshes present — full pipeline (shadows, lighting, normal maps)
    // Scene2_5D: sprites only but lights exist — skip shadows, populate full lighting UBO
    // Scene2D: sprites only, no lights — minimal UBO (ambient/fog only)
    if (m_SceneComposition.mesh3DCount > 0) {
        m_SceneComposition.mode = SceneRenderMode::Scene3D;
    } else if (hasAnyLights) {
        m_SceneComposition.mode = SceneRenderMode::Scene2_5D;
    } else {
        m_SceneComposition.mode = SceneRenderMode::Scene2D;
    }

    m_SceneComposition.dirty = false;

    // Diagnostic warnings (every 300 frames to avoid log spam)
    if (++m_DiagnosticFrameCounter >= 300) {
        m_DiagnosticFrameCounter = 0;

        // Warn if many unbatched sprites
        if (m_SceneComposition.spriteCount > 100 && !m_SpriteBatchRenderer) {
            ENJIN_LOG_WARN(Renderer, "%u sprites without batching - consider enabling SpriteBatchRenderer",
                m_SceneComposition.spriteCount);
        }

        // Log mixed 2D/3D scene info for debugging
        if (m_SceneComposition.spriteCount > 0 && m_SceneComposition.mesh3DCount > 0) {
            ENJIN_LOG_INFO(Renderer, "Mixed 2D/3D scene: %u sprites, %u meshes, %u shadow casters",
                m_SceneComposition.spriteCount, m_SceneComposition.mesh3DCount,
                static_cast<u32>(m_ShadowCasters.size()));
        }
    }
}

void RenderSystem::RebuildShadowCasterCache() {
    m_ShadowCasters.clear();
    if (!m_World) {
        m_ShadowCastersDirty = false;
        return;
    }

    // Reserve approximate capacity based on mesh count
    m_ShadowCasters.reserve(m_SceneComposition.mesh3DCount > 0 ? m_SceneComposition.mesh3DCount : 64);

    // Cache storage pointers to avoid per-entity type-ID hash lookups
    auto* spriteStorageSC = m_World->GetComponentStorage<Sprite2DComponent>();
    auto* tilemapStorageSC = m_World->GetComponentStorage<TilemapComponent>();

    // Iterate only entities with MeshComponent
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        // Skip entities without transform
        auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xform) continue;

        // Skip 2D sprites — they never cast shadows
        if (spriteStorageSC && spriteStorageSC->Has(entity)) continue;

        // Skip tilemaps
        if (tilemapStorageSC && tilemapStorageSC->Has(entity)) continue;

        // Skip invisible entities
        if (!xform->visible) continue;

        // Check if material casts shadows (default: yes)
        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material && !material->castShadows) continue;

        m_ShadowCasters.push_back(entity);
    }

    m_ShadowCastersDirty = false;
}

void RenderSystem::BuildCullableObjectList() {
    m_CullableObjects.clear();
    m_EntityToCullIndex.clear();

    if (!m_World || !m_GPUCulling) return;

    // Reserve capacity
    usize meshCount = m_SceneComposition.mesh3DCount;
    if (meshCount == 0) return;

    m_CullableObjects.reserve(meshCount);
    m_EntityToCullIndex.resize(m_World->GetEntityCount(), UINT32_MAX);

    // Track which entities are drawn via indirect (pool-eligible, non-textured or textured-batched)
    m_IndirectDrawn.clear();
    m_IndirectDrawn.resize(m_World->GetEntityCount(), false);

    // Reset texture-grouped indirect draw batcher for this frame
    if (m_IndirectDrawBatcher) m_IndirectDrawBatcher->Reset();

    u32 cullIndex = 0;

    // Cache storage pointers to avoid per-entity type-ID hash lookups
    auto* spriteStorageBCO = m_World->GetComponentStorage<Sprite2DComponent>();
    auto* tilemapStorageBCO = m_World->GetComponentStorage<TilemapComponent>();

    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xform || !xform->visible) continue;
        if (spriteStorageBCO && spriteStorageBCO->Has(entity)) continue;
        if (tilemapStorageBCO && tilemapStorageBCO->Has(entity)) continue;

        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!mesh || !mesh->IsValid()) continue;

        // Compute AABB from mesh vertices (cached on MeshComponent to avoid per-frame recomputation)
        if (mesh->aabbDirty) {
            Math::Vector3 bMin(1e30f, 1e30f, 1e30f);
            Math::Vector3 bMax(-1e30f, -1e30f, -1e30f);
            for (const auto& vertex : mesh->vertices) {
                bMin.x = Math::Min(bMin.x, vertex.position.x);
                bMin.y = Math::Min(bMin.y, vertex.position.y);
                bMin.z = Math::Min(bMin.z, vertex.position.z);
                bMax.x = Math::Max(bMax.x, vertex.position.x);
                bMax.y = Math::Max(bMax.y, vertex.position.y);
                bMax.z = Math::Max(bMax.z, vertex.position.z);
            }
            if (bMin.x > bMax.x) {
                bMin = Math::Vector3(-0.5f);
                bMax = Math::Vector3(0.5f);
            }
            mesh->cachedAABBMin = bMin;
            mesh->cachedAABBMax = bMax;
            mesh->aabbDirty = false;
        }

        Renderer::BoundingBox bounds;
        bounds.min = mesh->cachedAABBMin;
        bounds.max = mesh->cachedAABBMax;

        Renderer::CullableObject obj;
        obj.SetBounds(bounds);
        obj.transform = ECS::ComputeWorldMatrix(m_World, entity);
        obj.meshIndex = static_cast<u32>(entity); // Use entity ID as mesh index for now
        obj.indexCount = static_cast<u32>(mesh->indices.size());

        // Use pool offsets if entity has a merged geometry allocation
        bool hasPoolAlloc = false;
        if (static_cast<usize>(entity) < m_EntityRenderData.size() &&
            m_EntityRenderData[static_cast<usize>(entity)].valid &&
            m_EntityRenderData[static_cast<usize>(entity)].poolAlloc.valid) {
            obj.indexOffset = m_EntityRenderData[static_cast<usize>(entity)].poolAlloc.indexOffset;
            obj.vertexOffset = m_EntityRenderData[static_cast<usize>(entity)].poolAlloc.vertexOffset;
            hasPoolAlloc = true;
        } else {
            obj.indexOffset = 0;
            obj.vertexOffset = 0;
        }

        // Mark entity for indirect draw if pool-eligible and has no per-entity textures.
        // Entities with textures (flags bits 16-19) need per-entity descriptor updates
        // and must remain on the per-entity draw path. The indirectEligible flag on the
        // CullableObject controls whether the GPU cull shader emits an indirect draw command.
        if (hasPoolAlloc) {
            auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
            bool hasTextures = false;
            if (material) {
                hasTextures = material->cachedBaseColorTexture != nullptr
                    || material->cachedNormalTexture != nullptr
                    || material->cachedMetallicRoughnessTexture != nullptr
                    || material->cachedEmissiveTexture != nullptr
                    || material->cachedMatcapTexture != nullptr;
            }
            if (!hasTextures) {
                // Non-textured: GPU culling emits indirect draw commands directly
                obj.indirectEligible = 1;
                if (static_cast<usize>(entity) < m_IndirectDrawn.size()) {
                    m_IndirectDrawn[static_cast<usize>(entity)] = true;
                }
            } else if (m_IndirectDrawBatcher && material) {
                // Textured pool entity: add to texture-grouped indirect draw batcher.
                // The batcher groups entities by texture set hash and issues one
                // vkCmdDrawIndexedIndirect per group instead of per entity.
                material->ComputeSortKey(0.0f);
                u64 texHash = (material->cachedSortKey >> 16) & 0xFFFFFFFFFFULL; // 40 texture bits
                m_IndirectDrawBatcher->AddEntity(
                    static_cast<u32>(entity), texHash,
                    obj.indexCount, obj.indexOffset, obj.vertexOffset,
                    cullIndex);
                if (static_cast<usize>(entity) < m_IndirectDrawn.size()) {
                    m_IndirectDrawn[static_cast<usize>(entity)] = true;
                }
            }
        }

        // Map entity to cull index
        if (static_cast<usize>(entity) < m_EntityToCullIndex.size()) {
            m_EntityToCullIndex[static_cast<usize>(entity)] = cullIndex;
        }

        m_CullableObjects.push_back(obj);
        cullIndex++;
    }

    // Build texture-grouped indirect draw batches from accumulated textured entities
    if (m_IndirectDrawBatcher) {
        if (m_IndirectDrawBatcher->BuildBatches()) {
            m_IndirectDrawBatcher->UploadCommands();
        }
    }
}

void RenderSystem::PerformGPUCulling() {
    if (!m_GPUCulling || !m_GPUCullingEnabled || !m_Camera) return;
    if (m_CullableObjects.empty()) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Submit objects for culling
    m_GPUCulling->SubmitObjects(m_CullableObjects);

    VkBuffer indirectBuffer;
    u32 drawCount;

    // Use two-phase HiZ occlusion culling when Hi-Z pyramid is available
    if (m_HiZPyramid && m_GPUCulling->HasHiZ()) {
        if (m_GPUCulling->ExecuteTwoPhase(
                m_Camera->GetViewMatrix(),
                m_Camera->GetProjectionMatrix(),
                commandBuffer,
                indirectBuffer,
                drawCount)) {
            auto stats = m_GPUCulling->GetStats();
            (void)stats;
            return;
        }
    }

    // Fallback to single-phase culling
    if (m_GPUCulling->ExecuteCulling(
            m_Camera->GetViewMatrix(),
            m_Camera->GetProjectionMatrix(),
            commandBuffer,
            indirectBuffer,
            drawCount)) {
        auto stats = m_GPUCulling->GetStats();
        (void)stats;
    }
}

void RenderSystem::PerformGPUCullingAsync() {
    if (!m_GPUCulling || !m_GPUCullingEnabled || !m_Camera) return;
    if (m_CullableObjects.empty()) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentComputeCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    m_GPUCulling->SubmitObjects(m_CullableObjects);

    VkBuffer indirectBuffer;
    u32 drawCount;
    m_GPUCulling->ExecuteCulling(
        m_Camera->GetViewMatrix(),
        m_Camera->GetProjectionMatrix(),
        commandBuffer,
        indirectBuffer,
        drawCount);
}

void RenderSystem::UploadObjectData() {
    if (!m_GPUCulling || m_CullableObjects.empty()) return;

    // Build ObjectData array matching cullable objects 1:1
    m_ObjectDataCPU.resize(m_CullableObjects.size());

    // Cache storage pointers to avoid per-entity type-ID hash lookups
    auto* spriteStorageUOD = m_World->GetComponentStorage<Sprite2DComponent>();
    auto* tilemapStorageUOD = m_World->GetComponentStorage<TilemapComponent>();

    u32 idx = 0;
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xform || !xform->visible) continue;
        if (spriteStorageUOD && spriteStorageUOD->Has(entity)) continue;
        if (tilemapStorageUOD && tilemapStorageUOD->Has(entity)) continue;

        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!mesh || !mesh->IsValid()) continue;

        if (idx >= m_ObjectDataCPU.size()) break;

        ObjectDataGPU& obj = m_ObjectDataCPU[idx];
        obj.model = ECS::ComputeWorldMatrix(m_World, entity);

        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material) {
            obj.baseColor = material->baseColor;
            obj.metallic = material->metallic;
            obj.emissiveColor = material->emissiveColor;
            obj.roughness = material->roughness;
            obj.emissiveStrength = material->emissiveStrength;
            obj.opacity = material->opacity;
            obj.alphaCutoff = material->alphaCutoff;

            i32 flags = 0;
            if (material->doubleSided) flags |= 1;
            if (material->castShadows) flags |= 2;
            if (material->receiveShadows) flags |= 4;
            flags |= (static_cast<i32>(material->alphaMode) << 8);
            if (material->baseColorTexture >= 0) flags |= (1 << 16);
            if (material->normalTexture >= 0) flags |= (1 << 17);
            if (material->metallicRoughnessTexture >= 0) flags |= (1 << 18);
            if (material->emissiveTexture >= 0) flags |= (1 << 19);
            if (material->heightTexture >= 0) flags |= (1 << 10);
            if (material->flatShading) flags |= (1 << 20);
            if (material->affineTexturing) flags |= (1 << 21);
            if (material->vertexSnapping) flags |= (1 << 22);
            if (material->stippleTransparency) flags |= (1 << 23);
            if (material->uvQuantize) flags |= (1 << 12);
            if (material->gouraudOnly) flags |= (1 << 13);
            flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
            flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
            flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
            obj.flags = flags;
            obj.parallaxScale = material->parallaxScale;
        } else {
            obj.baseColor = Math::Vector3(1.0f);
            obj.metallic = 0.0f;
            obj.emissiveColor = Math::Vector3(0.0f);
            obj.roughness = 0.5f;
            obj.emissiveStrength = 0.0f;
            obj.opacity = 1.0f;
            obj.alphaCutoff = 0.5f;
            obj.flags = 0;
            obj.parallaxScale = 0.0f;
        }

        if (m_GlobalFlatShading) obj.flags |= (1 << 20);

        // Populate previous-frame model matrix for motion vector computation.
        // If no previous matrix exists (first frame for this entity), use current
        // model and mark as teleported so the shader outputs zero velocity.
        u64 entityId = static_cast<u64>(entity);
        auto prevIt = m_PrevModelMatrices.find(entityId);
        if (prevIt != m_PrevModelMatrices.end()) {
            obj.prevModel = prevIt->second;
            obj.teleported = 0;
        } else {
            obj.prevModel = obj.model;
            obj.teleported = 1;
        }

        // Network teleport detection: if the transform was flagged as teleported
        // this frame (large position snap, spawn, or respawn), force teleported = 1
        // so the shader zeroes the motion vector and TAA doesn't ghost.
        if (xform->teleportedThisFrame) {
            obj.teleported = 1;
            obj.prevModel = obj.model;  // Ensure prevModel matches current to zero velocity
            xform->teleportedThisFrame = false;  // Consume the flag (once per frame)
        }

        // Store current model matrix for next frame's previous-model lookup (eliminates second pass)
        m_PrevModelMatrices[entityId] = obj.model;

        obj._pad[0] = obj._pad[1] = 0.0f;
        idx++;
    }

    // Upload to GPU via GPUCulling's ObjectData buffer
    usize uploadSize = idx * sizeof(ObjectDataGPU);
    if (uploadSize > 0) {
        m_GPUCulling->UploadObjectData(m_ObjectDataCPU.data(), uploadSize);
    }
}

void RenderSystem::DrawIndirect(VkCommandBuffer commandBuffer) {
    if (!m_GPUCulling || !m_GPUCullingEnabled || !m_GeometryPool) return;
    if (m_CullableObjects.empty()) return;

    VkBuffer indirectBuffer = m_GPUCulling->GetIndirectDrawBuffer();
    VkBuffer drawCountBuffer = m_GPUCulling->GetDrawCountBuffer();
    if (indirectBuffer == VK_NULL_HANDLE || drawCountBuffer == VK_NULL_HANDLE) return;

    // Bind the merged geometry pool (single VB + IB for all static meshes)
    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }

    // Push sentinel constants to signal indirect mode to shaders.
    // parallaxScale = -1.0 tells the vertex/fragment shaders to read per-object data
    // from the ObjectData SSBO (binding 13) indexed by gl_InstanceIndex instead of push constants.
    Renderer::PushConstants indirectPC{};
    indirectPC.parallaxScale = -1.0f;
    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(Renderer::PushConstants), &indirectPC);

    // Issue a single indirect draw call for all visible static meshes.
    // The GPU culling compute shader has compacted visible objects into a contiguous
    // VkDrawIndexedIndirectCommand array. Each command's firstInstance encodes the
    // original object index for SSBO lookup via gl_InstanceIndex.
    vkCmdDrawIndexedIndirectCount(
        commandBuffer,
        indirectBuffer,             // VkDrawIndexedIndirectCommand array
        0,                          // offset
        drawCountBuffer,            // buffer containing actual draw count
        0,                          // count buffer offset
        m_GPUCulling->GetMaxObjects(), // maxDrawCount
        sizeof(VkDrawIndexedIndirectCommand) // stride
    );

    m_DrawCallCount++;  // Single draw call for all indirect objects
}

void RenderSystem::BuildTexturedIndirectBatches() {
    // This is now done in BuildCullableObjectList � entities are added to the batcher
    // during the main entity iteration loop, and batches are built/uploaded at the end.
}

void RenderSystem::DrawTexturedIndirect(VkCommandBuffer commandBuffer) {
    if (!m_IndirectDrawBatcher || !m_IndirectDrawBatcher->HasBatches()) return;
    if (!m_GeometryPool || !m_Pipeline) return;

    VkBuffer indirectBuffer = m_IndirectDrawBatcher->GetIndirectBuffer();
    if (indirectBuffer == VK_NULL_HANDLE) return;

    // Bind the merged geometry pool (single VB + IB for all static meshes)
    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }

    // Push sentinel constants for indirect mode (same as non-textured path)
    Renderer::PushConstants indirectPC{};
    indirectPC.parallaxScale = -1.0f;
    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(Renderer::PushConstants), &indirectPC);

    // Iterate texture draw batches: one indirect draw per texture group.
    // Each batch shares the same texture set, so we bind textures once per batch.
    for (const auto& batch : m_IndirectDrawBatcher->GetBatches()) {
        // Bind textures from the representative entity of this batch
        Entity representative = static_cast<Entity>(batch.representativeEntity);
        auto* material = m_World->GetComponent<MaterialComponent>(representative);
        if (material) {
            UpdateEntityTextureDescriptors(
                material->cachedBaseColorTexture,
                material->cachedHeightTexture,
                material->cachedNormalTexture,
                material->cachedMetallicRoughnessTexture,
                material->cachedEmissiveTexture,
                material->cachedMatcapTexture);
        }

        // Issue indirect draw for all entities in this texture group
        VkDeviceSize offset = static_cast<VkDeviceSize>(batch.firstCommand) * sizeof(VkDrawIndexedIndirectCommand);
        vkCmdDrawIndexedIndirect(
            commandBuffer,
            indirectBuffer,
            offset,
            batch.commandCount,
            sizeof(VkDrawIndexedIndirectCommand)
        );

        m_DrawCallCount++;
    }
}

void RenderSystem::DrawDGC(VkCommandBuffer commandBuffer) {
    if (!m_DGC || !m_DGC->IsEnabled()) return;
    if (!m_GPUCulling || !m_GeometryPool || !m_Pipeline) return;

    // Bind the merged geometry pool (single VB + IB for all static meshes)
    if (!m_GeometryPoolBound) {
        m_GeometryPool->BindBuffers(commandBuffer);
        m_GeometryPoolBound = true;
    }

    // Verify required buffers are available
    VkBuffer objectBuffer = m_GPUCulling->GetObjectBuffer();
    VkBuffer objectDataBuffer = m_GPUCulling->GetObjectDataBuffer();
    VkBuffer visibilityBuffer = m_GPUCulling->GetVisibilityBuffer();
    if (objectBuffer == VK_NULL_HANDLE || objectDataBuffer == VK_NULL_HANDLE ||
        visibilityBuffer == VK_NULL_HANDLE) {
        DrawIndirect(commandBuffer); // Fallback
        return;
    }

    // Step 1: Generate DGC command sequences from culling results.
    // The compute shader reads CullableObjects, ObjectData, and visibility flags,
    // then writes push constant + draw indexed sequences into the DGC sequence buffer.
    m_DGC->GenerateCommands(
        commandBuffer,
        objectBuffer,
        objectDataBuffer,
        visibilityBuffer,
        static_cast<u32>(m_CullableObjects.size())
    );

    // Step 2: Preprocess the generated command stream
    m_DGC->Preprocess(commandBuffer);

    // Step 3: Bind graphics pipeline and execute DGC
    m_Pipeline->Bind(commandBuffer);
    m_DGC->Execute(commandBuffer);

    m_DrawCallCount++; // Single DGC execution replaces all indirect draws
}

void RenderSystem::DispatchRTEffectsAsync(u32 frameIndex) {
    if (!m_AsyncComputeScheduler || !m_RTEnabled) return;

    // Record RT dispatches + denoising on async compute command buffer.
    // BeginFrame was already called at the start of Update().
    VkCommandBuffer computeCmd = m_AsyncComputeScheduler->BeginComputeWork(
        frameIndex, Renderer::AsyncComputeWorkType::RTDispatch);
    if (computeCmd == VK_NULL_HANDLE) {
        // Fallback: run on graphics queue
        VkCommandBuffer graphicsCmd = m_Renderer->GetCurrentCommandBuffer();
        if (graphicsCmd != VK_NULL_HANDLE) {
            DispatchRTEffects(graphicsCmd);
            TemporalReuseRTOutputs(graphicsCmd);
            DenoiseRTOutputs(graphicsCmd);
            CompositeRTResults(graphicsCmd);
        }
        return;
    }

    // Record RT dispatches, temporal reuse, and denoising on the compute command buffer
    DispatchRTEffects(computeCmd);
    TemporalReuseRTOutputs(computeCmd);
    DenoiseRTOutputs(computeCmd);

    // Submit compute work � signal semaphore for graphics queue to wait on
    m_AsyncComputeScheduler->SubmitComputeWork(frameIndex, false, true);

    // Register the compute-finished semaphore with VulkanRenderer so the graphics
    // queue submit waits for compute completion before present.
    VkSemaphore computeSem = m_AsyncComputeScheduler->GetComputeFinishedSemaphore(frameIndex);
    if (computeSem != VK_NULL_HANDLE) {
        m_Renderer->AddExternalWaitSemaphore(computeSem,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
}

void RenderSystem::DenoiseRTOutputsAsync(u32 frameIndex) {
    // Denoising is included in DispatchRTEffectsAsync (same compute command buffer).
    // This method is available for future use if we want to split denoise into a
    // separate compute submission that overlaps with post-processing.
    (void)frameIndex;
}

void RenderSystem::CreatePipeline() {
    Renderer::PipelineConfig config;
    config.renderPass = m_Renderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = m_BackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // Main pipeline always uses fill mode — wireframe only affects the offscreen
    // pipeline (scene view). Game view and Player always render filled.
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.msaaSamples = m_Renderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // Swapchain MRT: color + velocity (main pass only; offscreen uses 1)

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_Pipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_Pipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create graphics pipeline");
        m_Pipeline.reset();
    }
}

void RenderSystem::CreateShadowPipeline() {
    if (!m_ShadowMap || !m_Pipeline || !m_ShadowVertexShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_ShadowMap->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_NONE;  // Render all faces — front-only culling causes shadow holes under convex objects (capsule bottom)
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.depthBiasEnable = true;
    config.depthBiasConstant = 1.25f;  // Balanced: tight contact shadows without acne rings
    config.depthBiasSlope = 1.75f;     // Slope-scaled bias for angled surfaces
    config.hasColorAttachment = false;  // Depth-only pass

    m_ShadowPipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_ShadowPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    // Shadow shader uses push constants for MVP (avoids HOST_COHERENT UBO race).
    // Share descriptor set layout with main pipeline for compatibility.
    if (!m_ShadowPipeline->CreateWithLayout(config, m_ShadowVertexShader.get(), nullptr,
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow pipeline");
        m_ShadowPipeline.reset();
        m_ShadowsEnabled = false;
    }
}

void RenderSystem::CreateLinePipeline() {
    if (!m_Pipeline) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_Renderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    config.depthTest = true;
    config.depthWrite = false;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = true;
    config.msaaSamples = m_Renderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // MRT: must match render pass

    m_LinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_LinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_LinePipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(),
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create line pipeline");
        m_LinePipeline.reset();
    }
}

void RenderSystem::CreateOutlinePipeline() {
    if (!m_Pipeline || !m_OutlineVertexShader || !m_OutlineFragmentShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_Renderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_FRONT_BIT;  // Front-face culling: renders backfaces only (inverted hull)
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.msaaSamples = m_Renderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // Swapchain MRT: color + velocity (main pass only; offscreen uses 1) (must match render pass)

    m_OutlinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_OutlinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_OutlinePipeline->CreateWithLayout(config, m_OutlineVertexShader.get(), m_OutlineFragmentShader.get(),
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create outline pipeline");
        m_OutlinePipeline.reset();
    }
}

// ============================================================================
// Per-entity wireframe overlay pipeline + pass
// Renders entities with MeshRendererComponent::wireframe=true as LINE overlay
// on top of solid geometry. Uses the main vertex shader (for skinning support)
// and the outline fragment shader (flat color output).
// ============================================================================

void RenderSystem::CreateWireframeOverlayPipeline() {
    if (!m_Pipeline || !m_VertexShader || !m_OutlineFragmentShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_Renderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = false;   // Don't write depth — overlay on top of existing geometry
    config.cullMode = VK_CULL_MODE_NONE;  // Show all edges
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_LINE;
    config.msaaSamples = m_Renderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // Match main render pass MRT

    m_WireframeOverlayPipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_WireframeOverlayPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_WireframeOverlayPipeline->CreateWithLayout(config, m_VertexShader.get(), m_OutlineFragmentShader.get(),
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create wireframe overlay pipeline");
        m_WireframeOverlayPipeline.reset();
    }
}

void RenderSystem::RenderWireframeOverlayPass() {
    if (!m_WireframeOverlayPipeline || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Check if any entity has wireframe enabled — skip pass if none
    auto* meshRendererStorage = m_World->GetComponentStorage<MeshRendererComponent>();
    if (!meshRendererStorage) return;

    bool anyWireframe = false;
    for (Entity entity : m_SortedRenderList) {
        auto* mr = meshRendererStorage->Get(entity);
        if (mr && mr->wireframe) { anyWireframe = true; break; }
    }
    if (!anyWireframe) return;

    m_WireframeOverlayPipeline->Bind(commandBuffer);
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_WireframeOverlayPipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
    }

    for (Entity entity : m_SortedRenderList) {
        auto* mr = meshRendererStorage->Get(entity);
        if (!mr || !mr->wireframe) continue;

        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!transform || !transform->visible) continue;

        EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
            ? &m_EntityRenderData[static_cast<usize>(entity)] : nullptr;
        if (!pRD || !pRD->valid) continue;
        EntityRenderData& renderData = *pRD;

        // Build push constants — use wireframe color/opacity from component
        Renderer::PushConstants pc{};
        pc.baseColor = mr->wireframeColor;
        pc.opacity = mr->wireframeOpacity;
        pc.flags = 0;

        // Handle skinned meshes
        auto* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
        if (animComp && renderData.boneBuffer) {
            pc.flags |= (1 << 3); // FLAG_SKINNED
            pc.model = Math::Matrix4::Identity();
            const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
            if (!skinningMatrices.empty()) {
                renderData.boneBuffer->UploadData(skinningMatrices.data(),
                    skinningMatrices.size() * sizeof(Math::Matrix4));
            }
            UpdateBoneDescriptor(renderData.boneBuffer.get());
        } else {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) {
                UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
            }
        }

        vkCmdPushConstants(commandBuffer, m_WireframeOverlayPipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        // Draw
        if (renderData.poolAlloc.valid && m_GeometryPool) {
            if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
            vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                             renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
        } else if (renderData.vertexBuffer && renderData.indexCount > 0) {
            VkBuffer buffers[] = {renderData.vertexBuffer->GetBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
            if (renderData.indexBuffer) {
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            }
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
            m_GeometryPoolBound = false;
        }
    }
}

void RenderSystem::RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                                    u32 firstVertex, const Math::Vector3& color, f32 opacity) {
    if (!m_LinePipeline || !m_Renderer || !vertexBuffer || vertexCount == 0) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    m_LinePipeline->Bind(commandBuffer);

    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_LinePipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
    }

    VkExtent2D extent = m_Renderer->GetSwapchainExtent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    pc.baseColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    pc.metallic = 0.0f;
    pc.emissiveColor = color;
    pc.roughness = 1.0f;
    pc.emissiveStrength = 1.0f;
    pc.opacity = opacity;
    pc.alphaCutoff = 0.0f;
    pc.flags = 0;
    pc.parallaxScale = 0.0f;

    vkCmdPushConstants(commandBuffer, m_LinePipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    VkBuffer buffers[] = {vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    vkCmdDraw(commandBuffer, vertexCount, 1, firstVertex, 0);
}

void RenderSystem::RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                                    u32 firstVertex, const Math::Vector3& color, f32 opacity,
                                    u32 targetWidth, u32 targetHeight) {
    if (!m_LinePipeline || !m_Renderer || !vertexBuffer || vertexCount == 0) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Use offscreen line pipeline (matches offscreen UNORM render pass)
    auto* linePL = m_OffscreenLinePipeline ? m_OffscreenLinePipeline.get() : m_LinePipeline.get();
    linePL->Bind(commandBuffer);

    // Use offscreen descriptor sets (camera matrices match the editor viewport camera)
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            linePL->GetLayout(), 0, 1, &m_OffscreenDescriptorSets[currentFrame], 1, &zeroOff);
    }

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(targetWidth);
    viewport.height = static_cast<f32>(targetHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {targetWidth, targetHeight};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    pc.baseColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    pc.metallic = 0.0f;
    pc.emissiveColor = color;
    pc.roughness = 1.0f;
    pc.emissiveStrength = 1.0f;
    pc.opacity = opacity;
    pc.alphaCutoff = 0.0f;
    pc.flags = 0;
    pc.parallaxScale = 0.0f;

    vkCmdPushConstants(commandBuffer, linePL->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    VkBuffer buffers[] = {vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    vkCmdDraw(commandBuffer, vertexCount, 1, firstVertex, 0);
}

void RenderSystem::CreateUniformBuffers() {
    constexpr u32 framesInFlight = 2;
    const u32 offscreenCount = framesInFlight * MAX_SPLITSCREEN_VIEWPORTS;

    m_UniformBuffers.resize(framesInFlight);
    m_LightingBuffers.resize(framesInFlight);
    m_MaterialBuffers.resize(framesInFlight);
    m_OffscreenUniformBuffers.resize(offscreenCount);
    m_OffscreenLightingBuffers.resize(offscreenCount);

    for (u32 i = 0; i < framesInFlight; ++i) {
        // View/Projection uniform buffer (model matrix uses push constants now)
        m_UniformBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!m_UniformBuffers[i]->Create(sizeof(Renderer::UniformBufferObject), Renderer::BufferUsage::Uniform, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create uniform buffer %u", i);
            return;
        }

        // Lighting uniform buffer (multi-light support)
        m_LightingBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!m_LightingBuffers[i]->Create(sizeof(LightingUBO), Renderer::BufferUsage::Uniform, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create lighting buffer %u", i);
            return;
        }

        // Material SSBO (batched) — query device alignment and allocate for initial capacity
        {
            if (m_MaterialSSBOStride == 0) {
                VkPhysicalDeviceProperties devProps;
                vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &devProps);
                u32 minAlign = static_cast<u32>(devProps.limits.minStorageBufferOffsetAlignment);
                if (minAlign == 0) minAlign = 16;
                // Round sizeof(MaterialGPU) up to the required alignment
                m_MaterialSSBOStride = ((static_cast<u32>(sizeof(MaterialGPU)) + minAlign - 1) / minAlign) * minAlign;
                m_MaterialSSBOCapacity = 256;  // Initial capacity (grows as needed)
            }
            usize bufferSize = static_cast<usize>(m_MaterialSSBOStride) * m_MaterialSSBOCapacity;
            m_MaterialBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
            if (!m_MaterialBuffers[i]->Create(bufferSize, Renderer::BufferUsage::Storage, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create material SSBO %u", i);
                return;
            }
        }

        // Offscreen (game view) uniform buffers — one per viewport per frame for splitscreen
        for (u32 v = 0; v < MAX_SPLITSCREEN_VIEWPORTS; ++v) {
            u32 idx = GetOffscreenBufferIndex(i, v);

            m_OffscreenUniformBuffers[idx] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
            if (!m_OffscreenUniformBuffers[idx]->Create(sizeof(Renderer::UniformBufferObject), Renderer::BufferUsage::Uniform, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create offscreen uniform buffer %u (viewport %u)", i, v);
                return;
            }

            m_OffscreenLightingBuffers[idx] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
            if (!m_OffscreenLightingBuffers[idx]->Create(sizeof(LightingUBO), Renderer::BufferUsage::Uniform, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create offscreen lighting buffer %u (viewport %u)", i, v);
                return;
            }
        }
    }
}

void RenderSystem::RefreshDescriptorsIfDirty() {
    if (!m_ShadowDescriptorsDirty) return;
    m_ShadowDescriptorsDirty = false;
    // Rebuild descriptor sets so shadow map bindings reflect the current shadow state
    CreateDescriptorSets();
    ENJIN_LOG_INFO(Renderer, "Descriptor sets refreshed (shadow state changed)");
}

void RenderSystem::CreateDescriptorSets() {
    constexpr u32 framesInFlight = 2;
    const u32 offscreenSets = framesInFlight * MAX_SPLITSCREEN_VIEWPORTS;
    const u32 totalSets = framesInFlight + offscreenSets; // main + splitscreen offscreen

    // If a descriptor pool already exists, destroy it before allocating a new one.
    // RefreshDescriptorsIfDirty() can call this every frame (e.g. when the editor
    // toggles shadows for its scene view), and without this cleanup the old pool
    // is leaked AND m_OffscreenDescriptorSets is left dangling pointing into it,
    // which causes use-after-free crashes in the offscreen render path.
    if (m_DescriptorPool != VK_NULL_HANDLE && m_Renderer && m_Renderer->GetContext()) {
        m_Renderer->GetContext()->WaitForGPU();
        vkDestroyDescriptorPool(m_Renderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
        m_DescriptorSets.clear();
        m_OffscreenDescriptorSets.clear();
    }

    // Create descriptor pool (3 UBOs + 10 combined image samplers + 5 SSBOs per set)
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = totalSets * 2;   // bindings 0-1 (material moved to SSBO)
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = totalSets * 12;  // bindings 3-6, 8-11, 16-19
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = totalSets * 6;   // bindings 7, 12-15, 20 (morph targets)
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    poolSizes[3].descriptorCount = totalSets * 1;   // binding 2 (batched material SSBO)

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = totalSets;

    VkResult result = vkCreateDescriptorPool(
        m_Renderer->GetContext()->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create descriptor pool: %d", result);
        return;
    }

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_Pipeline->GetDescriptorSetLayout());
    m_DescriptorSets.resize(framesInFlight);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    result = vkAllocateDescriptorSets(
        m_Renderer->GetContext()->GetDevice(), &allocInfo, m_DescriptorSets.data());
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate descriptor sets: %d", result);
        return;
    }

    // Update descriptor sets with all UBOs and default texture
    for (u32 i = 0; i < framesInFlight; ++i) {
        std::array<VkDescriptorBufferInfo, 3> bufferInfos{};

        // MVP UBO
        bufferInfos[0].buffer = m_UniformBuffers[i]->GetBuffer();
        bufferInfos[0].offset = 0;
        bufferInfos[0].range = sizeof(Renderer::UniformBufferObject);

        // Lighting UBO (multi-light)
        bufferInfos[1].buffer = m_LightingBuffers[i]->GetBuffer();
        bufferInfos[1].offset = 0;
        bufferInfos[1].range = sizeof(LightingUBO);

        // Material SSBO (dynamic offset — range = one material entry stride)
        bufferInfos[2].buffer = m_MaterialBuffers[i]->GetBuffer();
        bufferInfos[2].offset = 0;
        bufferInfos[2].range = m_MaterialSSBOStride;

        // Default texture (binding 3)
        VkDescriptorImageInfo imageInfo{};
        if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
            imageInfo = m_DefaultWhiteTexture->GetDescriptorInfo();
        } else {
            ENJIN_LOG_ERROR(Renderer, "Default white texture unavailable - descriptor sets will be invalid");
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = VK_NULL_HANDLE;
            imageInfo.sampler = VK_NULL_HANDLE;
        }

        // Shadow map (binding 4) - 2D array for cascaded shadows
        VkDescriptorImageInfo shadowImageInfo{};
        if (m_ShadowMap && m_ShadowsEnabled) {
            shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowImageInfo.imageView = m_ShadowMap->GetDepthArrayView();
            shadowImageInfo.sampler = m_ShadowMap->GetShadowSampler();
        } else {
            // Use default white texture as fallback (will return 1.0 = no shadow)
            shadowImageInfo = imageInfo;
        }

        // Height map (binding 5) - default to white texture (no displacement)
        VkDescriptorImageInfo heightImageInfo = imageInfo;

        // Normal map (binding 6) - default to flat normal (white = (0.5,0.5,1) encoded)
        VkDescriptorImageInfo normalMapInfo = imageInfo;

        // Bone matrices SSBO (binding 7) - default identity
        VkDescriptorBufferInfo boneBufferInfo{};
        if (m_DefaultBoneBuffer) {
            boneBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            boneBufferInfo.offset = 0;
            boneBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Metallic-roughness texture (binding 8) - default to white
        VkDescriptorImageInfo metallicRoughnessImageInfo = imageInfo;

        // Emissive texture (binding 9) - default to white
        VkDescriptorImageInfo emissiveImageInfo = imageInfo;

        // Point light shadow cubemap array (binding 10) - default to white
        VkDescriptorImageInfo pointShadowImageInfo{};
        if (m_PointShadowMap) {
            pointShadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            pointShadowImageInfo.imageView = m_PointShadowMap->GetCubeArrayView();
            pointShadowImageInfo.sampler = m_PointShadowMap->GetShadowSampler();
        } else {
            pointShadowImageInfo = imageInfo;
        }

        // Spot light shadow 2D array (binding 11) - default to white
        VkDescriptorImageInfo spotShadowImageInfo{};
        if (m_SpotShadowMap) {
            spotShadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            spotShadowImageInfo.imageView = m_SpotShadowMap->GetArrayView();
            spotShadowImageInfo.sampler = m_SpotShadowMap->GetShadowSampler();
        } else {
            spotShadowImageInfo = imageInfo;
        }

        // Shadow data SSBO (binding 12)
        VkDescriptorBufferInfo shadowDataBufferInfo{};
        if (m_ShadowDataBuffer) {
            shadowDataBufferInfo.buffer = m_ShadowDataBuffer->GetBuffer();
            shadowDataBufferInfo.offset = 0;
            shadowDataBufferInfo.range = m_ShadowDataBuffer->GetSize();
        }

        // ObjectData SSBO (binding 13) - per-object material/transform for indirect draws
        VkDescriptorBufferInfo objectDataBufferInfo{};
        if (m_GPUCulling && m_GPUCulling->GetObjectDataBuffer() != VK_NULL_HANDLE) {
            objectDataBufferInfo.buffer = m_GPUCulling->GetObjectDataBuffer();
            objectDataBufferInfo.offset = 0;
            objectDataBufferInfo.range = VK_WHOLE_SIZE;
        } else if (m_DefaultBoneBuffer) {
            // Fallback: bind any valid SSBO so the descriptor isn't null
            objectDataBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            objectDataBufferInfo.offset = 0;
            objectDataBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Clustered lighting grid SSBO (binding 14) - fallback to default bone buffer
        VkDescriptorBufferInfo clusterGridBufferInfo{};
#ifdef ENJIN_CLUSTERED_LIGHTING
        if (m_ClusteredLighting && m_ClusteredLighting->GetLightGridBuffer() != VK_NULL_HANDLE) {
            clusterGridBufferInfo.buffer = m_ClusteredLighting->GetLightGridBuffer();
            clusterGridBufferInfo.offset = 0;
            clusterGridBufferInfo.range = VK_WHOLE_SIZE;
        } else
#endif
        if (m_DefaultBoneBuffer) {
            clusterGridBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            clusterGridBufferInfo.offset = 0;
            clusterGridBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Clustered lighting index SSBO (binding 15) - fallback to default bone buffer
        VkDescriptorBufferInfo clusterIndexBufferInfo{};
#ifdef ENJIN_CLUSTERED_LIGHTING
        if (m_ClusteredLighting && m_ClusteredLighting->GetLightIndexBuffer() != VK_NULL_HANDLE) {
            clusterIndexBufferInfo.buffer = m_ClusteredLighting->GetLightIndexBuffer();
            clusterIndexBufferInfo.offset = 0;
            clusterIndexBufferInfo.range = VK_WHOLE_SIZE;
        } else
#endif
        if (m_DefaultBoneBuffer) {
            clusterIndexBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            clusterIndexBufferInfo.offset = 0;
            clusterIndexBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Virtual texturing indirection texture (binding 16) - fallback to white
        VkDescriptorImageInfo vtIndirectionImageInfo = imageInfo;
        // Virtual texturing physical atlas (binding 17) - fallback to white
        VkDescriptorImageInfo vtAtlasImageInfo = imageInfo;

        // Matcap texture (binding 18) - fallback to white (procedural matcap used when white)
        VkDescriptorImageInfo matcapImageInfo = imageInfo;

        // Baked reflection probe cubemap (binding 19) - fallback to white
        // When a probe is baked, this is updated via UpdateProbeCubemapDescriptor()
        VkDescriptorImageInfo probeCubemapImageInfo = imageInfo;

        std::array<VkWriteDescriptorSet, 21> descriptorWrites{};

        // MVP descriptor
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfos[0];

        // Lighting descriptor
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_DescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &bufferInfos[1];

        // Material SSBO descriptor (dynamic offset)
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_DescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &bufferInfos[2];

        // Base color texture descriptor
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = m_DescriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &imageInfo;

        // Shadow map descriptor
        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = m_DescriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &shadowImageInfo;

        // Height map descriptor
        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = m_DescriptorSets[i];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pImageInfo = &heightImageInfo;

        // Normal map descriptor
        descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[6].dstSet = m_DescriptorSets[i];
        descriptorWrites[6].dstBinding = 6;
        descriptorWrites[6].dstArrayElement = 0;
        descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[6].descriptorCount = 1;
        descriptorWrites[6].pImageInfo = &normalMapInfo;

        // Bone matrices SSBO descriptor
        descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[7].dstSet = m_DescriptorSets[i];
        descriptorWrites[7].dstBinding = 7;
        descriptorWrites[7].dstArrayElement = 0;
        descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[7].descriptorCount = 1;
        descriptorWrites[7].pBufferInfo = &boneBufferInfo;

        // Metallic-roughness texture descriptor
        descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[8].dstSet = m_DescriptorSets[i];
        descriptorWrites[8].dstBinding = 8;
        descriptorWrites[8].dstArrayElement = 0;
        descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[8].descriptorCount = 1;
        descriptorWrites[8].pImageInfo = &metallicRoughnessImageInfo;

        // Emissive texture descriptor
        descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[9].dstSet = m_DescriptorSets[i];
        descriptorWrites[9].dstBinding = 9;
        descriptorWrites[9].dstArrayElement = 0;
        descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[9].descriptorCount = 1;
        descriptorWrites[9].pImageInfo = &emissiveImageInfo;

        // Point light shadow cubemap array descriptor
        descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[10].dstSet = m_DescriptorSets[i];
        descriptorWrites[10].dstBinding = 10;
        descriptorWrites[10].dstArrayElement = 0;
        descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[10].descriptorCount = 1;
        descriptorWrites[10].pImageInfo = &pointShadowImageInfo;

        // Spot light shadow 2D array descriptor
        descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[11].dstSet = m_DescriptorSets[i];
        descriptorWrites[11].dstBinding = 11;
        descriptorWrites[11].dstArrayElement = 0;
        descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[11].descriptorCount = 1;
        descriptorWrites[11].pImageInfo = &spotShadowImageInfo;

        // Shadow data SSBO descriptor
        descriptorWrites[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[12].dstSet = m_DescriptorSets[i];
        descriptorWrites[12].dstBinding = 12;
        descriptorWrites[12].dstArrayElement = 0;
        descriptorWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[12].descriptorCount = 1;
        descriptorWrites[12].pBufferInfo = &shadowDataBufferInfo;

        // ObjectData SSBO descriptor (binding 13)
        descriptorWrites[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[13].dstSet = m_DescriptorSets[i];
        descriptorWrites[13].dstBinding = 13;
        descriptorWrites[13].dstArrayElement = 0;
        descriptorWrites[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[13].descriptorCount = 1;
        descriptorWrites[13].pBufferInfo = &objectDataBufferInfo;

        // Cluster grid SSBO descriptor (binding 14)
        descriptorWrites[14].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[14].dstSet = m_DescriptorSets[i];
        descriptorWrites[14].dstBinding = 14;
        descriptorWrites[14].dstArrayElement = 0;
        descriptorWrites[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[14].descriptorCount = 1;
        descriptorWrites[14].pBufferInfo = &clusterGridBufferInfo;

        // Cluster light index SSBO descriptor (binding 15)
        descriptorWrites[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[15].dstSet = m_DescriptorSets[i];
        descriptorWrites[15].dstBinding = 15;
        descriptorWrites[15].dstArrayElement = 0;
        descriptorWrites[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[15].descriptorCount = 1;
        descriptorWrites[15].pBufferInfo = &clusterIndexBufferInfo;

        // VT indirection texture descriptor (binding 16)
        descriptorWrites[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[16].dstSet = m_DescriptorSets[i];
        descriptorWrites[16].dstBinding = 16;
        descriptorWrites[16].dstArrayElement = 0;
        descriptorWrites[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[16].descriptorCount = 1;
        descriptorWrites[16].pImageInfo = &vtIndirectionImageInfo;

        // VT physical atlas descriptor (binding 17)
        descriptorWrites[17].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[17].dstSet = m_DescriptorSets[i];
        descriptorWrites[17].dstBinding = 17;
        descriptorWrites[17].dstArrayElement = 0;
        descriptorWrites[17].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[17].descriptorCount = 1;
        descriptorWrites[17].pImageInfo = &vtAtlasImageInfo;

        // Matcap texture descriptor (binding 18)
        descriptorWrites[18].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[18].dstSet = m_DescriptorSets[i];
        descriptorWrites[18].dstBinding = 18;
        descriptorWrites[18].dstArrayElement = 0;
        descriptorWrites[18].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[18].descriptorCount = 1;
        descriptorWrites[18].pImageInfo = &matcapImageInfo;

        // Baked reflection probe cubemap descriptor (binding 19)
        descriptorWrites[19].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[19].dstSet = m_DescriptorSets[i];
        descriptorWrites[19].dstBinding = 19;
        descriptorWrites[19].dstArrayElement = 0;
        descriptorWrites[19].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[19].descriptorCount = 1;
        descriptorWrites[19].pImageInfo = &probeCubemapImageInfo;

        // Binding 20: morph target SSBO (default = empty, targetCount=0)
        VkDescriptorBufferInfo morphBufferInfo{};
        if (m_DefaultMorphBuffer) {
            morphBufferInfo.buffer = m_DefaultMorphBuffer->GetBuffer();
            morphBufferInfo.offset = 0;
            morphBufferInfo.range = m_DefaultMorphBuffer->GetSize();
        } else {
            morphBufferInfo = boneBufferInfo; // Fallback to bone buffer as dummy
        }
        descriptorWrites[20].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[20].dstSet = m_DescriptorSets[i];
        descriptorWrites[20].dstBinding = 20;
        descriptorWrites[20].dstArrayElement = 0;
        descriptorWrites[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[20].descriptorCount = 1;
        descriptorWrites[20].pBufferInfo = &morphBufferInfo;

        vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(),
            static_cast<u32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    // Allocate offscreen descriptor sets (one per viewport per frame for splitscreen)
    {
        std::vector<VkDescriptorSetLayout> offscreenLayouts(offscreenSets, m_Pipeline->GetDescriptorSetLayout());
        m_OffscreenDescriptorSets.resize(offscreenSets);

        VkDescriptorSetAllocateInfo offAllocInfo{};
        offAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        offAllocInfo.descriptorPool = m_DescriptorPool;
        offAllocInfo.descriptorSetCount = offscreenSets;
        offAllocInfo.pSetLayouts = offscreenLayouts.data();

        result = vkAllocateDescriptorSets(
            m_Renderer->GetContext()->GetDevice(), &offAllocInfo, m_OffscreenDescriptorSets.data());
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate offscreen descriptor sets: %d", result);
            return;
        }

        for (u32 i = 0; i < framesInFlight; ++i) {
            for (u32 v = 0; v < MAX_SPLITSCREEN_VIEWPORTS; ++v) {
                u32 idx = GetOffscreenBufferIndex(i, v);

                std::array<VkDescriptorBufferInfo, 3> offBufInfos{};
                offBufInfos[0].buffer = m_OffscreenUniformBuffers[idx]->GetBuffer();
                offBufInfos[0].offset = 0;
                offBufInfos[0].range = sizeof(Renderer::UniformBufferObject);
                offBufInfos[1].buffer = m_OffscreenLightingBuffers[idx]->GetBuffer();
                offBufInfos[1].offset = 0;
                offBufInfos[1].range = sizeof(LightingUBO);
                // Share the material SSBO — batched per-frame, indexed via dynamic offset
                offBufInfos[2].buffer = m_MaterialBuffers[i]->GetBuffer();
                offBufInfos[2].offset = 0;
                offBufInfos[2].range = m_MaterialSSBOStride;

                VkDescriptorImageInfo offImageInfo{};
                if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                    offImageInfo = m_DefaultWhiteTexture->GetDescriptorInfo();
                } else {
                    offImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    offImageInfo.imageView = VK_NULL_HANDLE;
                    offImageInfo.sampler = VK_NULL_HANDLE;
                }
                VkDescriptorImageInfo offShadowInfo{};
                if (m_ShadowMap && m_ShadowsEnabled) {
                    offShadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    offShadowInfo.imageView = m_ShadowMap->GetDepthArrayView();
                    offShadowInfo.sampler = m_ShadowMap->GetShadowSampler();
                } else {
                    offShadowInfo = offImageInfo;
                }
                VkDescriptorImageInfo offHeightInfo = offImageInfo;
                VkDescriptorImageInfo offNormalInfo = offImageInfo;
                VkDescriptorImageInfo offMetRoughInfo = offImageInfo;
                VkDescriptorImageInfo offEmissiveInfo = offImageInfo;

                VkDescriptorBufferInfo offBoneInfo{};
                if (m_DefaultBoneBuffer) {
                    offBoneInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offBoneInfo.offset = 0;
                    offBoneInfo.range = m_DefaultBoneBuffer->GetSize();
                }

                VkDescriptorImageInfo offPointShadowInfo{};
                if (m_PointShadowMap) {
                    offPointShadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    offPointShadowInfo.imageView = m_PointShadowMap->GetCubeArrayView();
                    offPointShadowInfo.sampler = m_PointShadowMap->GetShadowSampler();
                } else {
                    offPointShadowInfo = offImageInfo;
                }
                VkDescriptorImageInfo offSpotShadowInfo{};
                if (m_SpotShadowMap) {
                    offSpotShadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    offSpotShadowInfo.imageView = m_SpotShadowMap->GetArrayView();
                    offSpotShadowInfo.sampler = m_SpotShadowMap->GetShadowSampler();
                } else {
                    offSpotShadowInfo = offImageInfo;
                }
                VkDescriptorBufferInfo offShadowDataInfo{};
                if (m_ShadowDataBuffer) {
                    offShadowDataInfo.buffer = m_ShadowDataBuffer->GetBuffer();
                    offShadowDataInfo.offset = 0;
                    offShadowDataInfo.range = m_ShadowDataBuffer->GetSize();
                }

                VkDescriptorBufferInfo offObjectDataInfo{};
                if (m_GPUCulling && m_GPUCulling->GetObjectDataBuffer() != VK_NULL_HANDLE) {
                    offObjectDataInfo.buffer = m_GPUCulling->GetObjectDataBuffer();
                    offObjectDataInfo.offset = 0;
                    offObjectDataInfo.range = VK_WHOLE_SIZE;
                } else if (m_DefaultBoneBuffer) {
                    offObjectDataInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offObjectDataInfo.offset = 0;
                    offObjectDataInfo.range = m_DefaultBoneBuffer->GetSize();
                }

                // Offscreen cluster/VT fallback data
                VkDescriptorBufferInfo offClusterGridInfo{};
                VkDescriptorBufferInfo offClusterIdxInfo{};
#ifdef ENJIN_CLUSTERED_LIGHTING
                if (m_ClusteredLighting && m_ClusteredLighting->GetLightGridBuffer() != VK_NULL_HANDLE) {
                    offClusterGridInfo.buffer = m_ClusteredLighting->GetLightGridBuffer();
                    offClusterGridInfo.range = VK_WHOLE_SIZE;
                    offClusterIdxInfo.buffer = m_ClusteredLighting->GetLightIndexBuffer();
                    offClusterIdxInfo.range = VK_WHOLE_SIZE;
                } else
#endif
                if (m_DefaultBoneBuffer) {
                    offClusterGridInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offClusterGridInfo.range = m_DefaultBoneBuffer->GetSize();
                    offClusterIdxInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offClusterIdxInfo.range = m_DefaultBoneBuffer->GetSize();
                }
                VkDescriptorImageInfo offVtIndInfo = offImageInfo;
                VkDescriptorImageInfo offVtAtlasInfo = offImageInfo;
                VkDescriptorImageInfo offMatcapInfo = offImageInfo;
                VkDescriptorImageInfo offProbeCubemapInfo = offImageInfo;

                std::array<VkWriteDescriptorSet, 21> offWrites{};
                for (u32 w = 0; w < 21; ++w) {
                    offWrites[w].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    offWrites[w].dstSet = m_OffscreenDescriptorSets[idx];
                    offWrites[w].dstBinding = w;
                    offWrites[w].dstArrayElement = 0;
                    offWrites[w].descriptorCount = 1;
                }
                offWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                offWrites[0].pBufferInfo = &offBufInfos[0];
                offWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                offWrites[1].pBufferInfo = &offBufInfos[1];
                offWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                offWrites[2].pBufferInfo = &offBufInfos[2];
                offWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[3].pImageInfo = &offImageInfo;
                offWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[4].pImageInfo = &offShadowInfo;
                offWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[5].pImageInfo = &offHeightInfo;
                offWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[6].pImageInfo = &offNormalInfo;
                offWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[7].pBufferInfo = &offBoneInfo;
                offWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[8].pImageInfo = &offMetRoughInfo;
                offWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[9].pImageInfo = &offEmissiveInfo;
                offWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[10].pImageInfo = &offPointShadowInfo;
                offWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[11].pImageInfo = &offSpotShadowInfo;
                offWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[12].pBufferInfo = &offShadowDataInfo;
                offWrites[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[13].pBufferInfo = &offObjectDataInfo;
                offWrites[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[14].pBufferInfo = &offClusterGridInfo;
                offWrites[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[15].pBufferInfo = &offClusterIdxInfo;
                offWrites[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[16].pImageInfo = &offVtIndInfo;
                offWrites[17].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[17].pImageInfo = &offVtAtlasInfo;
                offWrites[18].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[18].pImageInfo = &offMatcapInfo;
                offWrites[19].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[19].pImageInfo = &offProbeCubemapInfo;

                // Binding 20: morph target SSBO
                VkDescriptorBufferInfo offMorphInfo{};
                if (m_DefaultMorphBuffer) {
                    offMorphInfo.buffer = m_DefaultMorphBuffer->GetBuffer();
                    offMorphInfo.range = m_DefaultMorphBuffer->GetSize();
                } else {
                    offMorphInfo = offBoneInfo;
                }
                offWrites[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[20].pBufferInfo = &offMorphInfo;

                vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(),
                    static_cast<u32>(offWrites.size()), offWrites.data(), 0, nullptr);
            }
        }
    }
}

bool RenderSystem::IsPoolEligible(Entity entity) const {
    if (!m_GeometryPool) return false;
    // Dynamic meshes: terrain, jelly, sprites, tilemaps, water — keep per-entity buffers
    if (m_World->HasComponent<Sprite2DComponent>(entity)) return false;
    if (m_World->HasComponent<TilemapComponent>(entity)) return false;
    if (m_World->HasComponent<TerrainComponent>(entity)) return false;
    if (m_World->HasComponent<Terrain2DComponent>(entity)) return false;
    if (m_World->HasComponent<JellyMeshComponent>(entity)) return false;
    if (m_World->HasComponent<WaterVolumeComponent>(entity)) return false;
    if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) return false;
    // Skinned meshes stay per-entity (bone deformation updates vertex data)
    if (m_World->HasComponent<AnimatorComponent>(entity)) return false;
    return true;
}

EntityRenderData* RenderSystem::SetupEntityBuffers(Entity entity) {
    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);
    if (!mesh || !mesh->IsValid()) {
        return nullptr;
    }

    // Ensure dense vector is large enough for this entity ID
    if (static_cast<usize>(entity) >= m_EntityRenderData.size()) {
        m_EntityRenderData.resize(static_cast<usize>(entity) + 1);
    }
    EntityRenderData& renderData = m_EntityRenderData[static_cast<usize>(entity)];
    renderData.Invalidate();
    renderData.valid = true;

    // Try merged geometry pool for static 3D meshes
    if (IsPoolEligible(entity)) {
        auto alloc = m_GeometryPool->Upload(
            mesh->vertices.data(),
            static_cast<u32>(mesh->vertices.size()),
            mesh->indices.data(),
            static_cast<u32>(mesh->indices.size()));
        if (alloc.valid) {
            renderData.poolAlloc = alloc;
            renderData.indexCount = alloc.indexCount;
            // No per-entity VB/IB needed — pool owns the memory
            return &renderData;
        }
        // Pool allocation failed (overflow) — fall through to per-entity buffers
    }

    // Per-entity buffers (dynamic meshes, pool overflow fallback)
    // Add ShaderDeviceAddress usage when RT is supported for BLAS building
    VkBufferUsageFlags vertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkBufferUsageFlags indexUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (IsRayTracingSupported()) {
        vertexUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        indexUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }

    usize vertexBufferSize = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
    renderData.vertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!renderData.vertexBuffer->Create(vertexBufferSize, vertexUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create vertex buffer for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }

    if (!renderData.vertexBuffer->UploadData(mesh->vertices.data(), vertexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload vertex data for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }

    // Create index buffer
    usize indexBufferSize = mesh->indices.size() * sizeof(u32);
    renderData.indexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!renderData.indexBuffer->Create(indexBufferSize, indexUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create index buffer for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }

    if (!renderData.indexBuffer->UploadData(mesh->indices.data(), indexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload index data for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }

    renderData.indexCount = static_cast<u32>(mesh->indices.size());

    // Create bone SSBO if entity has a skeleton for animation.
    // The AnimatorComponent may be on this entity or on a sibling (Mixamo FBX
    // puts mesh and skeleton on different entities). Search globally if needed.
    AnimatorComponent* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
    if (!animComp) {
        // Check if this mesh has bone weights — if so, find any AnimatorComponent
        bool hasBoneWeights = false;
        for (const auto& v : mesh->vertices) {
            if (v.boneWeights.x > 0.0f) { hasBoneWeights = true; break; }
        }
        ENJIN_LOG_INFO(Renderer, "BONE CHECK: entity %llu, %zu verts, hasBoneWeights=%d",
            (unsigned long long)entity, mesh->vertices.size(), hasBoneWeights);
        if (hasBoneWeights) {
            for (auto animEntity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
                auto* ac = m_World->GetComponent<AnimatorComponent>(animEntity);
                if (ac && ac->animator.GetSkeleton()) {
                    animComp = ac;
                    break;
                }
            }
        }
    }
    if (animComp && animComp->animator.GetSkeleton()) {
        usize boneCount = animComp->animator.GetSkeleton()->bones.size();
        if (boneCount > 0) {
            usize boneBufferSize = boneCount * sizeof(Math::Matrix4);
            renderData.boneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
            if (!renderData.boneBuffer->Create(boneBufferSize, Renderer::BufferUsage::Storage, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create bone buffer for entity %llu", entity);
                renderData.boneBuffer.reset();
            } else {
                // Initialize bone buffer with identity matrices so bind-pose
                // skinning is a no-op. Without this, the buffer contains garbage
                // until the first animation frame uploads real matrices.
                std::vector<Math::Matrix4> identityMatrices(boneCount, Math::Matrix4::Identity());
                renderData.boneBuffer->UploadData(identityMatrices.data(), boneBufferSize);
            }
        }
    }

    return &renderData;
}

void RenderSystem::UpdateProbeCubemapDescriptor() {
    if (!m_ReflectionProbes || !m_ReflectionProbes->HasActiveBakedCubemap()) return;
    if (!m_Renderer || !m_Pipeline) return;

    VkDescriptorImageInfo cubemapInfo = m_ReflectionProbes->GetActiveBakedCubemapDescriptor();
    if (cubemapInfo.imageView == VK_NULL_HANDLE) return;

    VkDevice device = m_Renderer->GetContext()->GetDevice();

    // Update binding 19 in all active descriptor sets (main + offscreen)
    // Main pass descriptor sets
    for (auto& descSet : m_DescriptorSets) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSet;
        write.dstBinding = 19;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &cubemapInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Offscreen descriptor sets
    for (auto& descSet : m_OffscreenDescriptorSets) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSet;
        write.dstBinding = 19;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &cubemapInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
}

void RenderSystem::UpdateFrameUniforms() {
    if (!m_Camera) {
        // No camera set — frame uniforms not uploaded. Scene will render with
        // stale view/projection matrices. This is expected briefly during
        // initialization or scene transitions.
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Update View/Projection UBO (shared across all objects)
    Renderer::UniformBufferObject ubo{};
    ubo.view = m_Camera->GetViewMatrix();
    ubo.proj = m_Camera->GetProjectionMatrix();
    ubo.prevViewProj = m_PrevViewProj;

    // TAA / Upscaler jitter injection: apply sub-pixel Halton offset to the projection
    // matrix so each frame samples a slightly different sub-pixel position. Both TAA
    // and temporal upscalers (FSR 2, DLSS, XeSS) require jittered input.
    if (m_AAMode == 2 || m_UpscalerType > 0) { // TAA or temporal upscaler
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        // When an upscaler is active, compute jitter relative to the lower render resolution
        // so that sub-pixel offsets are correctly sized for the internal rendering target.
        u32 jitterW = extent.width;
        u32 jitterH = extent.height;
        if (m_UpscalerType > 0 && m_Upscaler) {
            Renderer::IUpscaler::GetRenderResolution(
                extent.width, extent.height,
                static_cast<Renderer::UpscalerQuality>(m_UpscalerQuality),
                jitterW, jitterH);
        }
        if (jitterW > 0 && jitterH > 0) {
            Math::Vector2 jitter = Renderer::HaltonJitter(m_TAAFrameCounter, jitterW, jitterH);
            // Offset the projection matrix: translation in clip space X/Y
            ubo.proj.m[8]  += jitter.x;  // m[2][0] in column-major
            ubo.proj.m[9]  += jitter.y;  // m[2][1] in column-major
            ubo.jitterOffset = Math::Vector4(jitter.x, jitter.y, m_PrevJitter.x, m_PrevJitter.y);
            m_PrevJitter = jitter;
        } else {
            ubo.jitterOffset = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        m_TAAFrameCounter++;
    } else {
        ubo.jitterOffset = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        m_PrevJitter = Math::Vector2(0.0f, 0.0f);
        m_TAAFrameCounter = 0;
    }

    (*m_ActiveUniformBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&ubo, sizeof(ubo));

    // Store current viewProj for next frame's velocity computation
    m_PrevViewProj = ubo.proj * ubo.view;

    // Update Lighting UBO with all lights in the scene
    LightingUBO lighting{};
    lighting.ambientColor = m_AmbientColor;
    lighting.ambientIntensity = m_AmbientIntensity;
    lighting.cameraPosition = m_Camera->GetPosition();
    lighting._pad0 = 0.0f;
    lighting.directionalLightCount = 0;
    lighting.pointLightCount = 0;
    lighting.spotLightCount = 0;
    lighting._pad1 = 0;

    // Scene2D fast path: skip light iteration, shadow data, wind — no consumers
    if (m_SceneComposition.mode == SceneRenderMode::Scene2D) {
        lighting.directionalLightCount = 0;
        lighting.pointLightCount = 0;
        lighting.spotLightCount = 0;
        lighting.shadowEnabled = 0;
        lighting.pointShadowCount = 0;
        lighting.spotShadowCount = 0;
        lighting.fogParams = Math::Vector4(m_FogDensity, m_FogStart, m_FogEnd, m_FogHeightFalloff);
        lighting.fogColorSnow = Math::Vector4(m_FogColor.x, m_FogColor.y, m_FogColor.z, m_SnowIntensity);
        (*m_ActiveLightingBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&lighting, sizeof(lighting));
        m_CachedLightingData = lighting;
        return;
    }

    // Editor "Solid" view mode: render with flat ambient only, no lights
    if (m_EditorUnlit) {
        lighting.ambientColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        lighting.ambientIntensity = 1.0f;
        lighting.directionalLightCount = 0;
        lighting.pointLightCount = 0;
        lighting.spotLightCount = 0;
        lighting.shadowEnabled = 0;
        lighting.fogParams = Math::Vector4(0, 0, 0, 0); // No fog in solid view
        (*m_ActiveLightingBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&lighting, sizeof(lighting));
        m_CachedLightingData = lighting;
        return;
    }

    bool hasAnyLight = false;

    auto* lightStorageFU = m_World->GetComponentStorage<LightComponent>();
    for (Entity lightEntity : m_CachedLightEntities) {
        LightComponent* light = lightStorageFU ? lightStorageFU->Get(lightEntity) : nullptr;
        TransformComponent* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(lightEntity) : nullptr;
        if (!light) continue;

        hasAnyLight = true;

        switch (light->type) {
            case LightType::Directional: {
                if (lighting.directionalLightCount < MAX_DIRECTIONAL_LIGHTS) {
                    auto& dirLight = lighting.directionalLights[lighting.directionalLightCount];
                    if (lightTransform) {
                        Math::Vector3 forward(0.0f, 0.0f, -1.0f);
                        dirLight.direction = lightTransform->rotation.Rotate(forward).Normalized();
                    } else {
                        dirLight.direction = Math::Vector3(-0.5f, -0.8f, -0.3f).Normalized();
                    }
                    dirLight.color = light->color;
                    dirLight.intensity = light->intensity;
                    lighting.directionalLightCount++;
                }
                break;
            }
            case LightType::Point: {
                if (lighting.pointLightCount < MAX_POINT_LIGHTS) {
                    auto& pointLight = lighting.pointLights[lighting.pointLightCount];
                    pointLight.position = lightTransform ? lightTransform->position : Math::Vector3(0.0f);
                    pointLight.range = light->range;
                    pointLight.color = light->color;
                    pointLight.intensity = light->intensity;
                    pointLight.constantAttenuation = light->constantAttenuation;
                    pointLight.linearAttenuation = light->linearAttenuation;
                    pointLight.quadraticAttenuation = light->quadraticAttenuation;
                    lighting.pointLightCount++;
                }
                break;
            }
            case LightType::Spot: {
                if (lighting.spotLightCount < MAX_SPOT_LIGHTS) {
                    auto& spotLight = lighting.spotLights[lighting.spotLightCount];
                    spotLight.position = lightTransform ? lightTransform->position : Math::Vector3(0.0f);
                    spotLight.range = light->range;
                    if (lightTransform) {
                        Math::Vector3 forward(0.0f, 0.0f, -1.0f);
                        spotLight.direction = lightTransform->rotation.Rotate(forward).Normalized();
                    } else {
                        spotLight.direction = Math::Vector3(0.0f, -1.0f, 0.0f);
                    }
                    spotLight.color = light->color;
                    spotLight.intensity = light->intensity;
                    spotLight.innerCutoff = std::cos(light->innerConeAngle * 3.14159265f / 180.0f);
                    spotLight.outerCutoff = std::cos(light->outerConeAngle * 3.14159265f / 180.0f);
                    spotLight.constantAttenuation = light->constantAttenuation;
                    spotLight.linearAttenuation = light->linearAttenuation;
                    spotLight.quadraticAttenuation = light->quadraticAttenuation;
                    lighting.spotLightCount++;
                }
                break;
            }
        }
    }

    if (!hasAnyLight) {
        lighting.directionalLights[0].direction = Math::Vector3(-0.5f, -0.8f, -0.3f).Normalized();
        lighting.directionalLights[0].color = Math::Vector3(1.0f, 0.95f, 0.9f);
        lighting.directionalLights[0].intensity = 1.2f;
        lighting.directionalLightCount = 1;
    }

    if (m_ShadowsEnabled && m_ShadowMap) {
        for (u32 i = 0; i < Renderer::MAX_SHADOW_CASCADES; ++i) {
            lighting.cascadeViewProj[i] = m_ShadowMap->GetCascadeViewProj(i);
        }
        lighting.cascadeSplits = Math::Vector4(
            m_ShadowMap->GetCascadeSplit(0),
            m_ShadowMap->GetCascadeSplit(1),
            m_ShadowMap->GetCascadeSplit(2),
            m_ShadowMap->GetCascadeSplit(3));
        lighting.shadowSoftness = m_ShadowMap->GetShadowSoftness();
        lighting.shadowEnabled = 1;
        lighting.shadowStrength = m_ShadowMap->GetShadowStrength();
        lighting.shadowMaxDistance = m_ShadowDistance;
    } else {
        for (u32 i = 0; i < Renderer::MAX_SHADOW_CASCADES; ++i) {
            lighting.cascadeViewProj[i] = Math::Matrix4::Identity();
        }
        lighting.cascadeSplits = Math::Vector4(25.0f, 50.0f, 75.0f, 100.0f);
        lighting.shadowSoftness = 0.0f;
        lighting.shadowEnabled = 0;
        lighting.shadowStrength = 1.0f;
        lighting.shadowMaxDistance = 100.0f;
    }

    // Sort shadow-casting point/spot lights to front of UBO arrays
    // so indices 0..N-1 correspond to shadow slots 0..N-1
    lighting.pointShadowCount = static_cast<i32>(m_ActivePointShadowCount);
    lighting.spotShadowCount = static_cast<i32>(m_ActiveSpotShadowCount);
    lighting.celDiffuseBands = m_CelShadingEnabled ? m_CelDiffuseBands : 0.0f;
    lighting.celSpecularCutoff = m_CelShadingEnabled ? m_CelSpecularCutoff : 0.0f;

    // Pack shading model flags: bit0=GGX, bit1=Fresnel, bit2=EnergyConserv, bit3=GeometryTerm, bit4=SphereEnvMap
    lighting.shadingFlags = (m_ShadingModel & 1u)
        | (m_FresnelEnabled ? 2u : 0u)
        | (m_EnergyConservation ? 4u : 0u)
        | (m_GeometryTerm ? 8u : 0u)
        | (m_SphereEnvMapEnabled ? 16u : 0u)
        | (m_HalfLambert ? 32u : 0u);
    lighting.sphereEnvStrength = m_SphereEnvStrength;
    lighting.posterizeLevels = m_PosterizeLevels;
    lighting.texturePageSize = m_TexturePageSize;

    // Pack retro settings into worldCurvature reserved fields
    lighting.worldCurvature.y = m_DepthSortJitter;
    lighting.worldCurvature.z = m_NormalQuantizeSteps;
    lighting.worldCurvature.w = m_CelShadowMode;

    if (m_ActivePointShadowCount > 0 && lighting.pointLightCount > 1) {
        // Move shadow-casting lights to front: swap with non-shadow lights
        for (u32 s = 0; s < m_ActivePointShadowCount && s < lighting.pointLightCount; ++s) {
            auto& shadowLight = m_ShadowPointLights[s];
            // Find this light in the UBO array by matching position
            for (u32 j = s; j < lighting.pointLightCount; ++j) {
                auto& uboLight = lighting.pointLights[j];
                f32 dx = uboLight.position.x - shadowLight.position.x;
                f32 dy = uboLight.position.y - shadowLight.position.y;
                f32 dz = uboLight.position.z - shadowLight.position.z;
                if (dx*dx + dy*dy + dz*dz < 0.001f) {
                    if (j != s) {
                        std::swap(lighting.pointLights[s], lighting.pointLights[j]);
                    }
                    break;
                }
            }
        }
    }

    if (m_ActiveSpotShadowCount > 0 && lighting.spotLightCount > 1) {
        for (u32 s = 0; s < m_ActiveSpotShadowCount && s < lighting.spotLightCount; ++s) {
            auto& shadowLight = m_ShadowSpotLights[s];
            for (u32 j = s; j < lighting.spotLightCount; ++j) {
                auto& uboLight = lighting.spotLights[j];
                f32 dx = uboLight.position.x - shadowLight.position.x;
                f32 dy = uboLight.position.y - shadowLight.position.y;
                f32 dz = uboLight.position.z - shadowLight.position.z;
                if (dx*dx + dy*dy + dz*dz < 0.001f) {
                    if (j != s) {
                        std::swap(lighting.spotLights[s], lighting.spotLights[j]);
                    }
                    break;
                }
            }
        }
    }

    if (m_WindSystem) {
        lighting.windData = m_WindSystem->GetWindVector();
    } else {
        lighting.windData = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    lighting.fogParams = Math::Vector4(m_FogDensity, m_FogStart, m_FogEnd, m_FogHeightFalloff);
    lighting.fogColorSnow = Math::Vector4(m_FogColor.x, m_FogColor.y, m_FogColor.z, m_SnowIntensity);

    // Look up cached player entity position for vegetation stepping (O(1) instead of linear scan)
    lighting.playerPosition = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    if (m_CachedPlayerEntity != INVALID_ENTITY) {
        auto* transform = m_World->GetComponent<TransformComponent>(m_CachedPlayerEntity);
        if (transform) {
            lighting.playerPosition = Math::Vector4(
                transform->position.x, transform->position.y, transform->position.z,
                1.5f);  // w = step radius
        }
    }

    // World curvature
    lighting.worldCurvature = Math::Vector4(m_WorldCurvature, 0.0f, 0.0f, 0.0f);

    // Sky reflection color for water/ice fresnel
    {
        const auto& skyConfig = m_Skybox.GetConfig();
        Math::Vector3 skyCol(0.4f, 0.5f, 0.7f); // fallback matches old hardcoded value
        if (skyConfig.type == Renderer::SkyboxType::Procedural) {
            skyCol = Math::Vector3(
                skyConfig.horizonColor.x * 0.6f + skyConfig.topColor.x * 0.4f,
                skyConfig.horizonColor.y * 0.6f + skyConfig.topColor.y * 0.4f,
                skyConfig.horizonColor.z * 0.6f + skyConfig.topColor.z * 0.4f);
        } else if (skyConfig.type == Renderer::SkyboxType::SolidColor) {
            skyCol = skyConfig.solidColor;
        }
        lighting.skyReflectColor = Math::Vector4(skyCol.x, skyCol.y, skyCol.z, m_LightRampMode);
    }

    // Query SH light probe irradiance at camera position
    if (m_SHLighting && m_SceneComposition.mode == SceneRenderMode::Scene3D && m_Camera) {
        auto irr = m_SHLighting->GetIrradiance(m_Camera->GetPosition(), Math::Vector3(0.0f, 1.0f, 0.0f));
        lighting.shProbeIrradiance = Math::Vector4(irr.x, irr.y, irr.z, 1.0f);
    } else {
        lighting.shProbeIrradiance = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Query reflection probe at camera position for box-projected reflections
    if (m_ReflectionProbes && m_SceneComposition.mode == SceneRenderMode::Scene3D && m_Camera) {
        auto probe = m_ReflectionProbes->FindNearestProbe(m_World, m_Camera->GetPosition());
        if (probe.intensity > 0.0f) {
            lighting.reflectionProbePosition = Math::Vector4(
                probe.probePosition.x, probe.probePosition.y, probe.probePosition.z, probe.intensity);
            lighting.reflectionProbeBoxMin = Math::Vector4(
                probe.boxMin.x, probe.boxMin.y, probe.boxMin.z, probe.blendDistance);
            // w = isBaked (1.0 = baked cubemap at binding 19, 0.0 = skybox gradient fallback)
            lighting.reflectionProbeBoxMax = Math::Vector4(
                probe.boxMax.x, probe.boxMax.y, probe.boxMax.z, probe.isBaked);

            // When the active probe has a baked cubemap, update the descriptor for binding 19
            if (probe.isBaked > 0.5f && m_ReflectionProbes->HasActiveBakedCubemap()) {
                UpdateProbeCubemapDescriptor();
            }
        } else {
            lighting.reflectionProbePosition = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            lighting.reflectionProbeBoxMin = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            lighting.reflectionProbeBoxMax = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }
    } else {
        lighting.reflectionProbePosition = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        lighting.reflectionProbeBoxMin = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        lighting.reflectionProbeBoxMax = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    (*m_ActiveLightingBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&lighting, sizeof(lighting));

    // Cache lighting data for RT/path tracer NEE access
    m_CachedLightingData = lighting;

    // Upload shadow data SSBO for point/spot light shadows
    if (m_ShadowDataBuffer && m_ShadowsEnabled) {
        ShadowDataSSBO shadowData{};
        shadowData.pointShadowCount = static_cast<i32>(m_ActivePointShadowCount);
        shadowData.spotShadowCount = static_cast<i32>(m_ActiveSpotShadowCount);

        for (u32 i = 0; i < m_ActivePointShadowCount; ++i) {
            auto& sl = m_ShadowPointLights[i];
            shadowData.pointLightParams[i] = Math::Vector4(sl.position.x, sl.position.y, sl.position.z, sl.range);
            for (u32 f = 0; f < 6; ++f) {
                shadowData.pointFaceViewProj[i * 6 + f] =
                    Renderer::PointLightShadowMap::ComputeFaceViewProj(sl.position, sl.range, f);
            }
        }

        for (u32 i = 0; i < m_ActiveSpotShadowCount; ++i) {
            auto& sl = m_ShadowSpotLights[i];
            shadowData.spotViewProj[i] =
                Renderer::SpotLightShadowMap::ComputeViewProj(sl.position, sl.direction, sl.outerConeAngle, sl.range);
        }

        m_ShadowDataBuffer->UploadData(&shadowData, sizeof(shadowData));
    }
}

void RenderSystem::BuildMaterialSSBO() {
    if (m_MaterialSSBOBuilt) return;
    m_MaterialSSBOBuilt = true;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Collect all renderable entities and build material data
    const auto& meshEntities = m_World->GetEntitiesWithComponent<MeshComponent>();
    u32 entityCount = static_cast<u32>(meshEntities.size());
    if (entityCount == 0) {
        // Upload a single default material so the SSBO is never empty
        MaterialComponent defaultMat;
        defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        MaterialGPU gpu = MaterialGPU::FromComponent(defaultMat);
        m_MaterialBuffers[currentFrame]->UploadData(&gpu, sizeof(gpu), 0);
        m_MaterialSSBOCount = 1;
        m_MaterialSSBODirty = false;
        return;
    }

    // Fast path: if materials aren't dirty and entity count hasn't changed,
    // skip the per-entity iteration and just re-upload the cached staging buffer
    // to this frame's GPU buffer (needed because each frame-in-flight has its own buffer).
    if (!m_MaterialSSBODirty && m_MaterialSSBOCount > 0 && entityCount == m_MaterialSSBOCount) {
        usize uploadSize = static_cast<usize>(m_MaterialSSBOStride) * m_MaterialSSBOCount;
        m_MaterialBuffers[currentFrame]->UploadData(m_MaterialSSBOData.data(), uploadSize, 0);
        return;
    }

    // Full rebuild path: entity count changed or materials are dirty
    m_EntityMaterialIndex.clear();
    m_MaterialSSBOCount = 0;

    // Grow GPU buffer if needed (recreate with larger capacity)
    if (entityCount > m_MaterialSSBOCapacity) {
        u32 newCapacity = entityCount + (entityCount / 2);  // 1.5x growth
        if (newCapacity < 256) newCapacity = 256;
        usize bufferSize = static_cast<usize>(m_MaterialSSBOStride) * newCapacity;

        m_Renderer->GetContext()->WaitForGPU();
        m_MaterialBuffers[currentFrame] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!m_MaterialBuffers[currentFrame]->Create(bufferSize, Renderer::BufferUsage::Storage, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to grow material SSBO to %u entries", newCapacity);
            return;
        }
        m_MaterialSSBOCapacity = newCapacity;

        // Must update the descriptor set to point to the new buffer
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_MaterialBuffers[currentFrame]->GetBuffer();
        bufInfo.offset = 0;
        bufInfo.range = m_MaterialSSBOStride;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSets[currentFrame];
        write.dstBinding = 2;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufInfo;
        vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &write, 0, nullptr);

        // Also update offscreen descriptor sets that share this material buffer
        for (u32 v = 0; v < MAX_SPLITSCREEN_VIEWPORTS; ++v) {
            u32 offIdx = GetOffscreenBufferIndex(currentFrame, v);
            if (offIdx < m_OffscreenDescriptorSets.size()) {
                VkWriteDescriptorSet offWrite = write;
                offWrite.dstSet = m_OffscreenDescriptorSets[offIdx];
                vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &offWrite, 0, nullptr);
            }
        }
    }

    // Resize CPU staging buffer
    usize totalBytes = static_cast<usize>(m_MaterialSSBOStride) * entityCount;
    if (m_MaterialSSBOData.size() < totalBytes) {
        m_MaterialSSBOData.resize(totalBytes, 0);
    }

    // Helper: look up bindless texture handle for a cached texture pointer
    auto lookupBindless = [this](Renderer::Texture* tex) -> u32 {
        if (!tex || !m_BindlessManager) return m_DefaultBindlessHandle;
        auto it = m_TextureBindlessHandles.find(tex);
        return (it != m_TextureBindlessHandles.end()) ? it->second : m_DefaultBindlessHandle;
    };

    // Fill material data for each entity
    u32 index = 0;
    for (Entity entity : meshEntities) {
        MaterialGPU materialGPU;
        MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material) {
            materialGPU = MaterialGPU::FromComponent(*material);
            // Populate bindless texture indices from cached texture pointers
            materialGPU.baseColorTexIdx         = lookupBindless(material->cachedBaseColorTexture);
            materialGPU.heightTexIdx            = lookupBindless(material->cachedHeightTexture);
            materialGPU.normalTexIdx            = lookupBindless(material->cachedNormalTexture);
            materialGPU.metallicRoughnessTexIdx = lookupBindless(material->cachedMetallicRoughnessTexture);
            materialGPU.emissiveTexIdx          = lookupBindless(material->cachedEmissiveTexture);
            materialGPU.matcapTexIdx            = lookupBindless(material->cachedMatcapTexture);
        } else {
            MaterialComponent defaultMat;
            defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
            defaultMat.metallic = 0.0f;
            defaultMat.roughness = 0.5f;
            materialGPU = MaterialGPU::FromComponent(defaultMat);
        }

        // Apply per-entity ArtStyleComponent overrides to MaterialGPU
        ArtStyleComponent* artStyle = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
        if (artStyle && artStyle->style != ArtStyleType::Inherit) {
            if (artStyle->style == ArtStyleType::MaterialExpression) {
                if (artStyle->matExpr_sssIntensity > 0.0f) {
                    materialGPU.sssIntensity = artStyle->matExpr_sssIntensity;
                    materialGPU.sssRadius = artStyle->matExpr_sssRadius;
                    materialGPU.sssColor = artStyle->matExpr_sssColor;
                }
            }
            if (artStyle->style == ArtStyleType::CelToon && material) {
                // Force outline via MaterialGPU (outlineWidth/color are render-time only,
                // handled in the outline pass, not in the SSBO)
            }
        }

        // Write to aligned offset in staging buffer
        usize offset = static_cast<usize>(m_MaterialSSBOStride) * index;
        std::memcpy(m_MaterialSSBOData.data() + offset, &materialGPU, sizeof(MaterialGPU));

        m_EntityMaterialIndex[static_cast<u64>(entity)] = index;
        ++index;
    }

    m_MaterialSSBOCount = index;

    // Single batched upload to GPU
    usize uploadSize = static_cast<usize>(m_MaterialSSBOStride) * m_MaterialSSBOCount;
    m_MaterialBuffers[currentFrame]->UploadData(m_MaterialSSBOData.data(), uploadSize, 0);

    m_MaterialSSBODirty = false;
}

u32 RenderSystem::GetMaterialIndex(Entity entity) const {
    auto it = m_EntityMaterialIndex.find(static_cast<u64>(entity));
    return (it != m_EntityMaterialIndex.end()) ? it->second : 0;
}

void RenderSystem::UpdateMaterialBuffer(Entity entity) {
    // Legacy path: if BuildMaterialSSBO has already run, this is a no-op.
    // For entities not in the SSBO (e.g., RenderToTarget one-off draws),
    // write directly to index 0 as a fallback.
    if (m_MaterialSSBOBuilt) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    MaterialGPU materialGPU;
    MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
    if (material) {
        materialGPU = MaterialGPU::FromComponent(*material);
    } else {
        MaterialComponent defaultMat;
        defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        defaultMat.metallic = 0.0f;
        defaultMat.roughness = 0.5f;
        materialGPU = MaterialGPU::FromComponent(defaultMat);
    }
    m_MaterialBuffers[currentFrame]->UploadData(&materialGPU, sizeof(materialGPU), 0);
}

// Legacy wrapper for RenderToTarget which needs both frame + material updates
void RenderSystem::UpdateUniformBuffer(Entity entity) {
    UpdateFrameUniforms();
    UpdateMaterialBuffer(entity);
}

void RenderSystem::SetBackfaceCullingEnabled(bool enabled) {
    if (m_BackfaceCulling == enabled) return;
    m_BackfaceCulling = enabled;
    m_PendingRecreation = PendingRecreationType::PipelineOnly;
}

void RenderSystem::SetWireframeEnabled(bool enabled) {
    if (m_WireframeMode == enabled) return;
    m_WireframeMode = enabled;
    m_PendingRecreation = PendingRecreationType::PipelineOnly;
}

void RenderSystem::SetShadowDistance(f32 d) {
    m_ShadowDistance = d;
    if (m_ShadowMap) m_ShadowMap->SetShadowDistance(d);
}

f32 RenderSystem::GetShadowStrength() const {
    return m_ShadowMap ? m_ShadowMap->GetShadowStrength() : 1.0f;
}

void RenderSystem::SetShadowStrength(f32 s) {
    if (m_ShadowMap) m_ShadowMap->SetShadowStrength(s);
}

f32 RenderSystem::GetShadowSoftness() const {
    return m_ShadowMap ? m_ShadowMap->GetShadowSoftness() : 0.0f;
}

void RenderSystem::SetShadowSoftness(f32 s) {
    if (m_ShadowMap) m_ShadowMap->SetShadowSoftness(s);
}

u32 RenderSystem::GetShadowResolution() const {
    return m_ShadowMap ? m_ShadowMap->GetResolution() : 2048;
}

void RenderSystem::SetShadowResolution(u32 r) {
    if (!m_ShadowMap) return;
    if (r < 512) r = 512;
    if (r > 4096) r = 4096;
    // Skip if resolution hasn't actually changed
    if (r == m_ShadowMap->GetResolution()) return;
    // Defer the actual resize to FlushPendingChanges() where the GPU is already idle
    m_PendingShadowResolution = r;
    m_PendingRecreation = PendingRecreationType::PipelineOnly;
}

void RenderSystem::SetHDREnabled(bool enabled) {
    if (!m_Renderer) return;
    if (m_Renderer->IsHDREnabled() == enabled) return;

    // VulkanRenderer::SetHDREnabled handles: swapchain recreate, render pass recreate,
    // framebuffer recreate, and notifies resize callbacks. After that, our pipelines
    // (which reference the render pass) must be recreated too.
    m_Renderer->SetHDREnabled(enabled);
    RecreatePipelines(true);  // GPU already idle from VulkanRenderer::SetHDREnabled
}

void RenderSystem::SetAAMode(u32 mode) {
    if (mode == m_AAMode) return;

    u32 oldMode = m_AAMode;
    m_AAMode = mode;

    // MSAA modes (4=2x, 5=4x, 6=8x) require render pass + framebuffer + pipeline recreation.
    // This is unsafe mid-frame (crashes NVIDIA driver). Defer to the start of the next
    // RenderSystem::Update() via m_PendingMSAAChange flag.
    bool oldIsMSAA = oldMode >= 4 && oldMode <= 6;
    bool newIsMSAA = mode >= 4 && mode <= 6;

    if (newIsMSAA || oldIsMSAA) {
        m_PendingMSAAChange = true;
    }
}

void RenderSystem::ApplyPendingMSAAChange() {
    m_PendingMSAAChange = false;
    if (!m_Renderer) return;

    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    if (m_AAMode == 4) samples = VK_SAMPLE_COUNT_2_BIT;
    else if (m_AAMode == 5) samples = VK_SAMPLE_COUNT_4_BIT;
    else if (m_AAMode == 6) samples = VK_SAMPLE_COUNT_8_BIT;

    if (!m_Renderer->SetMSAASamples(samples)) {
        ENJIN_LOG_WARN(Renderer, "MSAA %dx not supported, reverting to no MSAA", static_cast<int>(samples));
        m_AAMode = 0;  // Fall back to no AA
        m_Renderer->SetMSAASamples(VK_SAMPLE_COUNT_1_BIT);
    }

    // Render pass changed — all pipelines must be recreated
    RecreatePipelines(true);  // GPU already idle from SetMSAASamples

    // Invalidate all entity render data so buffers are re-created
    for (auto& rd : m_EntityRenderData) {
        if (rd.valid) rd.Invalidate();
    }
}

u32 RenderSystem::GetMaxMSAASamples() const {
    if (!m_Renderer || !m_Renderer->GetContext()) return 1;
    return static_cast<u32>(m_Renderer->GetContext()->GetMaxUsableSampleCount());
}

void RenderSystem::RecreatePipelines(bool gpuAlreadyIdle) {
    if (!m_Pipeline || !m_Initialized) return;

    // Wait for GPU to finish all in-flight work before destroying pipelines
    // Skip if caller guarantees GPU is already idle (e.g., deferred recreation already waited)
    if (!gpuAlreadyIdle && m_Renderer) {
        m_Renderer->WaitForAllFrames();
    }

    // Destroy all pipelines that share the descriptor set layout
    m_OffscreenWireframeOverlayPipeline.reset();
    m_OffscreenOutlinePipeline.reset();
    m_OffscreenLinePipeline.reset();
    m_OffscreenPipeline.reset();
    m_WireframeOverlayPipeline.reset();
    m_OutlinePipeline.reset();
    m_LinePipeline.reset();
    m_ShadowPipeline.reset();
    m_Pipeline.reset();

    // Destroy old descriptor pool (implicitly frees all descriptor sets allocated from it)
    if (m_DescriptorPool != VK_NULL_HANDLE && m_Renderer && m_Renderer->GetContext()) {
        vkDestroyDescriptorPool(m_Renderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
        m_DescriptorSets.clear();
    }

    // Recreate main pipeline (creates new descriptor set layout)
    CreatePipeline();

    // Recreate descriptor sets with the new pipeline's layout, then dependent pipelines
    if (m_Pipeline) {
        CreateDescriptorSets();
        CreateLinePipeline();
        CreateOutlinePipeline();
        CreateWireframeOverlayPipeline();
        CreateShadowPipeline();
        CreatePointShadowPipeline();
        CreateSpotShadowPipeline();

        // Recreate offscreen pipelines if we have a cached offscreen render pass
        if (m_OffscreenRenderPass != VK_NULL_HANDLE) {
            RecreateEffectPipelinesForRenderPass(m_OffscreenRenderPass);
        }
    }
}

// ─── Shader Hot-Reload ────────────────────────────────────────────────────────

void RenderSystem::FindShaderDirectory() {
    namespace fs = std::filesystem;
    // Probe common relative paths from the working directory
    const char* candidates[] = {
        "Engine/shaders",
        "../Engine/shaders",
        "../../Engine/shaders",
        "../../../Engine/shaders",
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        fs::path p(candidate);
        if (fs::is_directory(p, ec) && !ec) {
            m_ShaderDir = fs::canonical(p, ec).string();
            if (!ec) {
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: found shader directory at %s", m_ShaderDir.c_str());
                return;
            }
        }
    }
    ENJIN_LOG_INFO(Renderer, "Shader hot-reload: shader source directory not found, hot-reload disabled");
    m_ShaderDir.clear();
}

static bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) return false;
    out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return true;
}

void RenderSystem::SetupShaderWatchers() {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty()) return;

    auto shaderPath = [&](const char* name) -> std::string {
        return (fs::path(m_ShaderDir) / name).string();
    };

    // Main pipeline shaders (triangle.vert/frag)
    auto mainReload = [this](const std::string& changedFile) { ReloadMainShaders(changedFile); };
    m_ShaderWatcher.Watch(shaderPath("triangle.vert"), mainReload);
    m_ShaderWatcher.Watch(shaderPath("triangle.frag"), mainReload);

    // Shadow pipeline (vertex-only)
    m_ShaderWatcher.Watch(shaderPath("shadow.vert"), [this](const std::string&) { ReloadShadowShaders(); });

    // Skybox pipeline
    auto skyboxReload = [this](const std::string&) { ReloadSkyboxShaders(); };
    m_ShaderWatcher.Watch(shaderPath("skybox.vert"), skyboxReload);
    m_ShaderWatcher.Watch(shaderPath("skybox.frag"), skyboxReload);

    // Sub-renderer shaders — each gets a lambda that calls ReloadShaders on the sub-renderer
    VkDescriptorSetLayout layout = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
    std::string dir = m_ShaderDir;

    // Grass
    auto grassReload = [this, dir, layout](const std::string&) {
        if (m_GrassRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_GrassRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: grass shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("grass.vert"), grassReload);
    m_ShaderWatcher.Watch(shaderPath("grass.frag"), grassReload);

    // Shrub
    auto shrubReload = [this, dir](const std::string&) {
        if (m_ShrubRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_ShrubRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: shrub shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("shrub.vert"), shrubReload);
    m_ShaderWatcher.Watch(shaderPath("shrub.frag"), shrubReload);

    // Tree
    auto treeReload = [this, dir](const std::string&) {
        if (m_TreeRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_TreeRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: tree shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("tree.vert"), treeReload);
    m_ShaderWatcher.Watch(shaderPath("tree.frag"), treeReload);

    // Particle + Weather + Fluid (shared shaders)
    auto particleReload = [this, dir](const std::string&) {
        VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
        bool any = false;
        if (m_ParticleRenderer && m_ParticleRenderer->ReloadShaders(dir, l)) any = true;
        if (m_WeatherRenderer && m_WeatherRenderer->ReloadShaders(dir, l)) any = true;
        if (m_FluidRenderer && m_FluidRenderer->ReloadShaders(dir, l)) any = true;
        if (any) ENJIN_LOG_INFO(Renderer, "Shader hot-reload: particle/weather/fluid shaders reloaded");
    };
    m_ShaderWatcher.Watch(shaderPath("particle.vert"), particleReload);
    m_ShaderWatcher.Watch(shaderPath("particle.frag"), particleReload);

    // Sprite (unlit + lit)
    auto spriteReload = [this, dir](const std::string&) {
        if (m_SpriteBatchRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_SpriteBatchRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: sprite shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("sprite.vert"), spriteReload);
    m_ShaderWatcher.Watch(shaderPath("sprite.frag"), spriteReload);
    m_ShaderWatcher.Watch(shaderPath("sprite_lit.vert"), spriteReload);
    m_ShaderWatcher.Watch(shaderPath("sprite_lit.frag"), spriteReload);

    ENJIN_LOG_INFO(Renderer, "Shader hot-reload: watching %zu shader files", m_ShaderWatcher.GetWatchCount());
}

void RenderSystem::ReloadMainShaders(const std::string& changedFile) {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty() || !m_Renderer || !m_Initialized) return;

    std::string vertPath = (fs::path(m_ShaderDir) / "triangle.vert").string();
    std::string fragPath = (fs::path(m_ShaderDir) / "triangle.frag").string();

    // Read GLSL source
    std::string vertSource, fragSource;
    if (!ReadFileToString(vertPath, vertSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read %s", vertPath.c_str());
        return;
    }
    if (!ReadFileToString(fragPath, fragSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read %s", fragPath.c_str());
        return;
    }

    // Compile to temporary shaders — if either fails, keep existing
    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSource, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: triangle.vert compilation failed, keeping old shader");
        return;
    }

    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSource, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: triangle.frag compilation failed, keeping old shader");
        return;
    }

    // Both compiled — defer swap and pipeline recreation to next frame start
    m_PendingVertexShader = std::move(tempVert);
    m_PendingFragmentShader = std::move(tempFrag);
    m_PendingRecreation = PendingRecreationType::MainShader;
}

void RenderSystem::ReloadSkyboxShaders() {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty() || !m_Renderer || !m_Initialized) return;
    if (m_SkyboxPipelineHandle == VK_NULL_HANDLE) return; // skybox not initialized

    std::string vertPath = (fs::path(m_ShaderDir) / "skybox.vert").string();
    std::string fragPath = (fs::path(m_ShaderDir) / "skybox.frag").string();

    std::string vertSource, fragSource;
    if (!ReadFileToString(vertPath, vertSource) || !ReadFileToString(fragPath, fragSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read skybox shader files");
        return;
    }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSource, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: skybox.vert compilation failed");
        return;
    }

    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSource, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: skybox.frag compilation failed");
        return;
    }

    // Shaders validated — defer pipeline recreation to next frame start
    m_PendingRecreation = PendingRecreationType::SkyboxShader;
}

void RenderSystem::ReloadShadowShaders() {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty() || !m_Renderer || !m_Initialized) return;
    if (!m_ShadowMap || !m_ShadowPipeline) return;

    std::string vertPath = (fs::path(m_ShaderDir) / "shadow.vert").string();
    std::string vertSource;
    if (!ReadFileToString(vertPath, vertSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read shadow.vert");
        return;
    }

    // Shadow pipeline uses the main vertex shader — compile and test
    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSource, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: shadow.vert compilation failed");
        return;
    }

    // Defer swap to m_ShadowVertexShader and pipeline recreation to next frame start
    m_PendingVertexShader = std::move(tempVert);
    m_PendingRecreation = PendingRecreationType::ShadowShader;
}

// ─── End Shader Hot-Reload ───────────────────────────────────────────────────

void RenderSystem::CreateDefaultMesh() {
    m_DefaultEntity = m_World->CreateEntity();

    // Add name
    NameComponent& name = m_World->AddComponent<NameComponent>(m_DefaultEntity);
    name.name = "Sphere";

    // Add transform at origin
    TransformComponent& transform = m_World->AddComponent<TransformComponent>(m_DefaultEntity);
    transform.position = Math::Vector3(0.0f, 1.0f, 0.0f);
    transform.scale = Math::Vector3(1.0f);

    // Add sphere mesh using MeshFactory
    MeshComponent& mesh = m_World->AddComponent<MeshComponent>(m_DefaultEntity);
    mesh = Renderer::MeshFactory::CreateSphere(0.5f, 32, 16);

    // Add a nice default material
    MaterialComponent& material = m_World->AddComponent<MaterialComponent>(m_DefaultEntity);
    material.baseColor = Math::Vector3(0.7f, 0.7f, 0.8f);
    material.metallic = 0.1f;
    material.roughness = 0.4f;

    SetupEntityBuffers(m_DefaultEntity);
    ENJIN_LOG_INFO(Renderer, "Created default sphere entity: %llu", m_DefaultEntity);
}

void RenderSystem::RenderEntity(Entity entity) {
    if (!m_Pipeline || !m_Renderer) {
        return;
    }

    // Skip entities already drawn by the multi-draw indirect batch
    if (m_GPUCullingEnabled && !m_IsEditorMode &&
        static_cast<usize>(entity) < m_IndirectDrawn.size() &&
        m_IndirectDrawn[static_cast<usize>(entity)]) {
        return;
    }

    // Use cached storage pointers (refreshed once per frame in RefreshStorageCache)
    // to skip the type-ID hash map lookup that GetComponent<T>() does on every call.
    TransformComponent* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
    MeshComponent* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;

    if (!transform || !mesh || !mesh->IsValid()) {
        return;
    }

    // Skip invisible entities (safety net — callers should also check)
    if (!transform->visible) return;

    EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
        ? &m_EntityRenderData[static_cast<usize>(entity)] : SetupEntityBuffers(entity);
    if (!pRD) return;
    EntityRenderData& renderData = *pRD;

    // Get command buffer
    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    // Bind material SSBO at this entity's dynamic offset (or fallback to legacy upload)
    if (m_MaterialSSBOBuilt) {
        u32 matIdx = GetMaterialIndex(entity);
        u32 dynOffset = matIdx * m_MaterialSSBOStride;
        u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &dynOffset);
    } else {
        UpdateMaterialBuffer(entity);
        u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
        u32 zeroOffset = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOffset);
    }

    // Push model matrix — skinned meshes use identity (skinning already transforms to world space)
    Renderer::PushConstants pushConstants{};
    {
        AnimatorComponent* ac = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
        pushConstants.model = (ac && ac->animator.GetSkeleton())
            ? Math::Matrix4::Identity()
            : ECS::ComputeWorldMatrix(m_World, entity);
    }

    // Set material data (cached storage avoids type-ID lookup)
    MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
    Renderer::Texture* boundTexture = nullptr;

    // Cached texture pointers for batched descriptor update
    Renderer::Texture* texHeight = nullptr;
    Renderer::Texture* texNormal = nullptr;
    Renderer::Texture* texMR = nullptr;
    Renderer::Texture* texEmissive = nullptr;
    Renderer::Texture* texMatcap = nullptr;

    if (material) {
        pushConstants.baseColor = material->baseColor;
        pushConstants.metallic = material->metallic;
        pushConstants.emissiveColor = material->emissiveColor;
        pushConstants.roughness = material->roughness;
        pushConstants.emissiveStrength = material->emissiveStrength;
        pushConstants.opacity = material->opacity;
        pushConstants.alphaCutoff = material->alphaCutoff;

        // Resolve textures using cache (avoids per-frame string hash lookups)
        if (material->textureCacheDirty) {
            // Cache miss - load all textures and cache pointers
            if (!material->baseColorTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->baseColorTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedBaseColorTexture = tex.get();
                    material->baseColorTexture = 1;
                }
            }
            if (!material->heightTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->heightTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedHeightTexture = tex.get();
                    material->heightTexture = 1;
                }
            }
            if (!material->normalTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->normalTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedNormalTexture = tex.get();
                    material->normalTexture = 1;
                }
            }
            if (!material->metallicRoughnessTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->metallicRoughnessTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedMetallicRoughnessTexture = tex.get();
                    material->metallicRoughnessTexture = 1;
                }
            }
            if (!material->emissiveTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedEmissiveTexture = tex.get();
                    material->emissiveTexture = 1;
                }
            }
            if (!material->matcapTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->matcapTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedMatcapTexture = tex.get();
                    material->matcapTexture = 1;
                }
            }
            material->textureCacheDirty = false;
            material->cachedTextureKey = { material->cachedBaseColorTexture,
                material->cachedHeightTexture, material->cachedNormalTexture,
                material->cachedMetallicRoughnessTexture, material->cachedEmissiveTexture,
                material->cachedMatcapTexture };
        }

        // Use cached texture pointers
        boundTexture = material->cachedBaseColorTexture;
        texHeight = material->cachedHeightTexture;
        texNormal = material->cachedNormalTexture;
        texMR = material->cachedMetallicRoughnessTexture;
        texEmissive = material->cachedEmissiveTexture;
        texMatcap = material->cachedMatcapTexture;

        // Compute flags same as MaterialGPU::FromComponent
        pushConstants.flags = 0;
        if (material->doubleSided) pushConstants.flags |= 1;
        if (material->castShadows) pushConstants.flags |= 2;
        if (material->receiveShadows) pushConstants.flags |= 4;
        pushConstants.flags |= (static_cast<i32>(material->alphaMode) << 8);
        if (boundTexture != nullptr) pushConstants.flags |= (1 << 16);
        if (material->normalTexture >= 0) pushConstants.flags |= (1 << 17);
        if (material->metallicRoughnessTexture >= 0) pushConstants.flags |= (1 << 18);
        if (material->emissiveTexture >= 0) pushConstants.flags |= (1 << 19);
        if (material->heightTexture >= 0) pushConstants.flags |= (1 << 10);
        // Retro flags (per-material)
        if (material->flatShading) pushConstants.flags |= (1 << 20);
        if (material->affineTexturing) pushConstants.flags |= (1 << 21);
        if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
        if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
        if (material->uvQuantize) pushConstants.flags |= (1 << 12);
        if (material->gouraudOnly) pushConstants.flags |= (1 << 13);
        pushConstants.flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
        pushConstants.flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
        pushConstants.flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
        pushConstants.parallaxScale = material->parallaxScale;
        // Artistic surface params (reused push constant slots)
        pushConstants.surfaceParam1 = material->reflectivity;
        pushConstants.surfaceParam2 = material->fresnelPower;
        pushConstants.surfaceParam3 = material->rimLightStrength;
        // Dithered gradient: encode bands + pattern into surfaceParam1
        if (material->ditherGradient) {
            pushConstants.flags |= (1 << 20); // Force flat shading
            pushConstants.surfaceParam1 = 100.0f + static_cast<f32>(material->ditherGradientBands)
                + static_cast<f32>(material->ditherGradientPattern) * 0.1f;
        }
        // Dithered transparency: encode pattern + opacity + blend color into surfaceParams
        if (material->ditherTransparency) {
            pushConstants.surfaceParam1 = 200.0f + static_cast<f32>(material->ditherTransPattern);
            pushConstants.surfaceParam2 = material->ditherTransOpacity;
            u32 r = static_cast<u32>(material->ditherTransBlendColor.x * 1023.0f) & 0x3FF;
            u32 g = static_cast<u32>(material->ditherTransBlendColor.y * 1023.0f) & 0x3FF;
            u32 b = static_cast<u32>(material->ditherTransBlendColor.z * 1023.0f) & 0x3FF;
            u32 packed = (r << 20) | (g << 10) | b;
            pushConstants.surfaceParam3 = *reinterpret_cast<f32*>(&packed);
        }
        // Elemental surface effects: encode char/wet/snow/frost into surfaceParams
        if (!material->ditherGradient && !material->ditherTransparency) {
            auto* elemSurface = m_World->GetComponent<ECS::ElementalSurfaceComponent>(entity);
            if (elemSurface && (elemSurface->charAmount > 0.01f || elemSurface->wetness > 0.01f ||
                                elemSurface->snowCoverage > 0.01f || elemSurface->frostAmount > 0.01f)) {
                pushConstants.surfaceParam1 = 300.0f + elemSurface->charAmount;
                pushConstants.surfaceParam2 = elemSurface->wetness + std::floor(elemSurface->snowCoverage * 256.0f);
                pushConstants.surfaceParam3 = elemSurface->frostAmount;
            }
        }
        // Procedural surface noise: encode scale/strength into surfaceParams (range 400+)
        // Only when no other effect has claimed the surfaceParam slots
        if (!material->ditherGradient && !material->ditherTransparency &&
            material->surfaceNoiseScale > 0.0f && pushConstants.surfaceParam1 < 100.0f) {
            pushConstants.surfaceParam1 = 400.0f + material->surfaceNoiseScale;
            pushConstants.surfaceParam2 = material->surfaceNoiseStrength;
        }
    } else {
        // Default material (light gray, non-metallic)
        pushConstants.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        pushConstants.metallic = 0.0f;
        pushConstants.emissiveColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        pushConstants.roughness = 0.5f;
        pushConstants.emissiveStrength = 0.0f;
        pushConstants.opacity = 1.0f;
        pushConstants.alphaCutoff = 0.5f;
        pushConstants.flags = 0;
        pushConstants.parallaxScale = 0.0f;
    }

    // Global retro overrides (OR with per-material — global forces on)
    if (m_GlobalFlatShading) pushConstants.flags |= (1 << 20);
    if (m_GlobalAffineTexturing) pushConstants.flags |= (1 << 21);
    if (m_GlobalVertexSnapping) pushConstants.flags |= (1 << 22);
    if (m_GlobalStippleTransparency) pushConstants.flags |= (1 << 23);
    if (m_GlobalUVQuantize) pushConstants.flags |= (1 << 12);
    if (m_GlobalGouraudOnly) pushConstants.flags |= (1 << 13);
    if (m_GlobalVertexSnapping && m_GlobalVertexSnapResolution > 0) {
        pushConstants.flags = (pushConstants.flags & ~(0x1F << 24)) | (static_cast<i32>((m_GlobalVertexSnapResolution / 8) & 0x1F) << 24);
    }

    // Sprite texture override — use sprite's texturePath instead of material's
    Sprite2DComponent* spriteComp = m_World->GetComponent<Sprite2DComponent>(entity);
    if (spriteComp && !spriteComp->texturePath.empty()) {
        auto tex = GetOrLoadTexture(spriteComp->texturePath);
        if (tex && tex->IsValid()) {
            boundTexture = tex.get();
            pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
        }
        pushConstants.baseColor = spriteComp->tint;
        pushConstants.opacity = spriteComp->alpha;
    }

    // Tilemap texture override — use tileset texture path
    TilemapComponent* tilemapComp = m_World->GetComponent<TilemapComponent>(entity);
    if (tilemapComp && !tilemapComp->tilesetPath.empty()) {
        auto tex = GetOrLoadTexture(tilemapComp->tilesetPath);
        if (tex && tex->IsValid()) {
            boundTexture = tex.get();
            pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
        }
    }

    // Set wind sway flag for vegetation entities
    VegetationComponent* vegComp = m_World->GetComponent<VegetationComponent>(entity);
    if (vegComp) {
        pushConstants.flags |= (1 << 4); // FLAG_WIND_SWAY
    }

    // Set water surface flag for water volume entities
    WaterVolumeComponent* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
    if (waterVol) {
        pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE — always set, shader handles freeze
        pushConstants.parallaxScale = waterVol->freezeProgress; // repurpose for water (POM skips water)

        if (m_RainActive && waterVol->freezeProgress < 0.5f) {
            pushConstants.flags |= (1 << 6); // FLAG_RAIN_RIPPLES (no ripples on ice)
        }
        if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
            pushConstants.flags |= (1 << 7); // FLAG_WATER_SHORE
            pushConstants.surfaceParam1 = waterVol->shoreWidth;
            pushConstants.surfaceParam2 = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
            pushConstants.surfaceParam3 = waterVol->foamScale;
        }
        if (waterVol->waterType == WaterType::Ocean) {
            pushConstants.flags |= (1 << 11); // FLAG_WATER_OCEAN
        }
        // Lerp base color toward ice color as water freezes
        f32 fp = waterVol->freezeProgress;
        pushConstants.baseColor = Math::Vector3(
            pushConstants.baseColor.x * (1.0f - fp) + waterVol->iceColor.x * fp,
            pushConstants.baseColor.y * (1.0f - fp) + waterVol->iceColor.y * fp,
            pushConstants.baseColor.z * (1.0f - fp) + waterVol->iceColor.z * fp
        );
        pushConstants.opacity = pushConstants.opacity * (1.0f - fp) + waterVol->iceOpacity * fp;
    } else if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) {
        auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
        pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE for Water3D
        pushConstants.parallaxScale = 0.0f; // no freeze
        if (water3d) {
            pushConstants.baseColor = water3d->settings.shallowColor;
            pushConstants.opacity = water3d->settings.opacity;
        }
    }

    // Rasterize text texture if entity has a TextComponent (cached storage)
    TextComponent* textComp = m_CachedTextStorage ? m_CachedTextStorage->Get(entity) : nullptr;
    if (textComp && textComp->dirty && !textComp->fontPath.empty() && !textComp->text.empty()) {
        auto pixels = m_TextRasterizer.Rasterize(*textComp);
        if (!pixels.empty()) {
            auto textTex = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
            if (textTex->CreateFromData(pixels.data(), textComp->textureWidth, textComp->textureHeight, 4)) {
                m_TextTextureCache[entity] = textTex;
            }
        }
        textComp->dirty = false;
    }

    // If entity has a text texture, override the base color texture
    auto textTexIt = m_TextTextureCache.find(entity);
    if (textComp && textTexIt != m_TextTextureCache.end() && textTexIt->second && textTexIt->second->IsValid()) {
        boundTexture = textTexIt->second.get();
        pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
    }

    // Update texture descriptor if entity has a texture
    // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 6)
    UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive, texMatcap);

    // Upload bone matrices for skinned meshes (cached storage avoids type-ID lookup).
    // Always upload when skeleton exists, not just when animation is playing.
    AnimatorComponent* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
    if (animComp && renderData.boneBuffer) {
        const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
        if (!skinningMatrices.empty()) {
            renderData.boneBuffer->UploadData(skinningMatrices.data(),
                skinningMatrices.size() * sizeof(Math::Matrix4));
            UpdateBoneDescriptor(renderData.boneBuffer.get());
            pushConstants.flags |= (1 << 3); // FLAG_SKINNED
        }
    } else {
        // Bind default bone buffer for static meshes
        if (m_DefaultBoneBuffer) {
            UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
        }
    }

    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::PushConstants), &pushConstants);

    // Bind and draw — pool-allocated entities use merged buffer with offsets
    if (renderData.poolAlloc.valid && m_GeometryPool) {
        if (!m_GeometryPoolBound) {
            m_GeometryPool->BindBuffers(commandBuffer);
            m_GeometryPoolBound = true;
        }
        vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                         renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
    } else if (renderData.vertexBuffer && renderData.indexBuffer) {
        VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
        m_GeometryPoolBound = false;  // Non-pool entity invalidates pool binding
    }
    m_DrawCallCount++;
    m_TriangleCount += renderData.indexCount / 3;
}

void RenderSystem::RenderEntityGhost(Entity entity, const Math::Matrix4& modelMatrix,
                                      const Math::Vector3& tint, f32 opacity,
                                      const std::vector<Math::Matrix4>* skinningMatrices) {
    if (!m_Pipeline || !m_Renderer) return;

    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);
    if (!mesh || !mesh->IsValid()) return;

    EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
        ? &m_EntityRenderData[static_cast<usize>(entity)] : SetupEntityBuffers(entity);
    if (!pRD) return;
    EntityRenderData& renderData = *pRD;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Build ghost push constants — override baseColor with tint, set opacity for transparency
    Renderer::PushConstants pushConstants{};
    pushConstants.model = modelMatrix;
    pushConstants.baseColor = tint;
    pushConstants.metallic = 0.0f;
    pushConstants.emissiveColor = Math::Vector3(0, 0, 0);
    pushConstants.roughness = 1.0f;
    pushConstants.emissiveStrength = 0.0f;
    pushConstants.opacity = opacity;
    pushConstants.alphaCutoff = 0.0f;
    // Set alpha blend flag (bit 9 = ALPHA_MODE_BLEND)
    pushConstants.flags = (2 << 8);
    pushConstants.parallaxScale = 0.0f;

    // Bind default white texture for ghost (no texture lookups needed)
    if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
        UpdateEntityTextureDescriptors(m_DefaultWhiteTexture.get(), nullptr, nullptr, nullptr, nullptr);
    }

    // Upload skeletal skinning matrices for 3D onion skin ghosts
    bool hasSkinning = skinningMatrices && !skinningMatrices->empty();
    if (hasSkinning) {
        usize requiredSize = skinningMatrices->size() * sizeof(Math::Matrix4);

        // Lazily create or grow the reusable ghost bone buffer
        if (!m_GhostBoneBuffer || m_GhostBoneBufferCapacity < requiredSize) {
            m_GhostBoneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
            if (m_GhostBoneBuffer->Create(requiredSize, Renderer::BufferUsage::Storage, true)) {
                m_GhostBoneBufferCapacity = requiredSize;
            } else {
                m_GhostBoneBuffer.reset();
                m_GhostBoneBufferCapacity = 0;
                hasSkinning = false;
            }
        }

        if (hasSkinning && m_GhostBoneBuffer) {
            m_GhostBoneBuffer->UploadData(skinningMatrices->data(), requiredSize);
            UpdateBoneDescriptor(m_GhostBoneBuffer.get());
            pushConstants.flags |= (1 << 3); // FLAG_SKINNED
        }
    }

    if (!hasSkinning) {
        // Bind default bone buffer for non-skinned ghosts
        if (m_DefaultBoneBuffer) {
            UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
        }
    }

    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::PushConstants), &pushConstants);

    // Bind and draw
    if (renderData.poolAlloc.valid && m_GeometryPool) {
        if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
        vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                         renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
    } else if (renderData.vertexBuffer && renderData.indexBuffer) {
        VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
    }
    m_DrawCallCount++;
    m_TriangleCount += renderData.indexCount / 3;
}

void RenderSystem::RenderOnionSkinGhosts() {
    if (m_OnionSkinGhosts.empty() || !m_IsEditorMode) return;

    m_LastBound.Reset(); m_GeometryPoolBound = false;  // Reset descriptor cache for ghost pass

    for (const auto& ghost : m_OnionSkinGhosts) {
        // Build model matrix from ghost transform
        TransformComponent ghostTransform;
        ghostTransform.position = ghost.position;
        ghostTransform.rotation = Math::Quaternion::FromEuler(ghost.rotation);
        ghostTransform.scale = ghost.scale;
        Math::Matrix4 modelMatrix = ghostTransform.ToMatrix();

        // Pass skinning matrices for 3D skeletal onion skin ghosts (empty = non-skinned)
        const std::vector<Math::Matrix4>* bones = ghost.skinningMatrices.empty() ? nullptr : &ghost.skinningMatrices;
        RenderEntityGhost(ghost.entity, modelMatrix, ghost.tint, ghost.ghostOpacity * ghost.alpha, bones);
    }
}

void RenderSystem::RenderOutlinePass() {
    if (!m_OutlinePipeline || !m_GeometryOutlinesEnabled || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    m_OutlinePipeline->Bind(commandBuffer);
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_OutlinePipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
    }

    // Cache storage pointers for the outline pass hot loop
    auto* spriteStorageOP = m_World->GetComponentStorage<Sprite2DComponent>();

    for (Entity entity : m_SortedRenderList) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!transform || !transform->visible) continue;

        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        // Skip entities excluded from cel shading (they don't get outlines)
        if (material && material->excludeFromCelShading) continue;
        // Skip 2D sprites
        if (spriteStorageOP && spriteStorageOP->Has(entity)) continue;
        // Skip transparent objects
        if (material && material->alphaMode == MaterialComponent::AlphaMode::Blend) continue;

        EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
            ? &m_EntityRenderData[static_cast<usize>(entity)] : nullptr;
        if (!pRD || !pRD->valid) continue;
        EntityRenderData& renderData = *pRD;

        // Use per-material outline settings if set, then art style, then global
        f32 outlineWidth = m_GeometryOutlineWidth;
        Math::Vector3 outlineColor = m_GeometryOutlineColor;
        if (material && material->outlineWidth > 0.0f) {
            outlineWidth = material->outlineWidth;
            outlineColor = material->outlineColor;
        }
        // Per-entity ArtStyleComponent cel/toon outline override
        auto* artStyleOutline = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
        if (artStyleOutline && artStyleOutline->style == ArtStyleType::CelToon && artStyleOutline->cel_outlineWidth > 0.0f) {
            outlineWidth = artStyleOutline->cel_outlineWidth;
            outlineColor = artStyleOutline->cel_outlineColor;
        }

        // Build push constants — repurpose baseColor for outlineColor, metallic for outlineWidth
        Renderer::PushConstants pc{};
        pc.baseColor = outlineColor;
        pc.metallic = outlineWidth;
        pc.flags = 0;

        // Propagate skinned flag so outline follows skeleton (playing or bind pose)
        auto* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
        if (animComp && renderData.boneBuffer) {
            pc.flags |= (1 << 3); // FLAG_SKINNED
            pc.model = Math::Matrix4::Identity(); // Skinned: identity, bone matrices handle positioning
            const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
            if (!skinningMatrices.empty()) {
                renderData.boneBuffer->UploadData(skinningMatrices.data(),
                    skinningMatrices.size() * sizeof(Math::Matrix4));
            }
            UpdateBoneDescriptor(renderData.boneBuffer.get());
        } else {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) {
                UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
            }
        }

        vkCmdPushConstants(commandBuffer, m_OutlinePipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        // Bind vertex/index buffers and draw
        if (renderData.poolAlloc.valid && m_GeometryPool) {
            if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
            vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                             renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
        } else if (renderData.vertexBuffer && renderData.indexCount > 0) {
            VkBuffer buffers[] = {renderData.vertexBuffer->GetBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
            if (renderData.indexBuffer) {
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            }
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
            m_GeometryPoolBound = false;
        }
    }
}

void RenderSystem::RenderOutlinePassForTarget() {
    if (!m_OutlinePipeline || !m_GeometryOutlinesEnabled || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Use offscreen outline pipeline (matches offscreen UNORM render pass)
    auto* outlinePL = m_OffscreenOutlinePipeline ? m_OffscreenOutlinePipeline.get() : m_OutlinePipeline.get();
    outlinePL->Bind(commandBuffer);
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            outlinePL->GetLayout(), 0, 1, &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
    }

    const auto& renderList = m_SortedRenderList.empty()
        ? m_World->GetEntitiesWithComponent<MeshComponent>()
        : m_SortedRenderList;

    // Cache storage pointers for the offscreen outline pass hot loop
    auto* spriteStorageOPT = m_World->GetComponentStorage<Sprite2DComponent>();

    for (Entity entity : renderList) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!transform || !transform->visible) continue;

        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material && material->excludeFromCelShading) continue;
        if (spriteStorageOPT && spriteStorageOPT->Has(entity)) continue;
        if (material && material->alphaMode == MaterialComponent::AlphaMode::Blend) continue;

        EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
            ? &m_EntityRenderData[static_cast<usize>(entity)] : nullptr;
        if (!pRD || !pRD->valid) continue;
        EntityRenderData& renderData = *pRD;

        f32 outlineWidth = m_GeometryOutlineWidth;
        Math::Vector3 outlineColor = m_GeometryOutlineColor;
        if (material && material->outlineWidth > 0.0f) {
            outlineWidth = material->outlineWidth;
            outlineColor = material->outlineColor;
        }
        auto* artStyleOPT = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
        if (artStyleOPT && artStyleOPT->style == ArtStyleType::CelToon && artStyleOPT->cel_outlineWidth > 0.0f) {
            outlineWidth = artStyleOPT->cel_outlineWidth;
            outlineColor = artStyleOPT->cel_outlineColor;
        }

        Renderer::PushConstants pc{};
        pc.baseColor = outlineColor;
        pc.metallic = outlineWidth;
        pc.flags = 0;

        auto* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
        if (animComp && renderData.boneBuffer) {
            pc.flags |= (1 << 3);
            pc.model = Math::Matrix4::Identity(); // Skinned: identity, bone matrices handle positioning
            const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
            if (!skinningMatrices.empty()) {
                renderData.boneBuffer->UploadData(skinningMatrices.data(),
                    skinningMatrices.size() * sizeof(Math::Matrix4));
            }
            UpdateBoneDescriptor(renderData.boneBuffer.get());
        } else {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) {
                UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
            }
        }

        vkCmdPushConstants(commandBuffer, outlinePL->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        if (renderData.poolAlloc.valid && m_GeometryPool) {
            if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
            vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                             renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
        } else if (renderData.vertexBuffer && renderData.indexCount > 0) {
            VkBuffer buffers[] = {renderData.vertexBuffer->GetBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
            if (renderData.indexBuffer) {
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            }
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
            m_GeometryPoolBound = false;
        }
    }
}

void RenderSystem::RenderSprites() {
    if (!m_Pipeline || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Render tilemaps first (layer -1000, behind sprites) via the per-entity path
    // Tilemaps are complex meshes that don't benefit from instance batching — render directly
    for (Entity entity : m_World->GetEntitiesWithComponent<TilemapComponent>()) {
        auto* xformTM = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xformTM || !xformTM->visible) continue;
        if (!(m_CachedMeshStorage && m_CachedMeshStorage->Has(entity))) continue;
        RenderEntity(entity);
    }

    // Render sprites via batch renderer (instanced draw calls grouped by texture)
    if (m_SpriteBatchRenderer) {
        // Populate sprite texture atlas with all sprite textures before rendering
        if (m_SpriteAtlas) {
            auto* spriteAtlasStorage = m_World->GetComponentStorage<Sprite2DComponent>();
            for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
                auto* sprite = spriteAtlasStorage ? spriteAtlasStorage->Get(entity) : nullptr;
                if (sprite && !sprite->texturePath.empty())
                    m_SpriteAtlas->RequestTexture(sprite->texturePath);
            }
            if (m_SpriteAtlas->IsDirty()) m_SpriteAtlas->Build();
        }

        // Determine lit mode: Scene2D = unlit, Scene2_5D/Scene3D = lit (sprites respond to lights)
        bool litMode = (m_SceneComposition.mode != SceneRenderMode::Scene2D);

        auto textureBindCallback = [this, litMode](const std::string& texturePath, const std::string& normalMapPath) {
            // Handle atlas sentinel — bind the packed atlas texture
            if (texturePath == "__atlas__" && m_SpriteAtlas && m_SpriteAtlas->IsValid()) {
                UpdateTextureDescriptor(m_SpriteAtlas->GetAtlasTexture());
            } else if (!texturePath.empty()) {
                auto tex = GetOrLoadTexture(texturePath);
                if (tex && tex->IsValid()) {
                    UpdateTextureDescriptor(tex.get());
                }
            }
            // Bind normal map for lit sprites (binding 6)
            if (litMode) {
                if (!normalMapPath.empty()) {
                    auto normalTex = GetOrLoadTexture(normalMapPath);
                    if (normalTex && normalTex->IsValid()) {
                        UpdateNormalMapDescriptor(normalTex.get());
                    } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                        UpdateNormalMapDescriptor(m_DefaultWhiteTexture.get());
                    }
                } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                    UpdateNormalMapDescriptor(m_DefaultWhiteTexture.get());
                }
            }
        };

        m_SpriteBatchRenderer->Render(
            commandBuffer,
            *m_ActiveDescriptorSets,
            GetActiveBufferIndex(currentFrame),
            m_World,
            textureBindCallback,
            0, 0,
            litMode);
    } else {
        // Fallback: per-entity sprite rendering (no batching)
        struct SpriteEntry {
            Entity entity;
            i32 sortingLayer;
            i32 orderInLayer;
        };

        std::vector<SpriteEntry> sprites;
        sprites.reserve(64);
        auto* spriteFBStorage = m_World->GetComponentStorage<Sprite2DComponent>();
        for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
            auto* sprite = spriteFBStorage ? spriteFBStorage->Get(entity) : nullptr;
            if (!sprite || !sprite->visible) continue;
            auto* xformSprite = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (!xformSprite || !xformSprite->visible) continue;
            if (!(m_CachedMeshStorage && m_CachedMeshStorage->Has(entity))) continue;

            sprites.push_back({ entity, sprite->sortingLayer, sprite->orderInLayer });
        }

        if (!sprites.empty()) {
            std::sort(sprites.begin(), sprites.end(), [](const SpriteEntry& a, const SpriteEntry& b) {
                if (a.sortingLayer != b.sortingLayer) return a.sortingLayer < b.sortingLayer;
                return a.orderInLayer < b.orderInLayer;
            });

            for (const auto& entry : sprites) {
                RenderEntity(entry.entity);
            }
        }
    }
}

bool RenderSystem::ShouldUpdateCascade(u32 cascade) const {
    if (!m_CascadeProgressiveUpdate) return true;
    u32 interval = (cascade <= 1) ? 1 : m_CascadeFarUpdateInterval;
    // Stagger far cascades so they don't all update on the same frame
    u32 offset = (cascade <= 1) ? 0 : (cascade - 1);
    return ((m_ShadowFrameCounter + offset) % interval) == 0;
}

void RenderSystem::RenderShadowPass() {
    if (!m_ShadowMap || !m_ShadowPipeline) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // GPU timestamp: shadow pass begin
    {
        VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_SHADOW_BEGIN);
        }
    }

    // Progressive cascade updates: increment frame counter and detect camera teleport
    m_ShadowFrameCounter++;
    bool forceFullUpdate = false;
    if (m_Camera) {
        auto pos = m_Camera->GetPosition();
        f32 dist = (pos - m_PrevShadowCameraPos).Length();
        if (dist > 5.0f) forceFullUpdate = true;
        m_PrevShadowCameraPos = pos;
    }

    // Find the first directional light for shadow casting
    bool foundShadowLight = false;

    Math::Vector3 shadowLightDir(0.5f, 0.8f, 0.3f);

    auto* lightStorageSP = m_World->GetComponentStorage<LightComponent>();
    for (Entity lightEntity : m_CachedLightEntities) {
        LightComponent* light = lightStorageSP ? lightStorageSP->Get(lightEntity) : nullptr;
        if (!light || light->type != LightType::Directional || !light->castShadows) continue;

        TransformComponent* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(lightEntity) : nullptr;
        if (lightTransform) {
            Math::Vector3 forward(0.0f, 0.0f, -1.0f);
            shadowLightDir = lightTransform->rotation.Rotate(forward).Normalized();
        }
        foundShadowLight = true;
        break;
    }

    // Update cascade frustums from camera
    if (m_Camera) {
        Math::Vector3 lightDir = foundShadowLight ? shadowLightDir : Math::Vector3(0.5f, 0.8f, 0.3f).Normalized();
        m_ShadowMap->UpdateCascades(
            m_Camera->GetViewMatrix(), m_Camera->GetProjectionMatrix(),
            m_Camera->GetNearPlane(), m_Camera->GetFarPlane(),
            lightDir);
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Render each cascade
    for (u32 cascade = 0; cascade < m_ShadowMap->GetCascadeCount(); ++cascade) {
        // Progressive update: skip far cascades on non-update frames
        if (!forceFullUpdate && !ShouldUpdateCascade(cascade)) continue;

        // Store cascade VP for RenderEntityShadow to pre-multiply with model matrix.
        // Push constants are embedded in the command buffer, so they're immune to
        // the HOST_COHERENT UBO race that was causing empty shadow maps.
        m_CurrentCascadeVP = m_ShadowMap->GetCascadeViewProj(cascade);

        m_ShadowMap->BeginCascadePass(commandBuffer, cascade);

        // Bind shadow pipeline
        m_ShadowPipeline->Bind(commandBuffer);

        // Bind descriptor set (pipeline layout requires it even though shadow shader doesn't use material)
        {
            u32 zeroOff = 0;
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_ShadowPipeline->GetLayout(),
                0, 1, &m_DescriptorSets[currentFrame],
                1, &zeroOff
            );
        }

        // Render cached shadow-casting entities (rebuilt when dirty)
        // This avoids O(n) iteration per cascade — instead we iterate O(k) shadow casters
        if (m_ShadowCastersDirty) {
            RebuildShadowCasterCache();
        }

        for (Entity entity : m_ShadowCasters) {
            // Quick visibility check (may have changed since cache was built)
            auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (xform && !xform->visible) continue;

            RenderEntityShadow(entity, commandBuffer);
        }

        m_ShadowMap->EndCascadePass(commandBuffer);
    }

    // GPU timestamp: shadow pass end
    {
        VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_SHADOW_END);
        }
    }
}

void RenderSystem::RenderShadowPassForCamera(Renderer::Camera* camera) {
    if (!camera) { ENJIN_LOG_WARN(Renderer, "ShadowPassForCamera: no camera"); return; }
    if (!m_ShadowsEnabled) return;
    if (!m_ShadowMap) { ENJIN_LOG_WARN(Renderer, "ShadowPassForCamera: no shadow map"); return; }
    if (!m_ShadowPipeline) { ENJIN_LOG_WARN(Renderer, "ShadowPassForCamera: no shadow pipeline"); return; }

    ClassifySceneComposition();
    if (m_SceneComposition.mode != SceneRenderMode::Scene3D) return;
    if (!m_SceneComposition.hasShadowCastingLights) return;

    Renderer::Camera* prevCamera = m_Camera;
    m_Camera = camera;

    // Clamp shadow distance to game camera far plane to align cascade splits
    f32 prevShadowDist = m_ShadowDistance;
    f32 camFar = camera->GetFarPlane();
    if (m_ShadowDistance > camFar) {
        m_ShadowDistance = camFar;
    }

    // Re-select shadow lights relative to game camera position
    SelectShadowLights();

    RenderShadowPass();

    // Also render point and spot shadow passes
    if (m_PointShadowMap && m_PointShadowPipeline && m_ActivePointShadowCount > 0) {
        RenderPointShadowPass();
    }
    if (m_SpotShadowMap && m_SpotShadowPipeline && m_ActiveSpotShadowCount > 0) {
        RenderSpotShadowPass();
    }

    m_ShadowDistance = prevShadowDist;
    m_Camera = prevCamera;
}

void RenderSystem::RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer) {
    TransformComponent* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
    MeshComponent* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;

    if (!transform || !mesh || !mesh->IsValid()) return;

    EntityRenderData* pRD = (static_cast<usize>(entity) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(entity)].valid)
        ? &m_EntityRenderData[static_cast<usize>(entity)] : SetupEntityBuffers(entity);
    if (!pRD) return;
    EntityRenderData& renderData = *pRD;

    // Push pre-multiplied cascadeVP * model as the MVP matrix.
    // The shadow vertex shader reads this from push constants (first 64 bytes),
    // avoiding the HOST_COHERENT UBO race condition.
    Renderer::PushConstants pushConstants{};

    // Skinned mesh handling: upload bone matrices and use identity model matrix
    // so shadow geometry matches the main pass's skinned positions.
    AnimatorComponent* animComp = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
    if (!animComp) {
        // Mesh entity may not have animator — search globally (same as main pass)
        for (auto animEntity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
            auto* ac = m_World->GetComponent<AnimatorComponent>(animEntity);
            if (ac && ac->animator.GetSkeleton()) { animComp = ac; break; }
        }
    }
    if (animComp && renderData.boneBuffer) {
        const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
        if (!skinningMatrices.empty()) {
            renderData.boneBuffer->UploadData(skinningMatrices.data(),
                skinningMatrices.size() * sizeof(Math::Matrix4));
            UpdateBoneDescriptor(renderData.boneBuffer.get());
            pushConstants.flags |= (1 << 3); // FLAG_SKINNED
            // Include entity's world matrix for parent scale (cm→m conversion)
            pushConstants.model = m_CurrentCascadeVP * ECS::ComputeWorldMatrix(m_World, entity);
        } else {
            pushConstants.model = m_CurrentCascadeVP * ECS::ComputeWorldMatrix(m_World, entity);
        }
    } else {
        pushConstants.model = m_CurrentCascadeVP * ECS::ComputeWorldMatrix(m_World, entity);
    }

    vkCmdPushConstants(commandBuffer, m_ShadowPipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::PushConstants), &pushConstants);

    // Bind and draw — pool-allocated entities use merged buffer with offsets
    if (renderData.poolAlloc.valid && m_GeometryPool) {
        if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
        vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                         renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
    } else if (renderData.vertexBuffer && renderData.indexBuffer) {
        VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
        m_GeometryPoolBound = false;
    }
}

void RenderSystem::CreatePointShadowPipeline() {
    if (!m_PointShadowMap || !m_Pipeline || !m_ShadowVertexShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_PointShadowMap->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_NONE;  // All faces for correct contact shadows
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.depthBiasEnable = true;
    config.depthBiasConstant = 1.25f;
    config.depthBiasSlope = 1.75f;
    config.hasColorAttachment = false;

    m_PointShadowPipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_PointShadowPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_PointShadowPipeline->CreateWithLayout(config, m_ShadowVertexShader.get(), nullptr,
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create point shadow pipeline");
        m_PointShadowPipeline.reset();
    }
}

void RenderSystem::CreateSpotShadowPipeline() {
    if (!m_SpotShadowMap || !m_Pipeline || !m_ShadowVertexShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_SpotShadowMap->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_FRONT_BIT;  // Render back faces to eliminate self-shadowing on curved geometry
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.depthBiasEnable = true;
    config.depthBiasConstant = 1.5f;
    config.depthBiasSlope = 1.5f;
    config.hasColorAttachment = false;

    m_SpotShadowPipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_SpotShadowPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_SpotShadowPipeline->CreateWithLayout(config, m_ShadowVertexShader.get(), nullptr,
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create spot shadow pipeline");
        m_SpotShadowPipeline.reset();
    }
}

void RenderSystem::SelectShadowLights() {
    m_ShadowPointLights.clear();
    m_ShadowSpotLights.clear();
    m_ActivePointShadowCount = 0;
    m_ActiveSpotShadowCount = 0;

    if (!m_Camera) return;
    Math::Vector3 camPos = m_Camera->GetPosition();

    auto* lightStorageSL = m_World->GetComponentStorage<LightComponent>();
    for (Entity lightEntity : m_CachedLightEntities) {
        LightComponent* light = lightStorageSL ? lightStorageSL->Get(lightEntity) : nullptr;
        TransformComponent* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(lightEntity) : nullptr;
        if (!light || !light->castShadows || !lightTransform) continue;

        Math::Vector3 pos = lightTransform->position;
        Math::Vector3 diff = pos - camPos;
        f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        f32 score = light->intensity / std::max(distSq, 1.0f);

        if (light->type == LightType::Point) {
            m_ShadowPointLights.push_back({lightEntity, pos, light->range, score});
        } else if (light->type == LightType::Spot) {
            Math::Vector3 forward(0.0f, 0.0f, -1.0f);
            Math::Vector3 dir = lightTransform->rotation.Rotate(forward).Normalized();
            m_ShadowSpotLights.push_back({lightEntity, pos, dir, light->outerConeAngle, light->range, score});
        }
    }

    // Sort by score descending, take top N
    std::sort(m_ShadowPointLights.begin(), m_ShadowPointLights.end(),
        [](const ShadowPointLight& a, const ShadowPointLight& b) { return a.score > b.score; });
    std::sort(m_ShadowSpotLights.begin(), m_ShadowSpotLights.end(),
        [](const ShadowSpotLight& a, const ShadowSpotLight& b) { return a.score > b.score; });

    m_ActivePointShadowCount = static_cast<u32>(std::min(m_ShadowPointLights.size(),
        static_cast<size_t>(MAX_SHADOW_POINT_LIGHTS)));
    m_ActiveSpotShadowCount = static_cast<u32>(std::min(m_ShadowSpotLights.size(),
        static_cast<size_t>(MAX_SHADOW_SPOT_LIGHTS)));

    m_ShadowPointLights.resize(m_ActivePointShadowCount);
    m_ShadowSpotLights.resize(m_ActiveSpotShadowCount);
}

void RenderSystem::RenderPointShadowPass() {
    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    if (m_ShadowCastersDirty) {
        RebuildShadowCasterCache();
    }

    for (u32 lightIdx = 0; lightIdx < m_ActivePointShadowCount; ++lightIdx) {
        auto& sl = m_ShadowPointLights[lightIdx];

        for (u32 face = 0; face < 6; ++face) {
            m_CurrentCascadeVP = Renderer::PointLightShadowMap::ComputeFaceViewProj(
                sl.position, sl.range, face);

            m_PointShadowMap->BeginFacePass(commandBuffer, lightIdx, face);
            m_PointShadowPipeline->Bind(commandBuffer);

            {
                u32 zeroOff = 0;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_PointShadowPipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
            }

            for (Entity entity : m_ShadowCasters) {
                auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                if (xform && !xform->visible) continue;
                RenderEntityShadow(entity, commandBuffer);
            }

            m_PointShadowMap->EndFacePass(commandBuffer);
        }
    }
}

void RenderSystem::RenderSpotShadowPass() {
    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    if (m_ShadowCastersDirty) {
        RebuildShadowCasterCache();
    }

    for (u32 spotIdx = 0; spotIdx < m_ActiveSpotShadowCount; ++spotIdx) {
        auto& sl = m_ShadowSpotLights[spotIdx];

        m_CurrentCascadeVP = Renderer::SpotLightShadowMap::ComputeViewProj(
            sl.position, sl.direction, sl.outerConeAngle, sl.range);

        m_SpotShadowMap->BeginPass(commandBuffer, spotIdx);
        m_SpotShadowPipeline->Bind(commandBuffer);

        {
            u32 zeroOff = 0;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_SpotShadowPipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
        }

        for (Entity entity : m_ShadowCasters) {
            auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (xform && !xform->visible) continue;
            RenderEntityShadow(entity, commandBuffer);
        }

        m_SpotShadowMap->EndPass(commandBuffer);
    }
}

std::shared_ptr<Renderer::Texture> RenderSystem::GetOrLoadTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    // Check integer-keyed cache first (O(1) after initial load — no string hashing)
    auto idIt = m_TexturePathToId.find(path);
    if (idIt != m_TexturePathToId.end()) {
        return m_TextureById[idIt->second];
    }

    // Check failed cache (avoid retrying broken paths every frame)
    if (m_FailedTextures.count(path)) {
        return nullptr;
    }

    // Load new texture (SVG or raster)
    std::shared_ptr<Renderer::Texture> texture;
    if (Renderer::SVGLoader::IsSVGFile(path)) {
        texture = Renderer::SVGLoader::LoadAsTexture(m_Renderer->GetContext(), path);
    } else {
        texture = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
        if (!texture->LoadFromFile(path)) {
            texture = nullptr;
        }
    }

    if (!texture) {
        ENJIN_LOG_WARN(Renderer, "Failed to load texture (will not retry): %s", path.c_str());
        m_FailedTextures.insert(path);
        return nullptr;
    }

    ENJIN_LOG_INFO(Renderer, "Loaded texture: %s (%dx%d)",
        path.c_str(), texture->GetWidth(), texture->GetHeight());

    // Cache with integer ID and watch for hot-reload
    u32 texId = static_cast<u32>(m_TextureById.size());
    m_TexturePathToId[path] = texId;
    m_TextureById.push_back(texture);
    m_TextureIdToPath.push_back(path);

    // Register in bindless descriptor set for indexed texture access
    if (m_BindlessManager && texture->IsValid()) {
        auto handle = m_BindlessManager->RegisterTexture(
            texture->GetImageView(), texture->GetSampler());
        if (handle != UINT32_MAX) {
            m_TextureBindlessHandles[texture.get()] = handle;
        }
    }

    m_TextureWatcher.Watch(path, [this](const std::string& changedPath) {
        ENJIN_LOG_INFO(Renderer, "Texture changed, reloading: %s", changedPath.c_str());
        auto pathIt = m_TexturePathToId.find(changedPath);
        if (pathIt != m_TexturePathToId.end()) {
            auto newTex = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
            if (newTex->LoadFromFile(changedPath)) {
                m_TextureById[pathIt->second] = newTex;
                // Invalidate cached raw pointers on all materials referencing this texture
                if (m_World) {
                    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::MaterialComponent>()) {
                        auto* mat = m_World->GetComponent<ECS::MaterialComponent>(e);
                        if (!mat) continue;
                        if (mat->baseColorTexturePath == changedPath ||
                            mat->normalTexturePath == changedPath ||
                            mat->heightTexturePath == changedPath ||
                            mat->metallicRoughnessTexturePath == changedPath ||
                            mat->emissiveTexturePath == changedPath) {
                            mat->textureCacheDirty = true;
                        }
                    }
                }
                // Invalidate sprite atlas so it rebuilds with the new texture data
                if (m_SpriteAtlas) m_SpriteAtlas->Invalidate();
            }
        }
    });
    return texture;
}

void RenderSystem::UpdateTextureDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) {
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Update binding 3 (base color texture) with the new texture
    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 3;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateHeightTextureDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) {
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 5;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateNormalMapDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) {
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 6;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateMetallicRoughnessDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 8;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateEmissiveDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 9;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateEntityTextureDescriptors(
    Renderer::Texture* baseColor,
    Renderer::Texture* height,
    Renderer::Texture* normal,
    Renderer::Texture* metallicRoughness,
    Renderer::Texture* emissive,
    Renderer::Texture* matcap)
{
    // Skip per-entity descriptor writes when bindless is active —
    // textures are indexed via MaterialSSBO handles instead.
    if (m_BindlessManager) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    VkDescriptorSet dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    VkDevice device = m_Renderer->GetContext()->GetDevice();

    // Use default white texture for any nullptr slots
    Renderer::Texture* defaultTex = m_DefaultWhiteTexture.get();
    if (!defaultTex || !defaultTex->IsValid()) return;

    Renderer::Texture* texBase = (baseColor && baseColor->IsValid()) ? baseColor : defaultTex;
    Renderer::Texture* texHeight = (height && height->IsValid()) ? height : defaultTex;
    Renderer::Texture* texNormal = (normal && normal->IsValid()) ? normal : defaultTex;
    Renderer::Texture* texMR = (metallicRoughness && metallicRoughness->IsValid()) ? metallicRoughness : defaultTex;
    Renderer::Texture* texEmissive = (emissive && emissive->IsValid()) ? emissive : defaultTex;
    Renderer::Texture* texMatcap = (matcap && matcap->IsValid()) ? matcap : defaultTex;

    // Skip vkUpdateDescriptorSets if these textures are already bound
    MaterialComponent::TextureKey currentKey{ texBase, texHeight, texNormal, texMR, texEmissive, texMatcap };
    if (currentKey == m_LastBound.textureKey) { ++m_DescriptorCacheHits; return; }
    m_LastBound.textureKey = currentKey;
    ++m_DescriptorCacheWrites;

    // Collect image infos (must persist until vkUpdateDescriptorSets returns)
    VkDescriptorImageInfo imageInfos[6];
    imageInfos[0] = texBase->GetDescriptorInfo();
    imageInfos[1] = texHeight->GetDescriptorInfo();
    imageInfos[2] = texNormal->GetDescriptorInfo();
    imageInfos[3] = texMR->GetDescriptorInfo();
    imageInfos[4] = texEmissive->GetDescriptorInfo();
    imageInfos[5] = texMatcap->GetDescriptorInfo();

    // Bindings: 3=baseColor, 5=height, 6=normal, 8=metallicRoughness, 9=emissive, 18=matcap
    VkWriteDescriptorSet writes[6] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = dstSet;
    writes[0].dstBinding = 3;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &imageInfos[0];

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = dstSet;
    writes[1].dstBinding = 5;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfos[1];

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = dstSet;
    writes[2].dstBinding = 6;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &imageInfos[2];

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = dstSet;
    writes[3].dstBinding = 8;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &imageInfos[3];

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = dstSet;
    writes[4].dstBinding = 9;
    writes[4].dstArrayElement = 0;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &imageInfos[4];

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = dstSet;
    writes[5].dstBinding = 18;
    writes[5].dstArrayElement = 0;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].descriptorCount = 1;
    writes[5].pImageInfo = &imageInfos[5];

    // Single batched call instead of 6 individual calls
    vkUpdateDescriptorSets(device, 6, writes, 0, nullptr);
}

void RenderSystem::UpdateBoneDescriptor(Renderer::VulkanBuffer* boneBuffer) {
    if (!boneBuffer) return;

    // Skip if this bone buffer is already bound
    if (boneBuffer == m_LastBound.boneBuffer) return;
    m_LastBound.boneBuffer = boneBuffer;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = boneBuffer->GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = boneBuffer->GetSize();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 7;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateMorphDescriptor(Renderer::VulkanBuffer* morphBuffer) {
    if (!morphBuffer) return;
    if (morphBuffer == m_LastBound.morphBuffer) return;
    m_LastBound.morphBuffer = morphBuffer;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = morphBuffer->GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = morphBuffer->GetSize();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    write.dstBinding = 20;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &write, 0, nullptr);
}

void RenderSystem::UploadMorphTargetSSBO(Entity entity, ECS::MorphTargetComponent& morph, EntityRenderData& rd) {
    auto* meshComp = m_World->GetComponent<MeshComponent>(entity);
    if (!meshComp) return;
    u32 vertexCount = static_cast<u32>(meshComp->vertices.size());
    u32 targetCount = static_cast<u32>(morph.targets.size());
    if (targetCount == 0 || vertexCount == 0) return;

    u32 headerSize = 2 + targetCount;
    usize totalFloats = headerSize + static_cast<usize>(targetCount) * vertexCount * 6;
    usize totalBytes = totalFloats * sizeof(f32);

    if (!rd.morphBuffer || rd.morphBuffer->GetSize() < totalBytes) {
        rd.morphBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!rd.morphBuffer->Create(totalBytes, Renderer::BufferUsage::Storage, true)) {
            rd.morphBuffer.reset();
            return;
        }
        morph.deltasDirty = true;
    }

    if (morph.deltasDirty) {
        std::vector<f32> ssbo(totalFloats, 0.0f);
        u32 vc = vertexCount, tc = targetCount;
        std::memcpy(&ssbo[0], &vc, 4);
        std::memcpy(&ssbo[1], &tc, 4);
        for (u32 t = 0; t < targetCount; ++t) ssbo[2 + t] = morph.weights[t];
        for (u32 t = 0; t < targetCount; ++t) {
            const auto& tgt = morph.targets[t];
            for (u32 v = 0; v < vertexCount && v < static_cast<u32>(tgt.deltas.size()); ++v) {
                u32 idx = headerSize + t * vertexCount * 6 + v * 6;
                ssbo[idx+0] = tgt.deltas[v].positionDelta.x;
                ssbo[idx+1] = tgt.deltas[v].positionDelta.y;
                ssbo[idx+2] = tgt.deltas[v].positionDelta.z;
                ssbo[idx+3] = tgt.deltas[v].normalDelta.x;
                ssbo[idx+4] = tgt.deltas[v].normalDelta.y;
                ssbo[idx+5] = tgt.deltas[v].normalDelta.z;
            }
        }
        rd.morphBuffer->UploadData(ssbo.data(), totalBytes);
        morph.deltasDirty = false;
        morph.weightsDirty = false;
    } else if (morph.weightsDirty) {
        std::vector<f32> hdr(headerSize);
        u32 vc = vertexCount, tc = targetCount;
        std::memcpy(&hdr[0], &vc, 4);
        std::memcpy(&hdr[1], &tc, 4);
        for (u32 t = 0; t < targetCount; ++t) hdr[2 + t] = morph.weights[t];
        rd.morphBuffer->UploadData(hdr.data(), headerSize * sizeof(f32));
        morph.weightsDirty = false;
    }
}

void RenderSystem::EnsureWaterMeshes() {
    for (Entity entity : m_World->GetEntitiesWithComponent<WaterVolumeComponent>()) {
        auto* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
        if (!waterVol) continue;
        if (waterVol->meshCreated && m_World->GetComponent<MeshComponent>(entity)) continue;

        // Create a subdivided plane mesh for the water surface
        MeshComponent mesh;
        f32 hx = waterVol->halfExtents.x;
        f32 hz = waterVol->halfExtents.z;

        const u32 segsX = 20;
        const u32 segsZ = 20;
        mesh.vertices.reserve((segsX + 1) * (segsZ + 1));
        mesh.indices.reserve(segsX * segsZ * 6);

        for (u32 zi = 0; zi <= segsZ; ++zi) {
            for (u32 xi = 0; xi <= segsX; ++xi) {
                MeshComponent::Vertex v;
                f32 u = static_cast<f32>(xi) / segsX;
                f32 vt = static_cast<f32>(zi) / segsZ;
                v.position = Math::Vector3(
                    -hx + u * 2.0f * hx,
                    0.0f,
                    -hz + vt * 2.0f * hz
                );
                v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
                v.uv = Math::Vector2(u, vt);

                // Compute minimum distance to any edge (normalized 0=edge, 1=center)
                f32 distLeft = u;
                f32 distRight = 1.0f - u;
                f32 distTop = vt;
                f32 distBottom = 1.0f - vt;
                f32 minEdgeDist = std::min(std::min(distLeft, distRight), std::min(distTop, distBottom));
                // Normalize so center = 1.0 (max edge dist is 0.5)
                f32 edgeDist = std::min(minEdgeDist * 2.0f, 1.0f);

                // R = water color red (unused by shader for water), G = edge distance, B = unused, A = opacity
                v.color = Math::Vector4(
                    waterVol->waterColor.x,
                    edgeDist,
                    waterVol->waterColor.z,
                    waterVol->opacity
                );
                mesh.vertices.push_back(v);
            }
        }

        for (u32 zi = 0; zi < segsZ; ++zi) {
            for (u32 xi = 0; xi < segsX; ++xi) {
                u32 tl = zi * (segsX + 1) + xi;
                u32 tr = tl + 1;
                u32 bl = (zi + 1) * (segsX + 1) + xi;
                u32 br = bl + 1;

                mesh.indices.push_back(tl);
                mesh.indices.push_back(bl);
                mesh.indices.push_back(tr);

                mesh.indices.push_back(tr);
                mesh.indices.push_back(bl);
                mesh.indices.push_back(br);
            }
        }

        m_World->AddComponent<MeshComponent>(entity, std::move(mesh));

        // Add material with water visual properties based on water type
        MaterialComponent material;
        material.baseColor = waterVol->waterColor;
        material.opacity = waterVol->opacity;
        material.doubleSided = true;
        material.castShadows = false;
        material.alphaMode = static_cast<MaterialComponent::AlphaMode>(0);  // Opaque — writes depth

        // Water type presets for material properties
        switch (waterVol->waterType) {
            case WaterType::Ocean:
                material.metallic = 0.4f;
                material.roughness = 0.05f;
                break;
            case WaterType::River:
                material.metallic = 0.25f;
                material.roughness = 0.15f;
                break;
            case WaterType::Pond:
                material.metallic = 0.2f;
                material.roughness = 0.2f;
                break;
            case WaterType::Lake:
            default:
                material.metallic = 0.3f;
                material.roughness = 0.1f;
                break;
        }

        m_World->AddComponent<MaterialComponent>(entity, material);

        SetupEntityBuffers(entity);
        waterVol->meshCreated = true;

        ENJIN_LOG_INFO(Renderer, "Created water surface mesh for entity %llu (%.0f x %.0f)",
            entity, hx * 2.0f, hz * 2.0f);
    }
}

void RenderSystem::EnsureWater3DMeshes() {
    if (!m_World) return;
    for (Entity entity : m_World->GetEntitiesWithComponent<Water3DComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
        if (!water3d) continue;
        if (water3d->meshCreated && m_World->GetComponent<MeshComponent>(entity)) continue;
        // Guard against zero/negative dimensions
        if (water3d->settings.width < 0.01f || water3d->settings.depth < 0.01f) continue;

        // Use Water3D to build the initial mesh on this entity
        Effects::Water3D builder;
        builder.Initialize(water3d->settings);
        builder.BuildEntityMesh(m_World, entity);

        SetupEntityBuffers(entity);
        water3d->meshCreated = true;

        ENJIN_LOG_INFO(Renderer, "Created Water3D surface mesh for entity %llu (%.0f x %.0f)",
            entity, water3d->settings.width, water3d->settings.depth);
    }
}

void RenderSystem::RenderWeatherParticles(const Effects::WeatherSystem& weather, bool isRain,
                                           u32 viewportWidth, u32 viewportHeight) {
    if (!m_WeatherRenderer || !m_Renderer || !m_Initialized || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_WeatherRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                              GetActiveBufferIndex(currentFrame), weather, isRain,
                              viewportWidth, viewportHeight);
}

void RenderSystem::RenderParticles(u32 viewportWidth, u32 viewportHeight) {
    if (!m_ParticleRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_ParticleRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                               GetActiveBufferIndex(currentFrame), m_World,
                               viewportWidth, viewportHeight);
}

void RenderSystem::RenderElementalParticles(const Effects::ElementalSystem& elementalSystem,
                                             u32 viewportWidth, u32 viewportHeight) {
    if (!m_ParticleRenderer || !m_Renderer || !m_Initialized || !m_ActiveDescriptorSets) return;
    if (elementalSystem.GetActiveCount() == 0) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_ParticleRenderer->RenderElementalParticles(commandBuffer, *m_ActiveDescriptorSets,
                                                  GetActiveBufferIndex(currentFrame), elementalSystem,
                                                  viewportWidth, viewportHeight);
}

void RenderSystem::RenderFluid(u32 viewportWidth, u32 viewportHeight) {
    if (!m_FluidRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_FluidRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                             GetActiveBufferIndex(currentFrame), m_World,
                             viewportWidth, viewportHeight);
}

void RenderSystem::SetFluidSimulation(Effects::FluidSimulation* sim) {
    if (m_FluidRenderer) {
        m_FluidRenderer->SetFluidSimulation(sim);
    }
}

void RenderSystem::RenderGrass(u32 viewportWidth, u32 viewportHeight) {
    if (!m_GrassRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_GrassRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                            GetActiveBufferIndex(currentFrame), m_World,
                            viewportWidth, viewportHeight);
}

void RenderSystem::RenderShrubs(u32 viewportWidth, u32 viewportHeight) {
    if (!m_ShrubRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_ShrubRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                            GetActiveBufferIndex(currentFrame), m_World,
                            viewportWidth, viewportHeight);
}

void RenderSystem::RenderTrees(u32 viewportWidth, u32 viewportHeight) {
    if (!m_TreeRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_TreeRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                           GetActiveBufferIndex(currentFrame), m_World,
                           viewportWidth, viewportHeight);
}

void RenderSystem::RecreateEffectPipelinesForRenderPass(VkRenderPass renderPass) {
    if (!m_Pipeline) return;
    m_OffscreenRenderPass = renderPass;  // Cache for RecreatePipelines
    VkDescriptorSetLayout layout = m_Pipeline->GetDescriptorSetLayout();

    if (m_WeatherRenderer) {
        m_WeatherRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_GrassRenderer) {
        m_GrassRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_ShrubRenderer) {
        m_ShrubRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_ParticleRenderer) {
        m_ParticleRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_FluidRenderer) {
        m_FluidRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_TreeRenderer) {
        m_TreeRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_SpriteBatchRenderer) {
        m_SpriteBatchRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }

    // Recreate main pipeline for offscreen render pass (fixes SRGB vs UNORM format mismatch
    // that causes NVIDIA driver crash on 2D scenes)
    {
        Renderer::PipelineConfig config;
        config.renderPass = renderPass;
        config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        config.depthTest = true;
        config.depthWrite = true;
        config.cullMode = m_BackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        config.polygonMode = m_WireframeMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        config.msaaSamples = VK_SAMPLE_COUNT_1_BIT;  // Offscreen RT is always 1 sample
        config.colorAttachmentCount = 1; // Single color output (no MRT velocity — avoids NVIDIA teal)

        m_OffscreenPipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_OffscreenPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
        if (!m_OffscreenPipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create offscreen pipeline");
            m_OffscreenPipeline.reset();
        }
    }

    // Offscreen line pipeline (editor grid inside offscreen render target)
    {
        Renderer::PipelineConfig config;
        config.renderPass = renderPass;
        config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        config.depthTest = true;
        config.depthWrite = false;
        config.cullMode = VK_CULL_MODE_NONE;
        config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        config.polygonMode = VK_POLYGON_MODE_FILL;
        config.alphaBlend = true;
        config.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        config.colorAttachmentCount = 1;

        m_OffscreenLinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_OffscreenLinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
        if (!m_OffscreenLinePipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), layout)) {
            ENJIN_LOG_WARN(Renderer, "Failed to create offscreen line pipeline");
            m_OffscreenLinePipeline.reset();
        }
    }

    // Offscreen outline pipeline (geometry outlines inside offscreen render target)
    if (m_OutlineVertexShader && m_OutlineFragmentShader) {
        Renderer::PipelineConfig config;
        config.renderPass = renderPass;
        config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        config.depthTest = true;
        config.depthWrite = true;
        config.cullMode = VK_CULL_MODE_FRONT_BIT;
        config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        config.polygonMode = VK_POLYGON_MODE_FILL;
        config.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        config.colorAttachmentCount = 1;

        m_OffscreenOutlinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessManager) m_OffscreenOutlinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
        if (!m_OffscreenOutlinePipeline->CreateWithLayout(config, m_OutlineVertexShader.get(), m_OutlineFragmentShader.get(), layout)) {
            ENJIN_LOG_WARN(Renderer, "Failed to create offscreen outline pipeline");
            m_OffscreenOutlinePipeline.reset();
        }
    }

    // Note: skybox pipeline is NOT recreated here — it was created for the swapchain
    // render pass in Initialize() and works in both passes via driver-level render pass
    // compatibility (SRGB/UNORM same memory layout). Destroying and recreating it here
    // would invalidate the descriptor set layout and break skybox rendering entirely.
}

void RenderSystem::SetSkybox(const Renderer::SkyboxConfig& config) {
    // Defer skybox config change to the start of the next frame — the old cubemap
    // may still be referenced by the current frame's command buffer which is being
    // recorded. Applying the change at the top of Update() is safe because all
    // in-flight frames have been submitted and can be waited on.
    m_PendingSkybox = config;
    m_PendingSkyboxConfig = true;
}

void RenderSystem::CreateSkyboxCubeVBO() {
    float cubeVertices[] = {
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f,
         1.0f, 1.0f,-1.0f, -1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f,-1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, -1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f, -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f, -1.0f, 1.0f, 1.0f, -1.0f,-1.0f, 1.0f,
         1.0f, 1.0f, 1.0f,  1.0f, 1.0f,-1.0f,  1.0f,-1.0f,-1.0f,
         1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,  1.0f,-1.0f,-1.0f,
         1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f, -1.0f,-1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,  1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,-1.0f,
    };

    m_SkyboxVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_SkyboxVertexBuffer->Create(sizeof(cubeVertices), Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create skybox VBO");
        m_SkyboxVertexBuffer.reset();
        return;
    }
    m_SkyboxVertexBuffer->UploadData(cubeVertices, sizeof(cubeVertices));
}

void RenderSystem::CreateSkyboxPipeline(VkRenderPass renderPass) {
    ENJIN_LOG_INFO(Renderer, "CreateSkyboxPipeline called");

    if (!m_Renderer || !m_Renderer->GetContext()) {
        ENJIN_LOG_ERROR(Renderer, "CreateSkyboxPipeline: No renderer or context!");
        return;
    }

    auto* context = m_Renderer->GetContext();
    VkDevice device = context->GetDevice();

    // Load skybox shaders
    auto skyboxVert = std::make_unique<Renderer::VulkanShader>(context);
    if (!skyboxVert->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SkyboxVertexShaderData),
        Renderer::ShaderData::SkyboxVertexShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Skybox vertex shader not available, skybox disabled");
        return;
    }

    auto skyboxFrag = std::make_unique<Renderer::VulkanShader>(context);
    if (!skyboxFrag->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SkyboxFragmentShaderData),
        Renderer::ShaderData::SkyboxFragmentShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Skybox fragment shader not available, skybox disabled");
        return;
    }

    // Create descriptor set layout (binding 0: UBO, binding 1: cubemap sampler)
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_SkyboxDescriptorSetLayoutHandle) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox descriptor set layout");
        return;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_SkyboxDescriptorSetLayoutHandle;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_SkyboxPipelineLayoutHandle) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox pipeline layout");
        return;
    }

    // Vertex input: position only (vec3)
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 3;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc{};
    attrDesc.binding = 0;
    attrDesc.location = 0;
    attrDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDesc.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &attrDesc;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // No culling — camera is always inside the skybox cube
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = m_Renderer->GetMSAASamples();

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // MRT: second attachment for velocity buffer (write zero velocity for skybox)
    VkPipelineColorBlendAttachmentState velocityBlendAttachment{};
    velocityBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    std::array<VkPipelineColorBlendAttachmentState, 2> skyboxBlendAttachments = { colorBlendAttachment, velocityBlendAttachment };

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 2; // Swapchain MRT: color + velocity (main pass only; offscreen uses 1)
    colorBlending.pAttachments = skyboxBlendAttachments.data();

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = skyboxVert->GetModule();
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = skyboxFrag->GetModule();
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_SkyboxPipelineLayoutHandle;
    pipelineInfo.renderPass = (renderPass != VK_NULL_HANDLE) ? renderPass : m_Renderer->GetRenderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_SkyboxPipelineHandle) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox pipeline");
        return;
    }

    // Create descriptor pool for skybox
    constexpr u32 framesInFlight = 2;
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = framesInFlight;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = framesInFlight;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_SkyboxDescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox descriptor pool");
        return;
    }

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_SkyboxDescriptorSetLayoutHandle);
    m_SkyboxDescriptorSets.resize(framesInFlight);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_SkyboxDescriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, m_SkyboxDescriptorSets.data()) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to allocate skybox descriptor sets");
        return;
    }

    // Create UBOs for skybox viewProj matrix
    m_SkyboxUniformBuffers.resize(framesInFlight);
    for (u32 i = 0; i < framesInFlight; ++i) {
        m_SkyboxUniformBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        m_SkyboxUniformBuffers[i]->Create(sizeof(Math::Matrix4), Renderer::BufferUsage::Uniform, true);
    }

    ENJIN_LOG_INFO(Renderer, "Skybox pipeline created");
}

void RenderSystem::RenderSkybox(VkCommandBuffer commandBuffer,
                                const VkViewport* viewportOverride,
                                const VkRect2D* scissorOverride) {
    if (!m_Skybox.IsValid() || m_SkyboxPipelineHandle == VK_NULL_HANDLE || !m_SkyboxVertexBuffer || !m_Camera) {
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Build view-projection matrix with translation removed (keep skybox centered on camera)
    Math::Matrix4 view = m_Camera->GetViewMatrix();
    // Zero out the translation column (column 3, rows 0-2) - flat column-major layout
    view.m[12] = 0.0f;
    view.m[13] = 0.0f;
    view.m[14] = 0.0f;
    Math::Matrix4 viewProj = m_Camera->GetProjectionMatrix() * view;

    // Upload UBO
    m_SkyboxUniformBuffers[currentFrame]->UploadData(&viewProj, sizeof(Math::Matrix4));

    // Update descriptor set with UBO and cubemap
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_SkyboxUniformBuffers[currentFrame]->GetBuffer();
    uboInfo.offset = 0;
    uboInfo.range = sizeof(Math::Matrix4);

    VkDescriptorImageInfo cubemapInfo = m_Skybox.GetDescriptorInfo();

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_SkyboxDescriptorSets[currentFrame];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &uboInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_SkyboxDescriptorSets[currentFrame];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &cubemapInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

    // Set viewport and scissor — use overrides if provided (offscreen / splitscreen),
    // otherwise fall back to swapchain extent (main pass single-camera)
    if (viewportOverride) {
        vkCmdSetViewport(commandBuffer, 0, 1, viewportOverride);
    } else {
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        VkViewport viewport{};
        viewport.width = static_cast<f32>(extent.width);
        viewport.height = static_cast<f32>(extent.height);
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    }

    if (scissorOverride) {
        vkCmdSetScissor(commandBuffer, 0, 1, scissorOverride);
    } else {
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        VkRect2D scissor{};
        scissor.extent = extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    // Bind skybox pipeline and draw
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyboxPipelineHandle);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_SkyboxPipelineLayoutHandle, 0, 1, &m_SkyboxDescriptorSets[currentFrame], 0, nullptr);

    VkBuffer vertexBuffers[] = { m_SkyboxVertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}

// ---------------------------------------------------------------------------
// Ray Tracing subsystem lifecycle
// ---------------------------------------------------------------------------

bool RenderSystem::IsRayTracingSupported() const {
    if (!m_Renderer || !m_Renderer->GetContext()) return false;
    return m_Renderer->GetContext()->IsRayTracingSupported();
}

void RenderSystem::InitializeRayTracing() {
    if (!m_RTEnabled) {
        ENJIN_LOG_INFO(Renderer, "Ray tracing disabled by configuration");
        return;
    }
    if (!IsRayTracingSupported()) {
        ENJIN_LOG_INFO(Renderer, "Ray tracing not supported on this device, RT features disabled");
        return;
    }

    auto* ctx = m_Renderer->GetContext();
    ENJIN_LOG_INFO(Renderer, "Initializing ray tracing subsystems...");

    // Create RT descriptor set layout (27 bindings: 0-16 existing + 17 SDF + 18 simplified materials + 19-20 ReSTIR + 21-23 radiance cache + 24-26 surfel cache)
    std::array<VkDescriptorSetLayoutBinding, 27> rtBindings{};

    // Binding 0: TLAS
    rtBindings[0].binding = 0;
    rtBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    rtBindings[0].descriptorCount = 1;
    rtBindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // Binding 1: Scene HDR (storage image)
    rtBindings[1].binding = 1;
    rtBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[1].descriptorCount = 1;
    rtBindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Depth buffer
    rtBindings[2].binding = 2;
    rtBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    rtBindings[2].descriptorCount = 1;
    rtBindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: World normals
    rtBindings[3].binding = 3;
    rtBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    rtBindings[3].descriptorCount = 1;
    rtBindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: Motion vectors
    rtBindings[4].binding = 4;
    rtBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    rtBindings[4].descriptorCount = 1;
    rtBindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 5: RT Shadow output (storage image)
    rtBindings[5].binding = 5;
    rtBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[5].descriptorCount = 1;
    rtBindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 6: RT Reflection output (storage image)
    rtBindings[6].binding = 6;
    rtBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[6].descriptorCount = 1;
    rtBindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 7: RT AO output (storage image)
    rtBindings[7].binding = 7;
    rtBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[7].descriptorCount = 1;
    rtBindings[7].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 8: RT GI output (storage image)
    rtBindings[8].binding = 8;
    rtBindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[8].descriptorCount = 1;
    rtBindings[8].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Bindings 9-12: Storage buffers (material, vertex, index, transforms)
    for (u32 i = 9; i <= 12; ++i) {
        rtBindings[i].binding = i;
        rtBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        rtBindings[i].descriptorCount = 1;
        rtBindings[i].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    }

    // Binding 13: Light data UBO (used by RT shaders + compute: ReSTIR, radiance cache)
    rtBindings[13].binding = 13;
    rtBindings[13].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    rtBindings[13].descriptorCount = 1;
    rtBindings[13].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 14: RT Translucency output (storage image)
    rtBindings[14].binding = 14;
    rtBindings[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[14].descriptorCount = 1;
    rtBindings[14].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 15: RT Caustics output (storage image)
    rtBindings[15].binding = 15;
    rtBindings[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[15].descriptorCount = 1;
    rtBindings[15].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 16: NEE light SSBO (scene lights for path tracer direct light sampling)
    rtBindings[16].binding = 16;
    rtBindings[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[16].descriptorCount = 1;
    rtBindings[16].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // Binding 17: SDF scene SSBO (SDF objects for reflection fallback sphere tracing)
    rtBindings[17].binding = 17;
    rtBindings[17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[17].descriptorCount = 1;
    rtBindings[17].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 18: Simplified material SSBO (pre-baked F0/kDiffuse/effectiveRoughness)
    rtBindings[18].binding = 18;
    rtBindings[18].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[18].descriptorCount = 1;
    rtBindings[18].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 19: ReSTIR reservoir SSBO — current frame (per-pixel light selection results)
    rtBindings[19].binding = 19;
    rtBindings[19].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[19].descriptorCount = 1;
    rtBindings[19].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 20: ReSTIR reservoir SSBO — previous frame (ping-pong for temporal reuse)
    rtBindings[20].binding = 20;
    rtBindings[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[20].descriptorCount = 1;
    rtBindings[20].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 21: Radiance cache tile buffer (per-tile cached irradiance + depth/normal/age)
    rtBindings[21].binding = 21;
    rtBindings[21].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[21].descriptorCount = 1;
    rtBindings[21].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 22: Radiance cache stale tile mask (bitfield — 1 bit per tile)
    rtBindings[22].binding = 22;
    rtBindings[22].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[22].descriptorCount = 1;
    rtBindings[22].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 23: Radiance cache output image (full-resolution interpolated irradiance)
    rtBindings[23].binding = 23;
    rtBindings[23].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[23].descriptorCount = 1;
    rtBindings[23].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 24: Surfel radiance cache buffer (world-space surfel array)
    rtBindings[24].binding = 24;
    rtBindings[24].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[24].descriptorCount = 1;
    rtBindings[24].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 25: Surfel counter/metadata buffer (active count, free slot, update count)
    rtBindings[25].binding = 25;
    rtBindings[25].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[25].descriptorCount = 1;
    rtBindings[25].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 26: Surfel radiance cache output image (blended surfel + screen-space irradiance)
    rtBindings[26].binding = 26;
    rtBindings[26].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[26].descriptorCount = 1;
    rtBindings[26].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(rtBindings.size());
    layoutInfo.pBindings = rtBindings.data();

    if (vkCreateDescriptorSetLayout(ctx->GetDevice(), &layoutInfo, nullptr, &m_RTDescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT descriptor set layout");
        return;
    }

    // Create RT descriptor pool (includes all descriptor types used by RT bindings)
    std::array<VkDescriptorPoolSize, 5> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 9 };   // 5-8, 14-15, 23, 26 + radiance cache output + surfel output
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 };
    poolSizes[3] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 13 }; // 9-12, 16-20, 21-22, 24-25 (radiance cache + surfel cache)
    poolSizes[4] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(ctx->GetDevice(), &poolInfo, nullptr, &m_RTDescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT descriptor pool");
        return;
    }

    // Allocate RT descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_RTDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_RTDescriptorSetLayout;

    if (vkAllocateDescriptorSets(ctx->GetDevice(), &allocInfo, &m_RTDescriptorSet) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT descriptor set");
        return;
    }

    // Initialize acceleration structure manager (shader-independent, always safe)
    m_ASManager = std::make_unique<Renderer::AccelerationStructureManager>(ctx);
    m_ASManager->Initialize();

    // Check if RT shaders have been compiled from GLSL to SPIR-V.
    // The embedded SPIR-V in RTShaderData.h contains placeholder stubs that are
    // not valid for actual pipeline creation. Skip subsystem initialization until
    // real compiled shaders are provided. The ASManager and descriptor layout are
    // still available for when shaders are compiled.
    //
    // To compile real shaders:
    //   cd Engine/shaders
    //   glslangValidator --target-env vulkan1.2 -S rgen rt_shadow.rgen -o rt_shadow.rgen.spv
    //   (etc. for all RT/compute shaders)
    //   Then convert to C arrays and update RTShaderData.h
    //
    // Detect stubs: check if the first shader has the stub word count (9 words = 36 bytes)
    {
        bool usingStubs = (sizeof(Renderer::RT_SHADOW_RGEN_SPV) <= 40 * sizeof(u32));
        if (usingStubs) {
            ENJIN_LOG_INFO(Renderer, "RT shaders are placeholder stubs — skipping pipeline creation. "
                           "Compile GLSL shaders in Engine/shaders/ and update RTShaderData.h for full RT support.");
            return;
        }
    }

    // Get render dimensions
    VkExtent2D extent = m_Renderer->GetSwapchainExtent();
    u32 width = extent.width;
    u32 height = extent.height;

    // Initialize RT effect subsystems
    m_RTShadows = std::make_unique<Renderer::RTShadows>(ctx);
    if (!m_RTShadows->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Shadows initialization failed");
        m_RTShadows.reset();
    }

    m_RTReflections = std::make_unique<Renderer::RTReflections>(ctx);
    if (!m_RTReflections->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Reflections initialization failed");
        m_RTReflections.reset();
    }

    m_RTAO = std::make_unique<Renderer::RTAmbientOcclusion>(ctx);
    if (!m_RTAO->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT AO initialization failed");
        m_RTAO.reset();
    }

    m_RTGI = std::make_unique<Renderer::RTGlobalIllumination>(ctx);
    if (!m_RTGI->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT GI initialization failed");
        m_RTGI.reset();
    }

    m_RTTranslucency = std::make_unique<Renderer::RTTranslucency>(ctx);
    if (!m_RTTranslucency->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Translucency initialization failed");
        m_RTTranslucency.reset();
    }

    m_RTCaustics = std::make_unique<Renderer::RTCaustics>(ctx);
    if (!m_RTCaustics->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Caustics initialization failed");
        m_RTCaustics.reset();
    }

    m_PathTracer = std::make_unique<Renderer::PathTracer>(ctx);
    if (!m_PathTracer->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "Path Tracer initialization failed");
        m_PathTracer.reset();
    }

    m_ReSTIR = std::make_unique<Renderer::ReSTIR>(ctx);
    if (!m_ReSTIR->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "ReSTIR initialization failed");
        m_ReSTIR.reset();
    }

    m_RadianceCache = std::make_unique<Renderer::RadianceCache>(ctx);
    if (!m_RadianceCache->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "Radiance cache initialization failed");
        m_RadianceCache.reset();
    }

    m_SurfelRadianceCache = std::make_unique<Renderer::SurfelRadianceCache>(ctx);
    if (!m_SurfelRadianceCache->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "Surfel radiance cache initialization failed");
        m_SurfelRadianceCache.reset();
    }

    // Initialize SVGF denoiser (always available as fallback)
    m_SVGFDenoiser = std::make_unique<Renderer::SVGFDenoiser>(ctx);
    if (!m_SVGFDenoiser->Initialize(width, height)) {
        ENJIN_LOG_WARN(Renderer, "SVGF Denoiser initialization failed");
        m_SVGFDenoiser.reset();
    }

    // Initialize OIDN denoiser (optional, compile-guarded)
    if (Renderer::OIDNDenoiser::IsAvailable()) {
        m_OIDNDenoiser = std::make_unique<Renderer::OIDNDenoiser>(ctx);
        if (!m_OIDNDenoiser->Initialize(width, height)) {
            ENJIN_LOG_WARN(Renderer, "OIDN Denoiser initialization failed — SVGF will be used");
            m_OIDNDenoiser.reset();
        }
    }

    // Register RT output images with OIDN denoiser so it can perform GPU<->CPU copies
    if (m_OIDNDenoiser) {
        if (m_RTShadows)
            m_OIDNDenoiser->RegisterImageMapping(m_RTShadows->GetOutputView(), m_RTShadows->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTReflections)
            m_OIDNDenoiser->RegisterImageMapping(m_RTReflections->GetOutputView(), m_RTReflections->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTAO)
            m_OIDNDenoiser->RegisterImageMapping(m_RTAO->GetOutputView(), m_RTAO->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTGI)
            m_OIDNDenoiser->RegisterImageMapping(m_RTGI->GetOutputView(), m_RTGI->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTTranslucency)
            m_OIDNDenoiser->RegisterImageMapping(m_RTTranslucency->GetOutputView(), m_RTTranslucency->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTCaustics)
            m_OIDNDenoiser->RegisterImageMapping(m_RTCaustics->GetOutputView(), m_RTCaustics->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    // Initialize OptiX AI Denoiser (optional, compile-guarded)
    if (Renderer::OptiXDenoiser::IsAvailable()) {
        m_OptiXDenoiser = std::make_unique<Renderer::OptiXDenoiser>(ctx);
        if (!m_OptiXDenoiser->Initialize(width, height)) {
            ENJIN_LOG_WARN(Renderer, "OptiX AI Denoiser initialization failed — SVGF/OIDN will be used");
            m_OptiXDenoiser.reset();
        }
    }

    // Register RT output images with OptiX denoiser
    if (m_OptiXDenoiser) {
        if (m_RTShadows)
            m_OptiXDenoiser->RegisterImageMapping(m_RTShadows->GetOutputView(), m_RTShadows->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTReflections)
            m_OptiXDenoiser->RegisterImageMapping(m_RTReflections->GetOutputView(), m_RTReflections->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTAO)
            m_OptiXDenoiser->RegisterImageMapping(m_RTAO->GetOutputView(), m_RTAO->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTGI)
            m_OptiXDenoiser->RegisterImageMapping(m_RTGI->GetOutputView(), m_RTGI->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTTranslucency)
            m_OptiXDenoiser->RegisterImageMapping(m_RTTranslucency->GetOutputView(), m_RTTranslucency->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTCaustics)
            m_OptiXDenoiser->RegisterImageMapping(m_RTCaustics->GetOutputView(), m_RTCaustics->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    // Initialize RT compositor (uses RT descriptor set layout for pipeline compatibility)
    m_RTCompositor = std::make_unique<Renderer::RTCompositor>(ctx);
    if (!m_RTCompositor->Initialize(m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Compositor initialization failed");
        m_RTCompositor.reset();
    }

    // Initialize RT temporal reuse (motion-vector-based history blending for RT outputs)
    m_RTTemporalReuse = std::make_unique<Renderer::RTTemporalReuse>(ctx);
    if (!m_RTTemporalReuse->Initialize(extent.width, extent.height)) {
        ENJIN_LOG_WARN(Renderer, "RT Temporal Reuse initialization failed");
        m_RTTemporalReuse.reset();
    }

    // Create dummy resources and RT light UBOs for descriptor binding
    CreateRTDummyResources();

    // Register dummy image with OIDN/OptiX so depth/normal/motion view lookups resolve
    if (m_OIDNDenoiser && m_RTDummyImageView != VK_NULL_HANDLE) {
        m_OIDNDenoiser->RegisterImageMapping(m_RTDummyImageView, m_RTDummyImage, VK_FORMAT_R8G8B8A8_UNORM);
    }
    if (m_OptiXDenoiser && m_RTDummyImageView != VK_NULL_HANDLE) {
        m_OptiXDenoiser->RegisterImageMapping(m_RTDummyImageView, m_RTDummyImage, VK_FORMAT_R8G8B8A8_UNORM);
    }

    // Register velocity buffer with denoisers for temporal accumulation
    auto* swapchain = m_Renderer->GetSwapchain();
    if (swapchain && swapchain->GetVelocityImageView() != VK_NULL_HANDLE) {
        VkImageView velView = swapchain->GetVelocityImageView();
        VkImage velImage = swapchain->GetVelocityImage();
        if (m_OIDNDenoiser)
            m_OIDNDenoiser->RegisterImageMapping(velView, velImage, Renderer::VulkanSwapchain::VELOCITY_FORMAT);
        if (m_OptiXDenoiser)
            m_OptiXDenoiser->RegisterImageMapping(velView, velImage, Renderer::VulkanSwapchain::VELOCITY_FORMAT);
    }

    ENJIN_LOG_INFO(Renderer, "Ray tracing subsystems initialized (shadows=%s, reflections=%s, AO=%s, GI=%s, pathtracer=%s, restir=%s, surfel_cache=%s)",
                   m_RTShadows ? "yes" : "no", m_RTReflections ? "yes" : "no",
                   m_RTAO ? "yes" : "no", m_RTGI ? "yes" : "no",
                   m_PathTracer ? "yes" : "no", m_ReSTIR ? "yes" : "no",
                   m_SurfelRadianceCache ? "yes" : "no");
}

void RenderSystem::ShutdownRayTracing() {
    m_SurfelRadianceCache.reset();
    m_RTTemporalReuse.reset();
    m_RTCompositor.reset();
    m_ReSTIR.reset();
    m_OptiXDenoiser.reset();
    m_OIDNDenoiser.reset();
    m_SVGFDenoiser.reset();
    m_PathTracer.reset();
    m_RTGI.reset();
    m_RTAO.reset();
    m_RTReflections.reset();
    m_RTShadows.reset();
    m_ASManager.reset();

    DestroyRTDummyResources();

    if (m_Renderer && m_Renderer->GetContext()) {
        VkDevice device = m_Renderer->GetContext()->GetDevice();
        if (m_RTDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, m_RTDescriptorPool, nullptr);
            m_RTDescriptorPool = VK_NULL_HANDLE;
        }
        if (m_RTDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_RTDescriptorSetLayout, nullptr);
            m_RTDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }
    m_RTDescriptorSet = VK_NULL_HANDLE;
    m_RTDescriptorsWritten = false;
}

void RenderSystem::RebuildTLAS(VkCommandBuffer cmd) {
    if (!m_ASManager || !m_RTEnabled) return;

    m_ASManager->ResetInstances();

    // Cache pool buffer device addresses (computed once, reused for all pool entities)
    VkDeviceAddress poolVertBase = 0;
    VkDeviceAddress poolIdxBase = 0;
    if (m_GeometryPool && m_GeometryPool->GetVertexBuffer() && m_GeometryPool->GetIndexBuffer()) {
        poolVertBase = m_GeometryPool->GetVertexBuffer()->GetDeviceAddress();
        poolIdxBase = m_GeometryPool->GetIndexBuffer()->GetDeviceAddress();
    }

    // Add all mesh entities to the TLAS (use cached storage pointers)
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!transform || !mesh || !transform->visible) continue;
        if (mesh->vertices.empty() || mesh->indices.empty()) continue;

        if (static_cast<usize>(entity) >= m_EntityRenderData.size()) continue;
        const auto& rd = m_EntityRenderData[static_cast<usize>(entity)];
        if (!rd.valid) continue;

        VkDeviceAddress vertAddr = 0;
        VkDeviceAddress idxAddr = 0;
        u32 vertexCount = 0;
        u32 indexCount = 0;

        if (rd.vertexBuffer && rd.indexBuffer) {
            // Per-entity buffers (dynamic meshes, skinned, pool overflow)
            vertAddr = rd.vertexBuffer->GetDeviceAddress();
            idxAddr = rd.indexBuffer->GetDeviceAddress();
            vertexCount = static_cast<u32>(mesh->vertices.size());
            indexCount = static_cast<u32>(mesh->indices.size());
        } else if (rd.poolAlloc.valid && poolVertBase != 0 && poolIdxBase != 0) {
            // Pool-allocated: base address + byte offset into merged buffer
            vertAddr = poolVertBase + static_cast<VkDeviceAddress>(rd.poolAlloc.vertexOffset) * sizeof(MeshComponent::Vertex);
            idxAddr = poolIdxBase + static_cast<VkDeviceAddress>(rd.poolAlloc.indexOffset) * sizeof(u32);
            vertexCount = rd.poolAlloc.vertexCount;
            indexCount = rd.poolAlloc.indexCount;
        } else {
            continue;
        }

        if (vertAddr == 0 || idxAddr == 0) continue;

        // Hash based on buffer addresses (unique per mesh region)
        u64 meshHash = vertAddr ^ (idxAddr << 32) ^ (idxAddr >> 32);

        u32 blasId = m_ASManager->RegisterMesh(
            meshHash,
            vertAddr, vertexCount, sizeof(MeshComponent::Vertex),
            idxAddr, indexCount);

        // Build world model matrix (includes parent chain)
        Math::Matrix4 model = ECS::ComputeWorldMatrix(m_World, entity);

        m_ASManager->AddInstance(blasId, model, static_cast<u32>(entity));
    }

    // Upload per-entity material data to the RT material SSBO (binding 9).
    // Must happen before TLAS build so the buffer is valid when descriptors are written,
    // and every frame thereafter since material properties can change at runtime.
    UploadRTMaterials();

    // Flush BLAS builds and build/update TLAS
    if (m_ASManager->HasPendingBuilds()) {
        m_ASManager->FlushPendingBLASBuilds(cmd);
    }
    m_ASManager->BuildTLAS(cmd);

    // Write all RT descriptors once TLAS is valid (need a real handle for binding 0)
    if (!m_RTDescriptorsWritten && m_ASManager->HasValidTLAS()) {
        WriteRTDescriptors();
        TransitionRTOutputImages(cmd);
        m_RTDescriptorsWritten = true;
    } else if (m_RTDescriptorsWritten && m_RTMaterialBuffer != VK_NULL_HANDLE) {
        // Update bindings 9 and 18 each frame to reflect current material data.
        // The buffer contents are already uploaded via UploadRTMaterials() above;
        // this ensures the descriptors point to the correct buffers after any reallocation.
        VkDescriptorBufferInfo matBufInfo{};
        matBufInfo.buffer = m_RTMaterialBuffer;
        matBufInfo.offset = 0;
        matBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo simplifiedMatBufInfo{};
        simplifiedMatBufInfo.buffer = (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE)
            ? m_RTSimplifiedMaterialBuffer : m_RTDummyBuffer;
        simplifiedMatBufInfo.offset = 0;
        simplifiedMatBufInfo.range = (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE)
            ? VK_WHOLE_SIZE : static_cast<VkDeviceSize>(256);

        std::array<VkWriteDescriptorSet, 2> matWrites{};

        // Binding 9: Full material SSBO
        matWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        matWrites[0].dstSet = m_RTDescriptorSet;
        matWrites[0].dstBinding = 9;
        matWrites[0].descriptorCount = 1;
        matWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        matWrites[0].pBufferInfo = &matBufInfo;

        // Binding 18: Simplified material SSBO
        matWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        matWrites[1].dstSet = m_RTDescriptorSet;
        matWrites[1].dstBinding = 18;
        matWrites[1].descriptorCount = 1;
        matWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        matWrites[1].pBufferInfo = &simplifiedMatBufInfo;

        vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(),
                               static_cast<u32>(matWrites.size()), matWrites.data(), 0, nullptr);
    }

}

void RenderSystem::DispatchRTEffects(VkCommandBuffer cmd) {
    if (!m_RTEnabled || !m_ASManager || !m_ASManager->HasValidTLAS()) return;
    if (!m_RTDescriptorsWritten) return;

    // GPU timestamp: RT effects begin
    {
        VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_RT_BEGIN);
        }
    }

    // Compute inverse view-projection and camera position
    Math::Matrix4 view = m_Camera->GetViewMatrix();
    Math::Matrix4 proj = m_Camera->GetProjectionMatrix();
    Math::Matrix4 viewProj = proj * view;
    Math::Matrix4 invViewProj = viewProj.Inverse();
    Math::Vector3 cameraPos = m_Camera->GetPosition();

    // Detect camera changes for path tracer accumulation reset
    bool cameraChanged = false;
    {
        // Compare VP matrices — any significant change resets accumulation
        const f32* a = viewProj.m;
        const f32* b = m_PrevViewProj.m;
        f32 diff = 0.0f;
        for (int i = 0; i < 16; ++i) diff += std::abs(a[i] - b[i]);
        cameraChanged = (diff > 0.001f);
        m_PrevViewProj = viewProj;
    }
    if (cameraChanged && m_PathTracer) {
        m_PathTracer->ResetAccumulation();
        m_RTFrameCount = 0;
    }
    if (cameraChanged && m_RadianceCache) {
        m_RadianceCache->InvalidateAll();
    }
    if (cameraChanged && m_SurfelRadianceCache) {
        // Surfel cache persists across camera movement (world-space), but on
        // major teleport/cut we invalidate to avoid stale data from distant locations.
        m_SurfelRadianceCache->InvalidateAll();
    }
    // Note: temporal reuse does NOT reset on camera change — it handles reprojection
    // via motion vectors and detects disocclusions automatically. Only reset on explicit
    // camera cuts (scene load, teleport) which are handled by ResetHistory() calls.

    // Find primary directional light direction from its transform
    Math::Vector3 lightDir(0.0f, -1.0f, 0.0f);
    f32 lightIntensity = 1.0f;
    f32 lightShadowDistance = 100.0f;
    Math::Vector3 lightColor(1.0f, 1.0f, 1.0f);
    auto* lightStorageRT = m_World->GetComponentStorage<LightComponent>();
    for (Entity entity : m_CachedLightEntities) {
        auto* light = lightStorageRT ? lightStorageRT->Get(entity) : nullptr;
        if (light && light->type == LightType::Directional) {
            auto* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (lightTransform) {
                Math::Vector3 forward(0.0f, 0.0f, -1.0f);
                lightDir = lightTransform->rotation.Rotate(forward);
            }
            lightIntensity = light->intensity;
            lightColor = light->color;
            break;
        }
    }

    VkExtent2D extent = m_Renderer->GetSwapchainExtent();
    m_RTFrameCount++;

    // Update RT light UBO with current frame data
    f32 shadowRadius = m_RTShadows ? m_RTShadows->GetConfig().radius : 0.01f;
    if (m_RTShadows) lightShadowDistance = m_RTShadows->GetConfig().maxDistance;

    // Path tracer config for shader upload
    f32 fireflyClamp = 10.0f;
    i32 enableNEE = 1, enableMIS = 1, rrMinBounce = 3;
    f32 rrMinProb = 0.05f;
    u32 ptMaxBounces = 4, ptAccumulatedSamples = 0;
    if (m_PathTracer) {
        const auto& ptCfg = m_PathTracer->GetConfig();
        fireflyClamp = ptCfg.fireflyClampValue;
        enableNEE = ptCfg.enableNEE ? 1 : 0;
        enableMIS = ptCfg.enableMIS ? 1 : 0;
        rrMinBounce = static_cast<i32>(ptCfg.russianRouletteMinBounce);
        rrMinProb = ptCfg.russianRouletteMinProb;
        ptMaxBounces = ptCfg.maxBounces;
        ptAccumulatedSamples = m_PathTracer->GetAccumulatedSamples();
    }

    // Light counts from cached forward renderer data
    u32 dirLightCount = m_CachedLightingData.directionalLightCount;
    u32 ptLightCount = m_CachedLightingData.pointLightCount;
    u32 sptLightCount = m_CachedLightingData.spotLightCount;

    UpdateRTLightUBO(invViewProj, lightDir, lightIntensity, lightShadowDistance,
                     shadowRadius, m_RTFrameCount,
                     fireflyClamp, enableNEE, enableMIS, rrMinBounce, rrMinProb,
                     dirLightCount, ptLightCount, sptLightCount,
                     ptMaxBounces, ptAccumulatedSamples);

    // Update TLAS descriptor (handle may change on rebuild)
    {
        auto* ctx = m_Renderer->GetContext();
        VkAccelerationStructureKHR tlas = m_ASManager->GetTLAS();
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &tlas;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = &asInfo;
        write.dstSet = m_RTDescriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        vkUpdateDescriptorSets(ctx->GetDevice(), 1, &write, 0, nullptr);
    }

    if (m_RTMode == 1 && m_PathTracer) {
        // Path trace mode — progressive accumulation
        m_PathTracer->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                               lightDir, m_RTFrameCount);
        return;
    }

    // Upload SDF scene data to SSBO (binding 17) for reflection fallback sphere tracing
    if (m_RTSDFMapped && m_SDFScene) {
        auto sdfObjects = m_SDFScene->GetObjectBuffer();
        u32 sdfCount = std::min(static_cast<u32>(sdfObjects.size()), 256u);

        // Buffer layout: [u32 objectCount, u32 pad0, u32 pad1, u32 pad2, SDFObjectGPU objects[...]]
        u32 header[4] = { sdfCount, 0, 0, 0 };
        std::memcpy(m_RTSDFMapped, header, sizeof(header));
        if (sdfCount > 0) {
            std::memcpy(static_cast<u8*>(m_RTSDFMapped) + 16,
                        sdfObjects.data(),
                        sdfCount * sizeof(Renderer::SDFObjectGPU));
        }
    } else if (m_RTSDFMapped) {
        // No SDF scene — zero the object count
        u32 zero = 0;
        std::memcpy(m_RTSDFMapped, &zero, sizeof(zero));
    }

    // ReSTIR — importance-weighted light selection (must run before RT shadows/GI)
    if (m_ReSTIR && m_ReSTIR->GetConfig().enabled) {
        u32 totalLightCount = dirLightCount + ptLightCount + sptLightCount;
        m_ReSTIR->Dispatch(cmd, m_RTDescriptorSet, m_RTFrameCount, totalLightCount);
    }

    // Hybrid mode — dispatch individual effects
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) {
        m_RTShadows->Dispatch(cmd, m_RTDescriptorSet, invViewProj, lightDir,
                              lightIntensity, m_RTFrameCount);
    }
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) {
        m_RTReflections->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                                  m_RTFrameCount);
    }
    if (m_RTAO && m_RTAO->GetConfig().enabled) {
        m_RTAO->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                         m_RTFrameCount);
    }
    if (m_RTGI && m_RTGI->GetConfig().enabled) {
        m_RTGI->Dispatch(cmd, m_RTDescriptorSet, invViewProj, lightDir,
                         cameraPos, m_RTFrameCount);
    }

    // Radiance cache — update tile validity and read cached irradiance.
    // Runs after GI dispatch so it can absorb fresh GI results into cache tiles.
    // The read pass interpolates tile irradiance to per-pixel output.
    if (m_RadianceCache && m_RadianceCache->GetConfig().enabled) {
        m_RadianceCache->DispatchUpdate(cmd, m_RTDescriptorSet, m_RTFrameCount);
        m_RadianceCache->DispatchRead(cmd, m_RTDescriptorSet, m_RTFrameCount);
    }

    // Surfel radiance cache — world-space surfel-based irradiance caching.
    // Runs after screen-space radiance cache so it can blend with its output.
    // Placement spawns/removes surfels, update traces rays, lookup interpolates per-pixel.
    if (m_SurfelRadianceCache && m_SurfelRadianceCache->GetConfig().enabled) {
        m_SurfelRadianceCache->DispatchPlacement(cmd, m_RTDescriptorSet, cameraPos, m_RTFrameCount);
        m_SurfelRadianceCache->DispatchUpdate(cmd, m_RTDescriptorSet, cameraPos, m_RTFrameCount);
        m_SurfelRadianceCache->DispatchLookup(cmd, m_RTDescriptorSet, m_RTFrameCount);
    }

    if (m_RTTranslucency && m_RTTranslucency->GetConfig().enabled) {
        m_RTTranslucency->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                                    m_RTFrameCount);
    }
    if (m_RTCaustics && m_RTCaustics->GetConfig().enabled) {
        m_RTCaustics->Dispatch(cmd, m_RTDescriptorSet, invViewProj, lightDir,
                                m_RTFrameCount);
    }

    // GPU timestamp: RT effects end
    {
        VkQueryPool tsPool = m_Renderer->GetTimestampPool(m_Renderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_RT_END);
        }
    }
}

void RenderSystem::TemporalReuseRTOutputs(VkCommandBuffer cmd) {
    if (!m_RTEnabled || m_RTMode == 1 || !m_RTTemporalReuse) return;
    if (!m_RTTemporalReuse->GetConfig().enabled) return;

    // Obtain depth, normal, and motion views (same logic as DenoiseRTOutputs)
    auto* swapchain = m_Renderer->GetSwapchain();
    VkImageView depthView = (swapchain && swapchain->GetDepthImageView() != VK_NULL_HANDLE)
        ? swapchain->GetDepthImageView() : m_RTDummyImageView;
    VkImageView normalView = m_RTDummyImageView;
    VkImageView motionView = (swapchain && swapchain->GetVelocityImageView() != VK_NULL_HANDLE)
        ? swapchain->GetVelocityImageView() : m_RTDummyImageView;

    // Temporal reuse for each enabled RT buffer (dispatch writes blended result back to output)
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseShadows(cmd, m_RTShadows->GetOutputView(),
            depthView, normalView, motionView, m_RTShadows->GetOutputView());
    }
    if (m_RTAO && m_RTAO->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseAO(cmd, m_RTAO->GetOutputView(),
            depthView, normalView, motionView, m_RTAO->GetOutputView());
    }
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseReflections(cmd, m_RTReflections->GetOutputView(),
            depthView, normalView, motionView, m_RTReflections->GetOutputView());
    }
    if (m_RTGI && m_RTGI->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseGI(cmd, m_RTGI->GetOutputView(),
            depthView, normalView, motionView, m_RTGI->GetOutputView());
    }
}

void RenderSystem::DenoiseRTOutputs(VkCommandBuffer cmd) {
    if (!m_RTEnabled || m_RTMode == 1) return;

    // Select active denoiser based on type setting (0=SVGF, 1=OIDN, 2=OptiX)
    Renderer::IDenoiser* denoiser = nullptr;
    if (m_DenoiserType == 2 && m_OptiXDenoiser) {
        denoiser = m_OptiXDenoiser.get();
    } else if (m_DenoiserType == 1 && m_OIDNDenoiser) {
        denoiser = m_OIDNDenoiser.get();
    } else if (m_SVGFDenoiser) {
        denoiser = m_SVGFDenoiser.get();
    }
    if (!denoiser) return;

    // Use real depth and velocity from swapchain when available; normals still use dummy
    // (no G-buffer MRT normal output yet — will be wired when deferred normals are added)
    auto* swapchain = m_Renderer->GetSwapchain();
    VkImageView depthView = (swapchain && swapchain->GetDepthImageView() != VK_NULL_HANDLE)
        ? swapchain->GetDepthImageView() : m_RTDummyImageView;
    VkImageView normalView = m_RTDummyImageView;
    VkImageView motionView = (swapchain && swapchain->GetVelocityImageView() != VK_NULL_HANDLE)
        ? swapchain->GetVelocityImageView() : m_RTDummyImageView;

    // Denoise shadow output (single channel R16F)
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) {
        denoiser->DenoiseSingleChannel(cmd,
            m_RTShadows->GetOutputView(), depthView, normalView, motionView,
            m_RTShadows->GetOutputView());
    }
    // Denoise AO output (single channel R16F)
    if (m_RTAO && m_RTAO->GetConfig().enabled) {
        denoiser->DenoiseSingleChannel(cmd,
            m_RTAO->GetOutputView(), depthView, normalView, motionView,
            m_RTAO->GetOutputView());
    }
    // Denoise reflections (RGBA16F)
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTReflections->GetOutputView(), depthView, normalView, motionView,
            m_RTReflections->GetOutputView());
    }
    // Denoise GI (RGBA16F)
    if (m_RTGI && m_RTGI->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTGI->GetOutputView(), depthView, normalView, motionView,
            m_RTGI->GetOutputView());
    }
    // Denoise translucency (RGBA16F)
    if (m_RTTranslucency && m_RTTranslucency->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTTranslucency->GetOutputView(), depthView, normalView, motionView,
            m_RTTranslucency->GetOutputView());
    }
    // Denoise caustics (RGBA16F)
    if (m_RTCaustics && m_RTCaustics->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTCaustics->GetOutputView(), depthView, normalView, motionView,
            m_RTCaustics->GetOutputView());
    }
}

void RenderSystem::CompositeRTResults(VkCommandBuffer cmd) {
    if (!m_RTEnabled || !m_RTCompositor || m_RTMode == 1) return;

    VkExtent2D extent = m_Renderer->GetSwapchainExtent();

    // Build enable flags: bit 0=shadow, 1=reflect, 2=ao, 3=gi, 4=translucency, 5=caustics
    u32 enableFlags = 0;
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) enableFlags |= 1;
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) enableFlags |= 2;
    if (m_RTAO && m_RTAO->GetConfig().enabled) enableFlags |= 4;
    if (m_RTGI && m_RTGI->GetConfig().enabled) enableFlags |= 8;
    if (m_RTTranslucency && m_RTTranslucency->GetConfig().enabled) enableFlags |= 16;
    if (m_RTCaustics && m_RTCaustics->GetConfig().enabled) enableFlags |= 32;

    if (enableFlags == 0) return;

    m_RTCompositor->Dispatch(cmd, m_RTDescriptorSet, extent.width, extent.height, enableFlags);
}

void RenderSystem::CreateRTDummyResources() {
    auto* ctx = m_Renderer->GetContext();
    VkDevice device = ctx->GetDevice();

    // Create 1x1 dummy image for placeholder descriptor bindings
    {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgInfo.extent = { 1, 1, 1 };
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &imgInfo, nullptr, &m_RTDummyImage) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy image");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, m_RTDummyImage, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_RTDummyImageMemory) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT dummy image memory");
            return;
        }
        if (vkBindImageMemory(device, m_RTDummyImage, m_RTDummyImageMemory, 0) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to bind RT dummy image memory");
            return;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_RTDummyImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (vkCreateImageView(device, &viewInfo, nullptr, &m_RTDummyImageView) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy image view");
            return;
        }
    }

    // Create sampler for combined image sampler bindings
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_RTDummySampler) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy sampler");
            return;
        }
    }

    // Create dummy buffer for storage buffer bindings (256 bytes)
    {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = 256;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufInfo, nullptr, &m_RTDummyBuffer) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy buffer");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTDummyBuffer, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_RTDummyBufferMemory) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT dummy buffer memory");
            return;
        }
        if (vkBindBufferMemory(device, m_RTDummyBuffer, m_RTDummyBufferMemory, 0) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to bind RT dummy buffer memory");
            return;
        }
    }

    // Create RT light UBOs (per frame in flight, host visible + coherent, persistently mapped)
    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = 256;  // Enough for RTLightUBO struct (112 bytes + padding)
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufInfo, nullptr, &m_RTLightUBO[i]);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTLightUBO[i], &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &allocInfo, nullptr, &m_RTLightUBOMemory[i]);
        vkBindBufferMemory(device, m_RTLightUBO[i], m_RTLightUBOMemory[i], 0);
        if (vkMapMemory(device, m_RTLightUBOMemory[i], 0, 256, 0, &m_RTLightUBOMapped[i]) != VK_SUCCESS) {
            m_RTLightUBOMapped[i] = nullptr;
            continue;
        }
        std::memset(m_RTLightUBOMapped[i], 0, 256);
    }

    // Create NEE light SSBOs (binding 16, per frame in flight — scene lights for path tracer)
    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = RT_NEE_LIGHT_BUFFER_SIZE;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufInfo, nullptr, &m_RTNEELightBuffer[i]);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTNEELightBuffer[i], &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &allocInfo, nullptr, &m_RTNEELightMemory[i]);
        vkBindBufferMemory(device, m_RTNEELightBuffer[i], m_RTNEELightMemory[i], 0);
        if (vkMapMemory(device, m_RTNEELightMemory[i], 0, RT_NEE_LIGHT_BUFFER_SIZE, 0, &m_RTNEELightMapped[i]) != VK_SUCCESS) {
            m_RTNEELightMapped[i] = nullptr;
            continue;
        }
        std::memset(m_RTNEELightMapped[i], 0, RT_NEE_LIGHT_BUFFER_SIZE);
    }

    // Create RT material SSBO (binding 9) — persistently mapped, host visible + coherent
    EnsureRTMaterialBuffer(RT_MATERIAL_BUFFER_INITIAL_CAPACITY);

    // Create RT simplified material SSBO (binding 18) — pre-baked material properties
    EnsureRTSimplifiedMaterialBuffer(RT_MATERIAL_BUFFER_INITIAL_CAPACITY);

    // Create SDF scene SSBO (binding 17) — persistently mapped for per-frame SDF object upload
    {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = RT_SDF_BUFFER_SIZE;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufInfo, nullptr, &m_RTSDFBuffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTSDFBuffer, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &allocInfo, nullptr, &m_RTSDFMemory);
        vkBindBufferMemory(device, m_RTSDFBuffer, m_RTSDFMemory, 0);
        if (vkMapMemory(device, m_RTSDFMemory, 0, RT_SDF_BUFFER_SIZE, 0, &m_RTSDFMapped) != VK_SUCCESS) {
            m_RTSDFMapped = nullptr;
        } else {
            std::memset(m_RTSDFMapped, 0, RT_SDF_BUFFER_SIZE);
        }
    }

    ENJIN_LOG_INFO(Renderer, "RT dummy resources, light UBOs, NEE light SSBOs, material SSBO, simplified material SSBO, and SDF SSBO created");
}

void RenderSystem::EnsureRTMaterialBuffer(u32 requiredCapacity) {
    if (requiredCapacity <= m_RTMaterialBufferCapacity && m_RTMaterialBuffer != VK_NULL_HANDLE) return;

    auto* ctx = m_Renderer->GetContext();
    VkDevice device = ctx->GetDevice();

    // Destroy old buffer if exists
    if (m_RTMaterialMapped) {
        vkUnmapMemory(device, m_RTMaterialMemory);
        m_RTMaterialMapped = nullptr;
    }
    if (m_RTMaterialBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_RTMaterialBuffer, nullptr);
        m_RTMaterialBuffer = VK_NULL_HANDLE;
    }
    if (m_RTMaterialMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_RTMaterialMemory, nullptr);
        m_RTMaterialMemory = VK_NULL_HANDLE;
    }

    // Grow by at least 2x to avoid frequent reallocations
    u32 newCapacity = m_RTMaterialBufferCapacity > 0
        ? m_RTMaterialBufferCapacity * 2
        : RT_MATERIAL_BUFFER_INITIAL_CAPACITY;
    if (newCapacity < requiredCapacity) newCapacity = requiredCapacity;

    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(newCapacity) * sizeof(MaterialGPU);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_RTMaterialBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT material SSBO (%u entries)", newCapacity);
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_RTMaterialBuffer, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_RTMaterialMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT material SSBO memory");
        vkDestroyBuffer(device, m_RTMaterialBuffer, nullptr);
        m_RTMaterialBuffer = VK_NULL_HANDLE;
        return;
    }

    if (vkBindBufferMemory(device, m_RTMaterialBuffer, m_RTMaterialMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind RT material SSBO memory");
        return;
    }

    if (vkMapMemory(device, m_RTMaterialMemory, 0, bufferSize, 0, &m_RTMaterialMapped) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map RT material SSBO");
        m_RTMaterialMapped = nullptr;
        return;
    }

    std::memset(m_RTMaterialMapped, 0, static_cast<size_t>(bufferSize));
    m_RTMaterialBufferCapacity = newCapacity;

    ENJIN_LOG_INFO(Renderer, "RT material SSBO created/resized: %u entries (%llu bytes)",
                   newCapacity, static_cast<unsigned long long>(bufferSize));
}

void RenderSystem::EnsureRTSimplifiedMaterialBuffer(u32 requiredCapacity) {
    if (requiredCapacity <= m_RTSimplifiedMaterialBufferCapacity && m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE) return;

    auto* ctx = m_Renderer->GetContext();
    VkDevice device = ctx->GetDevice();

    // Destroy old buffer if exists
    if (m_RTSimplifiedMaterialMapped) {
        vkUnmapMemory(device, m_RTSimplifiedMaterialMemory);
        m_RTSimplifiedMaterialMapped = nullptr;
    }
    if (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_RTSimplifiedMaterialBuffer, nullptr);
        m_RTSimplifiedMaterialBuffer = VK_NULL_HANDLE;
    }
    if (m_RTSimplifiedMaterialMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_RTSimplifiedMaterialMemory, nullptr);
        m_RTSimplifiedMaterialMemory = VK_NULL_HANDLE;
    }

    // Grow by at least 2x to avoid frequent reallocations
    u32 newCapacity = m_RTSimplifiedMaterialBufferCapacity > 0
        ? m_RTSimplifiedMaterialBufferCapacity * 2
        : RT_MATERIAL_BUFFER_INITIAL_CAPACITY;
    if (newCapacity < requiredCapacity) newCapacity = requiredCapacity;

    // RTSimplifiedMaterialGPU is 64 bytes per entry
    VkDeviceSize simplifiedBufSize = static_cast<VkDeviceSize>(newCapacity) * 64;

    VkBufferCreateInfo simplifiedBufCI{};
    simplifiedBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    simplifiedBufCI.size = simplifiedBufSize;
    simplifiedBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    simplifiedBufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &simplifiedBufCI, nullptr, &m_RTSimplifiedMaterialBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT simplified material SSBO (%u entries)", newCapacity);
        return;
    }

    VkMemoryRequirements smemReqs;
    vkGetBufferMemoryRequirements(device, m_RTSimplifiedMaterialBuffer, &smemReqs);
    VkMemoryAllocateInfo sallocInfo{};
    sallocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    sallocInfo.allocationSize = smemReqs.size;
    sallocInfo.memoryTypeIndex = ctx->FindMemoryType(smemReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &sallocInfo, nullptr, &m_RTSimplifiedMaterialMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT simplified material SSBO memory");
        vkDestroyBuffer(device, m_RTSimplifiedMaterialBuffer, nullptr);
        m_RTSimplifiedMaterialBuffer = VK_NULL_HANDLE;
        return;
    }

    if (vkBindBufferMemory(device, m_RTSimplifiedMaterialBuffer, m_RTSimplifiedMaterialMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind RT simplified material SSBO memory");
        return;
    }

    if (vkMapMemory(device, m_RTSimplifiedMaterialMemory, 0, simplifiedBufSize, 0, &m_RTSimplifiedMaterialMapped) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map RT simplified material SSBO");
        m_RTSimplifiedMaterialMapped = nullptr;
        return;
    }

    std::memset(m_RTSimplifiedMaterialMapped, 0, static_cast<size_t>(simplifiedBufSize));
    m_RTSimplifiedMaterialBufferCapacity = newCapacity;

    ENJIN_LOG_INFO(Renderer, "RT simplified material SSBO created/resized: %u entries (%llu bytes)",
                   newCapacity, static_cast<unsigned long long>(simplifiedBufSize));
}

void RenderSystem::DestroyRTDummyResources() {
    if (!m_Renderer || !m_Renderer->GetContext()) return;
    VkDevice device = m_Renderer->GetContext()->GetDevice();

    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        if (m_RTLightUBOMapped[i]) {
            vkUnmapMemory(device, m_RTLightUBOMemory[i]);
            m_RTLightUBOMapped[i] = nullptr;
        }
        if (m_RTLightUBO[i]) { vkDestroyBuffer(device, m_RTLightUBO[i], nullptr); m_RTLightUBO[i] = VK_NULL_HANDLE; }
        if (m_RTLightUBOMemory[i]) { vkFreeMemory(device, m_RTLightUBOMemory[i], nullptr); m_RTLightUBOMemory[i] = VK_NULL_HANDLE; }
    }

    if (m_RTDummySampler) { vkDestroySampler(device, m_RTDummySampler, nullptr); m_RTDummySampler = VK_NULL_HANDLE; }
    if (m_RTDummyImageView) { vkDestroyImageView(device, m_RTDummyImageView, nullptr); m_RTDummyImageView = VK_NULL_HANDLE; }
    if (m_RTDummyImage) { vkDestroyImage(device, m_RTDummyImage, nullptr); m_RTDummyImage = VK_NULL_HANDLE; }
    if (m_RTDummyImageMemory) { vkFreeMemory(device, m_RTDummyImageMemory, nullptr); m_RTDummyImageMemory = VK_NULL_HANDLE; }
    if (m_RTDummyBuffer) { vkDestroyBuffer(device, m_RTDummyBuffer, nullptr); m_RTDummyBuffer = VK_NULL_HANDLE; }
    if (m_RTDummyBufferMemory) { vkFreeMemory(device, m_RTDummyBufferMemory, nullptr); m_RTDummyBufferMemory = VK_NULL_HANDLE; }

    // Destroy NEE light SSBOs
    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        if (m_RTNEELightMapped[i]) {
            vkUnmapMemory(device, m_RTNEELightMemory[i]);
            m_RTNEELightMapped[i] = nullptr;
        }
        if (m_RTNEELightBuffer[i]) { vkDestroyBuffer(device, m_RTNEELightBuffer[i], nullptr); m_RTNEELightBuffer[i] = VK_NULL_HANDLE; }
        if (m_RTNEELightMemory[i]) { vkFreeMemory(device, m_RTNEELightMemory[i], nullptr); m_RTNEELightMemory[i] = VK_NULL_HANDLE; }
    }

    // Destroy RT material SSBO
    if (m_RTMaterialMapped) { vkUnmapMemory(device, m_RTMaterialMemory); m_RTMaterialMapped = nullptr; }
    if (m_RTMaterialBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_RTMaterialBuffer, nullptr); m_RTMaterialBuffer = VK_NULL_HANDLE; }
    if (m_RTMaterialMemory != VK_NULL_HANDLE) { vkFreeMemory(device, m_RTMaterialMemory, nullptr); m_RTMaterialMemory = VK_NULL_HANDLE; }
    m_RTMaterialBufferCapacity = 0;

    // Destroy SDF scene SSBO
    if (m_RTSDFMapped) { vkUnmapMemory(device, m_RTSDFMemory); m_RTSDFMapped = nullptr; }
    if (m_RTSDFBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_RTSDFBuffer, nullptr); m_RTSDFBuffer = VK_NULL_HANDLE; }
    if (m_RTSDFMemory != VK_NULL_HANDLE) { vkFreeMemory(device, m_RTSDFMemory, nullptr); m_RTSDFMemory = VK_NULL_HANDLE; }

    // Destroy RT simplified material SSBO
    if (m_RTSimplifiedMaterialMapped) { vkUnmapMemory(device, m_RTSimplifiedMaterialMemory); m_RTSimplifiedMaterialMapped = nullptr; }
    if (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_RTSimplifiedMaterialBuffer, nullptr); m_RTSimplifiedMaterialBuffer = VK_NULL_HANDLE; }
    if (m_RTSimplifiedMaterialMemory != VK_NULL_HANDLE) { vkFreeMemory(device, m_RTSimplifiedMaterialMemory, nullptr); m_RTSimplifiedMaterialMemory = VK_NULL_HANDLE; }
    m_RTSimplifiedMaterialBufferCapacity = 0;
}

void RenderSystem::UploadRTMaterials() {
    if (!m_RTEnabled || !m_ASManager) return;

    // Find the highest entity ID among renderable mesh entities to size the buffer
    u32 maxEntityId = 0;
    u32 entityCount = 0;
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!transform || !mesh || !transform->visible) continue;
        if (mesh->vertices.empty() || mesh->indices.empty()) continue;
        if (static_cast<usize>(entity) >= m_EntityRenderData.size()) continue;
        if (!m_EntityRenderData[static_cast<usize>(entity)].valid) continue;

        u32 eid = static_cast<u32>(entity);
        if (eid > maxEntityId) maxEntityId = eid;
        entityCount++;
    }

    if (entityCount == 0) return;

    // Ensure buffer is large enough (indexed by entity ID, so need maxEntityId + 1 entries)
    u32 requiredCapacity = maxEntityId + 1;
    bool bufferGrew = (requiredCapacity > m_RTMaterialBufferCapacity);
    bool simplifiedBufferGrew = (requiredCapacity > m_RTSimplifiedMaterialBufferCapacity);
    if (bufferGrew || simplifiedBufferGrew) {
        // GPU must be idle before reallocating a buffer that may be in-flight
        vkDeviceWaitIdle(m_Renderer->GetContext()->GetDevice());
        if (bufferGrew) EnsureRTMaterialBuffer(requiredCapacity);
        if (simplifiedBufferGrew) EnsureRTSimplifiedMaterialBuffer(requiredCapacity);

        // Force descriptor re-write since a buffer handle changed
        m_RTDescriptorsWritten = false;
    }

    if (!m_RTMaterialMapped) return;

    // GPU struct matching GLSL RTSimplifiedMaterial (std430 layout, 64 bytes)
    struct RTSimplifiedMaterialGPU {
        f32 albedo[3];          f32 effectiveRoughness;  // 16 bytes
        f32 f0[3];              f32 kDiffuse;             // 16 bytes
        f32 emissive[3];        f32 opacity;              // 16 bytes
        f32 transmission;       f32 ior;  f32 _pad0;  f32 _pad1; // 16 bytes
    };
    static_assert(sizeof(RTSimplifiedMaterialGPU) == 64, "RTSimplifiedMaterialGPU must be 64 bytes for std430");

    // Upload MaterialGPU for each renderable entity, indexed by entity ID
    // (matches gl_InstanceCustomIndexEXT set in AddInstance)
    auto* dst = static_cast<MaterialGPU*>(m_RTMaterialMapped);
    auto* sdst = m_RTSimplifiedMaterialMapped
                 ? static_cast<RTSimplifiedMaterialGPU*>(m_RTSimplifiedMaterialMapped)
                 : nullptr;
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!transform || !mesh || !transform->visible) continue;
        if (mesh->vertices.empty() || mesh->indices.empty()) continue;
        if (static_cast<usize>(entity) >= m_EntityRenderData.size()) continue;
        if (!m_EntityRenderData[static_cast<usize>(entity)].valid) continue;

        u32 eid = static_cast<u32>(entity);
        auto* mat = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (mat) {
            dst[eid] = MaterialGPU::FromComponent(*mat);
        } else {
            // Default material for entities without a MaterialComponent
            dst[eid] = MaterialGPU{};
            dst[eid].baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            dst[eid].roughness = 0.5f;
            dst[eid].opacity = 1.0f;
            dst[eid].alphaCutoff = 0.5f;
            dst[eid].ior = 1.5f;
            dst[eid].sssColor = Math::Vector3(1.0f, 0.2f, 0.1f);
            dst[eid].sssRadius = 1.0f;
        }

        // Pre-compute simplified material from the just-written MaterialGPU
        if (sdst) {
            const auto& m = dst[eid];
            auto& s = sdst[eid];

            // Albedo
            s.albedo[0] = m.baseColor.x;
            s.albedo[1] = m.baseColor.y;
            s.albedo[2] = m.baseColor.z;

            // Effective roughness (clamped to avoid singularities, matches fetchMaterial)
            s.effectiveRoughness = std::max(m.roughness, 0.04f);

            // F0: reflectance at normal incidence = mix(0.04, baseColor, metallic)
            f32 oneMinusMetallic = 1.0f - m.metallic;
            s.f0[0] = 0.04f * oneMinusMetallic + m.baseColor.x * m.metallic;
            s.f0[1] = 0.04f * oneMinusMetallic + m.baseColor.y * m.metallic;
            s.f0[2] = 0.04f * oneMinusMetallic + m.baseColor.z * m.metallic;

            // kDiffuse: diffuse weight = (1 - metallic)
            s.kDiffuse = oneMinusMetallic;

            // Emissive: pre-multiply color * strength
            s.emissive[0] = m.emissiveColor.x * m.emissiveStrength;
            s.emissive[1] = m.emissiveColor.y * m.emissiveStrength;
            s.emissive[2] = m.emissiveColor.z * m.emissiveStrength;

            s.opacity = m.opacity;
            s.transmission = m.transmission;
            s.ior = m.ior;
            s._pad0 = 0.0f;
            s._pad1 = 0.0f;
        }
    }
}

void RenderSystem::WriteRTDescriptors() {
    auto* ctx = m_Renderer->GetContext();
    VkDevice device = ctx->GetDevice();
    u32 frameIdx = m_Renderer->GetCurrentFrameIndex();

    // Binding 0: TLAS (acceleration structure)
    VkAccelerationStructureKHR tlas = m_ASManager->GetTLAS();
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &tlas;

    // Binding 1: Scene HDR / Accumulation (storage image) — use path tracer output or dummy
    VkDescriptorImageInfo storageImageInfo1{};
    storageImageInfo1.imageView = m_PathTracer ? m_PathTracer->GetOutputView() : m_RTDummyImageView;
    storageImageInfo1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Bindings 2, 3, 4: Depth, normals, motion vectors (combined image sampler)
    // Binding 2 uses real swapchain depth when available; bindings 3-4 use dummy (no G-buffer MRT yet)
    VkDescriptorImageInfo samplerInfos[3]{};
    for (auto& si : samplerInfos) {
        si.sampler = m_RTDummySampler;
        si.imageView = m_RTDummyImageView;
        si.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    // Wire real depth buffer from swapchain
    auto* swapchain = m_Renderer->GetSwapchain();
    if (swapchain && swapchain->GetDepthImageView() != VK_NULL_HANDLE) {
        samplerInfos[0].imageView = swapchain->GetDepthImageView();
        samplerInfos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    // Bindings 5-8: RT output images (storage image), 14-15: translucency, caustics
    VkDescriptorImageInfo rtOutputInfos[6]{};
    rtOutputInfos[0].imageView = m_RTShadows ? m_RTShadows->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[1].imageView = m_RTReflections ? m_RTReflections->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[2].imageView = m_RTAO ? m_RTAO->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[3].imageView = m_RTGI ? m_RTGI->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[4].imageView = m_RTTranslucency ? m_RTTranslucency->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[5].imageView = m_RTCaustics ? m_RTCaustics->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Bindings 9-12: Storage buffers
    // Binding 9 = material SSBO (real buffer), bindings 10-12 = dummy (vertex/index/transforms TBD)
    VkDescriptorBufferInfo dummyBufInfos[4]{};
    // Binding 9: Material SSBO — use real buffer if available, otherwise dummy
    if (m_RTMaterialBuffer != VK_NULL_HANDLE && m_RTMaterialBufferCapacity > 0) {
        dummyBufInfos[0].buffer = m_RTMaterialBuffer;
        dummyBufInfos[0].offset = 0;
        dummyBufInfos[0].range = VK_WHOLE_SIZE;
    } else {
        dummyBufInfos[0].buffer = m_RTDummyBuffer;
        dummyBufInfos[0].offset = 0;
        dummyBufInfos[0].range = 256;
    }
    // Bindings 10-12: Still dummy (vertex/index/transforms — future work)
    for (u32 i = 1; i < 4; ++i) {
        dummyBufInfos[i].buffer = m_RTDummyBuffer;
        dummyBufInfos[i].offset = 0;
        dummyBufInfos[i].range = 256;
    }

    // Binding 13: RT light UBO
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_RTLightUBO[frameIdx];
    uboInfo.offset = 0;
    uboInfo.range = 256;

    // Binding 16: NEE light SSBO
    VkDescriptorBufferInfo neeBufInfo{};
    neeBufInfo.buffer = m_RTNEELightBuffer[frameIdx] ? m_RTNEELightBuffer[frameIdx] : m_RTDummyBuffer;
    neeBufInfo.offset = 0;
    neeBufInfo.range = m_RTNEELightBuffer[frameIdx] ? static_cast<VkDeviceSize>(RT_NEE_LIGHT_BUFFER_SIZE) : 256;

    // Binding 17: SDF scene SSBO
    VkDescriptorBufferInfo sdfBufInfo{};
    sdfBufInfo.buffer = m_RTSDFBuffer ? m_RTSDFBuffer : m_RTDummyBuffer;
    sdfBufInfo.offset = 0;
    sdfBufInfo.range = m_RTSDFBuffer ? static_cast<VkDeviceSize>(RT_SDF_BUFFER_SIZE) : 256;

    // Binding 18: Simplified material SSBO
    VkDescriptorBufferInfo simplifiedMatBufInfo{};
    if (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE && m_RTSimplifiedMaterialBufferCapacity > 0) {
        simplifiedMatBufInfo.buffer = m_RTSimplifiedMaterialBuffer;
        simplifiedMatBufInfo.offset = 0;
        simplifiedMatBufInfo.range = VK_WHOLE_SIZE;
    } else {
        simplifiedMatBufInfo.buffer = m_RTDummyBuffer;
        simplifiedMatBufInfo.offset = 0;
        simplifiedMatBufInfo.range = 256;
    }

    // Binding 19: ReSTIR reservoir SSBO -- current frame
    VkDescriptorBufferInfo restirBufInfo{};
    if (m_ReSTIR && m_ReSTIR->GetReservoirBuffer() != VK_NULL_HANDLE) {
        restirBufInfo.buffer = m_ReSTIR->GetReservoirBuffer();
        restirBufInfo.offset = 0;
        restirBufInfo.range = VK_WHOLE_SIZE;
    } else {
        restirBufInfo.buffer = m_RTDummyBuffer;
        restirBufInfo.offset = 0;
        restirBufInfo.range = 256;
    }

    // Binding 20: ReSTIR reservoir SSBO -- previous frame (ping-pong for temporal reuse)
    VkDescriptorBufferInfo restirPrevBufInfo{};
    if (m_ReSTIR && m_ReSTIR->GetPrevReservoirBuffer() != VK_NULL_HANDLE) {
        restirPrevBufInfo.buffer = m_ReSTIR->GetPrevReservoirBuffer();
        restirPrevBufInfo.offset = 0;
        restirPrevBufInfo.range = VK_WHOLE_SIZE;
    } else {
        restirPrevBufInfo.buffer = m_RTDummyBuffer;
        restirPrevBufInfo.offset = 0;
        restirPrevBufInfo.range = 256;
    }

    // Binding 21: Radiance cache tile buffer
    VkDescriptorBufferInfo rcTileBufInfo{};
    if (m_RadianceCache && m_RadianceCache->GetTileBuffer() != VK_NULL_HANDLE) {
        rcTileBufInfo.buffer = m_RadianceCache->GetTileBuffer();
        rcTileBufInfo.offset = 0;
        rcTileBufInfo.range = VK_WHOLE_SIZE;
    } else {
        rcTileBufInfo.buffer = m_RTDummyBuffer;
        rcTileBufInfo.offset = 0;
        rcTileBufInfo.range = 256;
    }

    // Binding 22: Radiance cache stale mask buffer
    VkDescriptorBufferInfo rcStaleMaskBufInfo{};
    if (m_RadianceCache && m_RadianceCache->GetStaleMaskBuffer() != VK_NULL_HANDLE) {
        rcStaleMaskBufInfo.buffer = m_RadianceCache->GetStaleMaskBuffer();
        rcStaleMaskBufInfo.offset = 0;
        rcStaleMaskBufInfo.range = VK_WHOLE_SIZE;
    } else {
        rcStaleMaskBufInfo.buffer = m_RTDummyBuffer;
        rcStaleMaskBufInfo.offset = 0;
        rcStaleMaskBufInfo.range = 256;
    }

    // Binding 23: Radiance cache output image
    VkDescriptorImageInfo rcOutputImgInfo{};
    rcOutputImgInfo.imageView = m_RadianceCache ? m_RadianceCache->GetOutputView() : m_RTDummyImageView;
    rcOutputImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Binding 24: Surfel radiance cache buffer
    VkDescriptorBufferInfo surfelBufInfo{};
    if (m_SurfelRadianceCache && m_SurfelRadianceCache->GetSurfelBuffer() != VK_NULL_HANDLE) {
        surfelBufInfo.buffer = m_SurfelRadianceCache->GetSurfelBuffer();
        surfelBufInfo.offset = 0;
        surfelBufInfo.range = VK_WHOLE_SIZE;
    } else {
        surfelBufInfo.buffer = m_RTDummyBuffer;
        surfelBufInfo.offset = 0;
        surfelBufInfo.range = 256;
    }

    // Binding 25: Surfel counter/metadata buffer
    VkDescriptorBufferInfo surfelCounterBufInfo{};
    if (m_SurfelRadianceCache && m_SurfelRadianceCache->GetSurfelCounterBuffer() != VK_NULL_HANDLE) {
        surfelCounterBufInfo.buffer = m_SurfelRadianceCache->GetSurfelCounterBuffer();
        surfelCounterBufInfo.offset = 0;
        surfelCounterBufInfo.range = VK_WHOLE_SIZE;
    } else {
        surfelCounterBufInfo.buffer = m_RTDummyBuffer;
        surfelCounterBufInfo.offset = 0;
        surfelCounterBufInfo.range = 256;
    }

    // Binding 26: Surfel radiance cache output image
    VkDescriptorImageInfo surfelOutputImgInfo{};
    surfelOutputImgInfo.imageView = m_SurfelRadianceCache ? m_SurfelRadianceCache->GetOutputView() : m_RTDummyImageView;
    surfelOutputImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Build write array for all 27 bindings
    std::array<VkWriteDescriptorSet, 27> writes{};

    // Binding 0: TLAS
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &asInfo;
    writes[0].dstSet = m_RTDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    // Binding 1: Scene HDR (storage image)
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_RTDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &storageImageInfo1;

    // Bindings 2, 3, 4: Depth, normals, motion vectors
    for (u32 i = 0; i < 3; ++i) {
        writes[2 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2 + i].dstSet = m_RTDescriptorSet;
        writes[2 + i].dstBinding = 2 + i;
        writes[2 + i].descriptorCount = 1;
        writes[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2 + i].pImageInfo = &samplerInfos[i];
    }

    // Bindings 5-8: RT output images
    for (u32 i = 0; i < 4; ++i) {
        writes[5 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5 + i].dstSet = m_RTDescriptorSet;
        writes[5 + i].dstBinding = 5 + i;
        writes[5 + i].descriptorCount = 1;
        writes[5 + i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[5 + i].pImageInfo = &rtOutputInfos[i];
    }

    // Bindings 9-12: Storage buffers
    for (u32 i = 0; i < 4; ++i) {
        writes[9 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[9 + i].dstSet = m_RTDescriptorSet;
        writes[9 + i].dstBinding = 9 + i;
        writes[9 + i].descriptorCount = 1;
        writes[9 + i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[9 + i].pBufferInfo = &dummyBufInfos[i];
    }

    // Binding 13: Light UBO
    writes[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[13].dstSet = m_RTDescriptorSet;
    writes[13].dstBinding = 13;
    writes[13].descriptorCount = 1;
    writes[13].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[13].pBufferInfo = &uboInfo;

    // Binding 14: RT Translucency output
    writes[14].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[14].dstSet = m_RTDescriptorSet;
    writes[14].dstBinding = 14;
    writes[14].descriptorCount = 1;
    writes[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[14].pImageInfo = &rtOutputInfos[4];

    // Binding 15: RT Caustics output
    writes[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[15].dstSet = m_RTDescriptorSet;
    writes[15].dstBinding = 15;
    writes[15].descriptorCount = 1;
    writes[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[15].pImageInfo = &rtOutputInfos[5];

    // Binding 16: NEE light SSBO
    writes[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[16].dstSet = m_RTDescriptorSet;
    writes[16].dstBinding = 16;
    writes[16].descriptorCount = 1;
    writes[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[16].pBufferInfo = &neeBufInfo;

    // Binding 17: SDF scene SSBO
    writes[17].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[17].dstSet = m_RTDescriptorSet;
    writes[17].dstBinding = 17;
    writes[17].descriptorCount = 1;
    writes[17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[17].pBufferInfo = &sdfBufInfo;

    // Binding 18: Simplified material SSBO
    writes[18].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[18].dstSet = m_RTDescriptorSet;
    writes[18].dstBinding = 18;
    writes[18].descriptorCount = 1;
    writes[18].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[18].pBufferInfo = &simplifiedMatBufInfo;

    // Binding 19: ReSTIR reservoir SSBO
    writes[19].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[19].dstSet = m_RTDescriptorSet;
    writes[19].dstBinding = 19;
    writes[19].descriptorCount = 1;
    writes[19].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[19].pBufferInfo = &restirBufInfo;

    // Binding 20: ReSTIR reservoir SSBO — previous frame (placeholder, temporal reuse not yet active)
    writes[20].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[20].dstSet = m_RTDescriptorSet;
    writes[20].dstBinding = 20;
    writes[20].descriptorCount = 1;
    writes[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[20].pBufferInfo = &restirPrevBufInfo;

    // Binding 21: Radiance cache tile buffer
    writes[21].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[21].dstSet = m_RTDescriptorSet;
    writes[21].dstBinding = 21;
    writes[21].descriptorCount = 1;
    writes[21].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[21].pBufferInfo = &rcTileBufInfo;

    // Binding 22: Radiance cache stale mask buffer
    writes[22].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[22].dstSet = m_RTDescriptorSet;
    writes[22].dstBinding = 22;
    writes[22].descriptorCount = 1;
    writes[22].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[22].pBufferInfo = &rcStaleMaskBufInfo;

    // Binding 23: Radiance cache output image
    writes[23].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[23].dstSet = m_RTDescriptorSet;
    writes[23].dstBinding = 23;
    writes[23].descriptorCount = 1;
    writes[23].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[23].pImageInfo = &rcOutputImgInfo;

    // Binding 24: Surfel radiance cache buffer
    writes[24].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[24].dstSet = m_RTDescriptorSet;
    writes[24].dstBinding = 24;
    writes[24].descriptorCount = 1;
    writes[24].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[24].pBufferInfo = &surfelBufInfo;

    // Binding 25: Surfel counter/metadata buffer
    writes[25].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[25].dstSet = m_RTDescriptorSet;
    writes[25].dstBinding = 25;
    writes[25].descriptorCount = 1;
    writes[25].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[25].pBufferInfo = &surfelCounterBufInfo;

    // Binding 26: Surfel radiance cache output image
    writes[26].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[26].dstSet = m_RTDescriptorSet;
    writes[26].dstBinding = 26;
    writes[26].descriptorCount = 1;
    writes[26].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[26].pImageInfo = &surfelOutputImgInfo;

    vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

    ENJIN_LOG_INFO(Renderer, "RT descriptor set written (all 27 bindings)");
}

void RenderSystem::TransitionRTOutputImages(VkCommandBuffer cmd) {
    // Transition all RT output images from UNDEFINED → GENERAL for storage image usage
    std::vector<VkImageMemoryBarrier> barriers;

    auto addBarrier = [&](VkImage image) {
        if (image == VK_NULL_HANDLE) return;
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barriers.push_back(barrier);
    };

    if (m_RTShadows) addBarrier(m_RTShadows->GetOutputImage());
    if (m_RTReflections) addBarrier(m_RTReflections->GetOutputImage());
    if (m_RTAO) addBarrier(m_RTAO->GetOutputImage());
    if (m_RTGI) addBarrier(m_RTGI->GetOutputImage());
    if (m_RTTranslucency) addBarrier(m_RTTranslucency->GetOutputImage());
    if (m_RTCaustics) addBarrier(m_RTCaustics->GetOutputImage());
    if (m_PathTracer) addBarrier(m_PathTracer->GetOutputImage());
    if (m_RadianceCache) addBarrier(m_RadianceCache->GetOutputImage());
    if (m_SurfelRadianceCache) addBarrier(m_SurfelRadianceCache->GetOutputImage());

    // Also transition the dummy image
    addBarrier(m_RTDummyImage);

    if (!barriers.empty()) {
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr,
            static_cast<u32>(barriers.size()), barriers.data());
    }

    ENJIN_LOG_INFO(Renderer, "RT output images transitioned to GENERAL layout (%zu images)", barriers.size());
}

void RenderSystem::UpdateRTLightUBO(const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                                     f32 lightIntensity, f32 shadowDistance, f32 shadowRadius, u32 frameCount,
                                     f32 fireflyClamp, i32 enableNEE, i32 enableMIS,
                                     i32 rrMinBounce, f32 rrMinProb,
                                     u32 dirLightCount, u32 ptLightCount, u32 sptLightCount,
                                     u32 maxBounces, u32 accumulatedSamples) {
    u32 frameIdx = m_Renderer->GetCurrentFrameIndex();
    if (!m_RTLightUBOMapped[frameIdx]) return;

    VkExtent2D extent = m_Renderer->GetSwapchainExtent();

    // RT light UBO layout (std140, matches shader LightData uniform block):
    // vec4 lightDir            (offset 0)
    // vec4 lightColor          (offset 16)
    // mat4 invViewProj         (offset 32)
    // vec2 screenSize          (offset 96)
    // uint frameCount          (offset 104)
    // float shadowRadius       (offset 108)
    // float fireflyClamp       (offset 112)
    // int enableNEE            (offset 116)
    // int enableMIS            (offset 120)
    // int rrMinBounce          (offset 124)
    // float rrMinProb          (offset 128)
    // uint dirLightCount       (offset 132)
    // uint pointLightCount     (offset 136)
    // uint spotLightCount      (offset 140)
    // uint maxBounces          (offset 144)
    // uint accumulatedSamples  (offset 148)
    // uint _pad[2]             (offset 152)  -- pad to 160 bytes
    struct RTLightData {
        f32 lightDir[4];
        f32 lightColor[4];
        f32 invViewProj[16];
        f32 screenSize[2];
        u32 frameCount;
        f32 shadowRadius;
        // Path tracer config
        f32 fireflyClamp;
        i32 enableNEE;
        i32 enableMIS;
        i32 rrMinBounce;
        f32 rrMinProb;
        // NEE light counts
        u32 directionalLightCount;
        u32 pointLightCount;
        u32 spotLightCount;
        // Convergence
        u32 maxBounces;
        u32 accumulatedSamples;
        // SDF fallback (reflection shader reads these at offset 152-160)
        f32 sdfFallbackEnabled;   // >0.5 = SDF sphere-trace fallback active
        f32 sdfMaxDistance;       // Max sphere-trace distance
    };

    RTLightData data{};
    data.lightDir[0] = lightDir.x;
    data.lightDir[1] = lightDir.y;
    data.lightDir[2] = lightDir.z;
    data.lightDir[3] = lightIntensity;
    data.lightColor[0] = 1.0f;
    data.lightColor[1] = 1.0f;
    data.lightColor[2] = 1.0f;
    data.lightColor[3] = shadowDistance;

    // Copy mat4 (our Matrix4 stores as flat f32 m[16])
    for (int i = 0; i < 16; ++i) {
        data.invViewProj[i] = invViewProj.m[i];
    }

    data.screenSize[0] = static_cast<f32>(extent.width);
    data.screenSize[1] = static_cast<f32>(extent.height);
    data.frameCount = frameCount;
    data.shadowRadius = shadowRadius;

    // Path tracer config
    data.fireflyClamp = fireflyClamp;
    data.enableNEE = enableNEE;
    data.enableMIS = enableMIS;
    data.rrMinBounce = rrMinBounce;
    data.rrMinProb = rrMinProb;

    // NEE light counts
    data.directionalLightCount = dirLightCount;
    data.pointLightCount = ptLightCount;
    data.spotLightCount = sptLightCount;

    // Convergence
    data.maxBounces = maxBounces;
    data.accumulatedSamples = accumulatedSamples;

    // SDF fallback settings (read by reflection shader at offsets 152-160)
    if (m_RTReflections && m_SDFScene && m_SDFScene->GetObjectCount() > 0) {
        data.sdfFallbackEnabled = m_RTReflections->GetConfig().sdfFallback ? 1.0f : 0.0f;
        data.sdfMaxDistance = m_RTReflections->GetConfig().sdfMaxDistance;
    } else {
        data.sdfFallbackEnabled = 0.0f;
        data.sdfMaxDistance = 0.0f;
    }

    std::memcpy(m_RTLightUBOMapped[frameIdx], &data, sizeof(data));

    // Update descriptor binding 13 to point to this frame's UBO
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_RTLightUBO[frameIdx];
    uboInfo.offset = 0;
    uboInfo.range = 256;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_RTDescriptorSet;
    write.dstBinding = 13;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &uboInfo;
    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &write, 0, nullptr);

    // Upload NEE light data to SSBO (binding 16) for path tracer direct light sampling
    if (m_RTNEELightMapped[frameIdx] && (dirLightCount > 0 || ptLightCount > 0 || sptLightCount > 0)) {
        // NEE light SSBO layout: packed RTLight structs (64 bytes each, matches rt_common.glsl)
        // RTLight { vec3 position, float range, vec3 direction, float intensity,
        //           vec3 color, int type, float innerCutoff, float outerCutoff, float _pad0, float _pad1 }
        struct RTLightGPU {
            f32 position[3]; f32 range;
            f32 direction[3]; f32 intensity;
            f32 color[3]; i32 type;
            f32 innerCutoff; f32 outerCutoff;
            f32 _pad0; f32 _pad1;
        };

        u8* dst = static_cast<u8*>(m_RTNEELightMapped[frameIdx]);
        u32 offset = 0;
        u32 maxLights = RT_NEE_LIGHT_BUFFER_SIZE / sizeof(RTLightGPU);
        u32 totalLights = 0;

        // Copy directional lights
        for (u32 i = 0; i < dirLightCount && totalLights < maxLights; ++i) {
            RTLightGPU light{};
            light.position[0] = 0.0f; light.position[1] = 0.0f; light.position[2] = 0.0f;
            light.range = 0.0f;  // Infinite for directional
            light.direction[0] = m_CachedLightingData.directionalLights[i].direction.x;
            light.direction[1] = m_CachedLightingData.directionalLights[i].direction.y;
            light.direction[2] = m_CachedLightingData.directionalLights[i].direction.z;
            light.intensity = m_CachedLightingData.directionalLights[i].intensity;
            light.color[0] = m_CachedLightingData.directionalLights[i].color.x;
            light.color[1] = m_CachedLightingData.directionalLights[i].color.y;
            light.color[2] = m_CachedLightingData.directionalLights[i].color.z;
            light.type = 0;
            light.innerCutoff = 0.0f;
            light.outerCutoff = 0.0f;
            std::memcpy(dst + offset, &light, sizeof(RTLightGPU));
            offset += sizeof(RTLightGPU);
            totalLights++;
        }

        // Copy point lights
        for (u32 i = 0; i < ptLightCount && totalLights < maxLights; ++i) {
            RTLightGPU light{};
            light.position[0] = m_CachedLightingData.pointLights[i].position.x;
            light.position[1] = m_CachedLightingData.pointLights[i].position.y;
            light.position[2] = m_CachedLightingData.pointLights[i].position.z;
            light.range = m_CachedLightingData.pointLights[i].range;
            light.direction[0] = 0.0f; light.direction[1] = 0.0f; light.direction[2] = 0.0f;
            light.intensity = m_CachedLightingData.pointLights[i].intensity;
            light.color[0] = m_CachedLightingData.pointLights[i].color.x;
            light.color[1] = m_CachedLightingData.pointLights[i].color.y;
            light.color[2] = m_CachedLightingData.pointLights[i].color.z;
            light.type = 1;
            light.innerCutoff = 0.0f;
            light.outerCutoff = 0.0f;
            std::memcpy(dst + offset, &light, sizeof(RTLightGPU));
            offset += sizeof(RTLightGPU);
            totalLights++;
        }

        // Copy spot lights
        for (u32 i = 0; i < sptLightCount && totalLights < maxLights; ++i) {
            RTLightGPU light{};
            light.position[0] = m_CachedLightingData.spotLights[i].position.x;
            light.position[1] = m_CachedLightingData.spotLights[i].position.y;
            light.position[2] = m_CachedLightingData.spotLights[i].position.z;
            light.range = m_CachedLightingData.spotLights[i].range;
            light.direction[0] = m_CachedLightingData.spotLights[i].direction.x;
            light.direction[1] = m_CachedLightingData.spotLights[i].direction.y;
            light.direction[2] = m_CachedLightingData.spotLights[i].direction.z;
            light.intensity = m_CachedLightingData.spotLights[i].intensity;
            light.color[0] = m_CachedLightingData.spotLights[i].color.x;
            light.color[1] = m_CachedLightingData.spotLights[i].color.y;
            light.color[2] = m_CachedLightingData.spotLights[i].color.z;
            light.type = 2;
            light.innerCutoff = m_CachedLightingData.spotLights[i].innerCutoff;
            light.outerCutoff = m_CachedLightingData.spotLights[i].outerCutoff;
            std::memcpy(dst + offset, &light, sizeof(RTLightGPU));
            offset += sizeof(RTLightGPU);
            totalLights++;
        }

        // Update descriptor binding 16 to point to this frame's NEE light SSBO
        VkDescriptorBufferInfo neeBufInfo{};
        neeBufInfo.buffer = m_RTNEELightBuffer[frameIdx];
        neeBufInfo.offset = 0;
        neeBufInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet neeWrite{};
        neeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        neeWrite.dstSet = m_RTDescriptorSet;
        neeWrite.dstBinding = 16;
        neeWrite.descriptorCount = 1;
        neeWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        neeWrite.pBufferInfo = &neeBufInfo;
        vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &neeWrite, 0, nullptr);
    }
}

// ============================================================================
// TEMPORAL UPSCALING (FSR 2 / DLSS / XeSS)
// ============================================================================

void RenderSystem::SetUpscalerType(u32 type) {
    if (type == m_UpscalerType) return;

    // Tear down existing upscaler
    if (m_Upscaler) {
        m_Upscaler->Shutdown();
        m_Upscaler.reset();
    }

    m_UpscalerType = type;

    if (type == 0) {
        ENJIN_LOG_INFO(Renderer, "Temporal upscaler disabled");
        return;
    }

    // Create the requested backend
    auto* ctx = m_Renderer->GetContext();
    switch (type) {
        case 1: // FSR 2
            m_Upscaler = std::make_unique<Renderer::FSR2Upscaler>(ctx);
            break;
        case 2: // DLSS
            m_Upscaler = std::make_unique<Renderer::DLSSUpscaler>(ctx);
            break;
        case 3: // XeSS
            m_Upscaler = std::make_unique<Renderer::XeSSUpscaler>(ctx);
            break;
        default:
            m_UpscalerType = 0;
            return;
    }

    if (!m_Upscaler->IsAvailable()) {
        ENJIN_LOG_WARN(Renderer, "%s upscaler SDK not compiled in", m_Upscaler->GetName());
        m_Upscaler.reset();
        m_UpscalerType = 0;
        return;
    }

    // Initialize with current display resolution
    VkExtent2D extent = m_Renderer->GetSwapchainExtent();
    u32 renderW, renderH;
    Renderer::IUpscaler::GetRenderResolution(
        extent.width, extent.height,
        static_cast<Renderer::UpscalerQuality>(m_UpscalerQuality),
        renderW, renderH);

    if (!m_Upscaler->Initialize(renderW, renderH, extent.width, extent.height,
                                static_cast<Renderer::UpscalerQuality>(m_UpscalerQuality))) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize %s upscaler", m_Upscaler->GetName());
        m_Upscaler.reset();
        m_UpscalerType = 0;
        return;
    }

    ENJIN_LOG_INFO(Renderer, "%s upscaler active (%ux%u -> %ux%u)",
                   m_Upscaler->GetName(), renderW, renderH, extent.width, extent.height);
}

void RenderSystem::SetUpscalerQuality(u32 quality) {
    if (quality == m_UpscalerQuality) return;
    m_UpscalerQuality = quality;

    if (m_Upscaler && m_UpscalerType > 0) {
        VkExtent2D extent = m_Renderer->GetSwapchainExtent();
        u32 renderW, renderH;
        Renderer::IUpscaler::GetRenderResolution(
            extent.width, extent.height,
            static_cast<Renderer::UpscalerQuality>(quality),
            renderW, renderH);
        m_Upscaler->Resize(renderW, renderH, extent.width, extent.height);
        ENJIN_LOG_INFO(Renderer, "Upscaler quality changed: %ux%u -> %ux%u",
                       renderW, renderH, extent.width, extent.height);
    }
}

} // namespace ECS
} // namespace Enjin
