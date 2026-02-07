#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/Animation/IKSolver.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/MeshFactory.h"
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

    // Create pipeline
    CreatePipeline();

    // Create line pipeline for editor grid rendering
    CreateLinePipeline();

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

    // Create default white texture (used when no texture is bound)
    m_DefaultWhiteTexture = std::make_unique<Renderer::Texture>(m_Renderer->GetContext());
    if (!m_DefaultWhiteTexture->CreateSolidColor(255, 255, 255, 255)) {
        ENJIN_LOG_WARN(Renderer, "Failed to create default white texture");
        m_DefaultWhiteTexture.reset();
    }

    // Create default bone buffer (single identity matrix for static meshes)
    m_DefaultBoneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (m_DefaultBoneBuffer->Create(sizeof(Math::Matrix4), Renderer::BufferUsage::Storage, true)) {
        Math::Matrix4 identity = Math::Matrix4::Identity();
        m_DefaultBoneBuffer->UploadData(&identity, sizeof(Math::Matrix4));
    } else {
        ENJIN_LOG_WARN(Renderer, "Failed to create default bone buffer");
        m_DefaultBoneBuffer.reset();
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

    // Initialize skybox
    m_Skybox.Initialize(m_Renderer->GetContext());
    CreateSkyboxCubeVBO();
    CreateSkyboxPipeline();

    // Set up shader hot-reload (editor-only)
    FindShaderDirectory();
    if (!m_ShaderDir.empty() && m_ShaderHotReloadEnabled) {
        SetupShaderWatchers();
    }

    // Initialize GPU frustum culling system
    m_GPUCulling = std::make_unique<Renderer::GPUCullingSystem>(m_Renderer->GetContext());
    if (!m_GPUCulling->Initialize()) {
        ENJIN_LOG_WARN(Renderer, "GPUCullingSystem initialization failed, using CPU culling");
        m_GPUCulling.reset();
        m_GPUCullingEnabled = false;
    } else {
        ENJIN_LOG_INFO(Renderer, "GPU frustum culling enabled");
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RenderSystem initialized");
}

void RenderSystem::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    // Wait for GPU to finish
    if (m_Renderer && m_Renderer->GetContext()) {
        vkDeviceWaitIdle(m_Renderer->GetContext()->GetDevice());
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

    // Clean up weather, particle, grass, shrub, tree, and sprite batch renderers
    m_WeatherRenderer.reset();
    m_ParticleRenderer.reset();
    m_GrassRenderer.reset();
    m_ShrubRenderer.reset();
    m_TreeRenderer.reset();
    m_SpriteBatchRenderer.reset();

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

    // Clean up line pipeline
    m_LinePipeline.reset();

    // Clean up shadow resources
    m_ShadowPipeline.reset();
    m_ShadowMap.reset();

    // Clean up text texture cache and rasterizer
    m_TextTextureCache.clear();
    m_TextRasterizer.ClearFontCache();

    // Clean up textures and bone buffer
    m_DefaultWhiteTexture.reset();
    m_DefaultBoneBuffer.reset();

    // Clean up pipeline
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
            m_VertexShader = std::move(m_PendingVertexShader);
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
}

void RenderSystem::Update(f32 deltaTime) {
    if (!m_Renderer || !m_Initialized) {
        return;
    }

    // Process any pending changes not yet flushed (fallback if FlushPendingChanges
    // wasn't called earlier this frame, e.g. in standalone Player without editor)
    FlushPendingChanges();

    // Reset per-frame stats
    ResetFrameCounters();

    // Poll texture and shader file watchers every 30 frames (~0.5s at 60fps)
    if (++m_WatcherPollCounter >= 30) {
        m_WatcherPollCounter = 0;
        m_TextureWatcher.Poll();
        if (m_ShaderHotReloadEnabled && !m_ShaderDir.empty()) {
            m_ShaderWatcher.Poll();
        }
    }

    // Auto-create meshes for water volume entities that don't have one yet
    EnsureWaterMeshes();

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
                m_EntityRenderData.erase(entity);
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
                m_EntityRenderData.erase(entity);
                terrain2d->meshDirty = false;
            }
        }
        // JellyMesh dirty check (flower system vertex deformation)
        // Re-upload vertex data to existing buffer instead of erase/recreate,
        // because destroying buffers while the GPU is still reading them crashes the driver.
        for (Entity entity : m_World->GetEntitiesWithComponent<JellyMeshComponent>()) {
            auto* jelly = m_World->GetComponent<JellyMeshComponent>(entity);
            if (jelly && jelly->meshDirty) {
                auto it = m_EntityRenderData.find(entity);
                if (it != m_EntityRenderData.end() && it->second.vertexBuffer) {
                    auto* mesh = m_World->GetComponent<MeshComponent>(entity);
                    if (mesh && !mesh->vertices.empty()) {
                        usize dataSize = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
                        it->second.vertexBuffer->UploadData(mesh->vertices.data(), dataSize);
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
            m_EntityRenderData.erase(entity);
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
            m_EntityRenderData.erase(entity);
            tilemap->meshDirty = false;
        }
    }

    // Update skeletal animators (only iterate entities that have AnimatorComponent)
    for (Entity entity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
        AnimatorComponent* animComp = m_World->GetComponent<AnimatorComponent>(entity);
        if (animComp) {
            animComp->Update(deltaTime);
        }
    }

    // Apply IK constraints after animation update (modifies bone transforms before GPU upload)
    // Only iterate entities with AnimatorComponent (same set as above, typically very few)
    for (Entity entity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
        auto* animComp = m_World->GetComponent<AnimatorComponent>(entity);
        if (!animComp || !animComp->animator.IsPlaying()) continue;

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
                    std::vector<Math::Vector3> chain = {
                        entityTransform->position + Math::Vector3(0.2f, 1.3f, 0.0f), // shoulder
                        entityTransform->position + Math::Vector3(0.3f, 1.1f, 0.3f), // elbow
                        handPos                                                        // hand
                    };
                    Animation::FABRIK::Solve(chain, nearestTarget, 5);
                }
            }
        }
    }

    // Classify scene composition (2D / 2.5D / 3D) before rendering decisions
    ClassifySceneComposition();

    // Build list of cullable objects for GPU frustum culling
    // Only done when we have 3D meshes and GPU culling is enabled
    if (m_GPUCullingEnabled && m_SceneComposition.mesh3DCount > 0) {
        BuildCullableObjectList();
    }

    // Shadow pass first (if enabled) - only run when 3D meshes AND shadow-casting lights exist.
    // Pure 2D scenes skip entirely since sprites never cast shadows.
    // Scenes with no shadow-casting lights also skip to avoid rendering 4 cascades for nothing.
    if (m_ShadowsEnabled && m_ShadowMap && m_ShadowPipeline &&
        m_SceneComposition.mode == SceneRenderMode::Scene3D &&
        m_SceneComposition.hasShadowCastingLights) {
        RenderShadowPass();
    }

    // GPU frustum culling (compute shader dispatch before main render pass)
    // This runs the compute shader to determine which objects are visible
    if (m_GPUCullingEnabled && !m_CullableObjects.empty()) {
        PerformGPUCulling();
    }

    // Periodic diagnostic warnings (every 300 frames)
    if (++m_DiagnosticFrameCounter >= 300) {
        m_DiagnosticFrameCounter = 0;
        if (m_SceneComposition.spriteCount > 100 && !m_SpriteBatchRenderer) {
            ENJIN_LOG_WARN(Renderer, "Scene has %u sprites without batch renderer — high draw call count",
                           m_SceneComposition.spriteCount);
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
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 0, 1,
                &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 0, nullptr);
            vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
                if (!m_World->HasComponent<TransformComponent>(entity)) continue;
                auto* xform = m_World->GetComponent<TransformComponent>(entity);
                if (xform && !xform->visible) continue;
                // Skip GPU-culled entities (frustum culling)
                if (m_GPUCullingEnabled && m_GPUCulling && !m_CullableObjects.empty()) {
                    usize entityIdx = static_cast<usize>(entity);
                    if (entityIdx < m_EntityToCullIndex.size()) {
                        u32 cullIdx = m_EntityToCullIndex[entityIdx];
                        if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                            continue;
                        }
                    }
                }
                // Skip 2D sprites — rendered in sorted pass after 3D geometry
                if (m_World->HasComponent<Sprite2DComponent>(entity)) continue;
                RenderEntity(entity);
            }

            // Sorted 2D sprite rendering pass (after 3D geometry)
            RenderSprites();

            // Render effects for this viewport
            u32 vpW = static_cast<u32>(pixelW);
            u32 vpH = static_cast<u32>(pixelH);
            RenderGrass(vpW, vpH);
            RenderShrubs(vpW, vpH);
            RenderTrees(vpW, vpH);
            RenderParticles(vpW, vpH);
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

    // Render skybox first (behind all geometry)
    RenderSkybox(commandBuffer);

    // Upload frame-level uniforms once (view/proj + lighting)
    UpdateFrameUniforms();

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Bind pipeline, descriptor set, viewport, and scissor once for all entities
    m_Pipeline->Bind(commandBuffer);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 0, nullptr);

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
    {
        Math::Vector3 camPos;
        bool doLOD = (m_Camera != nullptr);
        if (doLOD) camPos = m_Camera->GetPosition();

        for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
            if (!m_World->HasComponent<TransformComponent>(entity)) continue;

            // Skip invisible entities
            {
                auto* xform = m_World->GetComponent<TransformComponent>(entity);
                if (xform && !xform->visible) continue;
            }

            // Skip GPU-culled entities (frustum culling)
            if (m_GPUCullingEnabled && m_GPUCulling && !m_CullableObjects.empty()) {
                usize entityIdx = static_cast<usize>(entity);
                if (entityIdx < m_EntityToCullIndex.size()) {
                    u32 cullIdx = m_EntityToCullIndex[entityIdx];
                    if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                        continue;
                    }
                }
            }

            // Skip 2D sprites — rendered in sorted pass after 3D geometry
            if (m_World->HasComponent<Sprite2DComponent>(entity)) continue;

            // LOD selection (if camera is available)
            if (doLOD) {
                auto* lod = m_World->GetComponent<LODComponent>(entity);
                if (lod && lod->enabled && lod->levelCount > 1) {
                    auto* transform = m_World->GetComponent<TransformComponent>(entity);
                    if (transform) {
                        f32 dist = (transform->position - camPos).Length();
                        i32 newLOD = 0;
                        for (i32 l = 0; l < lod->levelCount - 1; ++l) {
                            if (dist > lod->levels[l].maxDistance) {
                                newLOD = l + 1;
                            }
                        }
                        if (newLOD != lod->activeLOD && newLOD < lod->levelCount) {
                            auto* mesh = m_World->GetComponent<MeshComponent>(entity);
                            if (mesh && lod->levels[newLOD].mesh.IsValid()) {
                                *mesh = lod->levels[newLOD].mesh;
                                m_EntityRenderData.erase(entity);
                                lod->activeLOD = newLOD;
                            }
                        }
                    }
                }
            }

            RenderEntity(entity);
        }
    }

    // Sorted 2D sprite rendering pass (after 3D geometry)
    RenderSprites();

    // Render effect passes (grass, shrubs, trees, particles)
    RenderGrass(0, 0);
    RenderShrubs(0, 0);
    RenderTrees(0, 0);
    RenderParticles(0, 0);

    // Render weather particles in main pass if set (editor viewport)
    if (m_MainPassWeather) {
        RenderWeatherParticles(*m_MainPassWeather, m_MainPassWeatherIsRain, 0, 0);
    }
}

