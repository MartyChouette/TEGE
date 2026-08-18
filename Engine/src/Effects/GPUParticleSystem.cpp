#include "Enjin/Effects/GPUParticleSystem.h"
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cstring>

#if !ENJIN_RENDERER_WEBGPU

namespace Enjin {
namespace Effects {

GPUParticleSystem::GPUParticleSystem(Renderer::VulkanContext* context)
    : m_Context(context) {}

GPUParticleSystem::~GPUParticleSystem() {
    Shutdown();
}

bool GPUParticleSystem::Initialize(const GPUEmitterConfig& config) {
    if (m_Initialized) return true;
    m_Config = config;

    if (!CreateBuffers()) return false;
    // Compute pipeline created lazily

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "GPUParticleSystem initialized: max %u particles (%.1f MB SSBO)",
                   m_Config.maxParticles,
                   static_cast<f32>(m_Config.maxParticles * sizeof(GPUParticle)) / (1024.0f * 1024.0f));
    return true;
}

void GPUParticleSystem::Shutdown() {
    if (!m_Initialized) return;
    DestroyResources();
    m_Initialized = false;
}

// std140 mirror of particle_simulate.comp's EmitterParams (binding 2). Each
// vec3 is followed by a scalar that packs into its .w lane.
struct EmitterParamsUBO {
    Math::Vector3 emitterPosition;  f32 deltaTime;           // 0..15
    Math::Vector3 emitterDirection; f32 emitterSpread;       // 16..31
    Math::Vector3 gravity;          f32 damping;             // 32..47
    Math::Vector3 windForce;        u32 maxParticles;        // 48..63
    Math::Vector4 startColor;                                // 64..79
    Math::Vector4 endColor;                                  // 80..95
    f32 startSize; f32 endSize; f32 maxLifetime; f32 spawnRate;             // 96..111
    f32 turbulenceStrength; f32 turbulenceFrequency; u32 frameNumber; u32 colliderCount; // 112..127
};
static_assert(sizeof(EmitterParamsUBO) == 128, "must match particle_simulate.comp EmitterParams");

void GPUParticleSystem::Simulate(VkCommandBuffer cmd, f32 deltaTime, u32 frameNumber,
                                  const Math::Vector3& windForce,
                                  const std::vector<ParticleColliderShape>* colliders) {
    if (!m_Initialized) return;

    // Idle gate. Nothing spawns GPU particles yet (no ECS emitter wiring, no
    // caller of Spawn), and Render() has no draw path. Dispatching the sim over
    // maxParticles (65536) every frame on every scene was pure waste. Stay
    // dormant until something actually spawns; the plumbing below is correct
    // and ready the moment a spawn source exists.
    if (!m_HasSpawned) {
        if (!m_LoggedDormant) {
            m_LoggedDormant = true;
            ENJIN_LOG_INFO(Renderer,
                "GPUParticleSystem: dormant — no spawn source wired (Spawn has no callers) and no "
                "render path. Simulation UBO/dispatch are ready; the system runs once particles spawn.");
        }
        return;
    }

    // Lazy pipeline creation
    if (!m_PipelineCreated) {
        using BT = Renderer::BindType;
        // particle_simulate.comp bindings:
        // 0=SSBO(particles), 1=SSBO(aliveIndexBuffer), 2=UBO(emitterParams)
        m_SimulateSetup.Create(m_Context, {
            {0, BT::StorageBuffer}, {1, BT::StorageBuffer}, {2, BT::UniformBuffer},
            {3, BT::StorageBuffer}
        }, "particle_simulate.comp");

        if (!m_ColliderBuffer) {
            m_ColliderBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Context);
            m_ColliderBuffer->Create(kMaxParticleColliders * sizeof(ParticleColliderShape),
                                     Renderer::BufferUsage::Storage, true);
        }

        VkDevice device = m_Context->GetDevice();
        if (m_SimulateSetup.IsValid()) {
            m_SimulateSetup.WriteBuffer(device, 0, m_ParticleBuffer->GetBuffer(),
                                         m_Config.maxParticles * sizeof(GPUParticle));
            usize aliveSize = sizeof(u32) + m_Config.maxParticles * sizeof(u32);
            m_SimulateSetup.WriteBuffer(device, 1, m_AliveIndexBuffer->GetBuffer(), aliveSize);
            m_SimulateSetup.WriteBuffer(device, 2, m_EmitterParamsUBO->GetBuffer(), sizeof(EmitterParamsUBO),
                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            m_SimulateSetup.WriteBuffer(device, 3, m_ColliderBuffer->GetBuffer(),
                                         kMaxParticleColliders * sizeof(ParticleColliderShape));
        }
        m_PipelineCreated = true;
    }

    if (!m_SimulateSetup.IsValid()) return;

    // Reset alive count to 0 (atomic counter at offset 0 in alive buffer)
    vkCmdFillBuffer(cmd, m_AliveIndexBuffer->GetBuffer(), 0, sizeof(u32), 0);
    VkMemoryBarrier fillBarrier{};
    fillBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    fillBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    fillBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &fillBarrier, 0, nullptr, 0, nullptr);

