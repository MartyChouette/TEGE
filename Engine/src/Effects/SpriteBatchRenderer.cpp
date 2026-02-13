#include "Enjin/Effects/SpriteBatchRenderer.h"
#include "Enjin/Effects/SpriteTextureAtlas.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Effects {

SpriteBatchRenderer::~SpriteBatchRenderer() {
    Shutdown();
}

bool SpriteBatchRenderer::Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout) {
    if (m_Initialized) return true;

    m_Renderer = renderer;

    CreateQuadBuffers();
    CreateInstanceBuffer();
    CreatePipeline(sharedLayout);
    CreateLitPipeline(sharedLayout);

    if (!m_Pipeline || !m_QuadVertexBuffer || !m_QuadIndexBuffer || !m_InstanceBuffer) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to initialize resources");
        Shutdown();
        return false;
    }

    if (!m_LitPipeline) {
        ENJIN_LOG_WARN(Renderer, "SpriteBatchRenderer: Lit pipeline failed to create, lit mode unavailable");
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "SpriteBatchRenderer initialized");
    return true;
}

void SpriteBatchRenderer::Shutdown() {
    if (!m_Initialized) return;

    if (m_Renderer && m_Renderer->GetContext()) {
        vkDeviceWaitIdle(m_Renderer->GetContext()->GetDevice());
    }

    m_LitPipeline.reset();
    m_LitVertexShader.reset();
    m_LitFragmentShader.reset();
    m_Pipeline.reset();
    m_VertexShader.reset();
    m_FragmentShader.reset();
    m_InstanceBuffer.reset();
    m_QuadIndexBuffer.reset();
    m_QuadVertexBuffer.reset();

    m_Initialized = false;
}

void SpriteBatchRenderer::CreateQuadBuffers() {
    // Quad vertices: position (vec2) + UV (vec2) = 4 floats per vertex
    float quadVerts[] = {
        -0.5f, -0.5f,  0.0f,  0.0f,
         0.5f, -0.5f,  1.0f,  0.0f,
         0.5f,  0.5f,  1.0f,  1.0f,
        -0.5f,  0.5f,  0.0f,  1.0f,
    };

    m_QuadVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadVertexBuffer->Create(sizeof(quadVerts), Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to create quad vertex buffer");
        return;
    }
    m_QuadVertexBuffer->UploadData(quadVerts, sizeof(quadVerts));

    u32 quadIndices[] = { 0, 1, 2, 2, 3, 0 };

    m_QuadIndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_QuadIndexBuffer->Create(sizeof(quadIndices), Renderer::BufferUsage::Index, true)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to create quad index buffer");
        return;
    }
    m_QuadIndexBuffer->UploadData(quadIndices, sizeof(quadIndices));
}

void SpriteBatchRenderer::CreateInstanceBuffer() {
    usize bufferSize = MAX_SPRITES * sizeof(SpriteInstanceData);
    m_InstanceBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Renderer->GetContext());
    if (!m_InstanceBuffer->Create(bufferSize, Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to create instance buffer");
    }
}

void SpriteBatchRenderer::RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return;

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();
    m_LitPipeline.reset();

    CreatePipelineWithPass(renderPass, sharedLayout);
    CreateLitPipelineWithPass(renderPass, sharedLayout);
    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to recreate pipeline for render pass");
    }
}

void SpriteBatchRenderer::CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout) {
    // Load sprite shaders
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SpriteVertexShaderData),
        Renderer::ShaderData::SpriteVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to load sprite vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SpriteFragmentShaderData),
        Renderer::ShaderData::SpriteFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to load sprite fragment shader");
        return;
    }

    // Custom vertex input: binding 0 = quad vertex, binding 1 = instance data
    std::array<VkVertexInputBindingDescription, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].stride = sizeof(f32) * 4;
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings[1].binding = 1;
    bindings[1].stride = sizeof(SpriteInstanceData);
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    // 8 vertex attributes: 2 from quad (binding 0), 6 from instance (binding 1)
    std::array<VkVertexInputAttributeDescription, 8> attrs{};

    // Binding 0: quad vertex data
    // location 0: quad position (vec2)
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;

    // location 1: quad UV (vec2)
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = sizeof(f32) * 2;

    // Binding 1: per-instance sprite data
    // location 2: world position (vec3)
    attrs[2].binding = 1;
    attrs[2].location = 2;
    attrs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[2].offset = offsetof(SpriteInstanceData, position);

    // location 3: size (vec2 = sizeX, sizeY)
    attrs[3].binding = 1;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(SpriteInstanceData, sizeX);

    // location 4: rotation (float, radians)
    attrs[4].binding = 1;
    attrs[4].location = 4;
    attrs[4].format = VK_FORMAT_R32_SFLOAT;
    attrs[4].offset = offsetof(SpriteInstanceData, rotation);

    // location 5: UV rect (vec4 = left, top, right, bottom)
    attrs[5].binding = 1;
    attrs[5].location = 5;
    attrs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[5].offset = offsetof(SpriteInstanceData, uvLeft);

    // location 6: tint color + alpha (vec4 = r, g, b, a)
    attrs[6].binding = 1;
    attrs[6].location = 6;
    attrs[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[6].offset = offsetof(SpriteInstanceData, tintR);

    // location 7: flip flags (uint)
    attrs[7].binding = 1;
    attrs[7].location = 7;
    attrs[7].format = VK_FORMAT_R32_UINT;
    attrs[7].offset = offsetof(SpriteInstanceData, flipFlags);

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
    config.depthWrite = false;   // Sprites don't write depth (same as particles)
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = true;
    config.customVertexInput = &vertexInput;

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_Pipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to create sprite pipeline");
        m_Pipeline.reset();
    }
}

