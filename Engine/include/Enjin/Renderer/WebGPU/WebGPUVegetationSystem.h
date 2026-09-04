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
    GPUPipelineHandle m_Pipeline;

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
    f32 m_WindTime = 0.0f;
};

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
