#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/Camera.h"

namespace Enjin::ECS {

// ParallaxSystem — renders ParallaxMachineComponent layers each frame.
// Call Update() before scene rendering to advance auto-scroll, then
// call Render() during the 2D render pass to draw layers behind the scene.
class ParallaxSystem {
public:
    void SetWorld(World* world) { m_World = world; }
    void SetCamera(const Renderer::Camera* camera) { m_Camera = camera; }

    // Advance auto-scroll timers
    void Update(f32 deltaTime);

    // Render all parallax layers (call during 2D render pass, before scene geometry)
    // Layers are sorted by sortOrder (lowest first = furthest back)
    void Render(f32 viewportWidth, f32 viewportHeight);

private:
    World* m_World = nullptr;
    const Renderer::Camera* m_Camera = nullptr;
};

} // namespace Enjin::ECS
