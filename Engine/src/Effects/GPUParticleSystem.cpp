#include "Enjin/Effects/GPUParticleSystem.h"
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

    // TODO: Reset alive count atomic, upload emitter UBO, bind compute pipeline, dispatch
    // Dispatch: ceil(maxParticles / 256) workgroups
    (void)cmd; (void)deltaTime; (void)frameNumber; (void)windForce;
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
    if (!m_Initialized || m_AliveCountReadback == 0) return;

    // TODO: Bind particle vertex buffer (using alive index indirection),
    //       issue vkCmdDrawIndirect with count from alive buffer.
    //       The particle fragment shader uses the SAME lighting path as opaque:
    //       - Clustered lights (bindings 14-15)
    //       - Shadow atlas (binding 21 or shadow map bindings)
    //       - DDGI irradiance (binding 20)
    //       - Froxel fog volume (binding 21)
    //       This ensures ONE lighting system for ALL renderable objects.
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

    if (m_SimulatePipeline) { vkDestroyPipeline(device, m_SimulatePipeline, nullptr); m_SimulatePipeline = VK_NULL_HANDLE; }
    if (m_PipelineLayout) { vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
    if (m_DescLayout) { vkDestroyDescriptorSetLayout(device, m_DescLayout, nullptr); m_DescLayout = VK_NULL_HANDLE; }
    if (m_DescPool) { vkDestroyDescriptorPool(device, m_DescPool, nullptr); m_DescPool = VK_NULL_HANDLE; }

    m_ParticleBuffer.reset();
    m_AliveIndexBuffer.reset();
    m_EmitterParamsUBO.reset();
}

} // namespace Effects
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