void SpriteBatchRenderer::CreatePipeline(VkDescriptorSetLayout sharedLayout) {
    CreatePipelineWithPass(m_Renderer->GetRenderPass(), sharedLayout);
}

void SpriteBatchRenderer::CreateLitPipeline(VkDescriptorSetLayout sharedLayout) {
    CreateLitPipelineWithPass(m_Renderer->GetRenderPass(), sharedLayout);
}

void SpriteBatchRenderer::CreateLitPipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout) {
    // Load lit sprite shaders
    m_LitVertexShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_LitVertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SpriteLitVertexShaderData),
        Renderer::ShaderData::SpriteLitVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to load lit sprite vertex shader");
        return;
    }

    m_LitFragmentShader = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!m_LitFragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SpriteLitFragmentShaderData),
        Renderer::ShaderData::SpriteLitFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to load lit sprite fragment shader");
        return;
    }

    // Same vertex input as unlit pipeline (same SpriteInstanceData struct)
    std::array<VkVertexInputBindingDescription, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].stride = sizeof(f32) * 4;
    bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindings[1].binding = 1;
    bindings[1].stride = sizeof(SpriteInstanceData);
    bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 8> attrs{};

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
    attrs[2].offset = offsetof(SpriteInstanceData, position);

    attrs[3].binding = 1;
    attrs[3].location = 3;
    attrs[3].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[3].offset = offsetof(SpriteInstanceData, sizeX);

    attrs[4].binding = 1;
    attrs[4].location = 4;
    attrs[4].format = VK_FORMAT_R32_SFLOAT;
    attrs[4].offset = offsetof(SpriteInstanceData, rotation);

    attrs[5].binding = 1;
    attrs[5].location = 5;
    attrs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[5].offset = offsetof(SpriteInstanceData, uvLeft);

    attrs[6].binding = 1;
    attrs[6].location = 6;
    attrs[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attrs[6].offset = offsetof(SpriteInstanceData, tintR);

    attrs[7].binding = 1;
    attrs[7].location = 7;
    attrs[7].format = VK_FORMAT_R32_UINT;
    attrs[7].offset = offsetof(SpriteInstanceData, flipFlags);

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
    config.customVertexInput = &vertexInput;

    m_LitPipeline = std::make_unique<Renderer::VulkanPipeline>(m_Renderer->GetContext());
    if (!m_LitPipeline->CreateWithLayout(config, m_LitVertexShader.get(), m_LitFragmentShader.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: Failed to create lit sprite pipeline");
        m_LitPipeline.reset();
    }
}

void SpriteBatchRenderer::Render(VkCommandBuffer commandBuffer,
                                  const std::vector<VkDescriptorSet>& descriptorSets,
                                  u32 currentFrame,
                                  ECS::World* world,
                                  const std::function<void(const std::string& texturePath)>& textureBindCallback,
                                  u32 viewportWidth,
                                  u32 viewportHeight,
                                  bool litMode) {
    if (!m_Initialized || !m_Pipeline || !world) return;

    // Select active pipeline: lit (2.5D Blinn-Phong) or unlit (flat 2D)
    Renderer::VulkanPipeline* activePipeline = m_Pipeline.get();
    if (litMode && m_LitPipeline) {
        activePipeline = m_LitPipeline.get();
    }

    // Collect all visible Sprite2DComponent entities (skip tilemaps)
    struct SpriteEntry {
        ECS::Entity entity;
        i32 sortingLayer;
        i32 orderInLayer;
        const ECS::Sprite2DComponent* sprite;
        const AtlasRegion* cachedAtlasRegion;  // Cached to avoid double GetRegion() lookup
        bool isAtlased;  // Precomputed to avoid redundant GetRegion() in sort comparator
    };

    std::vector<SpriteEntry> sortedSprites;
    sortedSprites.reserve(world->GetEntitiesWithComponent<ECS::Sprite2DComponent>().size());

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::Sprite2DComponent>()) {
        // Skip invisible entities
        auto* xformBatch = world->GetComponent<ECS::TransformComponent>(entity);
        if (!xformBatch || !xformBatch->visible) continue;
        // Skip entities that also have TilemapComponent (those are rendered separately)
        if (world->HasComponent<ECS::TilemapComponent>(entity)) continue;

        auto* sprite = world->GetComponent<ECS::Sprite2DComponent>(entity);
        if (!sprite || !sprite->visible) continue;

        SpriteEntry entry;
        entry.entity = entity;
        entry.sortingLayer = sprite->sortingLayer;
        entry.orderInLayer = sprite->orderInLayer;
        entry.sprite = sprite;
        entry.cachedAtlasRegion = m_Atlas ? m_Atlas->GetRegion(sprite->texturePath) : nullptr;
        entry.isAtlased = entry.cachedAtlasRegion != nullptr;
        sortedSprites.push_back(entry);
    }

    if (sortedSprites.empty()) return;

    // Sort by sorting layer (ascending), then order in layer (ascending)
    // Tertiary sort groups atlased sprites together (empty key sorts first)
    std::sort(sortedSprites.begin(), sortedSprites.end(),
        [](const SpriteEntry& a, const SpriteEntry& b) {
            if (a.sortingLayer != b.sortingLayer)
                return a.sortingLayer < b.sortingLayer;
            if (a.orderInLayer != b.orderInLayer)
                return a.orderInLayer < b.orderInLayer;
            // Tertiary sort by effective texture key to maximize batching within same layer
            // Atlased sprites use precomputed flag so they group together
            if (a.isAtlased != b.isAtlased) return a.isAtlased;  // Atlased first
            if (a.isAtlased) return false;  // Both atlased — equal
            return a.sprite->texturePath < b.sprite->texturePath;
        });

    // Resolve viewport extent
    VkExtent2D extent;
    if (viewportWidth > 0 && viewportHeight > 0) {
        extent.width = viewportWidth;
        extent.height = viewportHeight;
    } else {
        extent = m_Renderer->GetSwapchainExtent();
    }
    if (extent.width == 0 || extent.height == 0) return;

    // Build instance data and batch by texture path
    m_InstanceDataCache.clear();
    m_InstanceDataCache.reserve(sortedSprites.size());
    std::string currentTexture;
    u32 batchStart = 0;

    // Bind pipeline, descriptor set, viewport, scissor, push constants, and buffers ONCE
    activePipeline->Bind(commandBuffer);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        activePipeline->GetLayout(), 0, 1, &descriptorSets[currentFrame], 0, nullptr);

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
    pc.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    pc.metallic = 0.0f;
    pc.opacity = 1.0f;
    pc.flags = 0;
    vkCmdPushConstants(commandBuffer, activePipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    VkBuffer vertexBuffers[] = { m_QuadVertexBuffer->GetBuffer(), m_InstanceBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_QuadIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    // Lambda to flush the current batch as an instanced draw call
    auto flushBatch = [&](u32 batchEnd) {
        u32 count = batchEnd - batchStart;
        if (count == 0) return;

        // Upload this batch's instance data
        m_InstanceBuffer->UploadData(
            m_InstanceDataCache.data() + batchStart,
            count * sizeof(SpriteInstanceData));

        // Bind the texture for this batch (only per-batch state that changes)
        if (textureBindCallback) {
            textureBindCallback(currentTexture);
        }

        // Re-bind instance buffer after upload (data may have been re-uploaded)
        VkBuffer instanceBufs[] = { m_QuadVertexBuffer->GetBuffer(), m_InstanceBuffer->GetBuffer() };
        VkDeviceSize instanceOffsets[] = { 0, 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, instanceBufs, instanceOffsets);

        // Draw instanced: 6 indices per quad, count instances
        vkCmdDrawIndexed(commandBuffer, 6, count, 0, 0, 0);

        batchStart = batchEnd;
    };

    for (const auto& entry : sortedSprites) {
        const auto* sprite = entry.sprite;
        const auto* transform = world->GetComponent<ECS::TransformComponent>(entry.entity);
        if (!transform) continue;

        // Determine effective texture for batching — atlased sprites share "__atlas__" key
        // Reuse the cached atlas region from the sort phase (avoids second GetRegion() lookup)
        const AtlasRegion* atlasRegion = entry.cachedAtlasRegion;
        static const std::string kAtlasSentinel("__atlas__");
        const std::string& effectiveKey = atlasRegion ? kAtlasSentinel : sprite->texturePath;

        // Flush when effective texture changes (but not on the first sprite)
        if (effectiveKey != currentTexture && m_InstanceDataCache.size() > batchStart) {
            flushBatch(static_cast<u32>(m_InstanceDataCache.size()));
            currentTexture = effectiveKey;
        } else if (currentTexture.empty()) {
            currentTexture = effectiveKey;
        }

        // Build instance data from sprite + transform
        SpriteInstanceData inst{};
        inst.position = transform->position;
        inst.sizeX = sprite->size.x;
        inst.sizeY = sprite->size.y;

        // Extract Z-axis rotation from quaternion (radians) — avoids full Euler decomposition
        inst.rotation = transform->rotation.GetRotationZ();

        // Compute UV coordinates from sprite source rectangle
        if (sprite->srcWidth > 0 && sprite->srcHeight > 0 &&
            sprite->texPixelWidth > 0 && sprite->texPixelHeight > 0) {
            inst.uvLeft   = sprite->srcX / sprite->texPixelWidth;
            inst.uvTop    = sprite->srcY / sprite->texPixelHeight;
            inst.uvRight  = (sprite->srcX + sprite->srcWidth) / sprite->texPixelWidth;
            inst.uvBottom = (sprite->srcY + sprite->srcHeight) / sprite->texPixelHeight;
        } else {
            // Use full texture
            inst.uvLeft   = 0.0f;
            inst.uvTop    = 0.0f;
            inst.uvRight  = 1.0f;
            inst.uvBottom = 1.0f;
        }

        // Remap UVs into atlas region if this sprite is atlased
        if (atlasRegion) {
            f32 rw = atlasRegion->uvRight - atlasRegion->uvLeft;
            f32 rh = atlasRegion->uvBottom - atlasRegion->uvTop;
            inst.uvLeft   = atlasRegion->uvLeft + inst.uvLeft * rw;
            inst.uvTop    = atlasRegion->uvTop  + inst.uvTop  * rh;
            inst.uvRight  = atlasRegion->uvLeft + inst.uvRight * rw;
            inst.uvBottom = atlasRegion->uvTop  + inst.uvBottom * rh;
        }

        // Tint color and alpha
        inst.tintR = sprite->tint.x;
        inst.tintG = sprite->tint.y;
        inst.tintB = sprite->tint.z;
        inst.tintA = sprite->alpha;

        // Flip flags
        inst.flipFlags = (sprite->flipX ? 1u : 0u) | (sprite->flipY ? 2u : 0u);

        m_InstanceDataCache.push_back(inst);

        // If we hit the max sprite limit, flush immediately
        if (m_InstanceDataCache.size() >= MAX_SPRITES) {
            flushBatch(static_cast<u32>(m_InstanceDataCache.size()));
            m_InstanceDataCache.clear();
            batchStart = 0;
        }
    }

    // Flush remaining sprites
    flushBatch(static_cast<u32>(m_InstanceDataCache.size()));
}

bool SpriteBatchRenderer::ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout) {
    if (!m_Initialized || !m_Renderer) return false;
    namespace fs = std::filesystem;

    // Compile unlit shaders
    std::string vertPath = (fs::path(shaderDir) / "sprite.vert").string();
    std::string fragPath = (fs::path(shaderDir) / "sprite.frag").string();
    std::string litVertPath = (fs::path(shaderDir) / "sprite_lit.vert").string();
    std::string litFragPath = (fs::path(shaderDir) / "sprite_lit.frag").string();

    std::string vertSrc, fragSrc, litVertSrc, litFragSrc;
    { std::ifstream f(vertPath); if (!f.is_open()) return false; vertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(fragPath); if (!f.is_open()) return false; fragSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(litVertPath); if (!f.is_open()) return false; litVertSrc.assign(std::istreambuf_iterator<char>(f), {}); }
    { std::ifstream f(litFragPath); if (!f.is_open()) return false; litFragSrc.assign(std::istreambuf_iterator<char>(f), {}); }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: sprite.vert compilation failed");
        return false;
    }
    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: sprite.frag compilation failed");
        return false;
    }
    auto tempLitVert = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempLitVert->CompileFromGLSL(litVertSrc, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: sprite_lit.vert compilation failed");
        return false;
    }
    auto tempLitFrag = std::make_unique<Renderer::VulkanShader>(m_Renderer->GetContext());
    if (!tempLitFrag->CompileFromGLSL(litFragSrc, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "SpriteBatchRenderer: sprite_lit.frag compilation failed");
        return false;
    }

    m_Renderer->WaitForAllFrames();
    m_Pipeline.reset();
    m_LitPipeline.reset();
    m_VertexShader = std::move(tempVert);
    m_FragmentShader = std::move(tempFrag);
    m_LitVertexShader = std::move(tempLitVert);
    m_LitFragmentShader = std::move(tempLitFrag);
    CreatePipeline(sharedLayout);
    CreateLitPipeline(sharedLayout);
    return m_Pipeline != nullptr && m_LitPipeline != nullptr;
}

} // namespace Effects
} // namespace Enjin
