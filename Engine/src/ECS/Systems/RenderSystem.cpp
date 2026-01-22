#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include <cstring>

namespace Enjin {
namespace ECS {

RenderSystem::RenderSystem(World* world, Renderer::VulkanRenderer* renderer)
    : m_World(world), m_Renderer(renderer) {
    // Camera will be set externally
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
            Math::Vector3(0.0f, 0.0f, -3.0f),
            Math::Vector3(0.0f, 0.0f, 0.0f),
            Math::Vector3(0.0f, 1.0f, 0.0f)
        );
        m_Camera = &defaultCamera;
    }

    // Create shaders
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TriangleVertexShader.data()),
        Renderer::ShaderData::TriangleVertexShader.size() * sizeof(u32))) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TriangleFragmentShader.data()),
        Renderer::ShaderData::TriangleFragmentShader.size() * sizeof(u32))) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load fragment shader");
        return;
    }

    // Create pipeline
    CreatePipeline();

    // Create uniform buffers
    CreateUniformBuffers();

    // Create descriptor sets
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

    // Clean up descriptor pool
    if (m_DescriptorPool != VK_NULL_HANDLE && m_Renderer->GetContext()) {
        vkDestroyDescriptorPool(m_Renderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    // Clean up entity render data
    m_EntityRenderData.clear();

    // Clean up uniform buffers
    m_UniformBuffers.clear();
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

    // Render all entities with Transform and Mesh components
    if (m_TriangleEntity != INVALID_ENTITY) {
        RenderEntity(m_TriangleEntity);
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
    config.cullMode = VK_CULL_MODE_BACK_BIT;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create graphics pipeline");
        m_Pipeline.reset();
    }
}

void RenderSystem::CreateUniformBuffers() {
    constexpr usize bufferSize = sizeof(Renderer::UniformBufferObject);
    constexpr u32 framesInFlight = 2; // Match renderer's MAX_FRAMES_IN_FLIGHT

    m_UniformBuffers.resize(framesInFlight);
    for (u32 i = 0; i < framesInFlight; ++i) {
        m_UniformBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
        if (!m_UniformBuffers[i]->Create(bufferSize, Renderer::BufferUsage::Uniform, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create uniform buffer %u", i);
            return;
        }
    }
}

void RenderSystem::CreateDescriptorSets() {
    constexpr u32 framesInFlight = 2;

    // Create descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = framesInFlight;

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

    // Update descriptor sets
    for (u32 i = 0; i < framesInFlight; ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffers[i]->GetBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Renderer::UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_DescriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_Renderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
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

    // Use the renderer's current frame index for proper synchronization
    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    Renderer::UniformBufferObject ubo{};
    ubo.model = transform->ToMatrix();
    ubo.view = m_Camera->GetViewMatrix();
    ubo.proj = m_Camera->GetProjectionMatrix();

    m_UniformBuffers[currentFrame]->UploadData(&ubo, sizeof(ubo));
}

void RenderSystem::CreateTriangleMesh() {
    m_TriangleEntity = m_World->CreateEntity();

    // Add transform
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
    static bool firstFrame = true;

    if (!m_Pipeline || !m_Renderer) {
        return;
    }

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: checking components...");

    TransformComponent* transform = m_World->GetComponent<TransformComponent>(entity);
    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);

    if (!transform || !mesh || !mesh->IsValid()) {
        return;
    }

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: getting render data...");

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

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: updating uniform buffer...");

    // Update uniform buffer
    UpdateUniformBuffer(entity);

    // Use the renderer's current frame index for proper synchronization
    u32 currentFrame = m_Renderer->GetCurrentFrameIndex();

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: binding pipeline...");

    // Bind pipeline
    m_Pipeline->Bind(commandBuffer);

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: binding descriptor set %u...", currentFrame);

    // Bind descriptor set
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(),
        0, 1, &m_DescriptorSets[currentFrame],
        0, nullptr
    );

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: setting viewport/scissor...");

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

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: binding vertex buffer...");

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: binding index buffer...");

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    if (firstFrame) ENJIN_LOG_INFO(Renderer, "RenderEntity: drawing %u indices...", renderData.indexCount);

    // Draw
    vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);

    if (firstFrame) {
        ENJIN_LOG_INFO(Renderer, "RenderEntity: draw call complete!");
        firstFrame = false;
    }
}

} // namespace ECS
} // namespace Enjin
