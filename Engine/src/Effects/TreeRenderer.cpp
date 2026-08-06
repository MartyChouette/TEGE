#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Effects/VegetationTemplates.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cstring>
#include <cmath>
#include <string>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

TreeRenderer::~TreeRenderer() {
    Shutdown();
}

bool TreeRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;

    CreateTreeMesh();
    CreatePipeline(sharedLayout);

    if (!m_Pipeline || !m_VertexBuffer || !m_IndexBuffer) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to initialize resources");
        Shutdown();
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "TreeRenderer initialized");
    return true;
}

void TreeRenderer::Shutdown() {
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

void TreeRenderer::CreateTreeMesh() {
    // Trunk: 2 intersecting tapered quads (8 verts, 12 indices)
    // Canopy: 3 intersecting quads (12 verts, 18 indices)
    // Total: 20 verts, 30 indices
    // UV.y < 0.5 = trunk, >= 0.5 = canopy (for color selection in shader)

    // Tree geometry (tapered trunk + diamond canopy) is defined once in
    // VegTemplates, shared with the RT path. tree.vert scales trunk (uv.y < 0.5)
    // and canopy (uv.y >= 0.5) by the volume's params in-shader. 5 floats/vertex.
    std::vector<Effects::VegTemplates::VegVertex> verts;
    std::vector<u32> indices;
    Effects::VegTemplates::BuildTree(verts, indices);

    m_IndexCount = static_cast<u32>(indices.size());
    usize vtxBytes = verts.size() * sizeof(Effects::VegTemplates::VegVertex);
    usize idxBytes = indices.size() * sizeof(u32);

    m_VertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_VertexBuffer->Create(vtxBytes, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to create vertex buffer");
        return;
    }
    m_VertexBuffer->UploadData(verts.data(), vtxBytes);

    m_IndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_IndexBuffer->Create(idxBytes, Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to create index buffer");
        return;
    }
    m_IndexBuffer->UploadData(indices.data(), idxBytes);
}

void TreeRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    if (!m_Initialized || !m_Renderer) return;

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout, colorAttachmentCount);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to recreate pipeline for render pass");
    }
}

void TreeRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TreeVertexShaderData),
        Renderer::ShaderData::TreeVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to load tree vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TreeFragmentShaderData),
        Renderer::ShaderData::TreeFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to load tree fragment shader");
        return;
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
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to create pipeline");
        m_Pipeline.reset();
    }
}

void TreeRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount) {
    if (!m_VertexShader) {
        m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_VertexShader->LoadFromSPIRV(
            reinterpret_cast<const u8*>(Renderer::ShaderData::TreeVertexShaderData),
            Renderer::ShaderData::TreeVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to load tree vertex shader");
            return;
        }
    }
    if (!m_FragmentShader) {
        m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
        if (!m_FragmentShader->LoadFromSPIRV(
            reinterpret_cast<const u8*>(Renderer::ShaderData::TreeFragmentShaderData),
            Renderer::ShaderData::TreeFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to load tree fragment shader");
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
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: Failed to create pipeline");
        m_Pipeline.reset();
    }
}

void TreeRenderer::SetSeasonState(Season season, f32 progress) {
    m_CurrentSeason = season;
    m_SeasonProgress = progress;
}

void TreeRenderer::Render(VkCommandBuffer commandBuffer,
                           const std::vector<VkDescriptorSet>& descriptorSets,
                           u32 currentFrame,
                           ECS::World* world,
                           u32 viewportWidth,
                           u32 viewportHeight) {
    if (!m_Initialized || !m_Pipeline || !world) return;

    bool hasBound = false;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::TreeVolumeComponent>()) {
        if (!world->HasComponent<ECS::TransformComponent>(entity)) continue;

        auto* tree = world->GetComponent<ECS::TreeVolumeComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!tree || !transform) continue;
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

        // Build model matrix encoding position and half-extents
        Math::Matrix4 model = Math::Matrix4::Identity();
        model.m[0] = tree->halfExtents.x;
        model.m[5] = 1.0f;
        model.m[10] = tree->halfExtents.z;
        model.m[12] = transform->position.x;
        model.m[13] = transform->position.y;
        model.m[14] = transform->position.z;

        // Compute seasonal canopy color and scale
        f32 canopyScale = 1.0f;
        Math::Vector3 canopyBase = tree->canopyBaseColor;
        Math::Vector3 canopyTip = tree->canopyTipColor;

        bool isDeciduous = (tree->treeType == ECS::TreeType::Deciduous);

        if (isDeciduous) {
            switch (m_CurrentSeason) {
                case Season::Spring:
                    canopyScale = 0.3f + 0.5f * m_SeasonProgress;  // Growing back
                    canopyBase = tree->springCanopyColor;
                    canopyTip = tree->springCanopyColor * 1.2f;
                    break;
                case Season::Summer:
                    canopyScale = 1.0f;
                    canopyBase = tree->summerCanopyColor;
                    canopyTip = tree->summerCanopyColor * 1.3f;
                    break;
                case Season::Fall: {
                    f32 p = m_SeasonProgress;
                    canopyScale = 1.0f - 0.7f * p;  // Thinning
                    // Lerp summer -> fall color
                    canopyBase.x = tree->summerCanopyColor.x + (tree->fallCanopyColor.x - tree->summerCanopyColor.x) * p;
                    canopyBase.y = tree->summerCanopyColor.y + (tree->fallCanopyColor.y - tree->summerCanopyColor.y) * p;
                    canopyBase.z = tree->summerCanopyColor.z + (tree->fallCanopyColor.z - tree->summerCanopyColor.z) * p;
                    canopyTip = canopyBase * 1.2f;
                    break;
                }
                case Season::Winter:
                    canopyScale = 0.0f;  // Bare branches
                    break;
            }
        } else {
            // Evergreen: stays full year-round, slight snow tint in winter
            canopyScale = 1.0f;
        }

        // Pack tree parameters into push constants
        Renderer::PushConstants pc{};
        pc.model = model;
        pc.baseColor = tree->trunkColor;
        pc.metallic = tree->trunkWidth;
        pc.emissiveColor = canopyTip;
        pc.roughness = tree->canopyRadius;
        pc.emissiveStrength = tree->trunkHeight;
        pc.opacity = tree->canopyOffset;
        pc.alphaCutoff = canopyBase.x;  // Pack canopyBase R
        pc.flags = static_cast<i32>(tree->density);
        pc.parallaxScale = tree->windSwayStrength;
        pc.surfaceParam1 = canopyBase.y;   // Pack canopyBase G
        pc.surfaceParam2 = canopyBase.z; // Pack canopyBase B
        pc.surfaceParam3 = canopyScale;  // seasonFactor

        vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        vkCmdDrawIndexed(commandBuffer, m_IndexCount, tree->density, 0, 0, 0);
    }
}

