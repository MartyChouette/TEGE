#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/Camera.h"
#include <unordered_map>
#include <string>
#include <vulkan/vulkan.h>

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

    // Render all parallax layers (call during 2D render pass, before scene geometry)
    // Layers are sorted by sortOrder (lowest first = furthest back)
    void Render(f32 viewportWidth, f32 viewportHeight);

    // Clear cached ImGui texture descriptors (call before shutdown)
    void ClearTextureCache();

private:
    VkDescriptorSet GetLayerTexture(const std::string& path);

    World* m_World = nullptr;
    const Renderer::Camera* m_Camera = nullptr;
    RenderSystem* m_RenderSystem = nullptr;
    std::unordered_map<std::string, VkDescriptorSet> m_TextureCache;
};

} // namespace Enjin::ECS
