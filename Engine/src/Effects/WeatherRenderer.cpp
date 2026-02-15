#include "Enjin/Effects/WeatherRenderer.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <array>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

WeatherRenderer::~WeatherRenderer() {
    Shutdown();
}

bool WeatherRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;

    CreateQuadBuffers();
    CreateInstanceBuffer();
    CreatePipeline(sharedLayout);

    if (!m_Pipeline || !m_QuadVertexBuffer || !m_QuadIndexBuffer || !m_InstanceBuffer) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to initialize resources");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "WeatherRenderer initialized");
    return true;
}

void WeatherRenderer::Shutdown() {
    if (!m_Initialized) return;

    if (m_Renderer && m_Renderer->GetContext()) {
        m_Renderer->GetContext()->WaitForGPU();
    }

    m_Pipeline.reset();
    m_VertexShader.reset();
    m_FragmentShader.reset();
    m_InstanceBuffer.reset();
    m_QuadIndexBuffer.reset();
    m_QuadVertexBuffer.reset();

    m_Initialized = false;
}

void WeatherRenderer::CreateQuadBuffers() {
    // Quad vertices: position (vec2) + UV (vec2) = 4 floats per vertex
    // Centered at origin, [-0.5, 0.5] range
    float quadVerts[] = {
        // pos.x,  pos.y,  uv.x,  uv.y
        -0.5f, -0.5f,  0.0f,  0.0f,  // bottom-left
         0.5f, -0.5f,  1.0f,  0.0f,  // bottom-right
         0.5f,  0.5f,  1.0f,  1.0f,  // top-right
        -0.5f,  0.5f,  0.0f,  1.0f,  // top-left
    };

    m_QuadVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadVertexBuffer->Create(sizeof(quadVerts), Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to create quad vertex buffer");
        return;
    }
    m_QuadVertexBuffer->UploadData(quadVerts, sizeof(quadVerts));

    // Quad indices (two triangles)
    u32 quadIndices[] = { 0, 1, 2, 2, 3, 0 };

    m_QuadIndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadIndexBuffer->Create(sizeof(quadIndices), Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to create quad index buffer");
        return;
    }
    m_QuadIndexBuffer->UploadData(quadIndices, sizeof(quadIndices));
}

void WeatherRenderer::CreateInstanceBuffer() {
    usize bufferSize = MAX_PARTICLES * sizeof(ParticleInstanceData);
    m_InstanceBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_InstanceBuffer->Create(bufferSize, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to create instance buffer");
    }
}

void WeatherRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return;

    // Wait for GPU to finish using the old pipeline
    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to recreate pipeline for render pass");
    }
}

void WeatherRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout) {
    // Load shaders
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ParticleVertexShaderData),
        Renderer::ShaderData::ParticleVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to load particle vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ParticleFragmentShaderData),
        Renderer::ShaderData::ParticleFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to load particle fragment shader");
        return;
    }

    // Custom vertex input: binding 0 = quad vertex, binding 1 = instance data
    std::array<VkVertexInputBindingDescription, 2> bindings{};
    // Binding 0: per-vertex quad data (pos + UV = 4 floats = 16 bytes)
    bindings[0].binding = 0;
    bindings[0].stride = sizeof(f32) * 4;
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    // Binding 1: per-instance data
    bindings[1].binding = 1;
    bindings[1].stride = sizeof(ParticleInstanceData);
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 6> attrs{};
    // Quad position (location 0)
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;
    // Quad UV (location 1)
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = sizeof(f32) * 2;
    // Instance world position (location 2)
    attrs[2].binding = 1;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = offsetof(ParticleInstanceData, position);
    // Instance size + alpha (location 3)
    attrs[3].binding = 1;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(ParticleInstanceData, size);
    // Instance stretch direction (location 4)
    attrs[4].binding = 1;
    attrs[4].location = 4;
    attrs[4].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[4].offset = offsetof(ParticleInstanceData, stretchDirX);
    // Instance stretch factor (location 5)
    attrs[5].binding = 1;
    attrs[5].location = 5;
    attrs[5].format = VK_FORMAT_R32_SFLOAT;
    attrs[5].offset = offsetof(ParticleInstanceData, stretch);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<u32>(bindings.size());
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    Renderer::PipelineConfig config;
    config.renderPass = renderPass;
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = false;   // Particles don't write depth
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = true;
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: Failed to create particle pipeline");
        m_Pipeline.reset();
    }
}

void WeatherRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    CreatePipelineWithPass(m_Renderer->GetRenderPass(), sharedLayout);
}

void WeatherRenderer::Render(VkCommandBuffer commandBuffer,
                              const std::vector<VkDescriptorSet>& descriptorSets,
                              u32 currentFrame,
                              const WeatherSystem& weather,
                              bool isRain,
                              u32 viewportWidth,
                              u32 viewportHeight) {
    if (!m_Initialized || !m_Pipeline) return;

    u32 particleCount = weather.GetActiveParticleCount();
    if (particleCount == 0) return;

    const auto& particles = weather.GetParticles();
    u32 instanceCount = (particleCount < MAX_PARTICLES) ? particleCount : MAX_PARTICLES;

    // Reduced motion: cut visible particles to 25%
    if (m_ReducedMotion) {
        instanceCount = instanceCount / 4;
        if (instanceCount == 0) return;
    }

    // Build instance data — filter out dead particles so we only upload live ones
    m_InstanceDataCache.resize(instanceCount);
    u32 liveCount = 0;
    for (u32 i = 0; i < instanceCount; ++i) {
        const auto& p = particles[i];
        if (p.lifetime <= 0.0f) continue;

        m_InstanceDataCache[liveCount].position = p.position;
        m_InstanceDataCache[liveCount].size = p.size * 2.0f;
        m_InstanceDataCache[liveCount].alpha = p.alpha;

        if (isRain && !m_ReducedMotion) {
            m_InstanceDataCache[liveCount].stretch = 6.0f;
            m_InstanceDataCache[liveCount].stretchDirX = 0.0f;
            m_InstanceDataCache[liveCount].stretchDirY = 1.0f;
        } else {
            m_InstanceDataCache[liveCount].stretch = 1.0f;
            m_InstanceDataCache[liveCount].stretchDirX = 0.0f;
            m_InstanceDataCache[liveCount].stretchDirY = 0.0f;
        }
        liveCount++;
    }

    if (liveCount == 0) return;

    // Upload only live particle data (avoids wasting bandwidth on dead entries)
    m_InstanceBuffer->UploadData(m_InstanceDataCache.data(), liveCount * sizeof(ParticleInstanceData));

    // Bind pipeline
    m_Pipeline->Bind(commandBuffer);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    // Set viewport and scissor (use override dimensions if provided, else swapchain)
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

    // Push constants for color
    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    if (isRain) {
        pc.baseColor = Math::Vector3(0.47f, 0.63f, 0.86f); // Bluish rain
        pc.metallic = 0.0f;  // Full fill (rain mode)
    } else {
        pc.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);    // White snow
        pc.metallic = 1.0f;  // Radial falloff (snow mode)
    }
    pc.opacity = 1.0f;
    pc.flags = 0;

    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    // Bind vertex buffers (quad = binding 0, instances = binding 1)
    VkBuffer vertexBuffers[] = { m_QuadVertexBuffer->GetBuffer(), m_InstanceBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, m_QuadIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    // Draw instanced: 6 indices per quad, liveCount instances
    vkCmdDrawIndexed(commandBuffer, 6, liveCount, 0, 0, 0);
}

bool WeatherRenderer::ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return false;
    namespace fs = std::filesystem;

    std::string vertPath = (fs::path(shaderDir) / "particle.vert").string();
    std::string fragPath = (fs::path(shaderDir) / "particle.frag").string();

    std::string vertSrc, fragSrc;
    { std::ifstream f(vertPath); if (!f.is_open()) return false; vertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(fragPath); if (!f.is_open()) return false; fragSrc.assign(std::istreambuf_iterator<char>(f), {}); }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: particle.vert compilation failed");
        return false;
    }
    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "WeatherRenderer: particle.frag compilation failed");
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
