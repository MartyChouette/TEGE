#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Enjin {

namespace ECS { class World; }

namespace Renderer {
class VulkanContext;
class VulkanBuffer;
}

namespace Effects {

// GPU particle data (must match GLSL struct exactly — 64 bytes)
struct GPUParticle {
    Math::Vector3 position;
    f32 lifetime;
    Math::Vector3 velocity;
    f32 age;
    Math::Vector4 color;
    f32 size;
    f32 rotation;
    f32 _pad0;
    f32 _pad1;
};
static_assert(sizeof(GPUParticle) == 64, "GPUParticle must match GLSL layout");

// Emitter configuration for GPU particles
struct GPUEmitterConfig {
    Math::Vector3 position = {0, 0, 0};
    Math::Vector3 direction = {0, 1, 0};
    f32 spread = 0.5f;            // Cone half-angle (radians)
    Math::Vector3 gravity = {0, -9.8f, 0};
    f32 damping = 0.1f;
    Math::Vector4 startColor = {1, 1, 1, 1};
    Math::Vector4 endColor = {1, 1, 1, 0};
    f32 startSize = 0.1f;
    f32 endSize = 0.3f;
    f32 maxLifetime = 3.0f;
    f32 spawnRate = 100.0f;        // Particles per second
    f32 turbulenceStrength = 0.0f;
    f32 turbulenceFrequency = 1.0f;
    u32 maxParticles = 65536;
};

// GPU-driven particle system.
// Simulation runs entirely on compute shader — no CPU readback.
// Alive particles are compacted via atomic append for indirect draw.
//
// Lighting: particles are rendered with the SAME lighting path as opaque geometry.
// The particle fragment shader reads from clustered lights, shadow atlas,
// DDGI probes, and froxel fog volume — one unified lighting system for everything.
class ENJIN_API GPUParticleSystem {
public:
    GPUParticleSystem(Renderer::VulkanContext* context);
    ~GPUParticleSystem();

    bool Initialize(const GPUEmitterConfig& config = GPUEmitterConfig{});
    void Shutdown();

    // Simulate: dispatch compute shader to update all particles
    void Simulate(VkCommandBuffer cmd, f32 deltaTime, u32 frameNumber,
                  const Math::Vector3& windForce);

    // Spawn new particles (CPU-side, uploads to staging region of particle SSBO)
    void Spawn(u32 count, const Math::Vector3& position, const Math::Vector3& direction);

    // Render: bind particle SSBO + alive index buffer, issue indirect draw
    void Render(VkCommandBuffer cmd);

    // Accessors
    u32 GetMaxParticles() const { return m_Config.maxParticles; }
    u32 GetAliveCount() const { return m_AliveCountReadback; }
    VkBuffer GetParticleBuffer() const;
    VkBuffer GetAliveIndexBuffer() const;

    GPUEmitterConfig& GetConfig() { return m_Config; }

private:
    bool CreateBuffers();
    bool CreateComputePipeline();
    void DestroyResources();

    Renderer::VulkanContext* m_Context = nullptr;
    GPUEmitterConfig m_Config;

    // Particle state SSBO
    std::unique_ptr<Renderer::VulkanBuffer> m_ParticleBuffer;

    // Alive index append buffer (atomic counter + indices)
    std::unique_ptr<Renderer::VulkanBuffer> m_AliveIndexBuffer;

    // Emitter params UBO
    std::unique_ptr<Renderer::VulkanBuffer> m_EmitterParamsUBO;

    // Compute pipeline (via helper)
    Renderer::ComputePipelineSetup m_SimulateSetup;
    bool m_PipelineCreated = false;

    // Spawn tracking
    u32 m_NextSpawnIndex = 0;
    u32 m_AliveCountReadback = 0; // Read back from GPU (1 frame latency)
    bool m_HasSpawned = false;    // idle until something spawns (no per-frame waste)
    bool m_LoggedDormant = false;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
