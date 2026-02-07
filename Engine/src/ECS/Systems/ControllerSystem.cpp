#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace ECS {

void ControllerSystem::UpdateGameCameraTransform(const Math::Vector3& position, const Math::Vector3& target, const Math::Vector3& up) {
    // When a game camera entity is set, write to its TransformComponent so the
    // game view (which renders from CameraComponent entities) sees the update.
    // The editor Camera object stays untouched.
    if (m_GameCameraEntity != INVALID_ENTITY && m_World) {
        auto* camTransform = m_World->GetComponent<TransformComponent>(m_GameCameraEntity);
        if (camTransform) {
            camTransform->position = position;
            // Compute rotation quaternion from look-at direction
            Math::Vector3 fwd = target - position;
            f32 fwdLen = fwd.Length();
            if (fwdLen > 1e-6f) {
                fwd = fwd * (1.0f / fwdLen);
                // Camera looks down -Z, so forward in camera space is -Z
                // We need: rotation * (0,0,-1) = fwd => rotation * (0,0,1) = -fwd
                Math::Vector3 right = fwd.Cross(up);
                f32 rightLen = right.Length();
                if (rightLen > 1e-6f) {
                    right = right * (1.0f / rightLen);
                    Math::Vector3 correctedUp = right.Cross(fwd);
                    // Convert rotation matrix to quaternion (Shepperd's method)
                    // Matrix columns: right, correctedUp, -forward (camera convention)
                    f32 m00 = right.x,        m01 = correctedUp.x, m02 = -fwd.x;
                    f32 m10 = right.y,        m11 = correctedUp.y, m12 = -fwd.y;
                    f32 m20 = right.z,        m21 = correctedUp.z, m22 = -fwd.z;
                    f32 trace = m00 + m11 + m22;
                    f32 qx, qy, qz, qw;
                    if (trace > 0.0f) {
                        f32 s = std::sqrt(trace + 1.0f) * 2.0f;
                        qw = 0.25f * s;
                        qx = (m21 - m12) / s;
                        qy = (m02 - m20) / s;
                        qz = (m10 - m01) / s;
                    } else if (m00 > m11 && m00 > m22) {
                        f32 s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
                        qw = (m21 - m12) / s;
                        qx = 0.25f * s;
                        qy = (m01 + m10) / s;
                        qz = (m02 + m20) / s;
                    } else if (m11 > m22) {
                        f32 s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
                        qw = (m02 - m20) / s;
                        qx = (m01 + m10) / s;
                        qy = 0.25f * s;
                        qz = (m12 + m21) / s;
                    } else {
                        f32 s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
                        qw = (m10 - m01) / s;
                        qx = (m02 + m20) / s;
                        qy = (m12 + m21) / s;
                        qz = 0.25f * s;
                    }
                    camTransform->rotation = Math::Quaternion(qx, qy, qz, qw).Normalized();
                }
            }
            return;
        }
    }

    // Fallback: update the editor camera directly (original behavior)
    if (m_Camera) {
        m_Camera->SetPosition(position);
        m_Camera->SetLookAt(position, target, up);
    }
}

void ControllerSystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World) {
        return;
    }

    // Update all 2D Platformer controllers
    for (Entity entity : m_World->GetEntitiesWithComponent<Platformer2DController>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        // Skip unpossessed entities
        if (m_World->HasComponent<PossessableComponent>(entity)) {
            auto* possess = m_World->GetComponent<PossessableComponent>(entity);
            if (!possess->isPossessed) continue;
        }
        auto* controller = m_World->GetComponent<Platformer2DController>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (controller->isEnabled) {
            UpdatePlatformer2D(entity, *controller, *transform, deltaTime);
        }
    }

    // Update all 2D Top-Down controllers
    for (Entity entity : m_World->GetEntitiesWithComponent<TopDown2DController>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        if (m_World->HasComponent<PossessableComponent>(entity)) {
            auto* possess = m_World->GetComponent<PossessableComponent>(entity);
            if (!possess->isPossessed) continue;
        }
        auto* controller = m_World->GetComponent<TopDown2DController>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (controller->isEnabled) {
            UpdateTopDown2D(entity, *controller, *transform, deltaTime);
        }
    }

    // Update all 3D Top-Down controllers
    for (Entity entity : m_World->GetEntitiesWithComponent<TopDown3DController>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        if (m_World->HasComponent<PossessableComponent>(entity)) {
            auto* possess = m_World->GetComponent<PossessableComponent>(entity);
            if (!possess->isPossessed) continue;
        }
        auto* controller = m_World->GetComponent<TopDown3DController>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (controller->isEnabled) {
            UpdateTopDown3D(entity, *controller, *transform, deltaTime);
        }
    }

    // Update all Third Person controllers
    for (Entity entity : m_World->GetEntitiesWithComponent<ThirdPersonController>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        if (m_World->HasComponent<PossessableComponent>(entity)) {
            auto* possess = m_World->GetComponent<PossessableComponent>(entity);
            if (!possess->isPossessed) continue;
        }
        auto* controller = m_World->GetComponent<ThirdPersonController>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (controller->isEnabled) {
            UpdateThirdPerson(entity, *controller, *transform, deltaTime);
        }
    }

    // Update all First Person controllers
    for (Entity entity : m_World->GetEntitiesWithComponent<FirstPersonController>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        if (m_World->HasComponent<PossessableComponent>(entity)) {
            auto* possess = m_World->GetComponent<PossessableComponent>(entity);
            if (!possess->isPossessed) continue;
        }
        auto* controller = m_World->GetComponent<FirstPersonController>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (controller->isEnabled) {
            UpdateFirstPerson(entity, *controller, *transform, deltaTime);
        }
    }

    // Update all Vehicle controllers
    for (Entity entity : m_World->GetEntitiesWithComponent<VehicleController>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        if (m_World->HasComponent<PossessableComponent>(entity)) {
            auto* possess = m_World->GetComponent<PossessableComponent>(entity);
            if (!possess->isPossessed) continue;
        }
        auto* controller = m_World->GetComponent<VehicleController>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (controller->isEnabled) {
            UpdateVehicle(entity, *controller, *transform, deltaTime);
        }
    }

    // Process FollowTarget components (camera follow, companion follow, etc.)
    for (Entity entity : m_World->GetEntitiesWithComponent<FollowTargetComponent>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        auto* follow = m_World->GetComponent<FollowTargetComponent>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (follow->target == INVALID_ENTITY) continue;

        auto* targetTransform = m_World->GetComponent<TransformComponent>(follow->target);
        if (!targetTransform) continue;

        Math::Vector3 targetPos = targetTransform->position + follow->offset;

        // Smooth follow via lerp
        f32 lerpFactor = 1.0f - std::exp(-follow->moveSpeed * deltaTime);
        transform->position = Math::Vector3(
            transform->position.x + (targetPos.x - transform->position.x) * lerpFactor,
            transform->position.y + (targetPos.y - transform->position.y) * lerpFactor,
            transform->position.z + (targetPos.z - transform->position.z) * lerpFactor
        );
    }

    // Process LookAtTarget components (camera look-at, turret tracking, etc.)
    for (Entity entity : m_World->GetEntitiesWithComponent<LookAtTargetComponent>()) {
        if (!m_World->HasComponent<TransformComponent>(entity)) continue;
        auto* lookAt = m_World->GetComponent<LookAtTargetComponent>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);

        Math::Vector3 targetPos;
        if (lookAt->useWorldTarget) {
            targetPos = lookAt->worldTarget;
        } else {
            if (lookAt->target == INVALID_ENTITY) continue;
            auto* targetTransform = m_World->GetComponent<TransformComponent>(lookAt->target);
            if (!targetTransform) continue;
            targetPos = targetTransform->position;
        }

        // Compute direction from entity to target
        Math::Vector3 dir = Math::Vector3(
            targetPos.x - transform->position.x,
            targetPos.y - transform->position.y,
            targetPos.z - transform->position.z
        );
        f32 len = dir.Length();
        if (len < 0.001f) continue;
        dir = dir * (1.0f / len);

        // Compute yaw and pitch from direction
        f32 yaw = std::atan2(-dir.x, -dir.z);
        f32 pitch = std::asin(-dir.y);

        // Clamp pitch
        if (pitch < Math::Radians(lookAt->minPitch)) pitch = Math::Radians(lookAt->minPitch);
        if (pitch > Math::Radians(lookAt->maxPitch)) pitch = Math::Radians(lookAt->maxPitch);

        Math::Quaternion targetRotation = Math::Quaternion::FromEuler(
            Math::Vector3(pitch, yaw, 0.0f));

        if (lookAt->instant) {
            transform->rotation = targetRotation;
        } else {
            f32 t = 1.0f - std::exp(-Math::Radians(lookAt->rotationSpeed) * deltaTime);
            transform->rotation = Math::Quaternion::Slerp(transform->rotation, targetRotation, t);
        }
    }

    // Camera2D bounds follow with advanced features
    for (Entity entity : m_World->GetEntitiesWithComponent<Camera2DBoundsComponent>()) {
        auto* cam2d = m_World->GetComponent<Camera2DBoundsComponent>(entity);
        auto* camTransform = m_World->GetComponent<TransformComponent>(entity);
        if (!cam2d || !camTransform) continue;

        // Skip if no primary target
        if (cam2d->followTarget == 0) continue;
        auto* targetTransform = m_World->GetComponent<TransformComponent>(cam2d->followTarget);
        if (!targetTransform) continue;

        // 1. Gather all targets (primary + additional)
        Math::Vector2 targetPos(targetTransform->position.x, targetTransform->position.y);
        Math::Vector2 minPos = targetPos;
        Math::Vector2 maxPos = targetPos;

        // Include additional targets for multi-target framing
        for (Entity additionalTarget : cam2d->additionalTargets) {
            if (additionalTarget == 0) continue;
            auto* addTransform = m_World->GetComponent<TransformComponent>(additionalTarget);
            if (!addTransform) continue;
            minPos.x = std::min(minPos.x, addTransform->position.x);
            minPos.y = std::min(minPos.y, addTransform->position.y);
            maxPos.x = std::max(maxPos.x, addTransform->position.x);
            maxPos.y = std::max(maxPos.y, addTransform->position.y);
        }

        // 2. Compute target center (single target or multi-target bounding box center)
        Math::Vector2 targetCenter;
        if (cam2d->additionalTargets.empty()) {
            targetCenter = targetPos;
        } else {
            targetCenter.x = (minPos.x + maxPos.x) * 0.5f;
            targetCenter.y = (minPos.y + maxPos.y) * 0.5f;
        }

        // 3. Auto-zoom to fit all targets
        if (cam2d->autoZoomToFitTargets && !cam2d->additionalTargets.empty()) {
            f32 width = maxPos.x - minPos.x + cam2d->multiTargetPadding * 2.0f;
            f32 height = maxPos.y - minPos.y + cam2d->multiTargetPadding * 2.0f;
            // Get camera component for aspect ratio
            auto* camComp = m_World->GetComponent<CameraComponent>(entity);
            if (camComp && camComp->orthoSize > 0.01f) {
                f32 aspect = 16.0f / 9.0f;  // Default aspect ratio
                f32 requiredHalfHeight = std::max(height * 0.5f, width * 0.5f / aspect);
                f32 requiredZoom = camComp->orthoSize / std::max(requiredHalfHeight, 0.1f);
                cam2d->targetZoom = std::clamp(requiredZoom, cam2d->minZoom, cam2d->maxZoom);
            }
        }

        // 4. Apply look-ahead based on target velocity
        if (cam2d->lookAheadDistance > 0.0f) {
            Math::Vector2 velocity(0.0f, 0.0f);
            // Try to get velocity from rigidbody or controller
            if (auto* rb = m_World->GetComponent<RigidbodyComponent>(cam2d->followTarget)) {
                velocity.x = rb->velocity.x;
                velocity.y = rb->velocity.y;
            } else if (auto* p2d = m_World->GetComponent<Platformer2DController>(cam2d->followTarget)) {
                velocity.x = p2d->velocity.x;
                velocity.y = p2d->velocity.y;
            } else if (auto* td2d = m_World->GetComponent<TopDown2DController>(cam2d->followTarget)) {
                velocity.x = td2d->velocity.x;
                velocity.y = td2d->velocity.y;
            }
            f32 velLen = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
            if (velLen > 0.1f) {
                Math::Vector2 targetLookAhead(
                    velocity.x / velLen * cam2d->lookAheadDistance,
                    velocity.y / velLen * cam2d->lookAheadDistance
                );
                f32 lat = 1.0f - std::exp(-cam2d->lookAheadSmoothing * deltaTime);
                cam2d->currentLookAhead.x += (targetLookAhead.x - cam2d->currentLookAhead.x) * lat;
                cam2d->currentLookAhead.y += (targetLookAhead.y - cam2d->currentLookAhead.y) * lat;
            } else {
                // Decay look-ahead when not moving
                f32 lat = 1.0f - std::exp(-cam2d->lookAheadSmoothing * deltaTime);
                cam2d->currentLookAhead.x *= (1.0f - lat);
                cam2d->currentLookAhead.y *= (1.0f - lat);
            }
            targetCenter.x += cam2d->currentLookAhead.x;
            targetCenter.y += cam2d->currentLookAhead.y;
        }

        // Add follow offset
        targetCenter.x += cam2d->followOffset.x;
        targetCenter.y += cam2d->followOffset.y;

        // 5. Apply dead zone - only move camera if target exits dead zone
        Math::Vector2 adjustedTarget(camTransform->position.x, camTransform->position.y);
        Math::Vector2 halfDeadZone(cam2d->deadZoneSize.x * 0.5f, cam2d->deadZoneSize.y * 0.5f);
        Math::Vector2 relativePos(
            targetCenter.x - camTransform->position.x,
            targetCenter.y - camTransform->position.y
        );

        if (halfDeadZone.x > 0.0f || halfDeadZone.y > 0.0f) {
            if (relativePos.x > halfDeadZone.x) {
                adjustedTarget.x = targetCenter.x - halfDeadZone.x;
            } else if (relativePos.x < -halfDeadZone.x) {
                adjustedTarget.x = targetCenter.x + halfDeadZone.x;
            }
            if (relativePos.y > halfDeadZone.y) {
                adjustedTarget.y = targetCenter.y - halfDeadZone.y;
            } else if (relativePos.y < -halfDeadZone.y) {
                adjustedTarget.y = targetCenter.y + halfDeadZone.y;
            }
        } else {
            adjustedTarget = targetCenter;
        }

        // 6. Smooth follow interpolation
        f32 t = 1.0f - std::exp(-cam2d->followSmoothing * deltaTime);
        camTransform->position.x += (adjustedTarget.x - camTransform->position.x) * t;
        camTransform->position.y += (adjustedTarget.y - camTransform->position.y) * t;

        // 7. Apply bounds clamping
        if (cam2d->useBounds) {
            f32 minX = cam2d->minBounds.x + cam2d->boundsPadding;
            f32 maxX = cam2d->maxBounds.x - cam2d->boundsPadding;
            f32 minY = cam2d->minBounds.y + cam2d->boundsPadding;
            f32 maxY = cam2d->maxBounds.y - cam2d->boundsPadding;
            camTransform->position.x = std::clamp(camTransform->position.x, minX, maxX);
            camTransform->position.y = std::clamp(camTransform->position.y, minY, maxY);
        }

        // 8. Apply screen shake
        if (cam2d->shakeDuration > 0.0f) {
            cam2d->shakeTimer += deltaTime;
            cam2d->shakeDuration -= deltaTime;
            f32 decay = std::max(0.0f, cam2d->shakeDuration / (cam2d->shakeDuration + deltaTime));
            f32 shakeX = std::sin(cam2d->shakeTimer * cam2d->shakeFrequency * 6.28f) * cam2d->shakeIntensity * decay;
            f32 shakeY = std::cos(cam2d->shakeTimer * cam2d->shakeFrequency * 4.17f) * cam2d->shakeIntensity * decay;
            camTransform->position.x += shakeX;
            camTransform->position.y += shakeY;
        }

        // 9. Smooth zoom interpolation and apply to CameraComponent
        if (cam2d->zoomSmoothing > 0.0f) {
            f32 zt = 1.0f - std::exp(-cam2d->zoomSmoothing * deltaTime);
            cam2d->currentZoom += (cam2d->targetZoom - cam2d->currentZoom) * zt;
        } else {
            cam2d->currentZoom = cam2d->targetZoom;
        }
        cam2d->currentZoom = std::clamp(cam2d->currentZoom, cam2d->minZoom, cam2d->maxZoom);

        // Apply zoom to camera ortho size (only if zoom != 1.0)
        auto* camComp = m_World->GetComponent<CameraComponent>(entity);
        if (camComp && camComp->projectionType == ProjectionType::Orthographic) {
            // Store base ortho size on first frame if needed
            static f32 baseOrthoSize = camComp->orthoSize;
            if (cam2d->currentZoom > 0.01f) {
                camComp->orthoSize = baseOrthoSize / cam2d->currentZoom;
            }
        }
    }
}

