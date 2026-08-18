#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Effects/GPUParticleTypes.h"
#include "Enjin/Renderer/GPUTypes.h"
#include "Enjin/Math/Matrix.h"
#include <webgpu/webgpu.h>

namespace Enjin {
namespace Renderer {

class WebGPURenderer;

// WebGPU GPU particle system. Mirrors the Vulkan Effects::GPUParticleSystem but
// runs the sim + draw through the WebGPU backend abstraction (compute pass +
// render pass). Same 64-byte GPUParticle layout and presets as the Vulkan path,
// so the emitter component authors both identically.
//
// Frame usage (from the web player's Render()):
//   Simulate(dt, frame, wind)  -- after BeginFrameWebGPU, BEFORE any render pass
//   Render(pass, view, proj)   -- inside an already-begun render pass
class ENJIN_API WebGPUParticleSystem {
public:
    explicit WebGPUParticleSystem(WebGPURenderer* renderer);
    ~WebGPUParticleSystem();

    bool Initialize(const Effects::GPUEmitterConfig& config = Effects::GPUEmitterConfig{});
    void Shutdown();

    // Upload `count` fresh particles at position/direction with the given look.
    void SpawnWithParams(u32 count, const Math::Vector3& position,
                         const Math::Vector3& direction, const Effects::ParticleSpawnParams& params,
                         u8 shape = 0, f32 shapeSize = 0.0f);

    // Dispatch the compute sim for this frame (compute pass on the frame encoder).
    void Simulate(f32 deltaTime, u32 frameNumber, const Math::Vector3& windForce);

    // Draw all particles into the SCENE pass (MSAA 4x RGBA16Float + real scene depth):
    // particles are depth-occluded by geometry and tonemapped with the scene.
    void RenderScene(WGPURenderPassEncoder pass, const Math::Matrix4& view, const Math::Matrix4& proj);

    // Fallback: draw into the swapchain overlay pass (no scene depth — composites on
    // top). Used only when the scene renders directly to the swapchain (post-process
    // off). No-ops if RenderScene already drew this frame.
    void Render(WGPURenderPassEncoder pass, const Math::Matrix4& view, const Math::Matrix4& proj);

    u32 GetMaxParticles() const { return m_Config.maxParticles; }
    bool HasSpawned() const { return m_HasSpawned; }

private:
    WebGPURenderer* m_Renderer = nullptr;
    Effects::GPUEmitterConfig m_Config;

    GPUBufferHandle m_ParticleBuffer;   // storage, maxParticles * 64 bytes
    GPUBufferHandle m_ParamsUBO;        // uniform, 128B (sim EmitterParams)
    GPUBufferHandle m_ViewProjUBO;      // uniform, 128B (draw: view + proj)

    GPUShaderHandle m_SimShader;
    GPUShaderHandle m_DrawShader;
    GPUBindGroupLayoutHandle m_ComputeLayout;
    GPUBindGroupLayoutHandle m_DrawLayout;
    GPUBindGroupHandle m_ComputeBindGroup;
    GPUBindGroupHandle m_DrawBindGroup;
    GPUPipelineHandle m_ComputePipeline;
    GPUPipelineHandle m_DrawPipeline;        // overlay fallback (swapchain formats)
    GPUPipelineHandle m_ScenePipeline;       // scene pass (RGBA16F, MSAA 4x, scene depth)
    bool m_SceneDrewThisFrame = false;

    u32 m_NextSpawnIndex = 0;
    bool m_HasSpawned = false;
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
