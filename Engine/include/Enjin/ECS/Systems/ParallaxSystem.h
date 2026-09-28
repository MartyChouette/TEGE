#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/Camera.h"
#include <unordered_map>
#include <string>
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace ECS { class RenderSystem; }
}

namespace Enjin::ECS {

// ParallaxSystem — renders ParallaxMachineComponent layers each frame.
// Call Update() before scene rendering to advance auto-scroll, then
// call Render() during the 2D render pass to draw layers behind the scene.
// Supports textured layers (loaded via RenderSystem) or solid-color fallback.
class ParallaxSystem {
public:
    void SetWorld(World* world) { m_World = world; }
    void SetCamera(const Renderer::Camera* camera) { m_Camera = camera; }
    void SetRenderSystem(RenderSystem* rs) { m_RenderSystem = rs; }

    // Advance auto-scroll timers
    void Update(f32 deltaTime);

    // Per-sprite parallax: offset every ParallaxLayerComponent entity by a
    // fraction of the camera's movement so it scrolls with depth. Static so the
    // editor's PlayMode and the standalone player can call it directly each frame
    // (both have a World + camera) without owning a ParallaxSystem instance.
    // Runs during play only — it mutates transforms, which play-mode stop
    // restores. Rides the ordinary sprite pipeline, so it works on every backend.
    // Reads the active camera ENTITY's transform (the authoritative view source)
    // so it tracks script/controller camera moves identically in the editor and
    // the player, with no dependence on the render camera's sync timing.
    static void ApplyParallaxLayers(World* world, f32 deltaTime);

    // Turns every ParallaxMachineComponent into ordinary sprite entities, one
    // per layer, each with a ParallaxLayerComponent, and removes the machine.
    // The machine was drawn only by the editor, as an ImGui overlay on the
    // background draw list with a fixed 20x12 world-to-screen guess, so it was
    // behind the Game View image and never reached a built game (EP-6; decided
    // 2026-09-27: convert to ParallaxLayer sprites). Run after a scene loads,
    // and by the machine inspector's button. Returns how many layers it made.
    static u32 ConvertMachinesToLayers(World* world);

    // Render all parallax layers (call during 2D render pass, before scene geometry)
    // Layers are sorted by sortOrder (lowest first = furthest back)
    void Render(f32 viewportWidth, f32 viewportHeight);

    // Clear cached ImGui texture descriptors (call before shutdown)
    void ClearTextureCache();

private:
#if !ENJIN_RENDERER_WEBGPU
    VkDescriptorSet GetLayerTexture(const std::string& path);
#endif

    World* m_World = nullptr;
    const Renderer::Camera* m_Camera = nullptr;
    RenderSystem* m_RenderSystem = nullptr;
#if !ENJIN_RENDERER_WEBGPU
    std::unordered_map<std::string, VkDescriptorSet> m_TextureCache;
#endif
};

} // namespace Enjin::ECS