void TreeRenderer::GenerateColliders(ECS::World* world, ECS::Entity volumeEntity) {
    if (!world) return;

    auto* tree = world->GetComponent<ECS::TreeVolumeComponent>(volumeEntity);
    auto* transform = world->GetComponent<ECS::TransformComponent>(volumeEntity);
    if (!tree || !transform) return;

    Math::Vector3 volumeCenter = transform->position;
    f32 halfX = tree->halfExtents.x;
    f32 halfZ = tree->halfExtents.z;

    // Replicate the XorShift hash from the vertex shader on CPU
    auto cpuHash = [](u32 n) -> f32 {
        n = (n << 13u) ^ n;
        n = n * (n * n * 15731u + 789221u) + 1376312589u;
        return static_cast<f32>(n & 0x7fffffffu) / static_cast<f32>(0x7fffffff);
    };

    for (u32 i = 0; i < tree->density; ++i) {
        f32 px = cpuHash(i * 3u + 0u) * 2.0f - 1.0f;
        f32 pz = cpuHash(i * 3u + 1u) * 2.0f - 1.0f;
        f32 sizeVar = cpuHash(i * 3u + 2u) * 0.8f + 0.6f;

        Math::Vector3 treePos = volumeCenter + Math::Vector3(px * halfX, 0.0f, pz * halfZ);

        f32 tHeight = tree->trunkHeight * sizeVar;
        f32 tWidth = tree->trunkWidth * sizeVar;

        // Create a collider entity for each tree trunk
        ECS::Entity collider = world->CreateEntity();

        ECS::NameComponent nameComp;
        nameComp.name = "TreeTrunk_" + std::to_string(i);
        world->AddComponent<ECS::NameComponent>(collider, nameComp);

        ECS::TransformComponent xform;
        xform.position = treePos + Math::Vector3(0, tHeight * 0.5f, 0);
        world->AddComponent<ECS::TransformComponent>(collider, xform);

        ECS::BoxColliderComponent box;
        box.center = Math::Vector3(0, 0, 0);
        box.size = Math::Vector3(tWidth * 2.0f, tHeight, tWidth * 2.0f);
        box.isTrigger = false;
        world->AddComponent<ECS::BoxColliderComponent>(collider, box);
    }

    ENJIN_LOG_INFO(Renderer, "Generated %u tree trunk colliders", tree->density);
}

bool TreeRenderer::ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return false;
    namespace fs = std::filesystem;

    std::string vertPath = (fs::path(shaderDir) / "tree.vert").string();
    std::string fragPath = (fs::path(shaderDir) / "tree.frag").string();

    std::string vertSrc, fragSrc;
    { std::ifstream f(vertPath); if (!f.is_open()) return false; vertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(fragPath); if (!f.is_open()) return false; fragSrc.assign(std::istreambuf_iterator<char>(f), {}); }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: tree.vert compilation failed");
        return false;
    }
    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "TreeRenderer: tree.frag compilation failed");
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
