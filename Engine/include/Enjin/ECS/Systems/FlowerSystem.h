#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Renderer/Camera.h"

namespace Enjin {
namespace ECS {

class RenderSystem;

class ENJIN_API FlowerSystem {
public:
    FlowerSystem() = default;
    ~FlowerSystem() = default;

    void SetWorld(World* world) { m_World = world; }
    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }
    void SetRenderSystem(RenderSystem* rs) { m_RenderSystem = rs; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Set the game view bounds in screen space (for mouse coordinate conversion)
    void SetGameViewBounds(f32 minX, f32 minY, f32 maxX, f32 maxY);

    // Set the render target dimensions (for picking coordinate conversion)
    void SetRenderTargetSize(u32 width, u32 height) { m_RTWidth = width; m_RTHeight = height; }

    // Set game camera entity (for building pick camera from CameraComponent)
    void SetGameCameraEntity(Entity entity) { m_GameCameraEntity = entity; }

    // Main update - call each frame
    void Update(f32 deltaTime);

    // Evaluate all flowers and update score displays
    void Evaluate();

private:
    void ProcessInput();
    void UpdateTethers(f32 dt);
    void UpdateJellyMeshes(f32 dt);
    void CheckBreaks();

    // Helper: project screen point to world plane at given depth facing camera
    Math::Vector3 ScreenToWorldOnPlane(f32 screenX, f32 screenY, f32 planeDepth);

    World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    RenderSystem* m_RenderSystem = nullptr;
    bool m_Enabled = false;

    // Game view bounds in screen space
    f32 m_ViewMinX = 0.0f, m_ViewMinY = 0.0f;
    f32 m_ViewMaxX = 0.0f, m_ViewMaxY = 0.0f;
    u32 m_RTWidth = 640, m_RTHeight = 360;

    Entity m_GameCameraEntity = INVALID_ENTITY;

    // Current grab state
    Entity m_GrabbedEntity = INVALID_ENTITY;
    f32 m_GrabDepth = 0.0f;  // Distance from camera at grab time
};

} // namespace ECS
} // namespace Enjin
