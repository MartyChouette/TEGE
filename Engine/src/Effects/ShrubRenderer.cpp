#include "Enjin/Effects/ShrubRenderer.h"
#include "Enjin/Effects/VegetationTemplates.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

ShrubRenderer::~ShrubRenderer() {
    Shutdown();
}

bool ShrubRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;

    CreateShrubMesh();
    CreatePipeline(sharedLayout);

    if (!m_Pipeline || !m_VertexBuffer || !m_IndexBuffer) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to initialize resources");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "ShrubRenderer initialized");
    return true;
}

void ShrubRenderer::Shutdown() {
    if (!m_Initialized) return;

    if (m_Renderer && m_Renderer->GetContext()) {
        m_Renderer->GetContext()->WaitForGPU();
    }

    m_Pipeline.reset();
    m_VertexShader.reset();
    m_FragmentShader.reset();
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();

    m_Initialized = false;
}

void ShrubRenderer::CreateShrubMesh() {
    // Shrub geometry is defined once in VegTemplates (tapered dome quads, shared
    // with the RT path). 5 floats/vertex.
    std::vector<Effects::VegTemplates::VegVertex> verts;
    std::vector<u32> indices;
    Effects::VegTemplates::BuildShrub(verts, indices);

    m_IndexCount = static_cast<u32>(indices.size());
    usize vtxBytes = verts.size() * sizeof(Effects::VegTemplates::VegVertex);
    usize idxBytes = indices.size() * sizeof(u32);

    m_VertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_VertexBuffer->Create(vtxBytes, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to create vertex buffer");
        return;
    }
    m_VertexBuffer->UploadData(verts.data(), vtxBytes);

    m_IndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_IndexBuffer->Create(idxBytes, Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to create index buffer");
        return;
    }
    m_IndexBuffer->UploadData(indices.data(), idxBytes);
}

void ShrubRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    if (!m_Initialized || !m_Renderer) return;

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout, colorAttachmentCount);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to recreate pipeline for render pass");
    }
}

void ShrubRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ShrubVertexShaderData),
        Renderer::ShaderData::ShrubVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to load shrub vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ShrubFragmentShaderData),
        Renderer::ShaderData::ShrubFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to load shrub fragment shader");
        return;
    }

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(f32) * 5;  // vec3 pos + vec2 uv
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
    config.renderPass = m_Renderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = false;
    config.colorAttachmentCount = 2; // MRT: color + velocity
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to create pipeline");
        m_Pipeline.reset();
    }
}

void ShrubRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    if (!m_VertexShader) {
        m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_VertexShader->LoadFromSPIRV(
            reinterpret_cast<const u8*>(Renderer::ShaderData::ShrubVertexShaderData),
            Renderer::ShaderData::ShrubVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to load shrub vertex shader");
            return;
        }
    }
    if (!m_FragmentShader) {
        m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_FragmentShader->LoadFromSPIRV(
            reinterpret_cast<const u8*>(Renderer::ShaderData::ShrubFragmentShaderData),
            Renderer::ShaderData::ShrubFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to load shrub fragment shader");
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
    config.colorAttachmentCount = colorAttachmentCount; // MRT: color + velocity
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: Failed to create pipeline");
        m_Pipeline.reset();
    }
}

void ShrubRenderer::Render(VkCommandBuffer commandBuffer,
                            const std::vector<VkDescriptorSet>& descriptorSets,
                            u32 currentFrame,
                            ECS::World* world,
                            u32 viewportWidth,
                            u32 viewportHeight) {
    if (!m_Initialized || !m_Pipeline || !world) return;

    bool hasBound = false;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ShrubVolumeComponent>()) {
        if (!world->HasComponent<ECS::TransformComponent>(entity)) continue;

        auto* shrub = world->GetComponent<ECS::ShrubVolumeComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!shrub || !transform) continue;
        if (!transform->visible) continue;

        if (!hasBound) {
            m_Pipeline->Bind(commandBuffer);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

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

            VkBuffer vertexBuffers[] = { m_VertexBuffer->GetBuffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

            hasBound = true;
        }

        // Build model matrix encoding position and half-extents (same as grass)
        Math::Matrix4 model = Math::Matrix4::Identity();
        model.m[0] = shrub->halfExtents.x;
        model.m[5] = 1.0f;
        model.m[10] = shrub->halfExtents.z;
        model.m[12] = transform->position.x;
        model.m[13] = transform->position.y;
        model.m[14] = transform->position.z;

        // Pack shrub parameters into push constants (same layout as grass)
        Renderer::PushConstants pc{};
        pc.model = model;
        pc.baseColor = shrub->baseColor;
        pc.emissiveColor = shrub->tipColor;
        pc.emissiveStrength = shrub->shrubHeight;
        pc.opacity = shrub->heightVariance;
        pc.alphaCutoff = shrub->width;
        pc.flags = static_cast<i32>(shrub->density);
        pc.parallaxScale = shrub->windSwayStrength;

        vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        vkCmdDrawIndexed(commandBuffer, m_IndexCount, shrub->density, 0, 0, 0);
    }
}

bool ShrubRenderer::ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return false;
    namespace fs = std::filesystem;

    std::string vertPath = (fs::path(shaderDir) / "shrub.vert").string();
    std::string fragPath = (fs::path(shaderDir) / "shrub.frag").string();

    std::string vertSrc, fragSrc;
    { std::ifstream f(vertPath); if (!f.is_open()) return false; vertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(fragPath); if (!f.is_open()) return false; fragSrc.assign(std::istreambuf_iterator<char>(f), {}); }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: shrub.vert compilation failed");
        return false;
    }
    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "ShrubRenderer: shrub.frag compilation failed");
        return false;
    }

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();
    m_VertexShader = std::move(tempVert);
    m_FragmentShader = std::move(tempFrag);
    CreatePipeline(sharedLayout);
    return m_Pipeline != nullptr;
}

} // namespace Effects
} // namespace Enjin
