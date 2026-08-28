#include "Enjin/Effects/ParticleRenderer.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <cmath>
#include <array>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

ParticleRenderer::~ParticleRenderer() {
    Shutdown();
}

bool ParticleRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;

    CreateQuadBuffers();
    CreateInstanceBuffer();
    CreatePipeline(sharedLayout);

    if (!m_Pipeline || !m_QuadVertexBuffer || !m_QuadIndexBuffer || !m_InstanceBuffer) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to initialize resources");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "ParticleRenderer initialized");
    return true;
}

void ParticleRenderer::Shutdown() {
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

void ParticleRenderer::CreateQuadBuffers() {
    // Quad vertices: position (vec2) + UV (vec2) = 4 floats per vertex
    float quadVerts[] = {
        -0.5f, -0.5f,  0.0f,  0.0f,
         0.5f, -0.5f,  1.0f,  0.0f,
         0.5f,  0.5f,  1.0f,  1.0f,
        -0.5f,  0.5f,  0.0f,  1.0f,
    };

    m_QuadVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadVertexBuffer->Create(sizeof(quadVerts), Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to create quad vertex buffer");
        return;
    }
    m_QuadVertexBuffer->UploadData(quadVerts, sizeof(quadVerts));

    u32 quadIndices[] = { 0, 1, 2, 2, 3, 0 };

    m_QuadIndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadIndexBuffer->Create(sizeof(quadIndices), Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to create quad index buffer");
        return;
    }
    m_QuadIndexBuffer->UploadData(quadIndices, sizeof(quadIndices));
}

void ParticleRenderer::CreateInstanceBuffer() {
    usize bufferSize = MAX_PARTICLES * sizeof(ParticleInstanceData);
    m_InstanceBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_InstanceBuffer->Create(bufferSize, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to create instance buffer");
    }
}

void ParticleRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    if (!m_Initialized || !m_Renderer) return;

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout, colorAttachmentCount);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to recreate pipeline for render pass");
    }
}

void ParticleRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    // Load same particle shaders used by WeatherRenderer
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ParticleVertexShaderData),
        Renderer::ShaderData::ParticleVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to load particle vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ParticleFragmentShaderData),
        Renderer::ShaderData::ParticleFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to load particle fragment shader");
        return;
    }

    // Custom vertex input: binding 0 = quad vertex, binding 1 = instance data
    std::array<VkVertexInputBindingDescription, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].stride = sizeof(f32) * 4;
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings[1].binding = 1;
    bindings[1].stride = sizeof(ParticleInstanceData);
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 7> attrs{};
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = sizeof(f32) * 2;
    attrs[2].binding = 1;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = offsetof(ParticleInstanceData, position);
    attrs[3].binding = 1;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(ParticleInstanceData, size);
    attrs[4].binding = 1;
    attrs[4].location = 4;
    attrs[4].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[4].offset = offsetof(ParticleInstanceData, stretchDirX);
    attrs[5].binding = 1;
    attrs[5].location = 5;
    attrs[5].format = VK_FORMAT_R32_SFLOAT;
    attrs[5].offset = offsetof(ParticleInstanceData, stretch);
    attrs[6].binding = 1;
    attrs[6].location = 6;
    attrs[6].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[6].offset = offsetof(ParticleInstanceData, color);

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
    config.colorAttachmentCount = colorAttachmentCount; // MRT: color + velocity
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: Failed to create particle pipeline");
        m_Pipeline.reset();
    }
}

void ParticleRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    CreatePipelineWithPass(m_Renderer->GetRenderPass(), sharedLayout);
}

