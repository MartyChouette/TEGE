#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Renderer/Camera.h"

namespace Enjin {
namespace ECS {

// System that updates all character controllers based on input
class ENJIN_API ControllerSystem {
public:
    ControllerSystem() = default;
    ~ControllerSystem() = default;

    void SetWorld(World* world) { m_World = world; }
    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }

    // Enable/disable all controller updates (e.g., when in editor mode vs play mode)
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Main update - call each frame with delta time
    void Update(f32 deltaTime);

private:
    // Individual controller update methods
    void UpdatePlatformer2D(Entity entity, Platformer2DController& controller, TransformComponent& transform, f32 dt);
    void UpdateTopDown2D(Entity entity, TopDown2DController& controller, TransformComponent& transform, f32 dt);
    void UpdateTopDown3D(Entity entity, TopDown3DController& controller, TransformComponent& transform, f32 dt);
    void UpdateThirdPerson(Entity entity, ThirdPersonController& controller, TransformComponent& transform, f32 dt);
    void UpdateFirstPerson(Entity entity, FirstPersonController& controller, TransformComponent& transform, f32 dt);

    // Helper methods
    Math::Vector2 GetMovementInput(const CharacterControllerBase& controller);
    bool IsJumpPressed();
    bool IsSprintHeld();
    bool IsCrouchPressed();
    bool IsDashPressed();

    World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    bool m_Enabled = false;  // Disabled by default (editor mode)
};

} // namespace ECS
} // namespace Enjin
