#pragma once

// WebGPU vegetation: grass/shrub/tree volumes drawn on web. The desktop
// GrassRenderer/ShrubRenderer/TreeRenderer are Vulkan-only, so until this
// existed flora simply did not render in web builds. One instanced,
// vertex-pulling draw per volume: the template mesh (shared with the desktop +
// RT paths via VegTemplates) lives in a storage buffer, and the vertex shader
// scatters/rotates/winds each instance procedurally from the instance index —
// the same approach as grass.vert. Draws inside the scene pass (real depth,
// scene tonemapping) via the SetWebScenePassHook path the particles use.

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Renderer/GPUTypes.h"
#include <webgpu/webgpu.h>

namespace Enjin {
namespace ECS { class World; }
namespace Renderer {

class WebGPURenderer;

class WebGPUVegetationSystem {
public:
    bool Initialize(WebGPURenderer* renderer);
    void Shutdown();

    // Wind for sway (direction * strength, plus a running clock).
    // Draw the same scattered plants into a DEPTH-only pass from the light's
    // point of view, so a grove casts a shadow. The scene shadow pass walks
    // entities with a MeshComponent and these have none -- they are a volume
    // plus a scatter hash -- so without this every tree was lit but shadowless.
    void RenderShadow(WGPURenderPassEncoder pass, const Math::Matrix4& lightViewProj,
                      ECS::World* world);

    // Settled snow, 0..1, from the scene's weather. Fed in per frame like the
    // wind and the sun: this system is owned by the player, not the render
    // system, so it cannot reach the lighting buffer the ground reads.
    void SetSnowAccumulation(f32 v) { m_SnowAccumulation = v; }

    void SetWind(const Math::Vector3& wind, f32 time) { m_Wind = wind; m_WindTime = time; }

    // The scene's light. This pass has its own pipeline with no lighting
    // buffer bound, so without this the shader lit every plant from a
    // hardcoded direction and the grove kept flat noon light while the sun
    // moved and everything around it changed.
    void SetSun(const Math::Vector3& dirToLight, const Math::Vector3& color, f32 intensity) {
        m_SunDir = dirToLight; m_SunColor = color; m_SunIntensity = intensity;
    }
    void SetAmbient(const Math::Vector3& color, f32 intensity) {
        m_Ambient = color; m_AmbientIntensity = intensity;
    }

    // Draw every Grass/Shrub/Tree volume into the scene pass. view/proj are the
    // scene camera's (the WebGPU Y-flip is applied internally, like particles).
    void RenderScene(WGPURenderPassEncoder pass, const Math::Matrix4& view,
                     const Math::Matrix4& proj, ECS::World* world);

    static constexpr u32 kMaxVolumes = 32;

private:
    struct TemplateRange { u32 indexOffset = 0; u32 indexCount = 0; };

    WebGPURenderer* m_Renderer = nullptr;
    bool m_Initialized = false;

    GPUShaderHandle m_Shader;
    GPUBindGroupLayoutHandle m_Layout;
    GPUBindGroupHandle m_BindGroup;
    // One entry per volume, plus what to draw for it. Built once and used by
    // BOTH passes, so the shadow scatters the identical plants to the identical
    // places as the scene -- two copies of this would drift and the shadows
    // would stop matching the grove casting them.
    struct DrawInfo { u32 indexCount; u32 density; };
    u32 BuildVolumeParams(ECS::World* world, struct VolumeParamsCPU* params,
                          DrawInfo* draws) const;

    GPUPipelineHandle m_Pipeline;
    // Depth-only twin of the above, so the grove casts a shadow. Separate UBO
    // and bind group because both passes run in one frame.
    GPUPipelineHandle m_ShadowPipeline;
    GPUBufferHandle m_ShadowUBO;
    GPUBindGroupHandle m_ShadowBindGroup;

    GPUBufferHandle m_ViewProjUBO;      // 128B: view + (Y-flipped) proj
    GPUBufferHandle m_TemplateVerts;    // storage: concatenated VegVertex (5 f32 each)
    GPUBufferHandle m_TemplateIndices;  // storage: concatenated u32 indices
    GPUBufferHandle m_VolumeParams;     // storage: kMaxVolumes x 96B params

    TemplateRange m_Grass, m_Shrub, m_Tree;

    Math::Vector3 m_Wind{0.0f, 0.0f, 0.0f};
    Math::Vector3 m_SunDir{0.4f, 0.8f, 0.45f};
    Math::Vector3 m_SunColor{1.0f, 0.97f, 0.92f};
    f32 m_SunIntensity = 0.65f;
    Math::Vector3 m_Ambient{0.35f, 0.38f, 0.42f};
    f32 m_AmbientIntensity = 1.0f;
    f32 m_SnowAccumulation = 0.0f;
    f32 m_WindTime = 0.0f;
};

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
