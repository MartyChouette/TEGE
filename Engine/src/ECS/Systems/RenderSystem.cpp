#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
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
        Renderer::ShaderData::TriangleVertexShaderSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TriangleFragmentShaderData),
        Renderer::ShaderData::TriangleFragmentShaderSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load fragment shader");
        return;
    }

    // Create pipeline
    CreatePipeline();

    // Create uniform buffers and descriptor sets
    CreateUniformBuffers();
    CreateDescriptorSets();

    // Create triangle mesh
    CreateTriangleMesh();

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

    // Clean up pipeline
    m_Pipeline.reset();
    m_FragmentShader.reset();
    m_VertexShader.reset();

    m_Initialized = false;
}

void RenderSystem::Update(f32 deltaTime) {
    (void)deltaTime;

    if (!m_Renderer || !m_Initialized) {
        return;
    }

    // Render all entities with mesh and transform components
    const auto& entities = m_World->GetAllEntities();
    for (Entity entity : entities) {
        if (m_World->HasComponent<TransformComponent>(entity) &&
            m_World->HasComponent<MeshComponent>(entity)) {
            RenderEntity(entity);
        }
    }
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
    config.cullMode = VK_CULL_MODE_NONE;  // Disable culling for now
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create graphics pipeline");
        m_Pipeline.reset();
    }
}

void RenderSystem::CreateUniformBuffers() {
    constexpr u32 framesInFlight = 2;

    m_UniformBuffers.resize(framesInFlight);
    m_LightingBuffers.resize(framesInFlight);
    m_MaterialBuffers.resize(framesInFlight);

    for (u32 i = 0; i < framesInFlight; ++i) {
        // MVP uniform buffer
        m_UniformBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!m_UniformBuffers[i]->Create(sizeof(Renderer::UniformBufferObject), Renderer::BufferUsage::Uniform, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create uniform buffer %u", i);
            return;
        }

        // Lighting uniform buffer
        m_LightingBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!m_LightingBuffers[i]->Create(sizeof(Renderer::LightingUBO), Renderer::BufferUsage::Uniform, true)) {
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

    // Create descriptor pool (3 UBOs per frame: MVP + Lighting + Material)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = framesInFlight * 3;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
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

    // Update descriptor sets with all UBOs
    for (u32 i = 0; i < framesInFlight; ++i) {
        std::array<VkDescriptorBufferInfo, 3> bufferInfos{};

        // MVP UBO
        bufferInfos[0].buffer = m_UniformBuffers[i]->GetBuffer();
        bufferInfos[0].offset = 0;
        bufferInfos[0].range = sizeof(Renderer::UniformBufferObject);

        // Lighting UBO
        bufferInfos[1].buffer = m_LightingBuffers[i]->GetBuffer();
        bufferInfos[1].offset = 0;
        bufferInfos[1].range = sizeof(Renderer::LightingUBO);

        // Material UBO
        bufferInfos[2].buffer = m_MaterialBuffers[i]->GetBuffer();
        bufferInfos[2].offset = 0;
        bufferInfos[2].range = sizeof(MaterialGPU);

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

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
}

void RenderSystem::UpdateUniformBuffer(Entity entity) {
    TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
    if (!transform || !m_Camera) {
        return;
    }

    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    // Update MVP UBO
    Renderer::UniformBufferObject ubo{};
    ubo.model = transform->ToMatrix();
    ubo.view = m_Camera->GetViewMatrix();
    ubo.proj = m_Camera->GetProjectionMatrix();
    m_UniformBuffers[currentFrame]->UploadData(&ubo, sizeof(ubo));

    // Update Lighting UBO
    Renderer::LightingUBO lighting{};
    lighting.ambientColor = Math::Vector3(0.1f, 0.1f, 0.15f);
    lighting.ambientIntensity = 1.0f;
    lighting.cameraPos = m_Camera->GetPosition();
    lighting._pad0 = 0.0f;
    // Default directional light (sun from upper-right)
    lighting.lightDir = Math::Vector3(0.5f, 0.8f, 0.3f).Normalized();
    lighting.lightIntensity = 1.2f;
    lighting.lightColor = Math::Vector3(1.0f, 0.95f, 0.9f);
    lighting._pad1[0] = 0.0f; lighting._pad1[1] = 0.0f; lighting._pad1[2] = 0.0f;
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

void RenderSystem::CreateTriangleMesh() {
    m_TriangleEntity = m_World->CreateEntity();

    // Add transform at origin
    TransformComponent& transform = m_World->AddComponent<TransformComponent>(m_TriangleEntity);
    transform.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    transform.scale = Math::Vector3(1.0f);

    // Add mesh (triangle)
    MeshComponent& mesh = m_World->AddComponent<MeshComponent>(m_TriangleEntity);
    mesh.vertices = {
        { Math::Vector3(0.0f, -0.5f, 0.0f), Math::Vector3(0.0f, 0.0f, 1.0f), Math::Vector2(0.5f, 0.0f) },
        { Math::Vector3(0.5f, 0.5f, 0.0f), Math::Vector3(0.0f, 0.0f, 1.0f), Math::Vector2(1.0f, 1.0f) },
        { Math::Vector3(-0.5f, 0.5f, 0.0f), Math::Vector3(0.0f, 0.0f, 1.0f), Math::Vector2(0.0f, 1.0f) }
    };
    mesh.indices = { 0, 1, 2 };

    SetupEntityBuffers(m_TriangleEntity);
    ENJIN_LOG_INFO(Renderer, "Created triangle entity: %llu", m_TriangleEntity);
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

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    // Draw indexed
    vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
}

} // namespace ECS
} // namespace Enjin