void RenderSystem::RenderToTarget(Renderer::RenderTarget* target, Renderer::Camera* camera) {
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
    m_CurrentViewportIndex = 0;

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

    // Upload frame-level uniforms to offscreen buffers (game camera view/proj + lighting)
    UpdateFrameUniforms();

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

    // Render all entities with mesh and transform (skip sprites — drawn in sorted pass)
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        if (m_World->HasComponent<TransformComponent>(entity)) {

            // Skip invisible entities
            auto* xformRT = m_World->GetComponent<TransformComponent>(entity);
            if (xformRT && !xformRT->visible) continue;

            // Skip GPU-culled entities (frustum culling)
            if (m_GPUCullingEnabled && m_GPUCulling && !m_CullableObjects.empty()) {
                usize entityIdx = static_cast<usize>(entity);
                if (entityIdx < m_EntityToCullIndex.size()) {
                    u32 cullIdx = m_EntityToCullIndex[entityIdx];
                    if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                        continue;
                    }
                }
            }

            // Skip 2D sprites — rendered in sorted pass after 3D geometry
            if (m_World->HasComponent<Sprite2DComponent>(entity)) continue;

            auto it = m_EntityRenderData.find(entity);
            if (it == m_EntityRenderData.end()) {
                it = SetupEntityBuffers(entity);
                if (it == m_EntityRenderData.end()) continue;
            }
            EntityRenderData& renderData = it->second;

            // Update per-entity material UBO
            UpdateMaterialBuffer(entity);

            // Bind pipeline and descriptor set (offscreen set with game camera UBOs)
            m_Pipeline->Bind(commandBuffer);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 0, 1, &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 0, nullptr);

            // Re-set viewport/scissor (pipeline bind may reset dynamic state)
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Push constants
            TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
            Renderer::PushConstants pushConstants{};
            pushConstants.model = transform->ToMatrix();

            MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
            Renderer::Texture* boundTexture = nullptr;
            Renderer::Texture* texHeight = nullptr;
            Renderer::Texture* texNormal = nullptr;
            Renderer::Texture* texMR = nullptr;
            Renderer::Texture* texEmissive = nullptr;

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
                    if (!material->emissiveTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedEmissiveTexture = tex.get();
                            material->emissiveTexture = 1;
                        }
                    }
                    material->textureCacheDirty = false;
                }

                // Use cached texture pointers
                boundTexture = material->cachedBaseColorTexture;
                texHeight = material->cachedHeightTexture;
                texNormal = material->cachedNormalTexture;
                texMR = material->cachedMetallicRoughnessTexture;
                texEmissive = material->cachedEmissiveTexture;

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
                pushConstants.flags |= (static_cast<i32>(material->vertexSnapResolution) << 24);
                pushConstants.parallaxScale = material->parallaxScale;
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
                pushConstants.flags = (pushConstants.flags & 0x00FFFFFF) | (static_cast<i32>(m_GlobalVertexSnapResolution) << 24);
            }

            // Set wind sway flag for vegetation entities
            VegetationComponent* vegComp = m_World->GetComponent<VegetationComponent>(entity);
            if (vegComp) {
                pushConstants.flags |= (1 << 4); // FLAG_WIND_SWAY
            }

            // Set water surface flag for water volume entities
            WaterVolumeComponent* waterVol = m_World->GetComponent<WaterVolumeComponent>(entity);
            if (waterVol) {
                pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE — always set, shader handles freeze
                pushConstants.parallaxScale = waterVol->freezeProgress; // repurpose for water (POM skips water)

                if (m_RainActive && waterVol->freezeProgress < 0.5f) {
                    pushConstants.flags |= (1 << 6); // FLAG_RAIN_RIPPLES (no ripples on ice)
                }
                if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
                    pushConstants.flags |= (1 << 7); // FLAG_WATER_SHORE
                    pushConstants.shoreWidth = waterVol->shoreWidth;
                    pushConstants.foamIntensity = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
                    pushConstants.foamScale = waterVol->foamScale;
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
            }

            // Rasterize text texture if entity has a TextComponent
            TextComponent* textComp = m_World->GetComponent<TextComponent>(entity);
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

            // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 5)
            UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive);

            // Upload bone matrices for skinned meshes
            AnimatorComponent* animComp = m_World->GetComponent<AnimatorComponent>(entity);
            if (animComp && renderData.boneBuffer && animComp->animator.IsPlaying()) {
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

            vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                sizeof(Renderer::PushConstants), &pushConstants);

            VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
            m_DrawCallCount++;
            m_TriangleCount += renderData.indexCount / 3;
        }
    }

    // Sorted 2D sprite rendering pass (after 3D geometry)
    RenderSprites();

    // Render effect passes (grass, shrubs, trees, particles)
    // Pass render target dimensions so vegetation renderers use the correct viewport
    u32 targetW = target->GetWidth();
    u32 targetH = target->GetHeight();
    RenderGrass(targetW, targetH);
    RenderShrubs(targetW, targetH);
    RenderTrees(targetW, targetH);
    RenderParticles(targetW, targetH);

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

        // Render all entities (skip sprites — drawn in sorted pass)
        for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
            if (!m_World->HasComponent<TransformComponent>(entity)) continue;

            // Skip invisible entities
            {
                auto* xformSS = m_World->GetComponent<TransformComponent>(entity);
                if (xformSS && !xformSS->visible) continue;
            }

            // Skip 2D sprites — rendered in sorted pass after 3D geometry
            if (m_World->HasComponent<Sprite2DComponent>(entity)) continue;

            auto it = m_EntityRenderData.find(entity);
            if (it == m_EntityRenderData.end()) {
                it = SetupEntityBuffers(entity);
                if (it == m_EntityRenderData.end()) continue;
            }
            EntityRenderData& renderData = it->second;

            UpdateMaterialBuffer(entity);

            m_Pipeline->Bind(commandBuffer);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 0, 1,
                &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 0, nullptr);

            // Restore viewport/scissor after pipeline bind
            vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Build push constants (same logic as RenderToTarget)
            TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
            Renderer::PushConstants pushConstants{};
            pushConstants.model = transform->ToMatrix();

            MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
            Renderer::Texture* boundTexture = nullptr;
            Renderer::Texture* texHeight = nullptr;
            Renderer::Texture* texNormal = nullptr;
            Renderer::Texture* texMR = nullptr;
            Renderer::Texture* texEmissive = nullptr;

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
                    if (!material->emissiveTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedEmissiveTexture = tex.get();
                            material->emissiveTexture = 1;
                        }
                    }
                    material->textureCacheDirty = false;
                }

                // Use cached texture pointers
                boundTexture = material->cachedBaseColorTexture;
                texHeight = material->cachedHeightTexture;
                texNormal = material->cachedNormalTexture;
                texMR = material->cachedMetallicRoughnessTexture;
                texEmissive = material->cachedEmissiveTexture;

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
                pushConstants.flags |= (static_cast<i32>(material->vertexSnapResolution) << 24);
                pushConstants.parallaxScale = material->parallaxScale;
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
                pushConstants.flags = (pushConstants.flags & 0x00FFFFFF) | (static_cast<i32>(m_GlobalVertexSnapResolution) << 24);
            }

            VegetationComponent* vegComp = m_World->GetComponent<VegetationComponent>(entity);
            if (vegComp) {
                pushConstants.flags |= (1 << 4);
            }

            WaterVolumeComponent* waterVol = m_World->GetComponent<WaterVolumeComponent>(entity);
            if (waterVol) {
                pushConstants.flags |= (1 << 5);
                pushConstants.parallaxScale = waterVol->freezeProgress;
                if (m_RainActive && waterVol->freezeProgress < 0.5f) {
                    pushConstants.flags |= (1 << 6);
                }
                if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
                    pushConstants.flags |= (1 << 7);
                    pushConstants.shoreWidth = waterVol->shoreWidth;
                    pushConstants.foamIntensity = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
                    pushConstants.foamScale = waterVol->foamScale;
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
            }

            // Text rendering
            TextComponent* textComp = m_World->GetComponent<TextComponent>(entity);
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

            // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 5)
            UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive);

            // Upload bone matrices for skinned meshes
            AnimatorComponent* animComp = m_World->GetComponent<AnimatorComponent>(entity);
            if (animComp && renderData.boneBuffer && animComp->animator.IsPlaying()) {
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

            vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                sizeof(Renderer::PushConstants), &pushConstants);

            VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
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

    // Invalidate shadow caster cache (new entity may be a shadow caster)
    m_ShadowCastersDirty = true;

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
    m_EntityRenderData.erase(entity);
    m_TextTextureCache.erase(entity);

    // Invalidate scene composition cache (removed entity may change 2D/3D classification)
    m_SceneComposition.dirty = true;

    // Invalidate shadow caster cache (removed entity may have been a shadow caster)
    m_ShadowCastersDirty = true;

    // Invalidate cached player entity and search for a replacement
    if (entity == m_CachedPlayerEntity) {
        m_CachedPlayerEntity = INVALID_ENTITY;
        if (m_World) {
            for (Entity e : m_World->GetAllEntities()) {
                if (e == entity) continue;
                bool hasController = m_World->HasComponent<ThirdPersonController>(e) ||
                                     m_World->HasComponent<FirstPersonController>(e) ||
                                     m_World->HasComponent<Platformer2DController>(e) ||
                                     m_World->HasComponent<TopDown2DController>(e) ||
                                     m_World->HasComponent<TopDown3DController>(e);
                if (hasController) {
                    m_CachedPlayerEntity = e;
                    break;
                }
            }
        }
    }
}