    // Upload this frame's emitter params (host-visible UBO).
    if (m_EmitterParamsUBO) {
        EmitterParamsUBO params{};
        params.emitterPosition     = m_Config.position;
        params.deltaTime           = deltaTime;
        params.emitterDirection    = m_Config.direction;
        params.emitterSpread       = m_Config.spread;
        params.gravity             = m_Config.gravity;
        params.damping             = m_Config.damping;
        params.windForce           = windForce;
        params.maxParticles        = m_Config.maxParticles;
        params.startColor          = m_Config.startColor;
        params.endColor            = m_Config.endColor;
        params.startSize           = m_Config.startSize;
        params.endSize             = m_Config.endSize;
        params.maxLifetime         = m_Config.maxLifetime;
        params.spawnRate           = m_Config.spawnRate;
        params.turbulenceStrength  = m_Config.turbulenceStrength;
        params.turbulenceFrequency = m_Config.turbulenceFrequency;
        params.frameNumber         = frameNumber;
        u32 nColliders = 0;
        if (colliders && !colliders->empty() && m_ColliderBuffer) {
            nColliders = std::min<u32>(static_cast<u32>(colliders->size()), kMaxParticleColliders);
            m_ColliderBuffer->UploadData(colliders->data(), nColliders * sizeof(ParticleColliderShape));
        }
        params.colliderCount       = nColliders;
        m_EmitterParamsUBO->UploadData(&params, sizeof(params));
    }

    // Dispatch simulation
    u32 workgroups = (m_Config.maxParticles + 255) / 256;
    m_SimulateSetup.Dispatch(cmd, workgroups);
    // Compute writes -> the draw reads the particle buffer as an instance
    // vertex buffer. The vertex-input stage only supports
    // VERTEX_ATTRIBUTE_READ (the helper's generic SHADER_READ trips
    // VUID-vkCmdPipelineBarrier-dstAccessMask-02816), so barrier explicitly.
    {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0,
                             1, &barrier, 0, nullptr, 0, nullptr);
    }
}

// Cheap deterministic hash -> [0,1) for per-particle jitter.
static f32 HashUnit(u32 n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<f32>(n & 0x7FFFFFFFu) / static_cast<f32>(0x7FFFFFFF);
}

// GPUParticlePresetName + PresetSpawnParams moved to GPUParticleTypes.cpp
// (backend-agnostic, shared with the WebGPU particle path).

void GPUParticleSystem::Spawn(u32 count, const Math::Vector3& position,
                               const Math::Vector3& direction) {
    // Default look = a plain white puff (kept for the debug burst buttons).
    ParticleSpawnParams params;
    params.color = m_Config.startColor;
    params.size = m_Config.startSize;
    params.lifetime = m_Config.maxLifetime;
    SpawnWithParams(count, position, direction, params);
}

