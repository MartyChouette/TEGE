#include "Enjin/Effects/GrassRenderer.h"
#include "Enjin/Effects/VegetationTemplates.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

GrassRenderer::~GrassRenderer() {
    Shutdown();
}

bool GrassRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout,
                               VkDescriptorSetLayout bindlessLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;
    m_BindlessLayout = bindlessLayout;

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
        m_Renderer->GetContext()->WaitForGPU();
    }

    m_Pipeline.reset();
    m_VertexShader.reset();
    m_FragmentShader.reset();
    m_BladeIndexBuffer.reset();
    m_BladeVertexBuffer.reset();

    m_Initialized = false;
}

void GrassRenderer::CreateBladeMesh() {
    // Blade geometry is defined once in VegTemplates (shared with the RT path so
    // raster and ray tracing draw the identical silhouette). 5 floats/vertex.
    std::vector<Effects::VegTemplates::VegVertex> verts;
    std::vector<u32> indices;
    Effects::VegTemplates::BuildGrassBlade(verts, indices);

    m_BladeIndexCount = static_cast<u32>(indices.size());
    usize vtxBytes = verts.size() * sizeof(Effects::VegTemplates::VegVertex);
    usize idxBytes = indices.size() * sizeof(u32);

    m_BladeVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_BladeVertexBuffer->Create(vtxBytes, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to create blade vertex buffer");
        return;
    }
    m_BladeVertexBuffer->UploadData(verts.data(), vtxBytes);

    m_BladeIndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_BladeIndexBuffer->Create(idxBytes, Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to create blade index buffer");
        return;
    }
    m_BladeIndexBuffer->UploadData(indices.data(), idxBytes);
}

void GrassRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    if (!m_Initialized || !m_Renderer) return;

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout, colorAttachmentCount);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to recreate pipeline for render pass");
    }
}

void GrassRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    // The swapchain build. Clearing this keeps ReloadShaders honest
    // about which pass the live pipeline belongs to.
    m_LastRenderPass = VK_NULL_HANDLE;
    m_LastColorAttachmentCount = 2;
    // Only load the baked SPIR-V into an EMPTY slot. ReloadShaders puts the
    // freshly compiled GLSL here before calling us; overwriting it is what
    // made every shader hot reload silently do nothing while reporting
    // success.
    if (!m_VertexShader) {
        m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_VertexShader->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::GrassVertexShaderData),
                Renderer::ShaderData::GrassVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass vertex shader");
            // A failed load must leave the slot EMPTY, or the guard above
            // skips the retry on every later call.
            m_VertexShader.reset();
            return;
        }
    }

    // Only load the baked SPIR-V into an EMPTY slot. ReloadShaders puts the
    // freshly compiled GLSL here before calling us; overwriting it is what
    // made every shader hot reload silently do nothing while reporting
    // success.
    if (!m_FragmentShader) {
        m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_FragmentShader->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::GrassFragmentShaderData),
                Renderer::ShaderData::GrassFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass fragment shader");
            // A failed load must leave the slot EMPTY, or the guard above
            // skips the retry on every later call.
            m_FragmentShader.reset();
            return;
        }
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
    config.colorAttachmentCount = 2; // MRT: color + velocity
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (m_BindlessLayout != VK_NULL_HANDLE) m_Pipeline->SetBindlessLayout(m_BindlessLayout);
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to create grass pipeline");
        m_Pipeline.reset();
    }
}

void GrassRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    // Remember it, so a later ReloadShaders rebuilds against this pass
    // rather than blindly against the swapchain.
    m_LastRenderPass = renderPass;
    m_LastColorAttachmentCount = colorAttachmentCount;

    // Only load the baked SPIR-V into an EMPTY slot. ReloadShaders puts the
    // freshly compiled GLSL here before calling us; overwriting it is what
    // made every shader hot reload silently do nothing while reporting
    // success.
    if (!m_VertexShader) {
        m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_VertexShader->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::GrassVertexShaderData),
                Renderer::ShaderData::GrassVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass vertex shader");
            // A failed load must leave the slot EMPTY, or the guard above
            // skips the retry on every later call.
            m_VertexShader.reset();
            return;
        }
    }
    // Only load the baked SPIR-V into an EMPTY slot. ReloadShaders puts the
    // freshly compiled GLSL here before calling us; overwriting it is what
    // made every shader hot reload silently do nothing while reporting
    // success.
    if (!m_FragmentShader) {
        m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_FragmentShader->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::GrassFragmentShaderData),
                Renderer::ShaderData::GrassFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GrassRenderer: Failed to load grass fragment shader");
            // A failed load must leave the slot EMPTY, or the guard above
            // skips the retry on every later call.
            m_FragmentShader.reset();
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
    if (m_BindlessLayout != VK_NULL_HANDLE) m_Pipeline->SetBindlessLayout(m_BindlessLayout);
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
                            u32 viewportHeight,
                            bool mode2D,
                            VkDescriptorSet bindlessSet) {
    if (!m_Initialized || !m_Pipeline || !world) return;

    bool hasBound = false;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::GrassVolumeComponent>()) {
        if (!world->HasComponent<ECS::TransformComponent>(entity)) continue;

        auto* grass = world->GetComponent<ECS::GrassVolumeComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!grass || !transform) continue;
        if (!transform->visible) continue;

        if (!hasBound) {
            // Bind pipeline and shared resources once
            m_Pipeline->Bind(commandBuffer);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

            // Set 1: bindless textures for volumes with a custom texture
            if (m_BindlessLayout != VK_NULL_HANDLE && bindlessSet != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_Pipeline->GetLayout(), 1, 1, &bindlessSet, 0, nullptr);
            }

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
        pc.flags = static_cast<i32>(grass->density) | (mode2D ? (1 << 30) : 0);
        pc.parallaxScale = grass->windSwayStrength;
        i32 texIndex = grass->cachedTexIndex;
        if (m_BindlessLayout == VK_NULL_HANDLE || bindlessSet == VK_NULL_HANDLE || texIndex < 0) texIndex = -1;
        pc.surfaceParam1 = static_cast<f32>(texIndex);

        vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        // Draw instanced: blade mesh * density instances
        vkCmdDrawIndexed(commandBuffer, m_BladeIndexCount, grass->density, 0, 0, 0);
    }
}

bool GrassRenderer::ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return false;
    namespace fs = std::filesystem;

    std::string vertPath = (fs::path(shaderDir) / "grass.vert").string();
    std::string fragPath = (fs::path(shaderDir) / "grass.frag").string();

    std::string vertSrc, fragSrc;
    { std::ifstream f(vertPath); if (!f.is_open()) return false; vertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(fragPath); if (!f.is_open()) return false; fragSrc.assign(std::istreambuf_iterator<char>(f), {}); }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: grass.vert compilation failed");
        return false;
    }
    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "GrassRenderer: grass.frag compilation failed");
        return false;
    }

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();
    m_VertexShader = std::move(tempVert);
    m_FragmentShader = std::move(tempFrag);
    if (m_LastRenderPass != VK_NULL_HANDLE) {
        CreatePipelineWithPass(m_LastRenderPass, sharedLayout, m_LastColorAttachmentCount);
    } else {
        CreatePipeline(sharedLayout);
    }
    return m_Pipeline != nullptr;
}

} // namespace Effects
} // namespace Enjin
