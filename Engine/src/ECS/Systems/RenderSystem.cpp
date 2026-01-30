#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/MeshFactory.h"
#include <cstring>
#include <array>

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

    // Create shadow map
    m_ShadowMap = std::make_unique<Renderer::ShadowMap>(m_Renderer->GetContext());
    Renderer::ShadowMapConfig shadowConfig;
    shadowConfig.resolution = 2048;
    shadowConfig.orthoSize = 30.0f;
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

    // Create default sphere mesh
    CreateDefaultMesh();

    // Initialize weather particle renderer
    m_WeatherRenderer = std::make_unique<Effects::WeatherRenderer>();
    if (!m_WeatherRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "WeatherRenderer initialization failed, 3D particles disabled");
        m_WeatherRenderer.reset();
    }

    // Initialize grass renderer
    m_GrassRenderer = std::make_unique<Effects::GrassRenderer>();
    if (!m_GrassRenderer->Initialize(m_Renderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "GrassRenderer initialization failed, grass disabled");
        m_GrassRenderer.reset();
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

    // Clean up weather and grass renderers
    m_WeatherRenderer.reset();
    m_GrassRenderer.reset();

    // Clean up line pipeline
    m_LinePipeline.reset();

    // Clean up shadow resources
    m_ShadowPipeline.reset();
    m_ShadowMap.reset();

    // Clean up textures and bone buffer
    m_DefaultWhiteTexture.reset();
    m_DefaultBoneBuffer.reset();

    // Clean up pipeline
    m_Pipeline.reset();
    m_FragmentShader.reset();
    m_VertexShader.reset();

    m_Initialized = false;
}

void RenderSystem::Update(f32 deltaTime) {
    if (!m_Renderer || !m_Initialized) {
        return;
    }

    // Auto-create meshes for water volume entities that don't have one yet
    EnsureWaterMeshes();

    // Update skeletal animators
    const auto& allEntities = m_World->GetAllEntities();
    for (Entity entity : allEntities) {
        AnimatorComponent* animComp = m_World->GetComponent<AnimatorComponent>(entity);
        if (animComp) {
            animComp->Update(deltaTime);
        }
    }

    // Shadow pass first (if enabled) - runs before main render pass
    if (m_ShadowsEnabled && m_ShadowMap && m_ShadowPipeline) {
        RenderShadowPass();
    }

    // Begin the main render pass (after any pre-passes like shadows)
    m_Renderer->BeginMainRenderPass();

    // Main render pass - render all entities with mesh and transform components
    const auto& entities = m_World->GetAllEntities();
    for (Entity entity : entities) {
        if (m_World->HasComponent<TransformComponent>(entity) &&
            m_World->HasComponent<MeshComponent>(entity)) {
            RenderEntity(entity);
        }
    }
}