Math::Vector2 ControllerSystem::GetMovementInput(const CharacterControllerBase& controller) {
    // Delegate to input action map if available
    if (m_InputMap) {
        return m_InputMap->GetMovementVector();
    }

    Math::Vector2 input(0.0f, 0.0f);

    if (controller.useWASD) {
        if (Input::IsKeyDown(KeyCode::W)) input.y += 1.0f;
        if (Input::IsKeyDown(KeyCode::S)) input.y -= 1.0f;
        if (Input::IsKeyDown(KeyCode::A)) input.x -= 1.0f;
        if (Input::IsKeyDown(KeyCode::D)) input.x += 1.0f;
    }

    if (controller.useArrowKeys) {
        if (Input::IsKeyDown(KeyCode::Up)) input.y += 1.0f;
        if (Input::IsKeyDown(KeyCode::Down)) input.y -= 1.0f;
        if (Input::IsKeyDown(KeyCode::Left)) input.x -= 1.0f;
        if (Input::IsKeyDown(KeyCode::Right)) input.x += 1.0f;
    }

    if (controller.useGamepad && Input::IsGamepadConnected(controller.gamepadIndex)) {
        // Left stick for movement
        Math::Vector2 stick = Input::GetGamepadLeftStick(controller.gamepadIndex);
        input.x += stick.x;
        input.y -= stick.y;  // GLFW Y is inverted (down = positive)

        // D-pad as digital movement
        if (Input::IsGamepadButtonDown(GamepadButton::DPadUp, controller.gamepadIndex))    input.y += 1.0f;
        if (Input::IsGamepadButtonDown(GamepadButton::DPadDown, controller.gamepadIndex))  input.y -= 1.0f;
        if (Input::IsGamepadButtonDown(GamepadButton::DPadLeft, controller.gamepadIndex))  input.x -= 1.0f;
        if (Input::IsGamepadButtonDown(GamepadButton::DPadRight, controller.gamepadIndex)) input.x += 1.0f;
    }

    // Normalize diagonal movement
    f32 length = input.Length();
    if (length > 1.0f) {
        input = input * (1.0f / length);
    }

    return input;
}

bool ControllerSystem::IsJumpPressed() {
    if (m_InputMap) return m_InputMap->IsActionPressed(InputSystem::GameAction::Jump);

    bool pressed = Input::IsKeyPressed(KeyCode::Space);
    // Check all connected gamepads for A button (jump)
    for (i32 gp = 0; gp < 4; ++gp) {
        if (Input::IsGamepadConnected(gp) && Input::IsGamepadButtonPressed(GamepadButton::A, gp)) {
            pressed = true;
        }
    }
    return pressed;
}

bool ControllerSystem::IsSprintHeld() {
    if (m_InputMap) return m_InputMap->IsActionDown(InputSystem::GameAction::Sprint);

    bool held = Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);
    for (i32 gp = 0; gp < 4; ++gp) {
        if (Input::IsGamepadConnected(gp)) {
            // Left stick click or left bumper for sprint
            if (Input::IsGamepadButtonDown(GamepadButton::LeftStick, gp) ||
                Input::IsGamepadButtonDown(GamepadButton::LeftBumper, gp)) {
                held = true;
            }
        }
    }
    return held;
}

bool ControllerSystem::IsCrouchPressed() {
    if (m_InputMap) return m_InputMap->IsActionPressed(InputSystem::GameAction::Crouch);

    bool pressed = Input::IsKeyPressed(KeyCode::LeftControl) || Input::IsKeyPressed(KeyCode::C);
    for (i32 gp = 0; gp < 4; ++gp) {
        if (Input::IsGamepadConnected(gp) && Input::IsGamepadButtonPressed(GamepadButton::B, gp)) {
            pressed = true;
        }
    }
    return pressed;
}

bool ControllerSystem::IsDashPressed() {
    if (m_InputMap) return m_InputMap->IsActionPressed(InputSystem::GameAction::Dash);

    bool pressed = Input::IsKeyPressed(KeyCode::LeftShift) || Input::IsKeyPressed(KeyCode::E);
    for (i32 gp = 0; gp < 4; ++gp) {
        if (Input::IsGamepadConnected(gp) && Input::IsGamepadButtonPressed(GamepadButton::RightBumper, gp)) {
            pressed = true;
        }
    }
    return pressed;
}

bool ControllerSystem::CheckGround(const Math::Vector3& position, f32& groundY) {
    if (m_Physics) {
        Physics::RaycastHit hit;
        if (m_Physics->CheckGround(position, 1.0f, hit)) {
            groundY = hit.point.y;
            return true;
        }
    }
    // Fallback: Y=0 plane
    if (position.y <= 0.1f) {
        groundY = 0.0f;
        return true;
    }
    return false;
}

bool ControllerSystem::UpdateGridMovement(CharacterControllerBase& ctrl, TransformComponent& transform,
                                          const Math::Vector2& input, f32 dt) {
    if (!ctrl.gridMovement) return false;

    f32 cellSize = ctrl.gridCellSize;

    if (ctrl.gridMoving) {
        // Currently moving between cells - advance the interpolation
        ctrl.gridMoveProgress += ctrl.gridMoveSpeed * dt;
        if (ctrl.gridMoveProgress >= 1.0f) {
            // Arrived at target cell
            ctrl.gridMoveProgress = 1.0f;
            transform.position = ctrl.gridMoveTarget;
            ctrl.gridMoving = false;
        } else {
            // Lerp between start and target
            f32 t = ctrl.gridMoveProgress;
            // Smooth step for nicer feel
            t = t * t * (3.0f - 2.0f * t);
            transform.position.x = ctrl.gridMoveStart.x + (ctrl.gridMoveTarget.x - ctrl.gridMoveStart.x) * t;
            transform.position.z = ctrl.gridMoveStart.z + (ctrl.gridMoveTarget.z - ctrl.gridMoveStart.z) * t;
        }
        return true;
    }

    // Not currently moving - check for new input to start a cell move
    if (Math::Abs(input.x) < 0.3f && Math::Abs(input.y) < 0.3f) {
        return true;  // No input, stay put (but still handled by grid system)
    }

    // Determine dominant direction (4-directional)
    f32 dx = 0.0f, dz = 0.0f;
    if (Math::Abs(input.x) > Math::Abs(input.y)) {
        dx = input.x > 0.0f ? cellSize : -cellSize;
    } else {
        dz = input.y > 0.0f ? cellSize : -cellSize;
    }

    // Snap current position to nearest grid cell first
    f32 ox = ctrl.gridOrigin.x;
    f32 oz = ctrl.gridOrigin.z;
    f32 snappedX = Math::Round((transform.position.x - ox) / cellSize) * cellSize + ox;
    f32 snappedZ = Math::Round((transform.position.z - oz) / cellSize) * cellSize + oz;
    transform.position.x = snappedX;
    transform.position.z = snappedZ;

    // Set up move to next cell
    ctrl.gridMoveStart = transform.position;
    ctrl.gridMoveTarget = transform.position;
    ctrl.gridMoveTarget.x += dx;
    ctrl.gridMoveTarget.z += dz;
    ctrl.gridMoveProgress = 0.0f;
    ctrl.gridMoving = true;

    return true;
}