void RenderSystem::ClassifySceneComposition() {
    if (!m_SceneComposition.dirty || !m_World) return;

    m_SceneComposition.spriteCount = 0;
    m_SceneComposition.tilemapCount = 0;
    m_SceneComposition.mesh3DCount = 0;
    m_SceneComposition.hasShadowCastingLights = false;

    // Count sprites
    for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
        (void)entity;
        m_SceneComposition.spriteCount++;
    }

    // Count tilemaps
    for (Entity entity : m_World->GetEntitiesWithComponent<TilemapComponent>()) {
        (void)entity;
        m_SceneComposition.tilemapCount++;
    }

    // Count 3D meshes (MeshComponent WITHOUT Sprite2DComponent and WITHOUT TilemapComponent)
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        if (m_World->HasComponent<Sprite2DComponent>(entity)) continue;
        if (m_World->HasComponent<TilemapComponent>(entity)) continue;
        m_SceneComposition.mesh3DCount++;
    }

    // Check for shadow-casting directional lights
    for (Entity entity : m_World->GetEntitiesWithComponent<LightComponent>()) {
        auto* light = m_World->GetComponent<LightComponent>(entity);
        if (light && light->type == LightType::Directional && light->castShadows) {
            m_SceneComposition.hasShadowCastingLights = true;
            break;
        }
    }

    // Classify scene mode
    if (m_SceneComposition.mesh3DCount > 0) {
        m_SceneComposition.mode = SceneRenderMode::Scene3D;
    } else if (m_SceneComposition.hasShadowCastingLights) {
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

    // Iterate only entities with MeshComponent
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        // Skip entities without transform
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;

        // Skip 2D sprites — they never cast shadows
        if (m_World->HasComponent<Sprite2DComponent>(entity)) continue;

        // Skip tilemaps
        if (m_World->HasComponent<TilemapComponent>(entity)) continue;

        // Skip invisible entities
        auto* xform = m_World->GetComponent<TransformComponent>(entity);
        if (xform && !xform->visible) continue;

        // Check if material casts shadows (default: yes)
        auto* material = m_World->GetComponent<MaterialComponent>(entity);
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

    u32 cullIndex = 0;

    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        if (m_World->HasComponent<Sprite2DComponent>(entity)) continue;
        if (m_World->HasComponent<TilemapComponent>(entity)) continue;

        auto* xform = m_World->GetComponent<TransformComponent>(entity);
        if (!xform || !xform->visible) continue;

        auto* mesh = m_World->GetComponent<MeshComponent>(entity);
        if (!mesh || !mesh->IsValid()) continue;

        // Compute AABB from mesh vertices
        Renderer::BoundingBox bounds;
        for (const auto& vertex : mesh->vertices) {
            bounds.min.x = Math::Min(bounds.min.x, vertex.position.x);
            bounds.min.y = Math::Min(bounds.min.y, vertex.position.y);
            bounds.min.z = Math::Min(bounds.min.z, vertex.position.z);
            bounds.max.x = Math::Max(bounds.max.x, vertex.position.x);
            bounds.max.y = Math::Max(bounds.max.y, vertex.position.y);
            bounds.max.z = Math::Max(bounds.max.z, vertex.position.z);
        }

        // Handle empty mesh
        if (bounds.min.x > bounds.max.x) {
            bounds.min = Math::Vector3(-0.5f);
            bounds.max = Math::Vector3(0.5f);
        }

        Renderer::CullableObject obj;
        obj.SetBounds(bounds);
        obj.transform = xform->ToMatrix();
        obj.meshIndex = static_cast<u32>(entity); // Use entity ID as mesh index for now
        obj.indexCount = static_cast<u32>(mesh->indices.size());
        obj.indexOffset = 0;
        obj.vertexOffset = 0;

        // Map entity to cull index
        if (static_cast<usize>(entity) < m_EntityToCullIndex.size()) {
            m_EntityToCullIndex[static_cast<usize>(entity)] = cullIndex;
        }

        m_CullableObjects.push_back(obj);
        cullIndex++;
    }
}