void RenderSystem::RenderToTarget(Renderer::RenderTarget* target, Renderer::Camera* camera) {
    if (!target || !target->IsValid() || !camera || !m_Renderer || !m_Initialized || !m_Pipeline) {
        return;
    }

    // Temporarily swap the camera so UpdateUniformBuffer uses the game camera
    Renderer::Camera* prevCamera = m_Camera;
    m_Camera = camera;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        m_Camera = prevCamera;
        return;
    }

    // The render target's Begin/End are handled by the caller (EditorLayer::RenderOffscreen)
    // We just need to bind our pipeline and draw within the active render pass

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

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

    // Render all entities with mesh and transform
    const auto& entities = m_World->GetAllEntities();
    for (Entity entity : entities) {
        if (m_World->HasComponent<TransformComponent>(entity) &&
            m_World->HasComponent<MeshComponent>(entity)) {

            auto it = m_EntityRenderData.find(entity);
            if (it == m_EntityRenderData.end()) {
                SetupEntityBuffers(entity);
                it = m_EntityRenderData.find(entity);
                if (it == m_EntityRenderData.end()) continue;
            }
            EntityRenderData& renderData = it->second;

            // Update UBOs for this entity (uses swapped m_Camera)
            UpdateUniformBuffer(entity);

            // Bind pipeline and descriptor set
            m_Pipeline->Bind(commandBuffer);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 0, nullptr);

            // Re-set viewport/scissor (pipeline bind may reset dynamic state)
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Push constants
            TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
            Renderer::PushConstants pushConstants{};
            pushConstants.model = transform->ToMatrix();

            MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
            Renderer::Texture* boundTexture = nullptr;

            if (material) {
                pushConstants.baseColor = material->baseColor;
                pushConstants.metallic = material->metallic;
                pushConstants.emissiveColor = material->emissiveColor;
                pushConstants.roughness = material->roughness;
                pushConstants.emissiveStrength = material->emissiveStrength;
                pushConstants.opacity = material->opacity;
                pushConstants.alphaCutoff = material->alphaCutoff;

                if (!material->baseColorTexturePath.empty()) {
                    auto tex = GetOrLoadTexture(material->baseColorTexturePath);
                    if (tex && tex->IsValid()) {
                        boundTexture = tex.get();
                        material->baseColorTexture = 1;
                    }
                }

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
                // Retro flags
                if (material->flatShading) pushConstants.flags |= (1 << 20);
                if (material->affineTexturing) pushConstants.flags |= (1 << 21);
                if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
                if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
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

            // Set wind sway flag for vegetation entities
            VegetationComponent* vegComp = m_World->GetComponent<VegetationComponent>(entity);
            if (vegComp) {
                pushConstants.flags |= (1 << 4); // FLAG_WIND_SWAY
            }

            if (boundTexture) {
                UpdateTextureDescriptor(boundTexture);
            } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                UpdateTextureDescriptor(m_DefaultWhiteTexture.get());
            }

            // Bind height map texture if available
            if (material && !material->heightTexturePath.empty()) {
                auto heightTex = GetOrLoadTexture(material->heightTexturePath);
                if (heightTex && heightTex->IsValid()) {
                    material->heightTexture = 1;
                    UpdateHeightTextureDescriptor(heightTex.get());
                }
            } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                UpdateHeightTextureDescriptor(m_DefaultWhiteTexture.get());
            }

            // Bind normal map texture if available
            if (material && !material->normalTexturePath.empty()) {
                auto normalTex = GetOrLoadTexture(material->normalTexturePath);
                if (normalTex && normalTex->IsValid()) {
                    material->normalTexture = 1;
                    UpdateNormalMapDescriptor(normalTex.get());
                }
            } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                UpdateNormalMapDescriptor(m_DefaultWhiteTexture.get());
            }

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
        }
    }

    // Restore previous camera
    m_Camera = prevCamera;
}

void RenderSystem::OnEntityAdded(Entity entity) {
    SetupEntityBuffers(entity);
}

void RenderSystem::OnEntityRemoved(Entity entity) {
    m_EntityRenderData.erase(entity);
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

    m_UniformBuffers.resize(framesInFlight);
    m_LightingBuffers.resize(framesInFlight);
    m_MaterialBuffers.resize(framesInFlight);

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
    }
}