void GPUParticleSystem::SpawnWithParams(u32 count, const Math::Vector3& position,
                                         const Math::Vector3& direction,
                                         const ParticleSpawnParams& params,
                                         u8 shape, f32 shapeSize) {
    if (!m_Initialized || count == 0) return;
    m_HasSpawned = true;   // wakes the simulation (see Simulate's idle gate)

    // Initialize particles in a staging region of the SSBO
    std::vector<GPUParticle> newParticles(count);
    for (u32 i = 0; i < count; ++i) {
        GPUParticle& p = newParticles[i];
        p.position = position + ShapeSpawnOffset(shape, shapeSize, i);
        p.lifetime = params.lifetime * (0.7f + 0.6f * HashUnit(i * 2654435761u + 11u));
        p.age = 0.0f;
        // Random velocity within a cone around direction
        f32 theta = static_cast<f32>(i) * 2.39996f; // golden angle
        f32 phi = params.spread * static_cast<f32>(i % 16) / 16.0f;
        p.velocity = Math::Vector3(
            direction.x + sinf(phi) * cosf(theta) * params.spread,
            direction.y + cosf(phi),
            direction.z + sinf(phi) * sinf(theta) * params.spread
        ) * params.speed;
        p.color = params.color;
        p.size = params.size * (1.0f - params.sizeJitter + 2.0f * params.sizeJitter * HashUnit(i * 40503u + 7u));
        p.rotation = HashUnit(i * 22699u + 3u) * 6.2831853f;
        p.gravityScale = params.gravityScale;
        p.drag = params.drag;
        p.sprite = static_cast<f32>(params.sprite);
        p.softness = params.softness;
        p.texIndex = params.texIndex;
        p._pad0 = 0.0f;
    }

    // Upload to SSBO at m_NextSpawnIndex
    u32 uploadCount = std::min(count, m_Config.maxParticles - m_NextSpawnIndex);
    if (uploadCount > 0 && m_ParticleBuffer) {
        bool ok = m_ParticleBuffer->UploadData(newParticles.data(),
                                                uploadCount * sizeof(GPUParticle),
                                                m_NextSpawnIndex * sizeof(GPUParticle));
        m_NextSpawnIndex = (m_NextSpawnIndex + uploadCount) % m_Config.maxParticles;
        ENJIN_LOG_INFO(Renderer, "GPUParticleSystem: spawned %u at (%.1f, %.1f, %.1f)%s",
                       uploadCount, position.x, position.y, position.z,
                       ok ? "" : " — UPLOAD FAILED");
    }
}

void GPUParticleSystem::EnsureDrawPipeline(VkRenderPass renderPass,
                                            VkDescriptorSetLayout sharedLayout,
                                            u32 colorAttachmentCount,
                                            VkDescriptorSetLayout bindlessLayout) {
    if (!m_Initialized || renderPass == VK_NULL_HANDLE) return;

    // Already have a pipeline for THIS pass? Select it and we're done. Different
    // passes are incompatible (swapchain MRT vs offscreen single-attachment), so
    // each gets its own pipeline; the cache stays tiny (2-3 entries).
    for (auto& e : m_DrawPipelines) {
        if (e.pass == renderPass) {
            m_CurrentDrawPipeline = e.pipeline.get();
            return;
        }
    }

    if (!m_DrawVS) {
        m_DrawVS = std::make_unique<Renderer::VulkanShader>(m_Context);
        if (!m_DrawVS->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::GpuParticleVertexShaderData),
                Renderer::ShaderData::GpuParticleVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GPUParticleSystem: failed to load draw vertex shader");
            m_DrawVS.reset();
            return;
        }
    }
    if (!m_DrawFS) {
        m_DrawFS = std::make_unique<Renderer::VulkanShader>(m_Context);
        if (!m_DrawFS->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::GpuParticleFragmentShaderData),
                Renderer::ShaderData::GpuParticleFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "GPUParticleSystem: failed to load draw fragment shader");
            m_DrawFS.reset();
            return;
        }
    }

    // Single instance-rate binding: the particle SSBO itself (stride = GPUParticle).
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(GPUParticle);
    binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 5> attrs{};
    // vec4 position + lifetime
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0};
    // vec4 velocity + age
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16};
    // vec4 color
    attrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 32};
    // vec4 size + rotation + gravityScale + drag
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 48};
    // vec4 sprite card + softness + texIndex + pad
    attrs[4] = {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 64};

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
    config.depthWrite = false;   // transparent particles don't write depth
    config.cullMode = VK_CULL_MODE_NONE;
    config.alphaBlend = true;
    config.colorAttachmentCount = colorAttachmentCount;  // MRT rule (VUID-07609)
    config.customVertexInput = &vertexInput;

    auto pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Context);
    if (bindlessLayout != VK_NULL_HANDLE) pipeline->SetBindlessLayout(bindlessLayout);
    if (!pipeline->CreateWithLayout(config, m_DrawVS.get(), m_DrawFS.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "GPUParticleSystem: failed to create draw pipeline");
        return;
    }
    DrawPipelineEntry entry;
    entry.pass = renderPass;
    entry.attachments = colorAttachmentCount;
    entry.pipeline = std::move(pipeline);
    m_CurrentDrawPipeline = entry.pipeline.get();
    m_DrawPipelines.push_back(std::move(entry));
    ENJIN_LOG_INFO(Renderer, "GPUParticleSystem: draw pipeline created (%u attachment(s), %zu pass(es) cached)",
                   colorAttachmentCount, m_DrawPipelines.size());
}

