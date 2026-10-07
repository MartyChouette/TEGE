#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include <memory>
#include <string>

namespace Enjin {
namespace ECS { class World; }
namespace Renderer { class VulkanRenderer; }

namespace Effects {

// Plays Effekseer effects (EffekseerEffectComponent) and draws them with
// Effekseer's own renderers: Vulkan on desktop, WebGPU on web.
//
// Effekseer owns its pipelines and buffers; the engine owns the frame. So
// nothing here begins a render pass. Draw() records into a pass the engine
// already has open, and the caller says which pass that is, because a
// pipeline is only valid against the attachment formats it was built for.
//
// No Effekseer, Vulkan or WebGPU type appears in this header on purpose: it is
// included by RenderSystem.h, which most of the engine includes.
class ENJIN_API EffekseerSystem {
public:
    // The engine's passes an effect can be drawn into. One Effekseer renderer
    // is created per kind, the first time that kind is drawn.
    enum class Pass : u8 {
        Main = 0,       // desktop swapchain pass: sRGB colour + velocity + depth
        Offscreen = 1,  // desktop RenderTarget (game view, PP target): colour + depth
        WebScene = 2,   // web scene pass: RGBA16Float + Depth24PlusStencil8
        Count
    };

    EffekseerSystem();
    ~EffekseerSystem();
    EffekseerSystem(const EffekseerSystem&) = delete;
    EffekseerSystem& operator=(const EffekseerSystem&) = delete;

#if ENJIN_RENDERER_WEBGPU
    // device is a WGPUDevice
    bool Initialize(void* device);
#else
    bool Initialize(Renderer::VulkanRenderer* renderer);
#endif
    void Shutdown();
    bool IsInitialized() const;

    // Load, start, stop and place every effect in the world, then step the
    // simulation. Owner thread only (it writes component runtime fields).
    // Safe to call with a null world or before Initialize: it does nothing.
    void Update(ECS::World* world, f32 deltaTime);

    // Stop every instance and forget which entity owned it. Call when a scene
    // is cleared or play stops; loaded effect files stay cached.
    void StopAll(ECS::World* world);

    // Drop cached effect files so the next Update reads them from disk again.
    void ReloadEffects(ECS::World* world);

    // Once per rendered frame, before the first Draw of that frame.
    void BeginFrame();

    // Record the live effects into a pass that is already open.
    //   desktop: nativeCommands is the VkCommandBuffer being recorded
    //   web:     nativeCommands is the WGPUCommandEncoder, nativePass the
    //            WGPURenderPassEncoder of the open scene pass
    // view and proj are the engine's own camera matrices for that pass.
    void Draw(Pass pass, void* nativeCommands, void* nativePass,
              const Math::Matrix4& view, const Math::Matrix4& proj,
              const Math::Vector3& cameraPosition);

    // How many effect instances are alive, for the profiler and for tests.
    u32 GetLiveInstanceCount() const;
    u32 GetLoadedEffectCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Effects
} // namespace Enjin
