#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/GPUParticleTypes.h"   // shared GPUParticle / presets / config
#include "Enjin/Effects/ParticleColliders.h"   // shapes particles bounce off
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Enjin {

namespace ECS { class World; }

namespace Renderer {
class VulkanContext;
class VulkanBuffer;
class VulkanPipeline;
class VulkanShader;
}

namespace Effects {

// GPUParticle, GPUParticlePreset, ParticleSpawnParams, GPUEmitterConfig, and the
// PresetSpawnParams/GPUParticlePresetName helpers now live in GPUParticleTypes.h
// (backend-agnostic, shared with the WebGPU path). Included above.

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

    // Simulate: dispatch compute shader to update all particles. `colliders`
    // (optional) are world shapes the particles bounce off this frame.
    void Simulate(VkCommandBuffer cmd, f32 deltaTime, u32 frameNumber,
                  const Math::Vector3& windForce,
                  const std::vector<ParticleColliderShape>* colliders = nullptr);

    // Collider strikes read back from the sim (two frames of latency; the slot
    // read at frame N was written by frame N-2, whose fence has been waited).
    const std::vector<ParticleImpactEvent>& GetImpactEvents() const { return m_ImpactEvents; }

    // Spawn new particles (CPU-side, uploads to staging region of particle SSBO)
    void Spawn(u32 count, const Math::Vector3& position, const Math::Vector3& direction);

    // Spawn with explicit per-particle appearance/physics (this is what emitters
    // and presets use so different looks coexist in the one shared buffer).
    void SpawnWithParams(u32 count, const Math::Vector3& position,
                         const Math::Vector3& direction, const ParticleSpawnParams& params,
                         u8 shape = 0, f32 shapeSize = 0.0f);

    // Render: draw the particle SSBO as an instance-rate vertex buffer inside
    // the current (already begun) render pass. Dead slots collapse to
    // degenerate quads in the vertex shader, so no readback and no indirect
    // buffer are needed. Inherits the pass's dynamic viewport/scissor.
    void Render(VkCommandBuffer cmd, VkDescriptorSet sharedSet,
                VkDescriptorSet bindlessSet = VK_NULL_HANDLE);

    // Create the draw pipeline if it doesn't exist yet (creation mid-recording
    // is legal; destruction is not — that's what RecreateDrawPipeline is for).
    void EnsureDrawPipeline(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout,
                            u32 colorAttachmentCount,
                            VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE);

    // Destroy + rebuild the draw pipeline for a new render pass. Call ONLY at
    // frame-safe times (RenderSystem::RecreateEffectPipelinesForRenderPass).
    void RecreateDrawPipeline(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout,
                              u32 colorAttachmentCount,
                              VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE);

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

    // World collider shapes (host-visible SSBO, re-uploaded each frame)
    std::unique_ptr<Renderer::VulkanBuffer> m_ColliderBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_ImpactEventBuffer;   // host-visible, ping-ponged
    std::vector<ParticleImpactEvent> m_ImpactEvents;

    // Compute pipeline (via helper)
    Renderer::ComputePipelineSetup m_SimulateSetup;
    bool m_PipelineCreated = false;

    // Draw pipelines (billboard from the particle buffer as instance VB).
    // ONE PER RENDER PASS: a Vulkan pipeline is only valid in a compatible render
    // pass, and particles draw into both the swapchain main pass (2-attachment MRT)
    // and offscreen targets (1 attachment). A single cached pipeline built for one
    // pass silently draws nothing in the other — that bug hid GPU particles in
    // exported games while the editor (always offscreen) looked fine.
    std::unique_ptr<Renderer::VulkanShader> m_DrawVS;
    std::unique_ptr<Renderer::VulkanShader> m_DrawFS;
    struct DrawPipelineEntry {
        VkRenderPass pass = VK_NULL_HANDLE;
        u32 attachments = 1;
        std::unique_ptr<Renderer::VulkanPipeline> pipeline;
    };
    std::vector<DrawPipelineEntry> m_DrawPipelines;
    Renderer::VulkanPipeline* m_CurrentDrawPipeline = nullptr;  // selected by EnsureDrawPipeline

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
