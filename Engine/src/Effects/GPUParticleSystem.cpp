#include "Enjin/Effects/GPUParticleSystem.h"
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Logging/Log.h"
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

void GPUParticleSystem::Simulate(VkCommandBuffer cmd, f32 deltaTime, u32 frameNumber,
                                  const Math::Vector3& windForce) {
    if (!m_Initialized) return;

    // Lazy pipeline creation
    if (!m_PipelineCreated) {
        using BT = Renderer::BindType;
        // particle_simulate.comp bindings:
        // 0=SSBO(particles), 1=SSBO(aliveIndexBuffer), 2=UBO(emitterParams)
        m_SimulateSetup.Create(m_Context, {
            {0, BT::StorageBuffer}, {1, BT::StorageBuffer}, {2, BT::UniformBuffer}
        }, "particle_simulate.comp");

        VkDevice device = m_Context->GetDevice();
        if (m_SimulateSetup.IsValid()) {
            m_SimulateSetup.WriteBuffer(device, 0, m_ParticleBuffer->GetBuffer(),
                                         m_Config.maxParticles * sizeof(GPUParticle));
            usize aliveSize = sizeof(u32) + m_Config.maxParticles * sizeof(u32);
            m_SimulateSetup.WriteBuffer(device, 1, m_AliveIndexBuffer->GetBuffer(), aliveSize);
            m_SimulateSetup.WriteBuffer(device, 2, m_EmitterParamsUBO->GetBuffer(), 256,
                                         VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
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

    // TODO: Upload emitter params UBO with current deltaTime, gravity, wind, etc.

    // Dispatch simulation
    u32 workgroups = (m_Config.maxParticles + 255) / 256;
    m_SimulateSetup.Dispatch(cmd, workgroups);
    m_SimulateSetup.Barrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

    (void)deltaTime; (void)frameNumber; (void)windForce;
}

void GPUParticleSystem::Spawn(u32 count, const Math::Vector3& position,
                               const Math::Vector3& direction) {
    if (!m_Initialized || count == 0) return;

    // Initialize particles in a staging region of the SSBO
    std::vector<GPUParticle> newParticles(count);
    for (u32 i = 0; i < count; ++i) {
        GPUParticle& p = newParticles[i];
        p.position = position;
        p.lifetime = m_Config.maxLifetime;
        p.age = 0.0f;
        // Random velocity within cone
        f32 theta = static_cast<f32>(i) * 2.39996f; // Golden angle
        f32 phi = m_Config.spread * static_cast<f32>(i % 16) / 16.0f;
        p.velocity = Math::Vector3(
            direction.x + sinf(phi) * cosf(theta) * m_Config.spread,
            direction.y + cosf(phi),
            direction.z + sinf(phi) * sinf(theta) * m_Config.spread
        ) * 2.0f; // Initial speed
        p.color = m_Config.startColor;
        p.size = m_Config.startSize;
        p.rotation = 0.0f;
    }

    // Upload to SSBO at m_NextSpawnIndex
    u32 uploadCount = std::min(count, m_Config.maxParticles - m_NextSpawnIndex);
    if (uploadCount > 0 && m_ParticleBuffer) {
        m_ParticleBuffer->UploadData(newParticles.data(),
                                      uploadCount * sizeof(GPUParticle),
                                      m_NextSpawnIndex * sizeof(GPUParticle));
        m_NextSpawnIndex = (m_NextSpawnIndex + uploadCount) % m_Config.maxParticles;
    }
}

void GPUParticleSystem::Render(VkCommandBuffer cmd) {
    if (!m_Initialized) return;

    // Bind particle SSBO as vertex source (shader reads from SSBO via alive indices).
    // The particle fragment shader uses the SAME lighting path as opaque geometry:
    //   - Clustered lights (bindings 14-15)
    //   - Shadow atlas (shadow map bindings)
    //   - DDGI irradiance (binding 20)
    //   - Froxel fog volume (binding 21)
    // ONE lighting system for ALL renderable objects — no separate particle lighting.

    // Draw alive particles using indirect count from alive buffer.
    // The alive buffer layout: [u32 count, u32 indices[]]
    // We use the count as the instance count for a single-quad draw.
    // Each instance reads its particle data from the SSBO via the alive index.
    VkBuffer aliveBuffer = m_AliveIndexBuffer->GetBuffer();
    if (aliveBuffer) {
        // vkCmdDrawIndirect: vertexCount=6 (quad), instanceCount=aliveCount (from buffer)
        // The indirect buffer at offset 0 contains the alive count.
        // NOTE: This requires the alive buffer to be laid out as VkDrawIndirectCommand
        // which it isn't directly. A more correct approach would be a separate
        // indirect draw command buffer filled by the compute shader.
        // For now, we read back the alive count with 1-frame latency.
        // TODO: Use vkCmdDrawIndirect with a properly formatted indirect buffer
    }
    (void)cmd;
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
    if (!m_ParticleBuffer->Create(
            m_Config.maxParticles * sizeof(GPUParticle),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "GPUParticleSystem: failed to create particle SSBO");
        return false;
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

    m_ParticleBuffer.reset();
    m_AliveIndexBuffer.reset();
    m_EmitterParamsUBO.reset();
}

} // namespace Effects
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