void RenderSystem::CreateDescriptorSets() {
    constexpr u32 framesInFlight = 2;

    // Create descriptor pool (3 UBOs + 4 combined image samplers + 1 SSBO per frame)
    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = framesInFlight * 3;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = framesInFlight * 4;  // base color + shadow map + height map + normal map
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = framesInFlight * 1;  // bone matrices SSBO

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = framesInFlight;

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
            // Fallback - shouldn't happen but be safe
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = VK_NULL_HANDLE;
            imageInfo.sampler = VK_NULL_HANDLE;
        }

        // Shadow map (binding 4)
        VkDescriptorImageInfo shadowImageInfo{};
        if (m_ShadowMap && m_ShadowsEnabled) {
            shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowImageInfo.imageView = m_ShadowMap->GetDepthImageView();
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

        std::array<VkWriteDescriptorSet, 8> descriptorWrites{};

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

        vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(),
            static_cast<u32>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

void RenderSystem::SetupEntityBuffers(Entity entity) {
    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);
    if (!mesh || !mesh->IsValid()) {
        return;
    }

    EntityRenderData& renderData = m_EntityRenderData[entity];

    // Create vertex buffer
    usize vertexBufferSize = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
    renderData.vertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!renderData.vertexBuffer->Create(vertexBufferSize, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create vertex buffer for entity %llu", entity);
        return;
    }

    if (!renderData.vertexBuffer->UploadData(mesh->vertices.data(), vertexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload vertex data for entity %llu", entity);
        return;
    }

    // Create index buffer
    usize indexBufferSize = mesh->indices.size() * sizeof(u32);
    renderData.indexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!renderData.indexBuffer->Create(indexBufferSize, Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create index buffer for entity %llu", entity);
        return;
    }

    if (!renderData.indexBuffer->UploadData(mesh->indices.data(), indexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload index data for entity %llu", entity);
        return;
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
}

void RenderSystem::UpdateUniformBuffer(Entity entity) {
    TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
    if (!transform || !m_Camera) {
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Update View/Projection UBO (shared across all objects)
    // Note: Model matrix is now sent via push constants in RenderEntity
    Renderer::UniformBufferObject ubo{};
    ubo.view = m_Camera->GetViewMatrix();
    ubo.proj = m_Camera->GetProjectionMatrix();
    m_UniformBuffers[currentFrame]->UploadData(&ubo, sizeof(ubo));

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

    // Query all entities and check for LightComponent
    const auto& allEntities = m_World->GetAllEntities();
    bool hasAnyLight = false;

    for (Entity lightEntity : allEntities) {
        if (!m_World->HasComponent<LightComponent>(lightEntity)) continue;
        LightComponent* light = m_World->GetComponent<LightComponent>(lightEntity);
        TransformComponent* lightTransform = m_World->GetComponent<TransformComponent>(lightEntity);
        if (!light) continue;

        hasAnyLight = true;

        switch (light->type) {
            case LightType::Directional: {
                if (lighting.directionalLightCount < MAX_DIRECTIONAL_LIGHTS) {
                    auto& dirLight = lighting.directionalLights[lighting.directionalLightCount];
                    // Use transform rotation to determine direction (where light points)
                    // Shader will negate this to get direction toward light source
                    if (lightTransform) {
                        Math::Vector3 forward(0.0f, 0.0f, -1.0f);
                        dirLight.direction = lightTransform->rotation.Rotate(forward).Normalized();
                    } else {
                        // Default: light pointing down and to the side (like sun)
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
                    // Use transform rotation to determine direction
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

    // If no lights in scene, add a default directional light
    if (!hasAnyLight) {
        // Direction light points (shader negates for "toward light" calculation)
        lighting.directionalLights[0].direction = Math::Vector3(-0.5f, -0.8f, -0.3f).Normalized();
        lighting.directionalLights[0].color = Math::Vector3(1.0f, 0.95f, 0.9f);
        lighting.directionalLights[0].intensity = 1.2f;
        lighting.directionalLightCount = 1;
    }

    // Shadow mapping data
    if (m_ShadowsEnabled && m_ShadowMap) {
        lighting.lightSpaceMatrix = m_ShadowMap->GetLightSpaceMatrix();
        lighting.shadowBias = m_ShadowMap->GetDepthBias();
        lighting.shadowEnabled = 1;
    } else {
        lighting.lightSpaceMatrix = Math::Matrix4::Identity();
        lighting.shadowBias = 0.005f;
        lighting.shadowEnabled = 0;
    }
    lighting._shadowPad[0] = 0.0f;
    lighting._shadowPad[1] = 0.0f;

    // Upload wind data from WindSystem
    if (m_WindSystem) {
        lighting.windData = m_WindSystem->GetWindVector();
    } else {
        lighting.windData = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    m_LightingBuffers[currentFrame]->UploadData(&lighting, sizeof(lighting));

    // Update Material UBO
    MaterialGPU materialGPU;
    MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
    if (material) {
        materialGPU = MaterialGPU::FromComponent(*material);
    } else {
        // Default material (light gray, non-metallic)
        MaterialComponent defaultMat;
        defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        defaultMat.metallic = 0.0f;
        defaultMat.roughness = 0.5f;
        materialGPU = MaterialGPU::FromComponent(defaultMat);
    }
    m_MaterialBuffers[currentFrame]->UploadData(&materialGPU, sizeof(materialGPU));
}

void RenderSystem::SetBackfaceCullingEnabled(bool enabled) {
    if (m_BackfaceCulling == enabled) return;
    m_BackfaceCulling = enabled;
    // Recreate pipeline with new cull mode
    if (m_Pipeline && m_Initialized) {
        m_Pipeline.reset();
        CreatePipeline();
    }
}

void RenderSystem::SetWireframeEnabled(bool enabled) {
    if (m_WireframeMode == enabled) return;
    m_WireframeMode = enabled;
    // Recreate pipeline with new polygon mode
    if (m_Pipeline && m_Initialized) {
        m_Pipeline.reset();
        CreatePipeline();
    }
}

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

    auto it = m_EntityRenderData.find(entity);
    if (it == m_EntityRenderData.end()) {
        SetupEntityBuffers(entity);
        it = m_EntityRenderData.find(entity);
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

    // Update uniform buffer with transforms
    UpdateUniformBuffer(entity);

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Bind pipeline
    m_Pipeline->Bind(commandBuffer);

    // Bind descriptor set (UBO)
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(),
        0, 1, &m_DescriptorSets[currentFrame],
        0, nullptr
    );

    // Set viewport and scissor
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

    // Push model matrix and material for this entity
    Renderer::PushConstants pushConstants{};
    pushConstants.model = transform->ToMatrix();

    // Set material data
    MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
    Renderer::Texture* boundTexture = nullptr;

    if (material) {
        pushConstants.baseColor = material->baseColor;
        pushConstants.metallic = material->metallic;
        pushConstants.emissiveColor = material->emissiveColor;
        pushConstants.roughness = material->roughness;
        pushConstants.emissiveStrength = material->emissiveStrength;
        pushConstants.opacity = material->opacity;
        pushConstants.alphaCutoff = material->alphaCutoff;

        // Try to load base color texture if path is set
        if (!material->baseColorTexturePath.empty()) {
            auto tex = GetOrLoadTexture(material->baseColorTexturePath);
            if (tex && tex->IsValid()) {
                boundTexture = tex.get();
                material->baseColorTexture = 1; // Mark as having texture
            }
        }

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
        // Retro flags
        if (material->flatShading) pushConstants.flags |= (1 << 20);
        if (material->affineTexturing) pushConstants.flags |= (1 << 21);
        if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
        if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
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

    // Set wind sway flag for vegetation entities
    VegetationComponent* vegComp = m_World->GetComponent<VegetationComponent>(entity);
    if (vegComp) {
        pushConstants.flags |= (1 << 4); // FLAG_WIND_SWAY
    }

    // Update texture descriptor if entity has a texture
    if (boundTexture) {
        UpdateTextureDescriptor(boundTexture);
    } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
        // Reset to default white texture
        UpdateTextureDescriptor(m_DefaultWhiteTexture.get());
    }

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
}

void RenderSystem::RenderShadowPass() {
    if (!m_ShadowMap || !m_ShadowPipeline) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Find the first directional light for shadow casting
    const auto& allEntities = m_World->GetAllEntities();
    bool foundShadowLight = false;

    for (Entity lightEntity : allEntities) {
        if (!m_World->HasComponent<LightComponent>(lightEntity)) continue;
        LightComponent* light = m_World->GetComponent<LightComponent>(lightEntity);
        if (!light || light->type != LightType::Directional || !light->castShadows) continue;

        TransformComponent* lightTransform = m_World->GetComponent<TransformComponent>(lightEntity);
        if (lightTransform) {
            Math::Vector3 forward(0.0f, 0.0f, -1.0f);
            Math::Vector3 lightDir = lightTransform->rotation.Rotate(forward).Normalized();
            m_ShadowMap->SetLightDirection(lightDir);
            // Position the shadow map to cover the scene
            m_ShadowMap->SetLightPosition(Math::Vector3(0.0f, 20.0f, 0.0f) - lightDir * 30.0f);
        }
        foundShadowLight = true;
        break;
    }

    // If no shadow-casting light, use default
    if (!foundShadowLight) {
        m_ShadowMap->SetLightDirection(Math::Vector3(0.5f, 0.8f, 0.3f).Normalized());
        m_ShadowMap->SetLightPosition(Math::Vector3(-15.0f, 24.0f, -9.0f));
    }

    // Begin shadow pass
    m_ShadowMap->BeginShadowPass(commandBuffer);

    // Bind shadow pipeline
    m_ShadowPipeline->Bind(commandBuffer);

    // Bind descriptor set for uniforms
    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_ShadowPipeline->GetLayout(),
        0, 1, &m_DescriptorSets[currentFrame],
        0, nullptr
    );

    // Render all shadow-casting entities
    for (Entity entity : allEntities) {
        if (m_World->HasComponent<TransformComponent>(entity) &&
            m_World->HasComponent<MeshComponent>(entity)) {
            // Check if material casts shadows (default: yes)
            MaterialComponent* material = m_World->GetComponent<MaterialComponent>(entity);
            if (material && !material->castShadows) continue;

            RenderEntityShadow(entity, commandBuffer);
        }
    }

    // End shadow pass
    m_ShadowMap->EndShadowPass(commandBuffer);
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

    // Load new texture
    auto texture = std::make_shared<Renderer::Texture>(m_Renderer->GetContext());
    if (!texture->LoadFromFile(path)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load texture: %s", path.c_str());
        return nullptr;
    }

    ENJIN_LOG_INFO(Renderer, "Loaded texture: %s (%dx%d)",
        path.c_str(), texture->GetWidth(), texture->GetHeight());

    // Cache and return
    m_TextureCache[path] = texture;
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
    descriptorWrite.dstSet = m_DescriptorSets[currentFrame];
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
    descriptorWrite.dstSet = m_DescriptorSets[currentFrame];
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
    descriptorWrite.dstSet = m_DescriptorSets[currentFrame];
    descriptorWrite.dstBinding = 6;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
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
    descriptorWrite.dstSet = m_DescriptorSets[currentFrame];
    descriptorWrite.dstBinding = 7;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::EnsureWaterMeshes() {
    const auto& entities = m_World->GetAllEntities();
    for (Entity entity : entities) {
        if (!m_World->HasComponent<WaterVolumeComponent>(entity)) continue;
        if (m_World->HasComponent<MeshComponent>(entity)) continue;

        auto* waterVol = m_World->GetComponent<WaterVolumeComponent>(entity);
        if (!waterVol) continue;

        // Create a subdivided plane mesh for the water surface
        MeshComponent mesh;
        f32 hx = waterVol->halfExtents.x;
        f32 hz = waterVol->halfExtents.z;

        const u32 segsX = 16;
        const u32 segsZ = 16;

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
                v.color = Math::Vector4(
                    waterVol->waterColor.x,
                    waterVol->waterColor.y,
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

        // Add material with water visual properties
        MaterialComponent material;
        material.baseColor = waterVol->waterColor;
        material.opacity = waterVol->opacity;
        material.doubleSided = true;
        material.metallic = 0.3f;
        material.roughness = 0.1f;
        material.castShadows = false;

        m_World->AddComponent<MaterialComponent>(entity, material);

        SetupEntityBuffers(entity);

        ENJIN_LOG_INFO(Renderer, "Created water surface mesh for entity %llu (%.0f x %.0f)",
            entity, hx * 2.0f, hz * 2.0f);
    }
}

void RenderSystem::RenderWeatherParticles(const Effects::WeatherSystem& weather, bool isRain) {
    if (!m_WeatherRenderer || !m_Renderer || !m_Initialized) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_WeatherRenderer->Render(commandBuffer, m_DescriptorSets, currentFrame, weather, isRain);
}

void RenderSystem::RenderGrass() {
    if (!m_GrassRenderer || !m_Renderer || !m_Initialized || !m_World) return;

    VkCommandBuffer commandBuffer = m_Renderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();
    m_GrassRenderer->Render(commandBuffer, m_DescriptorSets, currentFrame, m_World);
}

} // namespace ECS
} // namespace Enjin