void ControllerSystem::UpdatePlatformer2D(Entity entity, Platformer2DController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    Math::Vector2 input = GetMovementInput(ctrl);

    // Grid movement override (2D: uses X axis for horizontal, keeps Y for jump/gravity)
    if (ctrl.gridMovement && !ctrl.gridMoving && Math::Abs(input.x) > 0.3f) {
        f32 cellSize = ctrl.gridCellSize;
        f32 ox = ctrl.gridOrigin.x;
        f32 snappedX = Math::Round((transform.position.x - ox) / cellSize) * cellSize + ox;
        transform.position.x = snappedX;
        ctrl.gridMoveStart = transform.position;
        ctrl.gridMoveTarget = transform.position;
        ctrl.gridMoveTarget.x += (input.x > 0.0f ? cellSize : -cellSize);
        ctrl.gridMoveProgress = 0.0f;
        ctrl.gridMoving = true;
    }
    if (ctrl.gridMovement && ctrl.gridMoving) {
        ctrl.gridMoveProgress += ctrl.gridMoveSpeed * dt;
        if (ctrl.gridMoveProgress >= 1.0f) {
            transform.position.x = ctrl.gridMoveTarget.x;
            ctrl.gridMoving = false;
        } else {
            f32 t = ctrl.gridMoveProgress;
            t = t * t * (3.0f - 2.0f * t);
            transform.position.x = ctrl.gridMoveStart.x + (ctrl.gridMoveTarget.x - ctrl.gridMoveStart.x) * t;
        }
        // Still apply gravity/jump below, skip horizontal free movement
        input.x = 0.0f;
    }

    // Horizontal movement (X axis for 2D platformer)
    f32 targetSpeedX = input.x * ctrl.moveSpeed;
    if (IsSprintHeld()) {
        targetSpeedX *= ctrl.sprintMultiplier;
    }

    // Apply acceleration/deceleration
    f32 accel = ctrl.isGrounded ? ctrl.acceleration : ctrl.acceleration * ctrl.airControl;
    f32 decel = ctrl.isGrounded ? ctrl.deceleration : ctrl.deceleration * ctrl.airControl;

    if (Math::Abs(targetSpeedX) > 0.01f) {
        // Accelerating
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, targetSpeedX, accel * dt);
    } else {
        // Decelerating
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, decel * dt);
    }

    // Update facing direction
    if (Math::Abs(ctrl.velocity.x) > 0.1f) {
        ctrl.facingDirection = ctrl.velocity.x > 0 ? 1.0f : -1.0f;
    }

    // Coyote time (grace period after leaving ground)
    if (ctrl.isGrounded) {
        ctrl.coyoteTimer = ctrl.coyoteTime;
        ctrl.currentJumps = 0;
    } else {
        ctrl.coyoteTimer -= dt;
    }

    // Jump buffer
    if (IsJumpPressed()) {
        ctrl.jumpBufferTimer = ctrl.jumpBufferTime;
    } else {
        ctrl.jumpBufferTimer -= dt;
    }

    // Jumping
    bool canJump = (ctrl.coyoteTimer > 0.0f && ctrl.currentJumps == 0) ||
                   (ctrl.currentJumps < ctrl.maxJumps && ctrl.currentJumps > 0);

    if (ctrl.jumpBufferTimer > 0.0f && canJump) {
        ctrl.velocity.y = ctrl.jumpForce;
        ctrl.isJumping = true;
        ctrl.isGrounded = false;
        ctrl.currentJumps++;
        ctrl.jumpBufferTimer = 0.0f;
        ctrl.coyoteTimer = 0.0f;
    }

    // Apply gravity
    if (!ctrl.isGrounded) {
        ctrl.velocity.y -= ctrl.gravity * dt;
        ctrl.isFalling = ctrl.velocity.y < 0;
    }

    // Wall slide (optional)
    if (ctrl.enableWallSlide && ctrl.isWallSliding && !ctrl.isGrounded) {
        ctrl.velocity.y = Math::Max(ctrl.velocity.y, -ctrl.wallSlideSpeed);
    }

    // Apply velocity to position
    transform.position.x += ctrl.velocity.x * dt;
    transform.position.y += ctrl.velocity.y * dt;

    // Ground check via physics raycast with Y=0 fallback
    f32 groundY = 0.0f;
    if (CheckGround(transform.position, groundY) && transform.position.y <= groundY && ctrl.velocity.y <= 0.0f) {
        transform.position.y = groundY;
        ctrl.velocity.y = 0.0f;
        ctrl.isGrounded = true;
        ctrl.isJumping = false;
        ctrl.isFalling = false;
    } else if (ctrl.velocity.y < 0.0f) {
        ctrl.isGrounded = false;
    }

    // Update rotation to face movement direction
    if (ctrl.facingDirection < 0) {
        transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(180.0f));
    } else {
        transform.rotation = Math::Quaternion();
    }
}

void ControllerSystem::UpdateTopDown2D(Entity entity, TopDown2DController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Grid movement takes full control of position (XZ plane)
    if (ctrl.gridMovement) {
        Math::Vector2 input = GetMovementInput(ctrl);
        if (UpdateGridMovement(ctrl, transform, input, dt)) {
            // Update facing even in grid mode
            if (ctrl.rotateToFaceMovement && ctrl.gridMoving) {
                Math::Vector3 dir = ctrl.gridMoveTarget - ctrl.gridMoveStart;
                if (dir.x != 0.0f || dir.z != 0.0f) {
                    f32 targetAngle = Math::Degrees(Math::Atan2(dir.x, dir.z));
                    ctrl.facingAngle = targetAngle;
                    transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(ctrl.facingAngle));
                }
            }
            return;
        }
    }

    // Handle dash cooldown
    if (ctrl.dashCooldownTimer > 0.0f) {
        ctrl.dashCooldownTimer -= dt;
    }

    // Check for dash input
    if (ctrl.enableDash && IsDashPressed() && ctrl.dashCooldownTimer <= 0.0f && !ctrl.isDashing) {
        ctrl.isDashing = true;
        ctrl.dashTimer = ctrl.dashDuration;
        ctrl.dashCooldownTimer = ctrl.dashCooldown;
    }

    // Update dash
    if (ctrl.isDashing) {
        ctrl.dashTimer -= dt;
        if (ctrl.dashTimer <= 0.0f) {
            ctrl.isDashing = false;
        }
    }

    Math::Vector2 input = GetMovementInput(ctrl);

    // Calculate target velocity
    f32 speed = ctrl.moveSpeed;
    if (IsSprintHeld()) {
        speed *= ctrl.sprintMultiplier;
    }
    if (ctrl.isDashing) {
        speed = ctrl.dashSpeed;
    }

    Math::Vector2 targetVelocity = input * speed;

    // Apply acceleration/deceleration
    f32 accel = ctrl.isDashing ? 1000.0f : ctrl.acceleration;
    f32 decel = ctrl.deceleration;

    if (input.Length() > 0.01f) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, targetVelocity.x, accel * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, targetVelocity.y, accel * dt);
    } else if (!ctrl.isDashing) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, decel * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, 0.0f, decel * dt);
    }

    // Apply velocity to position (XZ plane for top-down)
    transform.position.x += ctrl.velocity.x * dt;
    transform.position.z += ctrl.velocity.z * dt;

    // Rotate to face movement direction
    if (ctrl.rotateToFaceMovement && input.Length() > 0.1f) {
        f32 targetAngle = Math::Degrees(Math::Atan2(input.x, input.y));
        ctrl.facingAngle = Math::MoveTowardsAngle(ctrl.facingAngle, targetAngle, ctrl.rotationSpeed * dt);
        transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(ctrl.facingAngle));
    }
}

