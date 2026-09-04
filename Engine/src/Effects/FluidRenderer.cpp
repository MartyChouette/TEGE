#include "Enjin/Effects/FluidRenderer.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

FluidRenderer::~FluidRenderer() {
    Shutdown();
}

bool FluidRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;

    CreateQuadBuffers();
    CreateInstanceBuffer();
    CreatePipeline(sharedLayout);

    if (!m_Pipeline || !m_QuadVertexBuffer || !m_QuadIndexBuffer || !m_InstanceBuffer) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to initialize resources");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "FluidRenderer initialized");
    return true;
}

void FluidRenderer::Shutdown() {
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

void FluidRenderer::CreateQuadBuffers() {
    // Quad vertices: position (vec2) + UV (vec2) = 4 floats per vertex
    float quadVerts[] = {
        -0.5f, -0.5f,  0.0f,  0.0f,
         0.5f, -0.5f,  1.0f,  0.0f,
         0.5f,  0.5f,  1.0f,  1.0f,
        -0.5f,  0.5f,  0.0f,  1.0f,
    };

    m_QuadVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadVertexBuffer->Create(sizeof(quadVerts), Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to create quad vertex buffer");
        return;
    }
    m_QuadVertexBuffer->UploadData(quadVerts, sizeof(quadVerts));

    u32 quadIndices[] = { 0, 1, 2, 2, 3, 0 };

    m_QuadIndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadIndexBuffer->Create(sizeof(quadIndices), Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to create quad index buffer");
        return;
    }
    m_QuadIndexBuffer->UploadData(quadIndices, sizeof(quadIndices));
}

void FluidRenderer::CreateInstanceBuffer() {
    usize bufferSize = MAX_FLUID_CELLS * sizeof(FluidCellInstanceData);
    m_InstanceBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_InstanceBuffer->Create(bufferSize, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to create instance buffer");
    }
}

void FluidRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    if (!m_Initialized || !m_Renderer) return;

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout, colorAttachmentCount);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to recreate pipeline for render pass");
    }
}

void FluidRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    // Remember it, so a later ReloadShaders rebuilds against this pass
    // rather than blindly against the swapchain.
    m_LastRenderPass = renderPass;
    m_LastColorAttachmentCount = colorAttachmentCount;
    // Use particle shaders as a base — fluid cells are similar billboard quads
    // Fluid shaders: same vertex layout as particles, but the fragment reads the
    // per-cell colour from the instance data instead of a flat white push constant,
    // so the Fluid Volume's Color knob actually shows. Reusing the particle shaders
    // routed the colour into an unused "stretch" attribute, so every fluid rendered white.
    // Only load the baked SPIR-V into an EMPTY slot. ReloadShaders puts the
    // freshly compiled GLSL here before calling us; overwriting it is what
    // made every shader hot reload silently do nothing while reporting
    // success.
    if (!m_VertexShader) {
        m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_VertexShader->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::FluidVertexShaderData),
                Renderer::ShaderData::FluidVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to load vertex shader");
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
                reinterpret_cast<const u8*>(Renderer::ShaderData::FluidFragmentShaderData),
                Renderer::ShaderData::FluidFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to load fragment shader");
            // A failed load must leave the slot EMPTY, or the guard above
            // skips the retry on every later call.
            m_FragmentShader.reset();
            return;
        }
    }

    // Custom vertex input: binding 0 = quad vertex, binding 1 = instance data
    std::array<VkVertexInputBindingDescription, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].stride = sizeof(f32) * 4;
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings[1].binding = 1;
    bindings[1].stride = sizeof(FluidCellInstanceData);
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    // Attributes: quad pos (loc 0), quad UV (loc 1), instance position (loc 2),
    // instance size+alpha mapped to ParticleInstanceData layout (loc 3), stretch dir (loc 4), stretch (loc 5)
    std::array<VkVertexInputAttributeDescription, 6> attrs{};
    // Quad position (vec2)
    attrs[0].binding = 0; attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = 0;
    // Quad UV (vec2)
    attrs[1].binding = 0; attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT; attrs[1].offset = sizeof(f32) * 2;
    // Instance position (vec3)
    attrs[2].binding = 1; attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = offsetof(FluidCellInstanceData, position);
    // Instance size + alpha mapped as vec2 (size, alpha) -> matches ParticleInstanceData (size, alpha)
    attrs[3].binding = 1; attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(FluidCellInstanceData, size);
    // colorR, colorG mapped as stretch dir (vec2) — unused by shader for fluid, but layout must match
    attrs[4].binding = 1; attrs[4].location = 4;
    attrs[4].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[4].offset = offsetof(FluidCellInstanceData, colorR);
    // colorB mapped as stretch (float) — unused by shader for fluid
    attrs[5].binding = 1; attrs[5].location = 5;
    attrs[5].format = VK_FORMAT_R32_SFLOAT;
    attrs[5].offset = offsetof(FluidCellInstanceData, colorB);

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
    config.depthWrite = false;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = true;
    config.colorAttachmentCount = colorAttachmentCount; // MRT: color + velocity
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: Failed to create pipeline");
        m_Pipeline.reset();
    }
}

void FluidRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    // The swapchain build. Clearing this keeps ReloadShaders honest
    // about which pass the live pipeline belongs to.
    m_LastRenderPass = VK_NULL_HANDLE;
    m_LastColorAttachmentCount = 2;
    CreatePipelineWithPass(m_Renderer->GetRenderPass(), sharedLayout);
}

void FluidRenderer::Render(VkCommandBuffer commandBuffer,
                            const std::vector<VkDescriptorSet>& descriptorSets,
                            u32 currentFrame,
                            ECS::World* world,
                            u32 viewportWidth,
                            u32 viewportHeight) {
    if (!m_Initialized || !m_Pipeline || !world || !m_Simulation) return;

    m_InstanceDataCache.clear();

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::FluidVolumeComponent>()) {
        auto* vol = world->GetComponent<ECS::FluidVolumeComponent>(entity);
        if (!vol || !vol->isActive || !vol->renderEnabled) continue;

        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!transform || !transform->visible) continue;

        const FluidGridData* grid = m_Simulation->GetGridData(entity);
        if (!grid || grid->N == 0) continue;

        u32 N = grid->N;
        bool is3D = grid->is3D;

        // Compute cell size in world space
        f32 cellSizeX = (vol->halfExtents.x * 2.0f) / N;
        f32 cellSizeY = (vol->halfExtents.y * 2.0f) / N;
        f32 cellSize = std::min(cellSizeX, cellSizeY);

        // Entity world position is the center of the volume
        Math::Vector3 origin(
            transform->position.x - vol->halfExtents.x,
            transform->position.y - vol->halfExtents.y,
            transform->position.z - vol->halfExtents.z);

        if (!is3D) {
            for (u32 j = 1; j <= N && m_InstanceDataCache.size() < MAX_FLUID_CELLS; ++j) {
                for (u32 i = 1; i <= N && m_InstanceDataCache.size() < MAX_FLUID_CELLS; ++i) {
                    f32 d = grid->density[grid->IX(i, j)];
                    if (d < vol->densityThreshold) continue;

                    FluidCellInstanceData inst;
                    inst.position.x = origin.x + (static_cast<f32>(i) - 0.5f) * cellSizeX;
                    inst.position.y = origin.y + (static_cast<f32>(j) - 0.5f) * cellSizeY;
                    inst.position.z = transform->position.z;
                    inst.size = cellSize;
                    f32 clamped = std::min(d, 1.0f);
                    inst.colorR = vol->fluidColor.x;
                    inst.colorG = vol->fluidColor.y;
                    inst.colorB = vol->fluidColor.z;
                    inst.alpha = std::min(d, 1.0f) * vol->opacity;

                    m_InstanceDataCache.push_back(inst);
                }
            }
        } else {
            f32 cellSizeZ = (vol->halfExtents.z * 2.0f) / N;
            cellSize = std::min(cellSize, cellSizeZ);

            for (u32 k = 1; k <= N && m_InstanceDataCache.size() < MAX_FLUID_CELLS; ++k) {
                for (u32 j = 1; j <= N && m_InstanceDataCache.size() < MAX_FLUID_CELLS; ++j) {
                    for (u32 i = 1; i <= N && m_InstanceDataCache.size() < MAX_FLUID_CELLS; ++i) {
                        f32 d = grid->density[grid->IX3(i, j, k)];
                        if (d < vol->densityThreshold) continue;

                        FluidCellInstanceData inst;
                        inst.position.x = origin.x + (static_cast<f32>(i) - 0.5f) * cellSizeX;
                        inst.position.y = origin.y + (static_cast<f32>(j) - 0.5f) * cellSizeY;
                        inst.position.z = origin.z + (static_cast<f32>(k) - 0.5f) * cellSizeZ;
                        inst.size = cellSize;
                        f32 clamped = std::min(d, 1.0f);
                        inst.colorR = vol->fluidColor.x;
                        inst.colorG = vol->fluidColor.y;
                        inst.colorB = vol->fluidColor.z;
                        inst.alpha = std::min(d, 1.0f) * vol->opacity;

                        m_InstanceDataCache.push_back(inst);
                    }
                }
            }
        }
    }

    u32 instanceCount = static_cast<u32>(m_InstanceDataCache.size());
    if (instanceCount == 0) return;

    m_InstanceBuffer->UploadData(m_InstanceDataCache.data(), instanceCount * sizeof(FluidCellInstanceData));

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

    // Push constants — fluid color comes from per-instance data, use white base + radial falloff
    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    pc.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    pc.metallic = 0.0f;  // No radial falloff — fluid cells are flat-filled quads
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

bool FluidRenderer::ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return false;
    namespace fs = std::filesystem;

    // Fluid renderer reuses particle shaders, so reload those
    std::string vertPath = (fs::path(shaderDir) / "particle.vert").string();
    std::string fragPath = (fs::path(shaderDir) / "particle.frag").string();

    std::string vertSrc, fragSrc;
    { std::ifstream f(vertPath); if (!f.is_open()) return false; vertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(fragPath); if (!f.is_open()) return false; fragSrc.assign(std::istreambuf_iterator<char>(f), {}); }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: vertex shader compilation failed");
        return false;
    }
    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "FluidRenderer: fragment shader compilation failed");
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