void RenderSystem::PerformGPUCulling() {
    if (!m_GPUCulling || !m_GPUCullingEnabled || !m_Camera) return;
    if (m_CullableObjects.empty()) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Submit objects for culling
    m_GPUCulling->SubmitObjects(m_CullableObjects);

    // Execute GPU culling
    VkBuffer indirectBuffer;
    u32 drawCount;
    if (m_GPUCulling->ExecuteCulling(
            m_Camera->GetViewMatrix(),
            m_Camera->GetProjectionMatrix(),
            commandBuffer,
            indirectBuffer,
            drawCount)) {
        // Culling stats are available via m_GPUCulling->GetStats()
        auto stats = m_GPUCulling->GetStats();
        (void)stats; // Stats available for profiler display
    }
}

void RenderSystem::CreatePipeline() {
    Renderer::PipelineConfig config;
    config.renderPass = m_Renderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = m_BackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = m_WireframeMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create graphics pipeline");
        m_Pipeline.reset();
    }
}

void RenderSystem::CreateShadowPipeline() {
    if (!m_ShadowMap || !m_Pipeline) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_ShadowMap->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_FRONT_BIT;  // Front-face culling reduces shadow acne
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.depthBiasEnable = true;
    config.depthBiasConstant = 1.25f;
    config.depthBiasSlope = 1.75f;
    config.hasColorAttachment = false;  // Depth-only pass

    m_ShadowPipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    // Share descriptor set layout with main pipeline so we can use the same descriptor sets
    if (!m_ShadowPipeline->CreateWithLayout(config, m_VertexShader.get(), nullptr,
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

    m_LinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_LinePipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(),
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create line pipeline");
        m_LinePipeline.reset();
    }
}

void RenderSystem::RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                                    u32 firstVertex, const Math::Vector3& color, f32 opacity) {
    if (!m_LinePipeline || !m_Renderer || !vertexBuffer || vertexCount == 0) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    m_LinePipeline->Bind(commandBuffer);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_LinePipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 0, nullptr);

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

        // Material uniform buffer
        m_MaterialBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!m_MaterialBuffers[i]->Create(sizeof(MaterialGPU), Renderer::BufferUsage::Uniform, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create material buffer %u", i);
            return;
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

void RenderSystem::CreateDescriptorSets() {
    constexpr u32 framesInFlight = 2;
    const u32 offscreenSets = framesInFlight * MAX_SPLITSCREEN_VIEWPORTS;
    const u32 totalSets = framesInFlight + offscreenSets; // main + splitscreen offscreen

    // Create descriptor pool (3 UBOs + 6 combined image samplers + 1 SSBO per set)
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = totalSets * 3;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = totalSets * 6;  // base color + shadow + height + normal + metallic-roughness + emissive
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = totalSets * 1;  // bone matrices SSBO

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

        // Material UBO
        bufferInfos[2].buffer = m_MaterialBuffers[i]->GetBuffer();
        bufferInfos[2].offset = 0;
        bufferInfos[2].range = sizeof(MaterialGPU);

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

        std::array<VkWriteDescriptorSet, 10> descriptorWrites{};

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

        // Material descriptor
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_DescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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
                // Share the material buffer — it's written per-draw-call anyway
                offBufInfos[2].buffer = m_MaterialBuffers[i]->GetBuffer();
                offBufInfos[2].offset = 0;
                offBufInfos[2].range = sizeof(MaterialGPU);

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

                std::array<VkWriteDescriptorSet, 10> offWrites{};
                for (u32 w = 0; w < 10; ++w) {
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
                offWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
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

                vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(),
                    static_cast<u32>(offWrites.size()), offWrites.data(), 0, nullptr);
            }
        }
    }
}