void ControllerSystem::UpdateTopDown3D(Entity entity, TopDown3DController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Grid movement takes full control of position (XZ plane)
    if (ctrl.gridMovement) {
        Math::Vector2 input = GetMovementInput(ctrl);
        if (UpdateGridMovement(ctrl, transform, input, dt)) {
            // Camera follow in grid mode
            if (ctrl.lockCameraToPlayer && m_Camera) {
                f32 camAngleRad = Math::Radians(ctrl.cameraAngle);
                Math::Vector3 camOffset(0.0f, ctrl.cameraHeight, ctrl.cameraDistance);
                Math::Vector3 camPos = transform.position + camOffset;
                m_Camera->SetPosition(camPos);
                m_Camera->SetLookAt(camPos, transform.position, Math::Vector3(0, 1, 0));
            }
            return;
        }
    }

    // Handle dash cooldown
    if (ctrl.dashCooldownTimer > 0.0f) {
        ctrl.dashCooldownTimer -= dt;
    }

    // Check for dash input
    if (ctrl.enableDash && IsDashPressed() && ctrl.dashCooldownTimer <= 0.0f && !ctrl.isDashing) {
        ctrl.isDashing = true;
        ctrl.dashTimer = ctrl.dashDuration;
        ctrl.dashCooldownTimer = ctrl.dashCooldown;
    }

    // Update dash
    if (ctrl.isDashing) {
        ctrl.dashTimer -= dt;
        if (ctrl.dashTimer <= 0.0f) {
            ctrl.isDashing = false;
        }
    }

    // Handle click-to-move
    if (ctrl.enableClickToMove && Input::IsMouseButtonPressed(MouseButton::Left)) {
        // In a real implementation, you'd raycast to find world position
        // For now, we'll just use keyboard input
    }

    Math::Vector2 input = GetMovementInput(ctrl);

    // Transform input based on camera angle (so "up" is always away from camera)
    f32 cameraYaw = Math::Radians(ctrl.cameraAngle);
    f32 cosYaw = Math::Cos(cameraYaw);
    f32 sinYaw = Math::Sin(cameraYaw);

    Math::Vector2 rotatedInput;
    rotatedInput.x = input.x * cosYaw - input.y * sinYaw;
    rotatedInput.y = input.x * sinYaw + input.y * cosYaw;

    // Calculate target velocity
    f32 speed = ctrl.moveSpeed;
    if (IsSprintHeld()) {
        speed *= ctrl.sprintMultiplier;
    }
    if (ctrl.isDashing) {
        speed = ctrl.dashSpeed;
    }

    Math::Vector3 targetVelocity(rotatedInput.x * speed, 0.0f, rotatedInput.y * speed);

    // Apply acceleration/deceleration
    f32 accel = ctrl.isDashing ? 1000.0f : ctrl.acceleration;
    f32 decel = ctrl.deceleration;

    if (input.Length() > 0.01f) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, targetVelocity.x, accel * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, targetVelocity.z, accel * dt);
    } else if (!ctrl.isDashing) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, decel * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, 0.0f, decel * dt);
    }

    // Apply velocity
    transform.position = transform.position + ctrl.velocity * dt;

    // Rotate to face movement direction
    if (ctrl.rotateToFaceMovement) {
        Math::Vector2 moveDir(ctrl.velocity.x, ctrl.velocity.z);
        if (moveDir.Length() > 0.1f) {
            f32 targetAngle = Math::Degrees(Math::Atan2(moveDir.x, moveDir.y));
            f32 currentAngle = Math::Degrees(Math::Atan2(
                transform.rotation.ToMatrix().m[8],
                transform.rotation.ToMatrix().m[10]
            ));
            f32 newAngle = Math::MoveTowardsAngle(currentAngle, targetAngle, ctrl.rotationSpeed * dt);
            transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(newAngle));
        }
    }

    // Update camera position (if we have access to it)
    if ((m_Camera || m_GameCameraEntity != INVALID_ENTITY) && ctrl.lockCameraToPlayer) {
        Math::Vector3 cameraOffset(
            0.0f,
            ctrl.cameraHeight,
            ctrl.cameraDistance
        );

        Math::Vector3 cameraPos = transform.position + cameraOffset;
        UpdateGameCameraTransform(cameraPos, transform.position, Math::Vector3(0, 1, 0));
    }
}

void ControllerSystem::UpdateThirdPerson(Entity entity, ThirdPersonController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Mouse look for camera orbit
    if (!ctrl.disableMouseLook) {
        // When mouse is captured (focus mode), orbit is always active.
        // When not captured (editor), requires RMB hold.
        if (Input::IsMouseCaptured() || Input::IsMouseButtonDown(MouseButton::Right)) {
            Math::Vector2 mouseDelta = Input::GetMouseDelta();
            ctrl.cameraYaw += mouseDelta.x * ctrl.cameraSensitivity;
            ctrl.cameraPitch -= mouseDelta.y * ctrl.cameraSensitivity;
            ctrl.cameraPitch = Math::Clamp(ctrl.cameraPitch, ctrl.cameraMinPitch, ctrl.cameraMaxPitch);
        }

        // Gamepad right stick for camera orbit
        if (ctrl.useGamepad && Input::IsGamepadConnected(ctrl.gamepadIndex)) {
            Math::Vector2 rightStick = Input::GetGamepadRightStick(ctrl.gamepadIndex);
            if (rightStick.x != 0.0f || rightStick.y != 0.0f) {
                ctrl.cameraYaw += rightStick.x * ctrl.gamepadLookSensitivity * 100.0f * dt;
                ctrl.cameraPitch -= rightStick.y * ctrl.gamepadLookSensitivity * 100.0f * dt;
                ctrl.cameraPitch = Math::Clamp(ctrl.cameraPitch, ctrl.cameraMinPitch, ctrl.cameraMaxPitch);
            }
        }
    }

    // Scroll to adjust camera distance
    Math::Vector2 scroll = Input::GetScrollDelta();
    if (scroll.y != 0.0f) {
        ctrl.cameraDistance -= scroll.y * 0.5f;
        ctrl.cameraDistance = Math::Clamp(ctrl.cameraDistance, ctrl.cameraMinDistance, ctrl.cameraMaxDistance);
    }

    // Get input relative to camera
    Math::Vector2 input = GetMovementInput(ctrl);

    // Grid movement override (XZ plane, still allows camera orbit + jump/gravity)
    if (ctrl.gridMovement) {
        if (UpdateGridMovement(ctrl, transform, input, dt)) {
            // Still update camera orbit even in grid mode
            f32 yawRad2 = Math::Radians(ctrl.cameraYaw);
            f32 pitchRad2 = Math::Radians(ctrl.cameraPitch);
            Math::Vector3 camOffset;
            camOffset.x = Math::Cos(pitchRad2) * Math::Sin(yawRad2) * ctrl.cameraDistance;
            camOffset.y = Math::Sin(pitchRad2) * ctrl.cameraDistance + ctrl.cameraHeight;
            camOffset.z = Math::Cos(pitchRad2) * Math::Cos(yawRad2) * ctrl.cameraDistance;
            {
                Math::Vector3 targetCameraPos = transform.position + camOffset;
                Math::Vector3 lookTarget = transform.position + Math::Vector3(0, ctrl.cameraHeight * 0.5f, 0);

                // Apply smooth lerp (same as free movement path)
                Math::Vector3 currentPos = targetCameraPos;
                if (m_GameCameraEntity != INVALID_ENTITY && m_World) {
                    auto* camTransform = m_World->GetComponent<TransformComponent>(m_GameCameraEntity);
                    if (camTransform) currentPos = camTransform->position;
                } else if (m_Camera) {
                    currentPos = m_Camera->GetPosition();
                }
                Math::Vector3 newPos = currentPos + (targetCameraPos - currentPos) * Math::Min(ctrl.cameraLerpSpeed * dt, 1.0f);
                UpdateGameCameraTransform(newPos, lookTarget, Math::Vector3(0, 1, 0));
            }
            // Rotate character to face movement direction
            if (ctrl.rotateToFaceMovement && ctrl.gridMoving) {
                Math::Vector3 dir = ctrl.gridMoveTarget - ctrl.gridMoveStart;
                if (dir.x != 0.0f || dir.z != 0.0f) {
                    f32 angle = Math::Atan2(dir.x, dir.z);
                    transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), angle);
                }
            }
            return;
        }
    }

    // Transform input to be relative to camera direction
    f32 yawRad = Math::Radians(ctrl.cameraYaw);
    f32 cosYaw = Math::Cos(yawRad);
    f32 sinYaw = Math::Sin(yawRad);

    Math::Vector3 forward(-sinYaw, 0.0f, -cosYaw);
    Math::Vector3 right(cosYaw, 0.0f, -sinYaw);

    Math::Vector3 moveDir = forward * input.y + right * input.x;
    f32 moveMag = moveDir.Length();
    if (moveMag > 1.0f) {
        moveDir = moveDir * (1.0f / moveMag);
        moveMag = 1.0f;
    }

    // Calculate speed (check stamina if ResourceComponent exists)
    ctrl.isSprinting = IsSprintHeld() && moveMag > 0.1f;
    if (ctrl.isSprinting && m_World) {
        auto* resource = m_World->GetComponent<ResourceComponent>(entity);
        if (resource && (resource->depleted || resource->currentValue <= 0.0f)) {
            ctrl.isSprinting = false;
        } else if (resource && ctrl.isSprinting) {
            resource->TryConsume(resource->sprintCostPerSec * dt);
        }
    }
    f32 speed = ctrl.moveSpeed;
    if (ctrl.isSprinting) {
        speed *= ctrl.sprintMultiplier;
    }

    // Apply horizontal movement
    Math::Vector3 targetVelocity = moveDir * speed;

    if (moveMag > 0.01f) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, targetVelocity.x, ctrl.acceleration * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, targetVelocity.z, ctrl.acceleration * dt);
    } else {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, ctrl.deceleration * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, 0.0f, ctrl.deceleration * dt);
    }

    // Jumping
    if (IsJumpPressed() && ctrl.isGrounded) {
        ctrl.velocity.y = ctrl.jumpForce;
        ctrl.isJumping = true;
        ctrl.isGrounded = false;
    }

    // Gravity
    if (!ctrl.isGrounded) {
        ctrl.velocity.y -= ctrl.gravity * dt;
        ctrl.isFalling = ctrl.velocity.y < 0;
    }

    // Apply velocity
    transform.position = transform.position + ctrl.velocity * dt;

    // Ground check via physics raycast with Y=0 fallback
    {
        f32 groundY = 0.0f;
        if (CheckGround(transform.position, groundY) && transform.position.y <= groundY && ctrl.velocity.y <= 0.0f) {
            transform.position.y = groundY;
            ctrl.velocity.y = 0.0f;
            ctrl.isGrounded = true;
            ctrl.isJumping = false;
            ctrl.isFalling = false;
        } else if (ctrl.velocity.y < 0.0f) {
            ctrl.isGrounded = false;
        }
    }

    // Rotate character to face movement direction
    if (ctrl.rotateToFaceMovement && moveMag > 0.1f) {
        f32 targetAngle = Math::Degrees(Math::Atan2(moveDir.x, moveDir.z));
        f32 currentAngle = Math::Degrees(Math::Atan2(
            transform.rotation.ToMatrix().m[8],
            transform.rotation.ToMatrix().m[10]
        ));
        f32 newAngle = Math::MoveTowardsAngle(currentAngle, targetAngle, ctrl.rotationSpeed * dt);
        transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(newAngle));
    } else if (ctrl.rotateToFaceCamera) {
        transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(ctrl.cameraYaw));
    }

    // Update camera position — orbit directly around the player
    {
        f32 pitchRad = Math::Radians(ctrl.cameraPitch);
        f32 yawRad2 = Math::Radians(ctrl.cameraYaw);

        Math::Vector3 cameraOffset;
        cameraOffset.x = Math::Cos(pitchRad) * Math::Sin(yawRad2) * ctrl.cameraDistance;
        cameraOffset.y = Math::Sin(pitchRad) * ctrl.cameraDistance + ctrl.cameraHeight;
        cameraOffset.z = Math::Cos(pitchRad) * Math::Cos(yawRad2) * ctrl.cameraDistance;

        Math::Vector3 cameraPos = transform.position + cameraOffset;
        Math::Vector3 lookTarget = transform.position + Math::Vector3(0, ctrl.cameraHeight * 0.5f, 0);

        UpdateGameCameraTransform(cameraPos, lookTarget, Math::Vector3(0, 1, 0));
    }
}