void ParticleRenderer::Render(VkCommandBuffer commandBuffer,
                               const std::vector<VkDescriptorSet>& descriptorSets,
                               u32 currentFrame,
                               ECS::World* world,
                               u32 viewportWidth,
                               u32 viewportHeight,
                               const std::function<bool(const std::string&)>& bindTexture) {
    if (!m_Initialized || !m_Pipeline || !world) return;

    // First non-empty emitter texture — the art asset for the particle billboards.
    // (One texture for the pass; the common case is a single emitter. Mixed textures
    // would need an atlas like sprites.)
    std::string particleTexPath;

    // Gather all emitter pool particles into instance cache
    m_InstanceDataCache.clear();

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ParticleEmitterComponent>()) {
        if (!world->HasComponent<ECS::TransformComponent>(entity)) continue;

        auto* emitter = world->GetComponent<ECS::ParticleEmitterComponent>(entity);
        if (!emitter) continue;
        auto* transformPR = world->GetComponent<ECS::TransformComponent>(entity);
        if (transformPR && !transformPR->visible) continue;

        const auto& pool = emitter->pool;
        if (!pool.initialized || pool.activeCount == 0) continue;

        if (particleTexPath.empty() && !emitter->texturePath.empty()) particleTexPath = emitter->texturePath;

        const bool velocityStretch = emitter->renderMode == ECS::ParticleEmitterComponent::RenderMode::VelocityStretch;
        const f32 stretchScale = emitter->velocityStretchScale;

        for (u32 i = 0; i < pool.activeCount && m_InstanceDataCache.size() < MAX_PARTICLES; ++i) {
            const auto& p = pool.particles[i];
            ParticleInstanceData inst;
            inst.position = p.position;
            inst.size = p.size * 2.0f;
            inst.alpha = p.alpha;
            inst.color = p.color;

            if (velocityStretch && stretchScale > 0.0f) {
                f32 velLen = p.velocity.Length();
                inst.stretch = std::max(1.0f, velLen * stretchScale);
                // Normalize velocity for stretch direction (Y-up default if near zero)
                if (velLen > 0.001f) {
                    inst.stretchDirX = p.velocity.x / velLen;
                    inst.stretchDirY = p.velocity.y / velLen;
                } else {
                    inst.stretchDirX = 0.0f;
                    inst.stretchDirY = 1.0f;
                }
            } else {
                inst.stretch = 1.0f;
                inst.stretchDirX = 0.0f;
                inst.stretchDirY = 0.0f;
            }

            m_InstanceDataCache.push_back(inst);
        }
    }

    u32 instanceCount = static_cast<u32>(m_InstanceDataCache.size());
    if (instanceCount == 0) return;

    // Upload instance data
    m_InstanceBuffer->UploadData(m_InstanceDataCache.data(), instanceCount * sizeof(ParticleInstanceData));

    // Bind pipeline
    m_Pipeline->Bind(commandBuffer);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    // Set viewport and scissor
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

    // Bind the emitter art asset if there is one; flags bit 0 tells the shader to sample
    // it. Otherwise particles fall back to the built-in soft dot (metallic=1). Colour is
    // now per-particle (instance data), not the push-constant baseColor.
    bool hasTexture = false;
    if (!particleTexPath.empty() && bindTexture) hasTexture = bindTexture(particleTexPath);

    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    pc.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    pc.metallic = 1.0f;  // Radial falloff for soft particles (no-texture path)
    pc.opacity = 1.0f;
    pc.flags = hasTexture ? 1 : 0;

    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    // Bind vertex buffers (quad = binding 0, instances = binding 1)
    VkBuffer vertexBuffers[] = { m_QuadVertexBuffer->GetBuffer(), m_InstanceBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, m_QuadIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    // Draw instanced: 6 indices per quad, instanceCount instances
    vkCmdDrawIndexed(commandBuffer, 6, instanceCount, 0, 0, 0);
}

void ParticleRenderer::RenderElementalParticles(VkCommandBuffer commandBuffer,
                                                  const std::vector<VkDescriptorSet>& descriptorSets,
                                                  u32 currentFrame,
                                                  const ElementalSystem& elementalSystem,
                                                  u32 viewportWidth,
                                                  u32 viewportHeight) {
    if (!m_Initialized || !m_Pipeline) return;

    const auto& pool = elementalSystem.GetPool();
    if (pool.activeCount == 0) return;

    m_InstanceDataCache.clear();

    for (u32 i = 0; i < pool.activeCount && m_InstanceDataCache.size() < MAX_PARTICLES; ++i) {
        ParticleInstanceData inst;
        inst.position = pool.positions[i];
        inst.size = pool.sizes[i] * 2.0f;
        inst.color = Math::Vector3(1.0f, 0.8f, 0.6f);  // warm elemental tint (was push-constant baseColor)

        // Alpha from lifetime ratio and intensity
        f32 ageRatio = (pool.maxLifetimes[i] > 0.0f)
            ? pool.lifetimes[i] / pool.maxLifetimes[i]
            : 0.0f;
        inst.alpha = pool.intensities[i] * Math::Min(1.0f, ageRatio * 3.0f); // fade in fast, fade out at end

        // Velocity stretch for fast-moving particles (rain, debris)
        f32 velLen = pool.velocities[i].Length();
        if (velLen > 2.0f) {
            inst.stretch = Math::Min(4.0f, velLen * 0.3f);
            inst.stretchDirX = pool.velocities[i].x / velLen;
            inst.stretchDirY = pool.velocities[i].y / velLen;
        } else {
            inst.stretch = 1.0f;
            inst.stretchDirX = 0.0f;
            inst.stretchDirY = 0.0f;
        }

        m_InstanceDataCache.push_back(inst);
    }

    u32 instanceCount = static_cast<u32>(m_InstanceDataCache.size());
    if (instanceCount == 0) return;

    m_InstanceBuffer->UploadData(m_InstanceDataCache.data(), instanceCount * sizeof(ParticleInstanceData));

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

    // Color derived from dominant element — encoded via push constants baseColor
    // We render all elemental particles in one draw call with white base color;
    // per-particle tinting is handled by the existing particle shader's alpha/falloff
    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    pc.baseColor = Math::Vector3(1.0f, 0.8f, 0.6f); // warm tint for elemental
    pc.metallic = 1.0f;
    pc.opacity = 1.0f;
    pc.flags = 0;

    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    VkBuffer vertexBuffers[] = { m_QuadVertexBuffer->GetBuffer(), m_InstanceBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_QuadIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, 6, instanceCount, 0, 0, 0);
}

bool ParticleRenderer::ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return false;
    namespace fs = std::filesystem;

    std::string vertPath = (fs::path(shaderDir) / "particle.vert").string();
    std::string fragPath = (fs::path(shaderDir) / "particle.frag").string();

    std::string vertSrc, fragSrc;
    { std::ifstream f(vertPath); if (!f.is_open()) return false; vertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(fragPath); if (!f.is_open()) return false; fragSrc.assign(std::istreambuf_iterator<char>(f), {}); }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: particle.vert compilation failed");
        return false;
    }
    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "ParticleRenderer: particle.frag compilation failed");
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