std::unordered_map<Entity, EntityRenderData>::iterator RenderSystem::SetupEntityBuffers(Entity entity) {
    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);
    if (!mesh || !mesh->IsValid()) {
        return m_EntityRenderData.end();
    }

    auto [insertIt, _] = m_EntityRenderData.try_emplace(entity);
    EntityRenderData& renderData = insertIt->second;

    // Create vertex buffer
    usize vertexBufferSize = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
    renderData.vertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!renderData.vertexBuffer->Create(vertexBufferSize, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create vertex buffer for entity %llu", entity);
        m_EntityRenderData.erase(insertIt);
        return m_EntityRenderData.end();
    }

    if (!renderData.vertexBuffer->UploadData(mesh->vertices.data(), vertexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload vertex data for entity %llu", entity);
        m_EntityRenderData.erase(insertIt);
        return m_EntityRenderData.end();
    }

    // Create index buffer
    usize indexBufferSize = mesh->indices.size() * sizeof(u32);
    renderData.indexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!renderData.indexBuffer->Create(indexBufferSize, Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create index buffer for entity %llu", entity);
        m_EntityRenderData.erase(insertIt);
        return m_EntityRenderData.end();
    }

    if (!renderData.indexBuffer->UploadData(mesh->indices.data(), indexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload index data for entity %llu", entity);
        m_EntityRenderData.erase(insertIt);
        return m_EntityRenderData.end();
    }

    renderData.indexCount = static_cast<u32>(mesh->indices.size());

    // Create bone SSBO if entity has a skeleton for animation
    AnimatorComponent* animComp = m_World->GetComponent<AnimatorComponent>(entity);
    if (animComp && animComp->animator.GetSkeleton()) {
        usize boneCount = animComp->animator.GetSkeleton()->bones.size();
        if (boneCount > 0) {
            usize boneBufferSize = boneCount * sizeof(Math::Matrix4);
            renderData.boneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
            if (!renderData.boneBuffer->Create(boneBufferSize, Renderer::BufferUsage::Storage, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create bone buffer for entity %llu", entity);
                renderData.boneBuffer.reset();
            }
        }
    }

    return insertIt;
}

void RenderSystem::UpdateFrameUniforms() {
    if (!m_Camera) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Update View/Projection UBO (shared across all objects)
    Renderer::UniformBufferObject ubo{};
    ubo.view = m_Camera->GetViewMatrix();
    ubo.proj = m_Camera->GetProjectionMatrix();
    (*m_ActiveUniformBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&ubo, sizeof(ubo));

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

    bool hasAnyLight = false;

    for (Entity lightEntity : m_World->GetEntitiesWithComponent<LightComponent>()) {
        LightComponent* light = m_World->GetComponent<LightComponent>(lightEntity);
        TransformComponent* lightTransform = m_World->GetComponent<TransformComponent>(lightEntity);
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
        lighting.shadowBias = m_ShadowMap->GetDepthBias();
        lighting.shadowEnabled = 1;
        lighting.shadowStrength = m_ShadowMap->GetShadowStrength();
        lighting.shadowMaxDistance = m_ShadowDistance;
    } else {
        for (u32 i = 0; i < Renderer::MAX_SHADOW_CASCADES; ++i) {
            lighting.cascadeViewProj[i] = Math::Matrix4::Identity();
        }
        lighting.cascadeSplits = Math::Vector4(25.0f, 50.0f, 75.0f, 100.0f);
        lighting.shadowBias = 0.005f;
        lighting.shadowEnabled = 0;
        lighting.shadowStrength = 1.0f;
        lighting.shadowMaxDistance = 100.0f;
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
        lighting.skyReflectColor = Math::Vector4(skyCol.x, skyCol.y, skyCol.z, 0.0f);
    }

    (*m_ActiveLightingBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&lighting, sizeof(lighting));
}

void RenderSystem::UpdateMaterialBuffer(Entity entity) {
    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    MaterialGPU materialGPU;
    MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
    if (material) {
        materialGPU = MaterialGPU::FromComponent(*material);
    } else {
        MaterialComponent defaultMat;
        defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        defaultMat.metallic = 0.0f;
        defaultMat.roughness = 0.5f;
        materialGPU = MaterialGPU::FromComponent(defaultMat);
    }
    m_MaterialBuffers[currentFrame]->UploadData(&materialGPU, sizeof(materialGPU));
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

u32 RenderSystem::GetShadowResolution() const {
    return m_ShadowMap ? m_ShadowMap->GetResolution() : 2048;
}

void RenderSystem::SetShadowResolution(u32 r) {
    if (!m_ShadowMap) return;
    // Skip pipeline recreation if resolution hasn't actually changed
    u32 current = m_ShadowMap->GetResolution();
    if (r < 512) r = 512;
    if (r > 4096) r = 4096;
    if (r == current) return;
    m_ShadowMap->SetResolution(r);
    // Recreate descriptor sets to pick up new image views
    m_PendingRecreation = PendingRecreationType::PipelineOnly;
}

void RenderSystem::RecreatePipelines(bool gpuAlreadyIdle) {
    if (!m_Pipeline || !m_Initialized) return;

    // Wait for GPU to finish all in-flight work before destroying pipelines
    // Skip if caller guarantees GPU is already idle (e.g., deferred recreation already waited)
    if (!gpuAlreadyIdle && m_Renderer) {
        m_Renderer->WaitForAllFrames();
    }

    // Destroy all pipelines that share the descriptor set layout
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
        CreateShadowPipeline();
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

    // Particle + Weather (shared shaders)
    auto particleReload = [this, dir](const std::string&) {
        VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
        bool any = false;
        if (m_ParticleRenderer && m_ParticleRenderer->ReloadShaders(dir, l)) any = true;
        if (m_WeatherRenderer && m_WeatherRenderer->ReloadShaders(dir, l)) any = true;
        if (any) ENJIN_LOG_INFO(Renderer, "Shader hot-reload: particle/weather shaders reloaded");
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

    // Shadow pipeline reuses m_VertexShader — defer swap and recreation to next frame start
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

    TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);

    if (!transform || !mesh || !mesh->IsValid()) {
        return;
    }

    // Skip invisible entities (safety net — callers should also check)
    if (!transform->visible) return;

    auto it = m_EntityRenderData.find(entity);
    if (it == m_EntityRenderData.end()) {
        it = SetupEntityBuffers(entity);
        if (it == m_EntityRenderData.end()) {
            return;
        }
    }

    EntityRenderData& renderData = it->second;

    // Get command buffer
    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    // Update per-entity material UBO
    UpdateMaterialBuffer(entity);

    // Push model matrix and material for this entity
    Renderer::PushConstants pushConstants{};
    pushConstants.model = transform->ToMatrix();

    // Set material data
    MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
    Renderer::Texture* boundTexture = nullptr;

    // Cached texture pointers for batched descriptor update
    Renderer::Texture* texHeight = nullptr;
    Renderer::Texture* texNormal = nullptr;
    Renderer::Texture* texMR = nullptr;
    Renderer::Texture* texEmissive = nullptr;

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
            material->textureCacheDirty = false;
        }

        // Use cached texture pointers
        boundTexture = material->cachedBaseColorTexture;
        texHeight = material->cachedHeightTexture;
        texNormal = material->cachedNormalTexture;
        texMR = material->cachedMetallicRoughnessTexture;
        texEmissive = material->cachedEmissiveTexture;

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
        pushConstants.flags |= (static_cast<i32>(material->vertexSnapResolution) << 24);
        pushConstants.parallaxScale = material->parallaxScale;
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
        pushConstants.flags = (pushConstants.flags & 0x00FFFFFF) | (static_cast<i32>(m_GlobalVertexSnapResolution) << 24);
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
    WaterVolumeComponent* waterVol = m_World->GetComponent<WaterVolumeComponent>(entity);
    if (waterVol) {
        pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE — always set, shader handles freeze
        pushConstants.parallaxScale = waterVol->freezeProgress; // repurpose for water (POM skips water)

        if (m_RainActive && waterVol->freezeProgress < 0.5f) {
            pushConstants.flags |= (1 << 6); // FLAG_RAIN_RIPPLES (no ripples on ice)
        }
        if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
            pushConstants.flags |= (1 << 7); // FLAG_WATER_SHORE
            pushConstants.shoreWidth = waterVol->shoreWidth;
            pushConstants.foamIntensity = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
            pushConstants.foamScale = waterVol->foamScale;
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
    }

    // Rasterize text texture if entity has a TextComponent
    TextComponent* textComp = m_World->GetComponent<TextComponent>(entity);
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
    // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 5)
    UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive);

    // Upload bone matrices for skinned meshes
    AnimatorComponent* animComp = m_World->GetComponent<AnimatorComponent>(entity);
    if (animComp && renderData.boneBuffer && animComp->animator.IsPlaying()) {
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

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    // Draw indexed
    vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
    m_DrawCallCount++;
    m_TriangleCount += renderData.indexCount / 3;
}

void RenderSystem::RenderSprites() {
    if (!m_Pipeline || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Render tilemaps first (layer -1000, behind sprites) via the per-entity path
    // Tilemaps are complex meshes that don't benefit from instance batching
    {
        struct TilemapEntry {
            Entity entity;
        };
        std::vector<TilemapEntry> tilemaps;
        for (Entity entity : m_World->GetEntitiesWithComponent<TilemapComponent>()) {
            if (!m_World->HasComponent<TransformComponent>(entity)) continue;
            if (!m_World->HasComponent<MeshComponent>(entity)) continue;
            auto* xformTM = m_World->GetComponent<TransformComponent>(entity);
            if (xformTM && !xformTM->visible) continue;
            tilemaps.push_back({ entity });
        }
        for (const auto& entry : tilemaps) {
            RenderEntity(entry.entity);
        }
    }

    // Render sprites via batch renderer (instanced draw calls grouped by texture)
    if (m_SpriteBatchRenderer) {
        // Determine lit mode: Scene2D = unlit, Scene2_5D/Scene3D = lit (sprites respond to lights)
        bool litMode = (m_SceneComposition.mode != SceneRenderMode::Scene2D);

        auto textureBindCallback = [this](const std::string& texturePath) {
            if (!texturePath.empty()) {
                auto tex = GetOrLoadTexture(texturePath);
                if (tex && tex->IsValid()) {
                    UpdateTextureDescriptor(tex.get());
                }
            }
            // Bind default normal map (flat normal) for lit sprites at binding 6
            if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                UpdateNormalMapDescriptor(m_DefaultWhiteTexture.get());
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
        for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
            auto* sprite = m_World->GetComponent<Sprite2DComponent>(entity);
            if (!sprite || !sprite->visible) continue;
            if (!m_World->HasComponent<TransformComponent>(entity)) continue;
            auto* xformSprite = m_World->GetComponent<TransformComponent>(entity);
            if (xformSprite && !xformSprite->visible) continue;
            if (!m_World->HasComponent<MeshComponent>(entity)) continue;

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

void RenderSystem::RenderShadowPass() {
    if (!m_ShadowMap || !m_ShadowPipeline) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Find the first directional light for shadow casting
    bool foundShadowLight = false;

    Math::Vector3 shadowLightDir(0.5f, 0.8f, 0.3f);

    for (Entity lightEntity : m_World->GetEntitiesWithComponent<LightComponent>()) {
        LightComponent* light = m_World->GetComponent<LightComponent>(lightEntity);
        if (!light || light->type != LightType::Directional || !light->castShadows) continue;

        TransformComponent* lightTransform = m_World->GetComponent<TransformComponent>(lightEntity);
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
        // Upload the cascade's view-projection matrix to the UBO as view=Identity, proj=cascadeVP
        // so the shadow vertex shader (which computes proj * view * model * pos) uses the cascade matrix
        Renderer::UniformBufferObject shadowUbo{};
        shadowUbo.view = Math::Matrix4::Identity();
        shadowUbo.proj = m_ShadowMap->GetCascadeViewProj(cascade);
        m_UniformBuffers[currentFrame]->UploadData(&shadowUbo, sizeof(shadowUbo));

        m_ShadowMap->BeginCascadePass(commandBuffer, cascade);

        // Bind shadow pipeline
        m_ShadowPipeline->Bind(commandBuffer);

        // Bind descriptor set for uniforms
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_ShadowPipeline->GetLayout(),
            0, 1, &m_DescriptorSets[currentFrame],
            0, nullptr
        );

        // Render cached shadow-casting entities (rebuilt when dirty)
        // This avoids O(n) iteration per cascade — instead we iterate O(k) shadow casters
        if (m_ShadowCastersDirty) {
            RebuildShadowCasterCache();
        }

        for (Entity entity : m_ShadowCasters) {
            // Quick visibility check (may have changed since cache was built)
            auto* xform = m_World->GetComponent<TransformComponent>(entity);
            if (xform && !xform->visible) continue;

            RenderEntityShadow(entity, commandBuffer);
        }

        m_ShadowMap->EndCascadePass(commandBuffer);
    }
}

void RenderSystem::RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer) {
    TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);

    if (!transform || !mesh || !mesh->IsValid()) return;

    auto it = m_EntityRenderData.find(entity);
    if (it == m_EntityRenderData.end()) return;

    EntityRenderData& renderData = it->second;

    // Push model matrix only (shadow pass doesn't need material)
    Renderer::PushConstants pushConstants{};
    pushConstants.model = transform->ToMatrix();

    vkCmdPushConstants(commandBuffer, m_ShadowPipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::PushConstants), &pushConstants);

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    // Draw indexed
    vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
}

std::shared_ptr<Renderer::Texture> RenderSystem::GetOrLoadTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    // Check cache first
    auto it = m_TextureCache.find(path);
    if (it != m_TextureCache.end()) {
        return it->second;
    }

    // Check failed cache (avoid retrying broken paths every frame)
    if (m_FailedTextures.count(path)) {
        return nullptr;
    }

    // Load new texture
    auto texture = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
    if (!texture->LoadFromFile(path)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load texture (will not retry): %s", path.c_str());
        m_FailedTextures.insert(path);
        return nullptr;
    }

    ENJIN_LOG_INFO(Renderer, "Loaded texture: %s (%dx%d)",
        path.c_str(), texture->GetWidth(), texture->GetHeight());

    // Cache and watch for hot-reload
    m_TextureCache[path] = texture;
    m_TextureWatcher.Watch(path, [this](const std::string& changedPath) {
        ENJIN_LOG_INFO(Renderer, "Texture changed, reloading: %s", changedPath.c_str());
        auto it = m_TextureCache.find(changedPath);
        if (it != m_TextureCache.end()) {
            auto newTex = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
            if (newTex->LoadFromFile(changedPath)) {
                it->second = newTex;
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
    Renderer::Texture* emissive)
{
    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    VkDescriptorSet dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    VkDevice device = m_Renderer->GetContext()->GetDevice();

    // Use default white texture for any nullptr slots
    Renderer::Texture* defaultTex = m_DefaultWhiteTexture.get();
    Renderer::Texture* texBase = (baseColor && baseColor->IsValid()) ? baseColor : defaultTex;
    Renderer::Texture* texHeight = (height && height->IsValid()) ? height : defaultTex;
    Renderer::Texture* texNormal = (normal && normal->IsValid()) ? normal : defaultTex;
    Renderer::Texture* texMR = (metallicRoughness && metallicRoughness->IsValid()) ? metallicRoughness : defaultTex;
    Renderer::Texture* texEmissive = (emissive && emissive->IsValid()) ? emissive : defaultTex;

    // Early out if no valid textures at all
    if (!texBase || !texBase->IsValid()) return;

    // Collect image infos (must persist until vkUpdateDescriptorSets returns)
    VkDescriptorImageInfo imageInfos[5];
    imageInfos[0] = texBase->GetDescriptorInfo();
    imageInfos[1] = texHeight->GetDescriptorInfo();
    imageInfos[2] = texNormal->GetDescriptorInfo();
    imageInfos[3] = texMR->GetDescriptorInfo();
    imageInfos[4] = texEmissive->GetDescriptorInfo();

    // Bindings: 3=baseColor, 5=height, 6=normal, 8=metallicRoughness, 9=emissive
    VkWriteDescriptorSet writes[5] = {};

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

    // Single batched call instead of 5 individual calls
    vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);
}

void RenderSystem::UpdateBoneDescriptor(Renderer::VulkanBuffer* boneBuffer) {
    if (!boneBuffer) return;

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

void RenderSystem::EnsureWaterMeshes() {
    for (Entity entity : m_World->GetEntitiesWithComponent<WaterVolumeComponent>()) {
        auto* waterVol = m_World->GetComponent<WaterVolumeComponent>(entity);
        if (!waterVol) continue;
        if (waterVol->meshCreated && m_World->HasComponent<MeshComponent>(entity)) continue;

        // Create a subdivided plane mesh for the water surface
        MeshComponent mesh;
        f32 hx = waterVol->halfExtents.x;
        f32 hz = waterVol->halfExtents.z;

        const u32 segsX = 20;
        const u32 segsZ = 20;

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
    VkDescriptorSetLayout layout = m_Pipeline->GetDescriptorSetLayout();

    if (m_WeatherRenderer) {
        m_WeatherRenderer->RecreateForRenderPass(renderPass, layout);
    }
    if (m_GrassRenderer) {
        m_GrassRenderer->RecreateForRenderPass(renderPass, layout);
    }
    if (m_ShrubRenderer) {
        m_ShrubRenderer->RecreateForRenderPass(renderPass, layout);
    }
    if (m_ParticleRenderer) {
        m_ParticleRenderer->RecreateForRenderPass(renderPass, layout);
    }
    if (m_TreeRenderer) {
        m_TreeRenderer->RecreateForRenderPass(renderPass, layout);
    }
    if (m_SpriteBatchRenderer) {
        m_SpriteBatchRenderer->RecreateForRenderPass(renderPass, layout);
    }

    // Note: skybox pipeline is NOT recreated here — it was created for the swapchain
    // render pass in Initialize() and works in both passes via driver-level render pass
    // compatibility (SRGB/UNORM same memory layout). Destroying and recreating it here
    // with the offscreen render pass would break the editor viewport's skybox rendering.
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
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

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

} // namespace ECS
} // namespace Enjin