void ControllerSystem::UpdateFirstPerson(Entity entity, FirstPersonController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Mouse look
    // In dungeon crawler mode, yaw is locked to snap turns — only allow pitch
    if (!ctrl.disableMouseLook) {
        bool lockYaw = ctrl.dungeonCrawlerMode && ctrl.gridMovement;
        if (Input::IsMouseCaptured() || Input::IsMouseButtonDown(MouseButton::Left)) {
            Math::Vector2 mouseDelta = Input::GetMouseDelta();

            if (!lockYaw) {
                ctrl.yaw -= mouseDelta.x * ctrl.mouseSensitivity;
            }
            if (ctrl.invertY) {
                ctrl.pitch -= mouseDelta.y * ctrl.mouseSensitivity;
            } else {
                ctrl.pitch += mouseDelta.y * ctrl.mouseSensitivity;
            }
            ctrl.pitch = Math::Clamp(ctrl.pitch, ctrl.minPitch, ctrl.maxPitch);
        }

        // Gamepad right stick for look
        if (ctrl.useGamepad && Input::IsGamepadConnected(ctrl.gamepadIndex)) {
            Math::Vector2 rightStick = Input::GetGamepadRightStick(ctrl.gamepadIndex);
            if (rightStick.x != 0.0f || rightStick.y != 0.0f) {
                if (!lockYaw) {
                    ctrl.yaw -= rightStick.x * ctrl.gamepadLookSensitivity * 100.0f * dt;
                }
                if (ctrl.invertY) {
                    ctrl.pitch -= rightStick.y * ctrl.gamepadLookSensitivity * 100.0f * dt;
                } else {
                    ctrl.pitch += rightStick.y * ctrl.gamepadLookSensitivity * 100.0f * dt;
                }
                ctrl.pitch = Math::Clamp(ctrl.pitch, ctrl.minPitch, ctrl.maxPitch);
            }
        }
    }

    // Crouch toggle
    if (ctrl.enableCrouch && IsCrouchPressed()) {
        ctrl.isCrouching = !ctrl.isCrouching;
    }

    // Update height for crouching
    f32 targetHeight = ctrl.isCrouching ? ctrl.crouchingHeight : ctrl.standingHeight;
    ctrl.currentHeight = Math::MoveTowards(ctrl.currentHeight, targetHeight, 5.0f * dt);

    // Get input
    Math::Vector2 input = GetMovementInput(ctrl);

    // Grid movement override (XZ plane, still allows look + jump/gravity)
    if (ctrl.gridMovement) {
        // Dungeon crawler mode: snap turns + facing-relative movement + wall collision
        if (ctrl.dungeonCrawlerMode) {
            // --- Snap turns on A/D (input.x) ---
            if (Math::Abs(input.x) > 0.3f) {
                if (!ctrl.snapTurnPending) {
                    ctrl.snapTurnPending = true;
                    if (input.x < 0.0f) {
                        ctrl.yaw -= ctrl.snapTurnAngle;  // Turn left (A key)
                    } else {
                        ctrl.yaw += ctrl.snapTurnAngle;  // Turn right (D key)
                    }
                    // Normalize yaw to 0-360
                    while (ctrl.yaw < 0.0f) ctrl.yaw += 360.0f;
                    while (ctrl.yaw >= 360.0f) ctrl.yaw -= 360.0f;
                }
            } else {
                ctrl.snapTurnPending = false;
            }

            // --- Facing-relative movement on W/S (input.y) ---
            // Convert forward/backward input into the direction the player faces
            Math::Vector2 facingInput(0.0f, 0.0f);
            if (Math::Abs(input.y) > 0.3f && !ctrl.gridMoving) {
                f32 yr = Math::Radians(ctrl.yaw);
                // Forward direction on XZ plane
                f32 fwdX = -Math::Sin(yr);
                f32 fwdZ = -Math::Cos(yr);
                // Snap to dominant cardinal direction
                if (Math::Abs(fwdX) > Math::Abs(fwdZ)) {
                    facingInput.x = fwdX > 0.0f ? 1.0f : -1.0f;
                    facingInput.y = 0.0f;
                } else {
                    facingInput.x = 0.0f;
                    facingInput.y = fwdZ > 0.0f ? 1.0f : -1.0f;
                }
                if (input.y < 0.0f) {
                    // Backward: reverse the direction
                    facingInput.x = -facingInput.x;
                    facingInput.y = -facingInput.y;
                }

                // --- Wall collision check ---
                // Compute target cell position and the midpoint (cell boundary)
                f32 cellSize = ctrl.gridCellSize;
                f32 ox = ctrl.gridOrigin.x;
                f32 oz = ctrl.gridOrigin.z;
                f32 snappedX = Math::Round((transform.position.x - ox) / cellSize) * cellSize + ox;
                f32 snappedZ = Math::Round((transform.position.z - oz) / cellSize) * cellSize + oz;
                f32 targetX = snappedX + facingInput.x * cellSize;
                f32 targetZ = snappedZ + facingInput.y * cellSize;
                // Walls sit on cell boundaries; check both the boundary midpoint and the target center
                f32 midX = (snappedX + targetX) * 0.5f;
                f32 midZ = (snappedZ + targetZ) * 0.5f;

                // Check if any entity with a BoxCollider blocks the path
                bool blocked = false;
                if (m_World) {
                    for (ECS::Entity other : m_World->GetEntitiesWithComponent<BoxColliderComponent>()) {
                        if (!m_World->HasComponent<TransformComponent>(other)) continue;
                        auto* col = m_World->GetComponent<BoxColliderComponent>(other);
                        if (col->isTrigger) continue;  // Triggers don't block
                        auto* otherT = m_World->GetComponent<TransformComponent>(other);
                        // AABB in world space
                        f32 colCX = otherT->position.x + col->center.x;
                        f32 colCZ = otherT->position.z + col->center.z;
                        f32 colHalfX = col->size.x * otherT->scale.x * 0.5f;
                        f32 colHalfZ = col->size.z * otherT->scale.z * 0.5f;
                        // Check if midpoint (boundary) or target center overlaps the collider
                        bool midHit = (midX >= colCX - colHalfX && midX <= colCX + colHalfX &&
                                       midZ >= colCZ - colHalfZ && midZ <= colCZ + colHalfZ);
                        bool tgtHit = (targetX >= colCX - colHalfX && targetX <= colCX + colHalfX &&
                                       targetZ >= colCZ - colHalfZ && targetZ <= colCZ + colHalfZ);
                        if (midHit || tgtHit) {
                            blocked = true;
                            break;
                        }
                    }
                }

                if (blocked) {
                    facingInput = Math::Vector2(0.0f, 0.0f);
                }
            }

            // Use facing-relative input for grid movement
            if (UpdateGridMovement(ctrl, transform, facingInput, dt)) {
                // Camera follows position with yaw/pitch
                Math::Vector3 eyePos = transform.position;
                eyePos.y += ctrl.currentHeight;
                f32 yr = Math::Radians(ctrl.yaw);
                f32 pr = Math::Radians(ctrl.pitch);
                Math::Vector3 fwd;
                fwd.x = -Math::Sin(yr) * Math::Cos(pr);
                fwd.y = Math::Sin(pr);
                fwd.z = -Math::Cos(yr) * Math::Cos(pr);
                UpdateGameCameraTransform(eyePos, eyePos + fwd, Math::Vector3(0, 1, 0));
            }
            // Update entity rotation to match yaw
            transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(ctrl.yaw));
            return;
        }

        // Non-crawler grid movement (original behavior)
        if (UpdateGridMovement(ctrl, transform, input, dt)) {
            // Camera follows position in grid mode
            {
                Math::Vector3 eyePos = transform.position;
                eyePos.y += ctrl.currentHeight;
                // Apply look rotation
                f32 yr = Math::Radians(ctrl.yaw);
                f32 pr = Math::Radians(ctrl.pitch);
                Math::Vector3 fwd;
                fwd.x = -Math::Sin(yr) * Math::Cos(pr);
                fwd.y = Math::Sin(pr);
                fwd.z = -Math::Cos(yr) * Math::Cos(pr);
                UpdateGameCameraTransform(eyePos, eyePos + fwd, Math::Vector3(0, 1, 0));
            }
            return;
        }
    }

    // Transform input to world space based on yaw
    f32 yawRad = Math::Radians(ctrl.yaw);
    f32 cosYaw = Math::Cos(yawRad);
    f32 sinYaw = Math::Sin(yawRad);

    Math::Vector3 forward(-sinYaw, 0.0f, -cosYaw);
    Math::Vector3 right(cosYaw, 0.0f, -sinYaw);

    Math::Vector3 moveDir = forward * input.y + right * input.x;
    f32 moveMag = moveDir.Length();
    if (moveMag > 1.0f) {
        moveDir = moveDir * (1.0f / moveMag);
        moveMag = 1.0f;
    }

    // Calculate speed (check stamina if ResourceComponent exists)
    ctrl.isSprinting = IsSprintHeld() && moveMag > 0.1f && !ctrl.isCrouching;
    if (ctrl.isSprinting && m_World) {
        auto* resource = m_World->GetComponent<ResourceComponent>(entity);
        if (resource && (resource->depleted || resource->currentValue <= 0.0f)) {
            ctrl.isSprinting = false; // Can't sprint without stamina
        } else if (resource && ctrl.isSprinting) {
            resource->TryConsume(resource->sprintCostPerSec * dt);
        }
    }
    f32 speed = ctrl.moveSpeed;
    if (ctrl.isSprinting) {
        speed *= ctrl.sprintMultiplier;
    }
    if (ctrl.isCrouching) {
        speed *= ctrl.crouchSpeed;
    }

    // Apply horizontal movement
    Math::Vector3 targetVelocity = moveDir * speed;

    if (moveMag > 0.01f) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, targetVelocity.x, ctrl.acceleration * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, targetVelocity.z, ctrl.acceleration * dt);
    } else {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, ctrl.deceleration * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, 0.0f, ctrl.deceleration * dt);
    }

    // Jumping (check stamina cost)
    if (IsJumpPressed() && ctrl.isGrounded && !ctrl.isCrouching) {
        bool canJump = true;
        if (m_World) {
            auto* resource = m_World->GetComponent<ResourceComponent>(entity);
            if (resource && resource->jumpCost > 0.0f) {
                canJump = resource->TryConsume(resource->jumpCost);
            }
        }
        if (!canJump) {} // Skip jump
        else
        ctrl.velocity.y = ctrl.jumpForce;
        ctrl.isJumping = true;
        ctrl.isGrounded = false;
    }

    // Gravity
    if (!ctrl.isGrounded) {
        ctrl.velocity.y -= ctrl.gravity * dt;
        ctrl.isFalling = ctrl.velocity.y < 0;
    }

    // Apply velocity
    transform.position = transform.position + ctrl.velocity * dt;

    // Ground check via physics raycast with Y=0 fallback
    {
        f32 groundY = 0.0f;
        if (CheckGround(transform.position, groundY) && transform.position.y <= groundY && ctrl.velocity.y <= 0.0f) {
            transform.position.y = groundY;
            ctrl.velocity.y = 0.0f;
            ctrl.isGrounded = true;
            ctrl.isJumping = false;
            ctrl.isFalling = false;
        } else if (ctrl.velocity.y < 0.0f) {
            ctrl.isGrounded = false;
        }
    }

    // Head bob (disabled when reduced motion is active)
    if (ctrl.enableHeadBob && !m_ReducedMotion && ctrl.isGrounded && moveMag > 0.1f) {
        ctrl.headBobTimer += dt * ctrl.headBobFrequency * (ctrl.isSprinting ? 1.5f : 1.0f);
    }

    // Update camera (first person camera IS the player's eyes)
    {
        Math::Vector3 eyePos = transform.position;
        eyePos.y += ctrl.currentHeight;

        // Add head bob offset
        if (ctrl.enableHeadBob) {
            eyePos.y += Math::Sin(ctrl.headBobTimer) * ctrl.headBobAmplitude;
        }

        // Calculate look direction
        f32 pitchRad = Math::Radians(ctrl.pitch);
        f32 yawRad2 = Math::Radians(ctrl.yaw);

        Math::Vector3 lookDir;
        lookDir.x = Math::Cos(pitchRad) * -Math::Sin(yawRad2);
        lookDir.y = Math::Sin(pitchRad);
        lookDir.z = Math::Cos(pitchRad) * -Math::Cos(yawRad2);

        UpdateGameCameraTransform(eyePos, eyePos + lookDir, Math::Vector3(0, 1, 0));
    }

    // Update entity rotation to match yaw (body rotation)
    transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(ctrl.yaw));
}

