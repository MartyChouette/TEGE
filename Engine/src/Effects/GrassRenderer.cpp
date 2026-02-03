#include "Enjin/Effects/GrassRenderer.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cstring>

namespace Enjin {
namespace Effects {

GrassRenderer::~GrassRenderer() {
    Shutdown();
}

bool GrassRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;

    CreateBladeMesh();
    CreatePipeline(sharedLayout);

    if (!m_Pipeline || !m_BladeVertexBuffer || !m_BladeIndexBuffer) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to initialize resources");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "GrassRenderer initialized");
    return true;
}

void GrassRenderer::Shutdown() {
    if (!m_Initialized) return;

    if (m_Renderer && m_Renderer->GetContext()) {
        vkDeviceWaitIdle(m_Renderer->GetContext()->GetDevice());
    }

    m_Pipeline.reset();
    m_VertexShader.reset();
    m_FragmentShader.reset();
    m_BladeIndexBuffer.reset();
    m_BladeVertexBuffer.reset();

    m_Initialized = false;
}

void GrassRenderer::CreateBladeMesh() {
    // Tapered grass blade: 7 vertices forming a tapered shape from base to tip
    // Viewed from front: wider at base, narrowing to a point at top
    //
    //       6 (tip)
    //      / \
    //     4   5
    //    / \ / \
    //   2       3
    //  / \     / \
    // 0         1   (base)
    //
    // Position (vec3) + UV (vec2) = 5 floats per vertex
    // Y ranges from 0 (base) to 1 (tip), X ranges from -0.5 to 0.5 at base

    struct BladeVertex {
        f32 px, py, pz;  // position
        f32 u, v;         // UV
    };

    BladeVertex verts[] = {
        // Base row (y=0, full width)
        { -0.5f, 0.0f, 0.0f,   0.0f, 0.0f },  // 0: bottom-left
        {  0.5f, 0.0f, 0.0f,   1.0f, 0.0f },  // 1: bottom-right
        // Middle-low row (y=0.33, narrower)
        { -0.35f, 0.33f, 0.0f, 0.15f, 0.33f }, // 2: mid-left
        {  0.35f, 0.33f, 0.0f, 0.85f, 0.33f }, // 3: mid-right
        // Middle-high row (y=0.66, narrower)
        { -0.2f, 0.66f, 0.0f,  0.3f, 0.66f },  // 4: upper-left
        {  0.2f, 0.66f, 0.0f,  0.7f, 0.66f },  // 5: upper-right
        // Tip (y=1, point)
        {  0.0f, 1.0f, 0.0f,   0.5f, 1.0f },   // 6: tip
    };

    u32 indices[] = {
        // Bottom quad (2 tris)
        0, 1, 2,
        2, 1, 3,
        // Middle quad (2 tris)
        2, 3, 4,
        4, 3, 5,
        // Top triangle
        4, 5, 6,
    };

    m_BladeIndexCount = sizeof(indices) / sizeof(u32);

    m_BladeVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_BladeVertexBuffer->Create(sizeof(verts), Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to create blade vertex buffer");
        return;
    }
    m_BladeVertexBuffer->UploadData(verts, sizeof(verts));

    m_BladeIndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_BladeIndexBuffer->Create(sizeof(indices), Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to create blade index buffer");
        return;
    }
    m_BladeIndexBuffer->UploadData(indices, sizeof(indices));
}

void GrassRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return;

    vkDeviceWaitIdle(m_Renderer->GetContext()->GetDevice());
    m_Pipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to recreate pipeline for render pass");
    }
}

void GrassRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::GrassVertexShaderData),
        Renderer::ShaderData::GrassVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::GrassFragmentShaderData),
        Renderer::ShaderData::GrassFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass fragment shader");
        return;
    }

    // Custom vertex input: single binding for blade mesh (pos + UV = 5 floats)
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(f32) * 5;  // vec3 pos + vec2 uv
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attrs{};
    // Position (location 0)
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    // UV (location 1)
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = sizeof(f32) * 3;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    Renderer::PipelineConfig config;
    config.renderPass = m_Renderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_NONE;  // Grass blades are thin, render both sides
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = false;
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to create grass pipeline");
        m_Pipeline.reset();
    }
}

void GrassRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout) {
    // Reuse existing shaders if loaded, otherwise load them
    if (!m_VertexShader) {
        m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_VertexShader->LoadFromSPIRV(
            reinterpret_cast<const u8*>(Renderer::ShaderData::GrassVertexShaderData),
            Renderer::ShaderData::GrassVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass vertex shader");
            return;
        }
    }
    if (!m_FragmentShader) {
        m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_FragmentShader->LoadFromSPIRV(
            reinterpret_cast<const u8*>(Renderer::ShaderData::GrassFragmentShaderData),
            Renderer::ShaderData::GrassFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass fragment shader");
            return;
        }
    }

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(f32) * 5;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attrs{};
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = sizeof(f32) * 3;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    Renderer::PipelineConfig config;
    config.renderPass = renderPass;
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = false;
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to create grass pipeline");
        m_Pipeline.reset();
    }
}

void GrassRenderer::Render(VkCommandBuffer commandBuffer,
                            const std::vector<VkDescriptorSet>& descriptorSets,
                            u32 currentFrame,
                            ECS::World* world,
                            u32 viewportWidth,
                            u32 viewportHeight) {
    if (!m_Initialized || !m_Pipeline || !world) return;

    const auto& entities = world->GetAllEntities();
    bool hasBound = false;

    for (ECS::Entity entity : entities) {
        if (!world->HasComponent<ECS::GrassVolumeComponent>(entity)) continue;
        if (!world->HasComponent<ECS::TransformComponent>(entity)) continue;

        auto* grass = world->GetComponent<ECS::GrassVolumeComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!grass || !transform) continue;

        if (!hasBound) {
            // Bind pipeline and shared resources once
            m_Pipeline->Bind(commandBuffer);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

            // Use override dimensions if provided, else swapchain
            VkExtent2D extent;
            if (viewportWidth > 0 && viewportHeight > 0) {
                extent.width = viewportWidth;
                extent.height = viewportHeight;
            } else {
                extent = m_Renderer->GetSwapchainExtent();
            }
            if (extent.width == 0 || extent.height == 0) return;
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

            VkBuffer vertexBuffers[] = { m_BladeVertexBuffer->GetBuffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, m_BladeIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

            hasBound = true;
        }

        // Build model matrix that encodes position and half-extents
        // Column-major flat array: m[0]=col0.x, m[5]=col1.y, m[10]=col2.z, m[12-14]=translation
        Math::Matrix4 model = Math::Matrix4::Identity();
        model.m[0] = grass->halfExtents.x;   // col0.x = half extent X
        model.m[5] = 1.0f;                    // col1.y = Y scale unused
        model.m[10] = grass->halfExtents.z;   // col2.z = half extent Z
        model.m[12] = transform->position.x;  // translation X
        model.m[13] = transform->position.y;  // translation Y
        model.m[14] = transform->position.z;  // translation Z

        // Pack grass parameters into push constants
        Renderer::PushConstants pc{};
        pc.model = model;
        pc.baseColor = grass->baseColor;
        pc.emissiveColor = grass->tipColor;
        pc.emissiveStrength = grass->bladeHeight;
        pc.opacity = grass->bladeHeightVariance;
        pc.alphaCutoff = grass->bladeWidth;
        pc.flags = static_cast<i32>(grass->density);
        pc.parallaxScale = grass->windSwayStrength;

        vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        // Draw instanced: blade mesh * density instances
        vkCmdDrawIndexed(commandBuffer, m_BladeIndexCount, grass->density, 0, 0, 0);
    }
}

} // namespace Effects
} // namespace Enjin