void GPUParticleSystem::RecreateDrawPipeline(VkRenderPass renderPass,
                                              VkDescriptorSetLayout sharedLayout,
                                              u32 colorAttachmentCount,
                                              VkDescriptorSetLayout bindlessLayout) {
    if (!m_Initialized) return;
    // Frame-safe caller contract: GPU idle / not mid-recording. Drop every cached
    // pipeline (any of their passes may have been destroyed); they lazily rebuild
    // on next use per pass.
    m_CurrentDrawPipeline = nullptr;
    m_DrawPipelines.clear();
    m_DrawVS.reset();
    m_DrawFS.reset();
    EnsureDrawPipeline(renderPass, sharedLayout, colorAttachmentCount, bindlessLayout);
}

void GPUParticleSystem::Render(VkCommandBuffer cmd, VkDescriptorSet sharedSet,
                                VkDescriptorSet bindlessSet) {
    if (!m_Initialized || !m_HasSpawned || !m_CurrentDrawPipeline) return;

    // The particle buffer doubles as an instance-rate vertex buffer; Simulate's
    // trailing barrier (COMPUTE -> VERTEX_INPUT) makes this read safe. Dead
    // slots collapse to degenerate quads in the vertex shader, so we can draw
    // maxParticles instances without any alive-count readback.
    m_CurrentDrawPipeline->Bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_CurrentDrawPipeline->GetLayout(), 0, 1, &sharedSet, 0, nullptr);
    // Set 1: bindless textures for Textured sprite cards (frag samples bindlessTextures[])
    if (bindlessSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_CurrentDrawPipeline->GetLayout(), 1, 1, &bindlessSet, 0, nullptr);
    }

    VkBuffer vb = m_ParticleBuffer->GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);

    vkCmdDraw(cmd, 6, m_Config.maxParticles, 0, 0);
}

VkBuffer GPUParticleSystem::GetParticleBuffer() const {
    return m_ParticleBuffer ? m_ParticleBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUParticleSystem::GetAliveIndexBuffer() const {
    return m_AliveIndexBuffer ? m_AliveIndexBuffer->GetBuffer() : VK_NULL_HANDLE;
}

// --- Resource creation ---

bool GPUParticleSystem::CreateBuffers() {
    // Particle state SSBO (read-write, GPU-local with staging upload)
    m_ParticleBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Context);
    // Host-visible: VulkanBuffer::UploadData has no staging path for
    // device-local memory, and Spawn() seeds particles from the CPU. 4 MB of
    // host-visible SSBO is fine for now; a staged device-local upgrade is a
    // perf follow-up, not a correctness need.
    if (!m_ParticleBuffer->Create(
            m_Config.maxParticles * sizeof(GPUParticle),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,   // draw path reads it as an instance VB
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "GPUParticleSystem: failed to create particle SSBO");
        return false;
    }
    // Zero the whole buffer so every un-spawned slot reads as dead
    // (lifetime = 0) instead of uninitialized garbage in the sim and draw.
    {
        std::vector<u8> zero(m_Config.maxParticles * sizeof(GPUParticle), 0);
        m_ParticleBuffer->UploadData(zero.data(), zero.size());
    }

    // Alive index buffer: 4 bytes (atomic counter) + maxParticles * 4 bytes (indices)
    usize aliveBufferSize = sizeof(u32) + m_Config.maxParticles * sizeof(u32);
    m_AliveIndexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Context);
    if (!m_AliveIndexBuffer->Create(
            aliveBufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "GPUParticleSystem: failed to create alive index buffer");
        return false;
    }

    // Emitter params UBO
    m_EmitterParamsUBO = std::make_unique<Renderer::VulkanBuffer>(m_Context);
    if (!m_EmitterParamsUBO->Create(
            256, // Generous for emitter params struct
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "GPUParticleSystem: failed to create emitter UBO");
        return false;
    }

    return true;
}

void GPUParticleSystem::DestroyResources() {
    VkDevice device = m_Context->GetDevice();

    m_SimulateSetup.Destroy(device);
    m_PipelineCreated = false;

    m_CurrentDrawPipeline = nullptr;
    m_DrawPipelines.clear();
    m_DrawVS.reset();
    m_DrawFS.reset();

    m_ParticleBuffer.reset();
    m_AliveIndexBuffer.reset();
    m_EmitterParamsUBO.reset();
    m_ColliderBuffer.reset();
}

} // namespace Effects
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