void ControllerSystem::UpdateVehicle(Entity entity, VehicleController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    Math::Vector2 input = GetMovementInput(ctrl);
    bool handbrakeInput = IsJumpPressed();

    // --- Throttle / Brake ---
    f32 throttle = input.y;   // W/S or left stick Y
    f32 steerInput = input.x; // A/D or left stick X

    // Determine braking: pressing opposite direction to current speed, or handbrake
    ctrl.handbrake = handbrakeInput;
    ctrl.isBraking = false;
    ctrl.isReversing = false;

    if (ctrl.handbrake) {
        // Handbrake: strong deceleration, reduced drift factor for skidding
        ctrl.isBraking = true;
        ctrl.currentSpeed = Math::MoveTowards(ctrl.currentSpeed, 0.0f, ctrl.brakeForce * 1.5f * dt);
    } else if (throttle > 0.01f) {
        // Forward throttle
        if (ctrl.currentSpeed < 0.0f) {
            // Currently reversing, apply brake first
            ctrl.isBraking = true;
            ctrl.currentSpeed = Math::MoveTowards(ctrl.currentSpeed, 0.0f, ctrl.brakeForce * dt);
        } else {
            ctrl.currentSpeed = Math::MoveTowards(ctrl.currentSpeed, ctrl.maxSpeed * throttle, ctrl.acceleration * dt);
        }
    } else if (throttle < -0.01f) {
        // Reverse / brake
        if (ctrl.currentSpeed > 0.5f) {
            // Moving forward: treat as brake
            ctrl.isBraking = true;
            ctrl.currentSpeed = Math::MoveTowards(ctrl.currentSpeed, 0.0f, ctrl.brakeForce * dt);
        } else {
            // Slow enough: allow reverse
            ctrl.isReversing = true;
            ctrl.currentSpeed = Math::MoveTowards(ctrl.currentSpeed, -ctrl.reverseMaxSpeed * (-throttle), ctrl.acceleration * 0.5f * dt);
        }
    } else {
        // No input: engine brake (coast to stop)
        ctrl.currentSpeed = Math::MoveTowards(ctrl.currentSpeed, 0.0f, ctrl.engineBrake * dt);
    }

    // --- Steering ---
    if (Math::Abs(steerInput) > 0.01f) {
        ctrl.currentSteerAngle = Math::MoveTowards(ctrl.currentSteerAngle,
            ctrl.maxSteerAngle * steerInput, ctrl.steerSpeed * dt);
    } else {
        // Auto-center steering
        ctrl.currentSteerAngle = Math::MoveTowards(ctrl.currentSteerAngle, 0.0f, ctrl.steerReturnSpeed * dt);
    }

    // Reduce max steer at high speed for stability
    f32 speedFactor = Math::Clamp(Math::Abs(ctrl.currentSpeed) / ctrl.maxSpeed, 0.0f, 1.0f);
    f32 effectiveSteer = ctrl.currentSteerAngle * (1.0f - speedFactor * 0.5f);

    // --- Bicycle model (Ackermann approximation) ---
    // turning radius = wheelBase / tan(steerAngle)
    // angular velocity = speed / turningRadius = speed * tan(steerAngle) / wheelBase
    f32 steerRad = Math::Radians(effectiveSteer);
    f32 angularVelocity = 0.0f;
    if (Math::Abs(steerRad) > 0.001f) {
        angularVelocity = ctrl.currentSpeed * std::tan(steerRad) / ctrl.wheelBase;
    }

    // Update heading
    ctrl.heading += Math::Degrees(angularVelocity * dt);
    // Normalize heading to 0-360
    while (ctrl.heading < 0.0f) ctrl.heading += 360.0f;
    while (ctrl.heading >= 360.0f) ctrl.heading -= 360.0f;

    // Compute forward direction from heading
    f32 headingRad = Math::Radians(ctrl.heading);
    ctrl.forwardDir = Math::Vector3(-Math::Sin(headingRad), 0.0f, -Math::Cos(headingRad));

    // --- Lateral velocity damping (drift) ---
    // Decompose velocity into forward and lateral components
    Math::Vector3 velocityWorld = ctrl.forwardDir * ctrl.currentSpeed;

    // Right vector (perpendicular to forward on XZ plane)
    Math::Vector3 rightDir(ctrl.forwardDir.z, 0.0f, -ctrl.forwardDir.x);

    // Project current velocity onto forward and right
    Math::Vector3 currentVel = ctrl.velocity;
    f32 forwardVel = currentVel.x * ctrl.forwardDir.x + currentVel.z * ctrl.forwardDir.z;
    f32 lateralVel = currentVel.x * rightDir.x + currentVel.z * rightDir.z;

    // Damp lateral velocity (driftFactor: 1.0 = full grip, 0.0 = ice)
    f32 driftDamp = ctrl.handbrake ? ctrl.driftFactor * 0.3f : ctrl.driftFactor;
    lateralVel *= std::pow(1.0f - driftDamp, dt * 60.0f);

    // Detect drifting
    ctrl.isDrifting = Math::Abs(lateralVel) > 1.0f;

    // Reconstruct velocity from forward speed + damped lateral
    ctrl.velocity.x = ctrl.forwardDir.x * ctrl.currentSpeed + rightDir.x * lateralVel;
    ctrl.velocity.y = 0.0f;
    ctrl.velocity.z = ctrl.forwardDir.z * ctrl.currentSpeed + rightDir.z * lateralVel;
    ctrl.lateralVelocity = rightDir * lateralVel;

    // --- Apply position ---
    transform.position.x += ctrl.velocity.x * dt;
    transform.position.z += ctrl.velocity.z * dt;

    // Ground check (keep on ground)
    f32 groundY = 0.0f;
    if (CheckGround(transform.position, groundY) && transform.position.y <= groundY + 0.1f) {
        transform.position.y = groundY;
        ctrl.isGrounded = true;
    } else {
        ctrl.isGrounded = false;
    }

    // --- Body rotation (heading + visual roll/pitch) ---
    f32 rollAngle = 0.0f;
    f32 pitchAngle = 0.0f;
    if (!m_ReducedMotion) {
        // Body roll from steering (lean into turns)
        rollAngle = -effectiveSteer / ctrl.maxSteerAngle * ctrl.bodyRollAmount * speedFactor;
        // Body pitch from acceleration/braking
        if (ctrl.isBraking) {
            pitchAngle = ctrl.bodyPitchAmount * Math::Clamp(ctrl.currentSpeed / ctrl.maxSpeed, 0.0f, 1.0f);
        } else if (Math::Abs(throttle) > 0.01f && ctrl.currentSpeed > 0.0f) {
            pitchAngle = -ctrl.bodyPitchAmount * 0.5f * throttle;
        }
    }

    // Compose rotation: yaw (heading) then pitch then roll
    Math::Quaternion yawQ = Math::Quaternion(Math::Vector3(0, 1, 0), headingRad);
    Math::Quaternion pitchQ = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(pitchAngle));
    Math::Quaternion rollQ = Math::Quaternion(Math::Vector3(0, 0, 1), Math::Radians(rollAngle));
    transform.rotation = (yawQ * pitchQ * rollQ).Normalized();

    // --- RPM (cosmetic, for sound/effects) ---
    ctrl.currentRPM = Math::Abs(ctrl.currentSpeed) / ctrl.maxSpeed * 6000.0f;
    if (ctrl.currentRPM < 800.0f) ctrl.currentRPM = 800.0f; // Idle RPM

    // --- Camera: chase cam behind vehicle ---
    {
        // Camera position: behind and above the vehicle
        Math::Vector3 camTargetPos = transform.position
            - ctrl.forwardDir * ctrl.cameraDistance
            + Math::Vector3(0.0f, ctrl.cameraHeight, 0.0f);

        // Look-ahead: shift look target forward based on speed
        Math::Vector3 lookTarget = transform.position
            + ctrl.forwardDir * ctrl.cameraLookAhead * (ctrl.currentSpeed / ctrl.maxSpeed);
        lookTarget.y += ctrl.cameraHeight * 0.3f;

        // Smooth follow via lerp
        Math::Vector3 currentCamPos = camTargetPos;
        if (m_GameCameraEntity != INVALID_ENTITY && m_World) {
            auto* camTransform = m_World->GetComponent<TransformComponent>(m_GameCameraEntity);
            if (camTransform) currentCamPos = camTransform->position;
        } else if (m_Camera) {
            currentCamPos = m_Camera->GetPosition();
        }

        f32 lerpT = 1.0f - std::exp(-ctrl.cameraLerpSpeed * dt);
        Math::Vector3 newCamPos = Math::Vector3(
            currentCamPos.x + (camTargetPos.x - currentCamPos.x) * lerpT,
            currentCamPos.y + (camTargetPos.y - currentCamPos.y) * lerpT,
            currentCamPos.z + (camTargetPos.z - currentCamPos.z) * lerpT
        );

        UpdateGameCameraTransform(newCamPos, lookTarget, Math::Vector3(0, 1, 0));
    }
}

} // namespace ECS
} // namespace Enjin
