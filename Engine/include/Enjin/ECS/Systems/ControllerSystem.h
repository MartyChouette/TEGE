#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/ECS/Components/GravityZone.h"

namespace Enjin {
namespace ECS {

// System that updates all character controllers based on input
class ENJIN_API ControllerSystem {
public:
    ControllerSystem() = default;
    ~ControllerSystem() = default;

    void SetWorld(World* world) { m_World = world; }
    void SetCamera(Renderer::Camera* camera) { m_Camera = camera; }
    void SetPhysics(Physics::IPhysicsBackend* physics) { m_Physics = physics; }
    void SetPhysics2D(Physics::IPhysicsBackend2D* physics) { m_Physics2D = physics; }
    void SetInputActionMap(InputSystem::InputActionMap* map) { m_InputMap = map; }

    // When set, controllers write to this entity's TransformComponent instead of the editor Camera.
    // The game view renders from CameraComponent entities, so this keeps the editor camera untouched.
    void SetGameCameraEntity(Entity entity) { m_GameCameraEntity = entity; }
    Entity GetGameCameraEntity() const { return m_GameCameraEntity; }

    // Reduced motion (disables head bob, FOV effects)
    void SetReducedMotion(bool enabled) { m_ReducedMotion = enabled; }
    bool GetReducedMotion() const { return m_ReducedMotion; }

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
    void UpdateVehicle(Entity entity, VehicleController& ctrl, TransformComponent& transform, f32 dt);
    void UpdateSurfaceAligned(Entity entity, SurfaceAlignedController& ctrl, TransformComponent& transform, f32 dt);

    // Helper methods
    Math::Vector2 GetMovementInput(const CharacterControllerBase& controller);
    bool IsJumpPressed();
    bool IsSprintHeld();
    bool IsCrouchPressed();
    bool IsDashPressed();
    bool CheckGround(const Math::Vector3& position, f32& groundY);
    bool CheckGround2D(const Math::Vector3& position, f32& groundY);

    // Grid movement: returns true if grid movement handled the position update (caller should skip free movement)
    bool UpdateGridMovement(CharacterControllerBase& controller, TransformComponent& transform,
                           const Math::Vector2& input, f32 dt);

    // Helper: update game camera entity transform to match position/lookAt
    void UpdateGameCameraTransform(const Math::Vector3& position, const Math::Vector3& target, const Math::Vector3& up);

    World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Physics::IPhysicsBackend* m_Physics = nullptr;
    Physics::IPhysicsBackend2D* m_Physics2D = nullptr;
    InputSystem::InputActionMap* m_InputMap = nullptr;
    Entity m_GameCameraEntity = INVALID_ENTITY;
    bool m_Enabled = false;  // Disabled by default (editor mode)
    bool m_ReducedMotion = false;
};

} // namespace ECS
} // namespace Enjin
