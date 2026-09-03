#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/ECS/Components/Ladder.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Door.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Physics/PhysicsTypes.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace ECS {

// Defined further down with the other gameplay-primitive helpers (ladder/swim).
static void UpdateDoors(World* world, f32 dt);

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
                    constexpr f32 eps = 1e-6f;
                    if (trace > 0.0f) {
                        f32 s = std::sqrt(trace + 1.0f) * 2.0f;
                        if (s < eps) s = eps;
                        qw = 0.25f * s;
                        qx = (m21 - m12) / s;
                        qy = (m02 - m20) / s;
                        qz = (m10 - m01) / s;
                    } else if (m00 > m11 && m00 > m22) {
                        f32 s = std::sqrt(std::max(0.0f, 1.0f + m00 - m11 - m22)) * 2.0f;
                        if (s < eps) s = eps;
                        qw = (m21 - m12) / s;
                        qx = 0.25f * s;
                        qy = (m01 + m10) / s;
                        qz = (m02 + m20) / s;
                    } else if (m11 > m22) {
                        f32 s = std::sqrt(std::max(0.0f, 1.0f + m11 - m00 - m22)) * 2.0f;
                        if (s < eps) s = eps;
                        qw = (m02 - m20) / s;
                        qx = (m01 + m10) / s;
                        qy = 0.25f * s;
                        qz = (m12 + m21) / s;
                    } else {
                        f32 s = std::sqrt(std::max(0.0f, 1.0f + m22 - m00 - m11)) * 2.0f;
                        if (s < eps) s = eps;
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

    // Fallback: drive the editor camera directly (original behavior). Disabled in the editor so a
    // controller without a game camera entity can't hijack the fly camera (e.g. drag it down while
    // the player falls). The standalone player always sets a game camera entity, so it never reaches
    // here anyway.
    if (m_Camera && m_DriveEditorCameraFallback) {
        m_Camera->SetPosition(position);
        m_Camera->SetLookAt(position, target, up);
    }
}

// Shared prologue for every controller loop, with the per-frame costs hoisted
// (audit 2026-08-31): storage pointers are fetched ONCE per loop instead of
// 2-3 type-ID hash lookups per entity, and the per-entity IsValid() (which
// takes the world mutex) only runs on frames that actually have deferred
// destructions pending. The body lambda gets (entity, controller&, transform&).
template <typename TController, typename Fn>
static void ForEachActiveController(World* world, bool realtimePass, Fn&& fn) {
    auto* store = world->GetComponentStorage<TController>();
    if (!store) return;
    auto* xformStore = world->GetComponentStorage<TransformComponent>();
    auto* possessStore = world->GetComponentStorage<PossessableComponent>();
    const bool checkValid = world->HasPendingDestructions();
    for (Entity entity : store->GetEntities()) {
        if (checkValid && !world->IsValid(entity)) continue;
        auto* controller = store->Get(entity);
        if (!controller || !controller->isEnabled) continue;
        if (controller->ignoreGlobalTimeScale != realtimePass) continue;
        auto* transform = xformStore ? xformStore->Get(entity) : nullptr;
        if (!transform) continue;
        auto* possess = possessStore ? possessStore->Get(entity) : nullptr;
        if (possess && !possess->isPossessed) continue;
        fn(entity, *controller, *transform);
    }
}

void ControllerSystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World) {
        return;
    }

    // Doors (G7): once per frame, normal pass only (the bullet-time realtime
    // pass calls Update a second time and would double-step/double-toggle).
    if (!m_RealtimePass) UpdateDoors(m_World, deltaTime);

    ForEachActiveController<Platformer2DController>(m_World, m_RealtimePass,
        [&](Entity entity, Platformer2DController& controller, TransformComponent& transform) {
            UpdatePlatformer2D(entity, controller, transform, deltaTime);
        });

    ForEachActiveController<TopDown2DController>(m_World, m_RealtimePass,
        [&](Entity entity, TopDown2DController& controller, TransformComponent& transform) {
            UpdateTopDown2D(entity, controller, transform, deltaTime);
        });

    ForEachActiveController<TopDown3DController>(m_World, m_RealtimePass,
        [&](Entity entity, TopDown3DController& controller, TransformComponent& transform) {
            UpdateTopDown3D(entity, controller, transform, deltaTime);
        });

    ForEachActiveController<ThirdPersonController>(m_World, m_RealtimePass,
        [&](Entity entity, ThirdPersonController& controller, TransformComponent& transform) {
            // Lazy-create Jolt CharacterVirtual on first use (requires physics backend)
            // Skip on web — Jolt collider queries don't work on Emscripten, use fallback path
            if (m_Physics && !m_Physics->HasCharacterController(entity)) {
                f32 radius = 0.3f, totalHalfH = 0.8f;
                if (auto* cap = m_World->GetComponent<CapsuleColliderComponent>(entity)) {
                    radius = cap->radius;
                    totalHalfH = cap->height * 0.5f + cap->radius;
                }
                m_Physics->CreateCharacterController(entity, radius, totalHalfH, transform.position);
            }
            UpdateThirdPerson(entity, controller, transform, deltaTime);
        });

    ForEachActiveController<FirstPersonController>(m_World, m_RealtimePass,
        [&](Entity entity, FirstPersonController& controller, TransformComponent& transform) {
#if !ENJIN_PLATFORM_WEB
            // Lazy-create Jolt CharacterVirtual on first use
            if (m_Physics && !m_Physics->HasCharacterController(entity)) {
                f32 radius = 0.3f, totalHalfH = 0.8f;
                if (auto* cap = m_World->GetComponent<CapsuleColliderComponent>(entity)) {
                    radius = cap->radius;
                    totalHalfH = cap->height * 0.5f + cap->radius;
                }
                m_Physics->CreateCharacterController(entity, radius, totalHalfH, transform.position);
            }
#endif
            UpdateFirstPerson(entity, controller, transform, deltaTime);
        });

    ForEachActiveController<SurfaceAlignedController>(m_World, m_RealtimePass,
        [&](Entity entity, SurfaceAlignedController& controller, TransformComponent& transform) {
            UpdateSurfaceAligned(entity, controller, transform, deltaTime);
        });

    ForEachActiveController<VehicleController>(m_World, m_RealtimePass,
        [&](Entity entity, VehicleController& controller, TransformComponent& transform) {
            UpdateVehicle(entity, controller, transform, deltaTime);
        });

    // Process FollowTarget components (camera follow, companion follow, etc.)
    // Camera-follow / look-at / 2D-camera work is per-frame presentation, not
    // per-entity simulation - skip it in the realtime (bullet-time) pass so it
    // never runs twice per frame.
    if (m_RealtimePass) return;

    for (Entity entity : m_World->GetEntitiesWithComponent<FollowTargetComponent>()) {
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;
        auto* follow = m_World->GetComponent<FollowTargetComponent>(entity);
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
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (!transform) continue;
        auto* lookAt = m_World->GetComponent<LookAtTargetComponent>(entity);

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

        // 8. Apply screen shake (skip if accessibility setting disables it)
        if (cam2d->shakeDuration > 0.0f && !m_DisableScreenShake) {
            cam2d->shakeTimer += deltaTime;
            cam2d->shakeDuration -= deltaTime;
            f32 decay = std::max(0.0f, cam2d->shakeDuration / (cam2d->shakeDuration + deltaTime));
            f32 shakeX = std::sin(cam2d->shakeTimer * cam2d->shakeFrequency * 6.28f) * cam2d->shakeIntensity * decay;
            f32 shakeY = std::cos(cam2d->shakeTimer * cam2d->shakeFrequency * 4.17f) * cam2d->shakeIntensity * decay;
            camTransform->position.x += shakeX;
            camTransform->position.y += shakeY;
        } else if (cam2d->shakeDuration > 0.0f && m_DisableScreenShake) {
            // Still decrement timer so shake expires, but don't apply offset
            cam2d->shakeDuration -= deltaTime;
        }

        // 9. Smooth zoom interpolation and apply to CameraComponent
        if (cam2d->zoomSmoothing > 0.0f) {
            f32 zt = 1.0f - std::exp(-cam2d->zoomSmoothing * deltaTime);
            cam2d->currentZoom += (cam2d->targetZoom - cam2d->currentZoom) * zt;
        } else {
            cam2d->currentZoom = cam2d->targetZoom;
        }
        cam2d->currentZoom = std::clamp(cam2d->currentZoom, std::max(cam2d->minZoom, 0.01f), cam2d->maxZoom);

        // Apply zoom to camera ortho size (only if zoom != 1.0)
        auto* camComp = m_World->GetComponent<CameraComponent>(entity);
        if (camComp && camComp->projectionType == ProjectionType::Orthographic) {
            // S4: Per-entity base ortho size (not static — would be shared across all cameras)
            if (cam2d->baseOrthoSize <= 0.0f) {
                cam2d->baseOrthoSize = camComp->orthoSize;
            }
            // Guard: clamp zoom to minimum 0.01f to prevent division by near-zero
            if (cam2d->currentZoom > 0.01f) {
                camComp->orthoSize = cam2d->baseOrthoSize / cam2d->currentZoom;
            }
        }
    }

    // Fixed-tick mode: this tick consumed the frame's latched edges/deltas.
    // Later ticks in the same frame see nothing (no double-fired jumps, no
    // double-applied mouse look); zero-tick frames keep latches for the next.
    if (m_ExternalFixedClock && !m_RealtimePass) {
        m_LatchJump = m_LatchCrouch = m_LatchDash = m_LatchPrimaryClick = false;
        m_LatchMouseDelta = Math::Vector2(0.0f, 0.0f);
        m_LatchScroll = Math::Vector2(0.0f, 0.0f);
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

void ControllerSystem::UpdateRealtime(f32 scaledFrameDt) {
    // Bullet time: flagged controllers run once per RENDERED frame at
    // wall-clock rate (frame dt with the global time scale divided back out),
    // in every mode. At scale ~0 (hard hitstop) everyone freezes, flags or no.
    f32 s = Scripting::GetTimeScale();
    if (s < 0.0001f) return;
    m_RealtimePass = true;
    Update(scaledFrameDt / s);
    m_RealtimePass = false;
}

void ControllerSystem::PumpFrameInput() {
    if (!m_ExternalFixedClock) return;
    // Once per RENDER frame: edges latch ON until a tick consumes them,
    // deltas accumulate so no mouse movement is lost on zero-tick frames.
    m_LatchJump   = m_LatchJump   || QueryJumpPressedNow();
    m_LatchCrouch = m_LatchCrouch || QueryCrouchPressedNow();
    m_LatchDash   = m_LatchDash   || QueryDashPressedNow();
    m_LatchPrimaryClick = m_LatchPrimaryClick || Input::IsMouseButtonPressed(MouseButton::Left);
    m_LatchMouseDelta = m_LatchMouseDelta + Input::GetMouseDelta();
    m_LatchScroll = m_LatchScroll + Input::GetScrollDelta();
}

Math::Vector2 ControllerSystem::GetLookDelta() {
    return (m_ExternalFixedClock && !m_RealtimePass) ? m_LatchMouseDelta : Input::GetMouseDelta();
}

Math::Vector2 ControllerSystem::GetZoomScroll() {
    return (m_ExternalFixedClock && !m_RealtimePass) ? m_LatchScroll : Input::GetScrollDelta();
}

bool ControllerSystem::IsPrimaryClickPressed() {
    return (m_ExternalFixedClock && !m_RealtimePass) ? m_LatchPrimaryClick : Input::IsMouseButtonPressed(MouseButton::Left);
}

bool ControllerSystem::IsJumpPressed() {
    if (m_ExternalFixedClock && !m_RealtimePass) return m_LatchJump;
    return QueryJumpPressedNow();
}

bool ControllerSystem::QueryJumpPressedNow() {
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
    if (m_ExternalFixedClock && !m_RealtimePass) return m_LatchCrouch;
    return QueryCrouchPressedNow();
}

bool ControllerSystem::QueryCrouchPressedNow() {
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
    if (m_ExternalFixedClock && !m_RealtimePass) return m_LatchDash;
    return QueryDashPressedNow();
}

bool ControllerSystem::QueryDashPressedNow() {
    if (m_InputMap) return m_InputMap->IsActionPressed(InputSystem::GameAction::Dash);

    bool pressed = Input::IsKeyPressed(KeyCode::LeftShift);
    for (i32 gp = 0; gp < 4; ++gp) {
        if (Input::IsGamepadConnected(gp) && Input::IsGamepadButtonPressed(GamepadButton::RightBumper, gp)) {
            pressed = true;
        }
    }
    return pressed;
}

bool ControllerSystem::CheckGround(const Math::Vector3& position, f32& groundY, ECS::Entity selfEntity) {
    if (m_Physics) {
        Physics::RaycastHit hit;
        if (m_Physics->CheckGround(position, 1.0f, hit, 0xFFFFFFFF, selfEntity)) {
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

bool ControllerSystem::CheckGround2D(const Math::Vector3& position, f32& groundY, Entity& groundEntity,
                                     f32 capsuleRadius, f32 capsuleHalfHeight) {
    if (!m_Physics2D) return false;

    // Multi-ray capsule ground check: cast 3 downward rays from the capsule bottom
    // (center, left edge, right edge) to match the capsule's curved underside.
    // Use the highest hit to ensure the capsule sits on top of geometry.
    f32 radius = capsuleRadius;
    f32 halfHeight = capsuleHalfHeight;

    // Bottom of capsule = position.y - halfHeight + radius (center of bottom semicircle)
    f32 bottomCenterY = position.y - halfHeight + radius;
    Math::Vector2 direction(0.0f, -1.0f);
    constexpr f32 kRayLength = 2.0f;

    bool found = false;
    f32 bestGroundY = -1e9f;
    Entity bestEntity = INVALID_ENTITY;

    // Cast from 3 points along the capsule bottom: left, center, right
    f32 offsets[3] = { -radius * 0.9f, 0.0f, radius * 0.9f };
    for (int i = 0; i < 3; ++i) {
        Physics::RayHit2D hit;
        Math::Vector2 origin(position.x + offsets[i], bottomCenterY);
        if (m_Physics2D->Raycast(origin, direction, kRayLength, hit)) {
            if (hit.normal.y > 0.5f && hit.point.y > bestGroundY) {
                bestGroundY = hit.point.y;
                bestEntity = hit.entity;
                found = true;
            }
        }
    }

    if (found) {
        groundY = bestGroundY;
        groundEntity = bestEntity;
    }
    return found;
}

bool ControllerSystem::CheckWall2D(const Math::Vector3& position, f32 moveDirX, f32& wallX,
                                   f32 capsuleRadius, f32 capsuleHalfHeight) {
    if (!m_Physics2D || moveDirX == 0.0f) return false;

    // Multi-ray capsule wall check: cast 3 horizontal rays from the capsule side
    // (top, middle, bottom) to detect walls along the full height.
    f32 radius = capsuleRadius;
    f32 halfHeight = capsuleHalfHeight;

    f32 sign = moveDirX > 0.0f ? 1.0f : -1.0f;
    Math::Vector2 direction(sign, 0.0f);
    // Cast distance = radius + small margin (detect walls at capsule edge)
    f32 checkDist = radius + 0.15f;

    bool found = false;
    f32 nearestWallX = sign > 0 ? 1e9f : -1e9f;

    // Cast from top, middle, and bottom of the capsule body
    f32 yOffsets[3] = {
        position.y + halfHeight - radius,   // Top of capsule body
        position.y,                          // Middle
        position.y - halfHeight + radius     // Bottom of capsule body
    };

    for (int i = 0; i < 3; ++i) {
        Physics::RayHit2D hit;
        Math::Vector2 origin(position.x, yOffsets[i]);
        if (m_Physics2D->Raycast(origin, direction, checkDist, hit)) {
            if (Math::Abs(hit.normal.x) > 0.5f) {
                // Take the nearest wall hit
                if ((sign > 0 && hit.point.x < nearestWallX) ||
                    (sign < 0 && hit.point.x > nearestWallX)) {
                    nearestWallX = hit.point.x;
                    found = true;
                }
            }
        }
    }

    if (found) {
        wallX = nearestWallX;
    }
    return found;
}

bool ControllerSystem::UpdateGridMovement(CharacterControllerBase& ctrl, TransformComponent& transform,
                                          const Math::Vector2& input, f32 dt, bool useXYPlane) {
    if (!ctrl.gridMovement) return false;

    f32 cellSize = ctrl.gridCellSize;

    // Secondary axis: Y for 2D (XY plane), Z for 3D (XZ plane)
    auto& secAxis = useXYPlane ? transform.position.y : transform.position.z;
    auto secStart = useXYPlane ? ctrl.gridMoveStart.y : ctrl.gridMoveStart.z;
    auto secTarget = useXYPlane ? ctrl.gridMoveTarget.y : ctrl.gridMoveTarget.z;
    auto secOrigin = useXYPlane ? ctrl.gridOrigin.y : ctrl.gridOrigin.z;

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
            secAxis = secStart + (secTarget - secStart) * t;
        }
        return true;
    }

    // Not currently moving - check for new input to start a cell move
    if (Math::Abs(input.x) < 0.3f && Math::Abs(input.y) < 0.3f) {
        return true;  // No input, stay put (but still handled by grid system)
    }

    // Determine dominant direction (4-directional)
    f32 dx = 0.0f, dSec = 0.0f;
    if (Math::Abs(input.x) > Math::Abs(input.y)) {
        dx = input.x > 0.0f ? cellSize : -cellSize;
    } else {
        dSec = input.y > 0.0f ? cellSize : -cellSize;
    }

    // Snap current position to nearest grid cell first
    f32 ox = ctrl.gridOrigin.x;
    f32 snappedX = Math::Round((transform.position.x - ox) / cellSize) * cellSize + ox;
    f32 snappedSec = Math::Round((secAxis - secOrigin) / cellSize) * cellSize + secOrigin;
    transform.position.x = snappedX;
    secAxis = snappedSec;

    // Set up move to next cell
    ctrl.gridMoveStart = transform.position;
    ctrl.gridMoveTarget = transform.position;
    ctrl.gridMoveTarget.x += dx;
    if (useXYPlane) ctrl.gridMoveTarget.y += dSec;
    else            ctrl.gridMoveTarget.z += dSec;
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

    // Celeste-style: move X and Y axes independently with per-axis collision checks.
    // Capsule collision dimensions from the controller
    f32 capsuleRadius = ctrl.collisionRadius;
    f32 capsuleHalfHeight = ctrl.collisionHeight * 0.5f;

    // Step 1: Apply X movement, then check for walls
    f32 moveX = ctrl.velocity.x * dt;
    if (moveX != 0.0f) {
        f32 wallX = 0.0f;
        // Tentatively apply X movement
        transform.position.x += moveX;
        // Check for wall collision at new position
        if (CheckWall2D(transform.position, moveX, wallX, capsuleRadius, capsuleHalfHeight)) {
            // Wall detected — clamp position to wall surface with capsule radius offset
            if (moveX > 0.0f) {
                transform.position.x = wallX - capsuleRadius;
            } else {
                transform.position.x = wallX + capsuleRadius;
            }
            ctrl.velocity.x = 0.0f;
            // Wall detection for wall slide/jump
            ctrl.isWallSliding = ctrl.enableWallSlide && !ctrl.isGrounded;
        } else {
            ctrl.isWallSliding = false;
        }
    }

    // Step 2: Apply Y movement, then check for ground
    transform.position.y += ctrl.velocity.y * dt;

    // Ground check via 2D physics raycast only — no 3D fallback for 2D controllers.
    // The Y=0 fallback in CheckGround creates invisible floors between level zones.
    f32 groundY = 0.0f;
    Entity groundEntity = INVALID_ENTITY;
    bool ground2D = CheckGround2D(transform.position, groundY, groundEntity, capsuleRadius, capsuleHalfHeight);
    bool groundHit = ground2D;
    // For 2D raycasts, the hit point is the surface top. Standing offset = capsule half-height
    // so the capsule bottom sits on the surface.
    f32 standingY = ground2D ? (groundY + capsuleHalfHeight) : groundY;
    if (groundHit && transform.position.y <= standingY && ctrl.velocity.y <= 0.0f) {
        transform.position.y = standingY;
        ctrl.velocity.y = 0.0f;
        ctrl.isGrounded = true;
        ctrl.isJumping = false;
        ctrl.isFalling = false;
    } else if (!groundHit || transform.position.y > standingY + 0.1f) {
        // No ground detected below, or player is above the ground surface —
        // start falling (handles walking off ledges where velocity.y is still 0)
        ctrl.isGrounded = false;
    }

    // Celeste-style platform carrying: if riding a moving entity, apply its position delta
    if (ctrl.isGrounded && groundEntity != INVALID_ENTITY && m_World) {
        auto* platTransform = m_World->GetComponent<TransformComponent>(groundEntity);
        if (platTransform) {
            if (ctrl.ridingEntity == groundEntity) {
                // Same platform as last frame — apply its movement delta to the player
                Math::Vector3 platDelta = platTransform->position - ctrl.ridingEntityLastPos;
                transform.position.x += platDelta.x;
                transform.position.y += platDelta.y;
            }
            // Track this platform for next frame
            ctrl.ridingEntity = groundEntity;
            ctrl.ridingEntityLastPos = platTransform->position;
        }
    } else {
        // Not riding anything
        ctrl.ridingEntity = INVALID_ENTITY;
    }

    // Flip sprite horizontally via scale.x (2D mirror, not 3D rotation)
    if (ctrl.facingDirection < 0) {
        transform.scale.x = -Math::Abs(transform.scale.x);
    } else {
        transform.scale.x = Math::Abs(transform.scale.x);
    }
}

void ControllerSystem::UpdateTopDown2D(Entity entity, TopDown2DController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Grid movement takes full control of position (XY plane for 2D)
    if (ctrl.gridMovement) {
        Math::Vector2 input = GetMovementInput(ctrl);
        if (UpdateGridMovement(ctrl, transform, input, dt, true)) {
            // Update facing even in grid mode
            if (ctrl.rotateToFaceMovement && ctrl.gridMoving) {
                Math::Vector3 dir = ctrl.gridMoveTarget - ctrl.gridMoveStart;
                if (dir.x != 0.0f || dir.y != 0.0f) {
                    f32 targetAngle = Math::Degrees(Math::Atan2(dir.x, dir.y));
                    ctrl.facingAngle = targetAngle;
                    transform.rotation = Math::Quaternion::FromEuler(Math::Vector3(0, 0, Math::Radians(-ctrl.facingAngle)));
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
        ctrl.velocity.y = Math::MoveTowards(ctrl.velocity.y, targetVelocity.y, accel * dt);
    } else if (!ctrl.isDashing) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, decel * dt);
        ctrl.velocity.y = Math::MoveTowards(ctrl.velocity.y, 0.0f, decel * dt);
    }

    // Apply velocity to position (XY plane for 2D top-down)
    transform.position.x += ctrl.velocity.x * dt;
    transform.position.y += ctrl.velocity.y * dt;

    // Rotate to face movement direction (Z axis for 2D)
    if (ctrl.rotateToFaceMovement && input.Length() > 0.1f) {
        f32 targetAngle = Math::Degrees(Math::Atan2(input.x, input.y));
        ctrl.facingAngle = Math::MoveTowardsAngle(ctrl.facingAngle, targetAngle, ctrl.rotationSpeed * dt);
        transform.rotation = Math::Quaternion::FromEuler(Math::Vector3(0, 0, Math::Radians(-ctrl.facingAngle)));
    }
}

void ControllerSystem::UpdateTopDown3D(Entity entity, TopDown3DController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Grid movement takes full control of position (XZ plane)
    if (ctrl.gridMovement) {
        Math::Vector2 input = GetMovementInput(ctrl);
        input.y = -input.y;  // camera looks down -Z, so up input = -Z (away from camera)
        if (UpdateGridMovement(ctrl, transform, input, dt)) {
            // Camera follow in grid mode — route through UpdateGameCameraTransform so it drives the
            // game camera entity (and respects the editor-camera fallback gate) instead of writing
            // the editor fly camera directly.
            if (ctrl.lockCameraToPlayer) {
                Math::Vector3 camOffset(0.0f, ctrl.cameraHeight, ctrl.cameraDistance);
                Math::Vector3 camPos = transform.position + camOffset;
                UpdateGameCameraTransform(camPos, transform.position, Math::Vector3(0, 1, 0));
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
    if (ctrl.enableClickToMove && IsPrimaryClickPressed()) {
        // In a real implementation, you'd raycast to find world position
        // For now, we'll just use keyboard input
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

    // The follow camera sits behind the player at +Z looking toward -Z with no
    // yaw, so camera-relative movement is a direct mapping: up input = -Z (away
    // from camera), right input = +X. cameraAngle is the camera's pitch from
    // horizontal, NOT a yaw — it must not rotate movement input (doing so was
    // the isometric template's "controls don't match camera" bug).
    Math::Vector3 targetVelocity(input.x * speed, 0.0f, -input.y * speed);

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
            // Extract yaw from forward direction (avoids full ToMatrix decomposition)
            Math::Vector3 fwd = transform.rotation.GetForward();
            f32 currentAngle = Math::Degrees(Math::Atan2(-fwd.x, -fwd.z));
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

// G1: is this world position inside a ladder volume? Returns the ladder and
// its top height so the climb code can mantle at the top.
static bool FindLadderAt(World* world, const Math::Vector3& pos,
                         const LadderComponent** outLadder, f32* outTopY) {
    for (Entity e : world->GetEntitiesWithComponent<LadderComponent>()) {
        auto* lad = world->GetComponent<LadderComponent>(e);
        auto* lt = world->GetComponent<TransformComponent>(e);
        if (!lad || !lt) continue;
        Math::Vector3 d = pos - lt->position;
        if (d.x >= -lad->halfExtents.x && d.x <= lad->halfExtents.x &&
            d.y >= -lad->halfExtents.y && d.y <= lad->halfExtents.y &&
            d.z >= -lad->halfExtents.z && d.z <= lad->halfExtents.z) {
            if (outLadder) *outLadder = lad;
            if (outTopY) *outTopY = lt->position.y + lad->halfExtents.y;
            return true;
        }
    }
    return false;
}

// Doors (G7): swing animation + E-to-toggle for any controller in range.
// Lives here because this system has input + world in every runtime. Called
// once per ControllerSystem::Update (non-realtime pass only - the realtime
// bullet-time pass runs Update a second time).
static void UpdateDoors(World* world, f32 dt) {
    // Gather controller positions once (any FP/TP character can open doors).
    Math::Vector3 users[8];
    u32 userCount = 0;
    for (Entity e : world->GetEntitiesWithComponent<ThirdPersonController>()) {
        if (userCount >= 8) break;
        if (auto* tf = world->GetComponent<TransformComponent>(e)) users[userCount++] = tf->position;
    }
    for (Entity e : world->GetEntitiesWithComponent<FirstPersonController>()) {
        if (userCount >= 8) break;
        if (auto* tf = world->GetComponent<TransformComponent>(e)) users[userCount++] = tf->position;
    }

    bool interactPressed = Input::IsKeyPressed(KeyCode::E);

    for (Entity e : world->GetEntitiesWithComponent<DoorComponent>()) {
        auto* door = world->GetComponent<DoorComponent>(e);
        auto* tf = world->GetComponent<TransformComponent>(e);
        if (!door || !tf) continue;

        if (!door->initialized) {
            door->baseRotation = tf->rotation;
            door->open = door->startOpen;
            door->currentAngle = door->open ? door->openAngle : 0.0f;
            door->initialized = true;
        }

        // Toggle when a character in range presses E.
        if (interactPressed && !door->locked) {
            for (u32 i = 0; i < userCount; ++i) {
                Math::Vector3 d = users[i] - tf->position;
                d.y = 0.0f;
                if (d.LengthSquared() <= door->interactRadius * door->interactRadius) {
                    door->open = !door->open;
                    door->closeTimer = 0.0f;
                    break;
                }
            }
        }

        // Auto-close countdown.
        if (door->open && door->autoCloseDelay > 0.0f) {
            door->closeTimer += dt;
            if (door->closeTimer >= door->autoCloseDelay) door->open = false;
        }

        // Swing toward the target angle; the hinge is this entity's origin.
        f32 target = door->open ? door->openAngle : 0.0f;
        f32 step = door->openSpeed * dt;
        f32 delta = target - door->currentAngle;
        if (Math::Abs(delta) <= step) door->currentAngle = target;
        else door->currentAngle += (delta > 0 ? step : -step);
        tf->rotation = door->baseRotation *
                       Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(door->currentAngle));
    }
}

// Swimming (the water ask, 2026-08-30): is this position inside a water
// volume, below its surface? Mirrors JoltBackend's buoyancy-zone rule:
// WaterVolume surface = the entity's Y, bottom = Y - 2*halfY, footprint =
// the XZ half extents.
static bool FindWaterAt(World* world, const Math::Vector3& pos, f32* outSurfaceY) {
    for (Entity e : world->GetEntitiesWithComponent<WaterVolumeComponent>()) {
        auto* wv = world->GetComponent<WaterVolumeComponent>(e);
        auto* tf = world->GetComponent<TransformComponent>(e);
        if (!wv || !tf) continue;
        if (pos.x < tf->position.x - wv->halfExtents.x || pos.x > tf->position.x + wv->halfExtents.x) continue;
        if (pos.z < tf->position.z - wv->halfExtents.z || pos.z > tf->position.z + wv->halfExtents.z) continue;
        f32 surf = tf->position.y;
        f32 bottom = surf - wv->halfExtents.y * 2.0f;
        if (pos.y >= bottom && pos.y <= surf) {
            if (outSurfaceY) *outSurfaceY = surf;
            return true;
        }
    }
    return false;
}

// Shared swim step for FP/TP controllers. Returns true while swimming (the
// caller skips its jump and gravity blocks). Space (held) rises, forward
// input swims level, everything drags like water. Rising past the surface
// hands back to normal movement, so you bob out at the shore.
template <typename CtrlT>
static bool UpdateSwim(World* world, CtrlT& ctrl, TransformComponent& tf,
                       const Math::Vector3& moveDir, f32 moveMag, f32 dt) {
    // Chest probe: swim engages when the chest is underwater, so wading
    // stays walking and swimming starts where it should.
    f32 surfaceY = 0.0f;
    Math::Vector3 probe = tf.position + Math::Vector3(0.0f, 0.9f, 0.0f);
    if (!world || !FindWaterAt(world, probe, &surfaceY)) {
        ctrl.isSwimming = false;
        return false;
    }
    ctrl.isSwimming = true;
    ctrl.isGrounded = false;
    ctrl.isJumping = false;
    ctrl.isFalling = false;

    // Water drag pulls all motion toward the swim target quickly.
    const f32 kDrag = 4.0f;
    f32 swimSpeed = ctrl.moveSpeed * 0.6f;
    Math::Vector3 target = moveDir * (swimSpeed * moveMag);

    // Vertical: Space rises, otherwise a gentle sink. Rising is capped at
    // the surface (the FindWaterAt probe leaving the water exits the state).
    bool rise = Input::IsKeyDown(KeyCode::Space);
    target.y = rise ? swimSpeed : -0.5f;
    if (rise && probe.y > surfaceY - 0.25f) target.y = 0.6f;   // gentle breach at the top

    ctrl.velocity = ctrl.velocity + (target - ctrl.velocity) * Math::Min(kDrag * dt, 1.0f);
    return true;
}

// G1: shared ladder-climb step for FP/TP controllers (same field names).
// Returns true while the climb owns vertical motion this frame — the caller
// must then skip its jump and gravity blocks. Rules: overlap + forward push
// grabs on; forward climbs, back descends; jump (if allowed) pushes off;
// pushing past the top mantles with a boost; reaching the floor while
// descending lets go. The probe point is chest height (feet + 1).
template <typename CtrlT>
static bool UpdateLadderClimb(World* world, CtrlT& ctrl, TransformComponent& transform,
                              const Math::Vector2& input, bool jumpInput, f32 dt) {
    const LadderComponent* ladder = nullptr;
    f32 topY = 0.0f;
    Math::Vector3 probe = transform.position + Math::Vector3(0.0f, 1.0f, 0.0f);
    bool inLadder = world && FindLadderAt(world, probe, &ladder, &topY);

    if (!ctrl.isClimbing) {
        if (!(inLadder && input.y > 0.1f)) return false;
        ctrl.isClimbing = true;
    }
    if (!inLadder) {                              // slid out the side or bottom
        ctrl.isClimbing = false;
        return false;
    }
    if (ctrl.isGrounded && input.y < -0.1f) {     // climbed down onto the floor
        ctrl.isClimbing = false;
        return false;
    }
    if (jumpInput && ladder->allowJumpOff) {      // push off
        ctrl.isClimbing = false;
        ctrl.velocity.y = ctrl.jumpForce * 0.7f;
        ctrl.isJumping = true;
        ctrl.isGrounded = false;
        return true;
    }
    // Mantle: pushing up with the chest at the top edge. The window is wider
    // than one climb step (climbSpeed*dt) so a frame can't tunnel past it.
    if (input.y > 0.1f && probe.y >= topY - 0.2f) {
        ctrl.isClimbing = false;
        ctrl.velocity.y = ladder->topBoost;
        ctrl.isJumping = false;
        ctrl.isGrounded = false;
        return true;
    }
    // Steady climb: vertical from forward/back input, horizontal damped out.
    ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, ctrl.deceleration * 2.0f * dt);
    ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, 0.0f, ctrl.deceleration * 2.0f * dt);
    ctrl.velocity.y = input.y * ladder->climbSpeed;
    ctrl.isGrounded = false;
    ctrl.isJumping = false;
    ctrl.isFalling = false;
    return true;
}

void ControllerSystem::UpdateThirdPerson(Entity entity, ThirdPersonController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Mouse look for camera orbit
    if (!ctrl.disableMouseLook) {
        // When mouse is captured (focus mode), orbit is always active.
        // When not captured (editor), requires RMB hold.
        if (Input::IsMouseCaptured() || Input::IsMouseButtonDown(MouseButton::Right)) {
            Math::Vector2 mouseDelta = GetLookDelta() * MouseSensitivityScale();
            ctrl.cameraYaw += mouseDelta.x * ctrl.cameraSensitivity;  // horizontal look (Marty's preferred convention)
            f32 pitchSign = m_InvertMouseY ? 1.0f : -1.0f;
            ctrl.cameraPitch += mouseDelta.y * ctrl.cameraSensitivity * pitchSign;
            ctrl.cameraPitch = Math::Clamp(ctrl.cameraPitch, ctrl.cameraMinPitch, ctrl.cameraMaxPitch);
        }

        // Gamepad right stick for camera orbit
        if (ctrl.useGamepad && Input::IsGamepadConnected(ctrl.gamepadIndex)) {
            Math::Vector2 rightStick = Input::GetGamepadRightStick(ctrl.gamepadIndex);
            if (rightStick.x != 0.0f || rightStick.y != 0.0f) {
                ctrl.cameraYaw += rightStick.x * ctrl.gamepadLookSensitivity * 100.0f * dt;
                f32 gpPitchSign = m_InvertMouseY ? 1.0f : -1.0f;
                ctrl.cameraPitch += rightStick.y * ctrl.gamepadLookSensitivity * 100.0f * dt * gpPitchSign;
                ctrl.cameraPitch = Math::Clamp(ctrl.cameraPitch, ctrl.cameraMinPitch, ctrl.cameraMaxPitch);
            }
        }
    }

    // Scroll to adjust camera distance
    Math::Vector2 scroll = GetZoomScroll();
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

    // Transform input to be relative to camera view direction.
    // When over-the-shoulder framing is active, the screen center points slightly
    // off from the raw camera yaw. Derive forward from the actual camera-to-lookTarget
    // vector so pressing W moves exactly where the player is looking on screen.
    f32 yawRad = Math::Radians(ctrl.cameraYaw);
    f32 cosYaw = Math::Cos(yawRad);
    f32 sinYaw = Math::Sin(yawRad);

    Math::Vector3 forward(-sinYaw, 0.0f, -cosYaw);
    Math::Vector3 right(cosYaw, 0.0f, -sinYaw);

    // Adjust for over-the-shoulder horizontal bias
    f32 hBias = 0.0f;
    if (ctrl.frameSide == ThirdPersonController::FrameSide::Right) hBias = ctrl.frameHorizontalBias;
    else if (ctrl.frameSide == ThirdPersonController::FrameSide::Left) hBias = -ctrl.frameHorizontalBias;
    if (hBias != 0.0f) {
        f32 pitchRad = Math::Radians(ctrl.cameraPitch);
        Math::Vector3 camRight(cosYaw, 0.0f, -sinYaw);
        Math::Vector3 cameraOffset;
        cameraOffset.x = Math::Cos(pitchRad) * sinYaw * ctrl.cameraDistance;
        cameraOffset.z = Math::Cos(pitchRad) * cosYaw * ctrl.cameraDistance;
        Math::Vector3 camPos = cameraOffset + camRight * hBias;
        Math::Vector3 lookOff = camRight * hBias * 0.3f;
        Math::Vector3 viewDir = lookOff - camPos;  // lookTarget(0,0,0) + lookOff - (player(0,0,0) + camPos)
        f32 viewLen = Math::Sqrt(viewDir.x * viewDir.x + viewDir.z * viewDir.z);
        if (viewLen > 1e-4f) {
            forward = Math::Vector3(viewDir.x / viewLen, 0.0f, viewDir.z / viewLen);
            right = Math::Vector3(-forward.z, 0.0f, forward.x);
        }
    }

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
    // WASM workaround: IsKeyPressed (edge detection) doesn't work on Emscripten because
    // key callbacks fire before Input::Update copies state. Use IsKeyDown + manual flag instead.
    bool jumpInput = IsJumpPressed();
#if ENJIN_PLATFORM_WEB
    bool spaceDown = Input::IsKeyDown(static_cast<KeyCode>(32));
    if (spaceDown && !ctrl.jumpKeyWasDown) jumpInput = true;
    ctrl.jumpKeyWasDown = spaceDown;
#endif

    // G1 ladder: while climbing, the climb owns velocity.y (jump/gravity skip).
    bool climbing = UpdateLadderClimb(m_World, ctrl, transform, input, jumpInput, dt);
    // Swimming: inside a water volume below its surface (ladder wins if both).
    bool swimming = !climbing && UpdateSwim(m_World, ctrl, transform, moveDir, moveMag, dt);
    if (climbing) ctrl.isSwimming = false;

    if (jumpInput && ctrl.isGrounded && !climbing) {
        ctrl.velocity.y = ctrl.jumpForce;
        ctrl.isJumping = true;
        ctrl.isGrounded = false;
    }

    // Gravity
    if (!ctrl.isGrounded && !climbing && !swimming) {
        ctrl.velocity.y -= ctrl.gravity * dt;
        ctrl.isFalling = ctrl.velocity.y < 0;
    }

    // Character controller movement via Jolt CharacterVirtual
    if (m_Physics && m_Physics->HasCharacterController(entity)) {
        auto state = m_Physics->UpdateCharacterController(entity, ctrl.velocity, dt);
        transform.position = state.position;

        // Update ground state from physics
        bool physicsGrounded = (state.groundState == Physics::IPhysicsBackend::CharacterGroundState::OnGround ||
                                state.groundState == Physics::IPhysicsBackend::CharacterGroundState::OnSteepGround);
        // WASM workaround: CharacterVirtual reports InAir for valid surfaces.
        // Use raycast to detect actual ground below the character.
        // Only check if not already falling fast (prevents re-grounding mid-fall).
        if (!physicsGrounded && !ctrl.isJumping && ctrl.velocity.y > -2.0f) {
            f32 groundY = 0.0f;
            if (CheckGround(state.position, groundY, entity)) {
                f32 distToGround = state.position.y - groundY;
                if (distToGround < 0.15f) physicsGrounded = true;
            }
        }

        if (physicsGrounded && ctrl.velocity.y <= 0.0f) {
            ctrl.isGrounded = true;
            ctrl.isJumping = false;
            ctrl.isFalling = false;
            ctrl.velocity.y = 0.0f;
        } else if (!physicsGrounded) {
            ctrl.isGrounded = false;
            ctrl.isFalling = ctrl.velocity.y < 0.0f;
        }
    } else {
        // Fallback: direct position update with raycast ground check
        transform.position = transform.position + ctrl.velocity * dt;

        f32 groundY = 0.0f;
        bool onGround = CheckGround(transform.position, groundY, entity);

        if (onGround && transform.position.y <= groundY + 0.05f && ctrl.velocity.y <= 0.0f) {
            transform.position.y = groundY;
            ctrl.velocity.y = 0.0f;
            ctrl.isGrounded = true;
            ctrl.isJumping = false;
            ctrl.isFalling = false;
        } else if (ctrl.velocity.y < 0.0f || (onGround && transform.position.y > groundY + 0.1f)) {
            ctrl.isGrounded = false;
        }
    }

    // Rotate character to face movement direction
    if (ctrl.rotateToFaceMovement && moveMag > 0.1f) {
        f32 targetAngle = Math::Degrees(Math::Atan2(moveDir.x, moveDir.z));
        // Extract yaw from forward direction (avoids full ToMatrix decomposition)
        Math::Vector3 fwd = transform.rotation.GetForward();
        f32 currentAngle = Math::Degrees(Math::Atan2(-fwd.x, -fwd.z));
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

        // Apply horizontal framing bias (over-the-shoulder offset)
        f32 hBias = 0.0f;
        if (ctrl.frameSide == ThirdPersonController::FrameSide::Right) hBias = ctrl.frameHorizontalBias;
        else if (ctrl.frameSide == ThirdPersonController::FrameSide::Left) hBias = -ctrl.frameHorizontalBias;
        // Compute camera right vector from yaw to offset horizontally
        Math::Vector3 camRight(Math::Cos(yawRad2), 0.0f, -Math::Sin(yawRad2));

        Math::Vector3 cameraPos = transform.position + cameraOffset + camRight * hBias;
        Math::Vector3 lookTarget = transform.position + Math::Vector3(0, ctrl.cameraHeight * 0.5f, 0) + camRight * hBias * 0.3f;

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
            Math::Vector2 mouseDelta = GetLookDelta() * MouseSensitivityScale();

            if (!lockYaw) {
                ctrl.yaw -= mouseDelta.x * ctrl.mouseSensitivity;
            }
            // XOR per-controller invertY with global accessibility invertMouseY.
            // Mouse delta is screen-space (y grows downward), pitch is positive-up,
            // so the non-inverted default must subtract.
            bool effectiveInvertY = ctrl.invertY != m_InvertMouseY;
            if (effectiveInvertY) {
                ctrl.pitch += mouseDelta.y * ctrl.mouseSensitivity;
            } else {
                ctrl.pitch -= mouseDelta.y * ctrl.mouseSensitivity;
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
                // XOR per-controller invertY with global accessibility invertMouseY
                // (same sign convention as mouse — see above)
                bool effectiveInvertYGP = ctrl.invertY != m_InvertMouseY;
                if (effectiveInvertYGP) {
                    ctrl.pitch += rightStick.y * ctrl.gamepadLookSensitivity * 100.0f * dt;
                } else {
                    ctrl.pitch -= rightStick.y * ctrl.gamepadLookSensitivity * 100.0f * dt;
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
                        auto* col = m_World->GetComponent<BoxColliderComponent>(other);
                        if (!col || col->isTrigger) continue;  // Triggers don't block
                        auto* otherT = m_World->GetComponent<TransformComponent>(other);
                        if (!otherT) continue;
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
    if (ctrl.isDashing) {
        speed = ctrl.dashSpeed;
        // Dash straight ahead when there is no movement input
        if (moveMag <= 0.01f) {
            moveDir = forward;
            moveMag = 1.0f;
        }
    }

    // Apply horizontal movement
    Math::Vector3 targetVelocity = moveDir * speed;

    // Dash snaps to dash speed near-instantly (camera is welded to the body, so it comes along 1:1)
    f32 accel = ctrl.isDashing ? 1000.0f : ctrl.acceleration;

    if (moveMag > 0.01f) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, targetVelocity.x, accel * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, targetVelocity.z, accel * dt);
    } else if (!ctrl.isDashing) {
        ctrl.velocity.x = Math::MoveTowards(ctrl.velocity.x, 0.0f, ctrl.deceleration * dt);
        ctrl.velocity.z = Math::MoveTowards(ctrl.velocity.z, 0.0f, ctrl.deceleration * dt);
    }

    // G1 ladder: while climbing, the climb owns velocity.y (jump/gravity skip).
    bool jumpInput = IsJumpPressed();
    bool climbing = UpdateLadderClimb(m_World, ctrl, transform, input, jumpInput, dt);
    // Swimming: inside a water volume below its surface (ladder wins if both).
    bool swimming = !climbing && UpdateSwim(m_World, ctrl, transform, moveDir, moveMag, dt);
    if (climbing) ctrl.isSwimming = false;

    // Jumping (check stamina cost)
    if (jumpInput && ctrl.isGrounded && !ctrl.isCrouching && !climbing && !swimming) {
        bool canJump = true;
        if (m_World) {
            auto* resource = m_World->GetComponent<ResourceComponent>(entity);
            if (resource && resource->jumpCost > 0.0f) {
                canJump = resource->TryConsume(resource->jumpCost);
            }
        }
        if (canJump) {
            ctrl.velocity.y = ctrl.jumpForce;
            ctrl.isJumping = true;
            ctrl.isGrounded = false;
        }
    }

    // Gravity
    if (!ctrl.isGrounded && !climbing && !swimming) {
        ctrl.velocity.y -= ctrl.gravity * dt;
        ctrl.isFalling = ctrl.velocity.y < 0;
    }

    // Apply velocity via Jolt CharacterVirtual (handles wall + ground collisions)
    // Falls back to direct position update when physics is unavailable.
    if (m_Physics && m_Physics->HasCharacterController(entity)) {
        auto state = m_Physics->UpdateCharacterController(entity, ctrl.velocity, dt);
        transform.position = state.position;

        // Update ground state from physics
        if (state.groundState == Physics::IPhysicsBackend::CharacterGroundState::OnGround) {
            if (ctrl.velocity.y <= 0.0f) {
                ctrl.velocity.y = 0.0f;
            }
            ctrl.isGrounded = true;
            ctrl.isJumping = false;
            ctrl.isFalling = false;
        } else {
            ctrl.isGrounded = false;
        }
    } else {
        // Fallback: direct position update (no wall collision)
        transform.position = transform.position + ctrl.velocity * dt;

        // Ground check via physics raycast with Y=0 fallback
        f32 groundY = 0.0f;
        if (CheckGround(transform.position, groundY, entity) && transform.position.y <= groundY && ctrl.velocity.y <= 0.0f) {
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

        // Sprint FOV effect (disabled by accessibility disableFOVEffects or reducedMotion)
        if (ctrl.sprintFOVIncrease > 0.0f && !m_DisableFOVEffects && !m_ReducedMotion && m_World) {
            f32 prevFOV = ctrl.sprintFOVCurrent;
            if (ctrl.isSprinting) {
                ctrl.sprintFOVCurrent = Math::Min(ctrl.sprintFOVCurrent + dt * ctrl.sprintFOVIncrease * 4.0f, ctrl.sprintFOVIncrease);
            } else {
                ctrl.sprintFOVCurrent = Math::Max(ctrl.sprintFOVCurrent - dt * ctrl.sprintFOVIncrease * 4.0f, 0.0f);
            }
            // Apply delta to game camera FOV
            f32 fovDelta = ctrl.sprintFOVCurrent - prevFOV;
            if (fovDelta != 0.0f && m_GameCameraEntity != INVALID_ENTITY && m_World->IsValid(m_GameCameraEntity)) {
                auto* camComp = m_World->GetComponent<CameraComponent>(m_GameCameraEntity);
                if (camComp) {
                    camComp->fieldOfView += fovDelta;
                }
            }
        } else if (ctrl.sprintFOVCurrent > 0.0f) {
            // Remove any remaining FOV offset when feature is disabled
            if (m_GameCameraEntity != INVALID_ENTITY && m_World && m_World->IsValid(m_GameCameraEntity)) {
                auto* camComp = m_World->GetComponent<CameraComponent>(m_GameCameraEntity);
                if (camComp) {
                    camComp->fieldOfView -= ctrl.sprintFOVCurrent;
                }
            }
            ctrl.sprintFOVCurrent = 0.0f;
        }
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

    // Right vector, engine-consistent (Rotate(+X) about +Y). For forward
    // F = (-sin h, 0, -cos h) this gives (cos h, 0, -sin h) = +X at heading 0,
    // matching Quaternion::GetRight(). The old form was sign-flipped (left-
    // handed); harmless for the symmetric drift math below but wrong as a basis.
    Math::Vector3 rightDir(-ctrl.forwardDir.z, 0.0f, ctrl.forwardDir.x);

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
    if (CheckGround(transform.position, groundY, entity) && transform.position.y <= groundY + 0.1f) {
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

    // Compose rotation: heading yaw, then the model-forward alignment offset
    // (so an oddly-authored model's nose lines up with -Z drive), then pitch/roll.
    Math::Quaternion yawQ = Math::Quaternion(Math::Vector3(0, 1, 0), headingRad);
    Math::Quaternion modelQ = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(ctrl.modelForwardYaw));
    Math::Quaternion pitchQ = Math::Quaternion(Math::Vector3(1, 0, 0), Math::Radians(pitchAngle));
    Math::Quaternion rollQ = Math::Quaternion(Math::Vector3(0, 0, 1), Math::Radians(rollAngle));
    transform.rotation = (yawQ * modelQ * pitchQ * rollQ).Normalized();

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

void ControllerSystem::UpdateSurfaceAligned(Entity entity, SurfaceAlignedController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Find the closest overlapping Point-mode gravity zone by distance to surface.
    // This allows natural gravity transfer between planets when airborne.
    Math::Vector3 gravity(0.0f, -9.81f, 0.0f);
    if (m_World) {
        f32 bestDist = 1e9f;
        for (Entity zone : m_World->GetEntitiesWithComponent<GravityZoneComponent>()) {
            auto* gz = m_World->GetComponent<GravityZoneComponent>(zone);
            if (!gz || !gz->isActive || gz->mode != GravityZoneMode::Point) continue;
            auto* zt = m_World->GetComponent<TransformComponent>(zone);
            if (!zt) continue;
            if (!gz->ContainsPoint(zt->position, transform.position)) continue;

            f32 dist = (zt->position - transform.position).Length();
            if (dist < bestDist) {
                gravity = gz->GetGravityAt(zt->position, transform.position);
                bestDist = dist;
            }
        }
    }

    // Compute local up from gravity direction
    f32 gravLen = gravity.Length();
    if (gravLen > 0.001f) {
        ctrl.localUp = gravity * (-1.0f / gravLen);
    }

    // Build target surface rotation aligning +Y to localUp. FromToRotation
    // aligns only the up axis and leaves yaw arbitrary, so crossing a planet's
    // pole (localUp flipping toward -Y) made the tangent frame spin/jump. Build
    // a yaw-STABLE rotation instead: keep the previous frame's tangent forward,
    // re-project it onto the new tangent plane, and LookRotation from it. This
    // stays continuous over the whole sphere, poles included.
    Math::Vector3 prevFwd = ctrl.surfaceRotation.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f));
    Math::Vector3 tangentFwd = prevFwd - ctrl.localUp * ctrl.localUp.Dot(prevFwd);
    if (tangentFwd.LengthSquared() < 1e-4f) {
        // Previous forward parallel to the new up: reseed from the previous right.
        Math::Vector3 prevRight = ctrl.surfaceRotation.GetRight();
        tangentFwd = prevRight - ctrl.localUp * ctrl.localUp.Dot(prevRight);
        if (tangentFwd.LengthSquared() < 1e-4f) {
            Math::Vector3 ref = (Math::Abs(ctrl.localUp.y) < 0.99f)
                ? Math::Vector3(0, 1, 0) : Math::Vector3(1, 0, 0);
            tangentFwd = ref - ctrl.localUp * ctrl.localUp.Dot(ref);
        }
    }
    Math::Quaternion targetRot = Math::Quaternion::LookRotation(tangentFwd, ctrl.localUp);

    // On first frame (surfaceRotation is identity/default), snap immediately
    f32 dotCheck = ctrl.surfaceRotation.x * ctrl.surfaceRotation.x +
                   ctrl.surfaceRotation.y * ctrl.surfaceRotation.y +
                   ctrl.surfaceRotation.z * ctrl.surfaceRotation.z;
    if (dotCheck < 0.0001f && ctrl.surfaceRotation.w > 0.999f) {
        ctrl.surfaceRotation = targetRot; // snap on first frame
    } else {
        f32 slerpT = 1.0f - std::exp(-ctrl.alignSpeed * dt);
        ctrl.surfaceRotation = Math::Quaternion::Slerp(ctrl.surfaceRotation, targetRot, slerpT);
    }
    ctrl.surfaceRotation = ctrl.surfaceRotation.Normalized();

    // Camera input: mouse/gamepad -> cameraYaw/cameraPitch (same as ThirdPerson)
    if (!ctrl.disableMouseLook) {
        if (Input::IsMouseCaptured() || Input::IsMouseButtonDown(MouseButton::Right)) {
            Math::Vector2 mouseDelta = GetLookDelta() * MouseSensitivityScale();
            ctrl.cameraYaw += mouseDelta.x * ctrl.cameraSensitivity;
            f32 saPitchSign = m_InvertMouseY ? 1.0f : -1.0f;
            ctrl.cameraPitch += mouseDelta.y * ctrl.cameraSensitivity * saPitchSign;
            ctrl.cameraPitch = Math::Clamp(ctrl.cameraPitch, ctrl.cameraMinPitch, ctrl.cameraMaxPitch);
        }

        if (ctrl.useGamepad && Input::IsGamepadConnected(ctrl.gamepadIndex)) {
            Math::Vector2 rightStick = Input::GetGamepadRightStick(ctrl.gamepadIndex);
            if (rightStick.x != 0.0f || rightStick.y != 0.0f) {
                ctrl.cameraYaw += rightStick.x * ctrl.gamepadLookSensitivity * 100.0f * dt;
                f32 saGpPitchSign = m_InvertMouseY ? 1.0f : -1.0f;
                ctrl.cameraPitch += rightStick.y * ctrl.gamepadLookSensitivity * 100.0f * dt * saGpPitchSign;
                ctrl.cameraPitch = Math::Clamp(ctrl.cameraPitch, ctrl.cameraMinPitch, ctrl.cameraMaxPitch);
            }
        }
    }

    // Movement input
    Math::Vector2 input = GetMovementInput(ctrl);

    // Build tangent frame from surfaceRotation (using Rotate() avoids full ToMatrix)
    Math::Vector3 surfaceRight = ctrl.surfaceRotation.GetRight();
    Math::Vector3 surfaceForward = ctrl.surfaceRotation.Rotate(Math::Vector3(0, 0, 1));

    // Rotate tangent frame by camera yaw
    f32 yawRad = Math::Radians(ctrl.cameraYaw);
    f32 cosYaw = Math::Cos(yawRad);
    f32 sinYaw = Math::Sin(yawRad);
    Math::Vector3 forward = surfaceForward * (-cosYaw) + surfaceRight * (-sinYaw);
    Math::Vector3 right = surfaceForward * sinYaw + surfaceRight * (-cosYaw);

    // Compute movement direction on surface
    Math::Vector3 moveDir = forward * input.y + right * input.x;
    f32 moveMag = moveDir.Length();
    if (moveMag > 1.0f) {
        moveDir = moveDir * (1.0f / moveMag);
        moveMag = 1.0f;
    }

    // Speed
    f32 speed = ctrl.moveSpeed;
    if (IsSprintHeld() && moveMag > 0.1f) {
        speed *= ctrl.sprintMultiplier;
    }

    // Find the CLOSEST gravity zone by distance to its surface (allows planet transfer)
    Math::Vector3 planetCenter(0, 0, 0);
    f32 planetRadius = 5.0f;
    f32 capsuleOffset = 0.8f;
    bool hasZone = false;
    f32 closestSurfaceDist = 1e9f;

    auto* capsuleCol = m_World ? m_World->GetComponent<CapsuleColliderComponent>(entity) : nullptr;
    if (capsuleCol) {
        capsuleOffset = capsuleCol->height * 0.5f + capsuleCol->radius;
    }

    if (m_World) {
        for (Entity zone : m_World->GetEntitiesWithComponent<GravityZoneComponent>()) {
            auto* gz = m_World->GetComponent<GravityZoneComponent>(zone);
            if (!gz || !gz->isActive || gz->mode != GravityZoneMode::Point) continue;
            auto* zt = m_World->GetComponent<TransformComponent>(zone);
            if (!zt) continue;
            if (!gz->ContainsPoint(zt->position, transform.position)) continue;

            f32 zoneRadius = gz->halfExtents.x * 0.1f;
            auto* sphereCol = m_World->GetComponent<SphereColliderComponent>(zone);
            if (sphereCol) zoneRadius = sphereCol->radius;

            Math::Vector3 toCenter = zt->position - transform.position;
            f32 distToSurface = toCenter.Length() - zoneRadius;
            if (distToSurface < closestSurfaceDist) {
                closestSurfaceDist = distToSurface;
                planetCenter = zt->position;
                planetRadius = zoneRadius;
                hasZone = true;
            }
        }
    }

    f32 standRadius = planetRadius + capsuleOffset;

    if (ctrl.isGrounded && hasZone) {
        // GROUNDED: slide along sphere surface at fixed radius
        if (moveMag > 0.01f) {
            f32 arcSpeed = speed * dt / standRadius;
            Math::Vector3 pos = transform.position - planetCenter;
            Math::Vector3 posN = pos.Normalized();
            Math::Vector3 rotAxis = posN.Cross(moveDir);
            f32 axisLen = rotAxis.Length();
            if (axisLen > 0.001f) {
                rotAxis = rotAxis * (1.0f / axisLen);
                Math::Quaternion arcRot(rotAxis, arcSpeed);
                pos = arcRot.Rotate(pos);
                transform.position = planetCenter + pos.Normalized() * standRadius;
            }
        }

        // Re-derive localUp from new position
        Math::Vector3 fromCenter = transform.position - planetCenter;
        f32 fromDist = fromCenter.Length();
        if (fromDist > 0.001f) {
            ctrl.localUp = fromCenter * (1.0f / fromDist);
        }

        // Jump
        if (IsJumpPressed()) {
            ctrl.velocity = ctrl.localUp * ctrl.jumpForce;
            ctrl.isJumping = true;
            ctrl.isGrounded = false;
        } else {
            ctrl.velocity = Math::Vector3(0, 0, 0);
        }
    } else {
        // AIRBORNE: apply gravity from closest zone, free movement
        ctrl.velocity = ctrl.velocity + gravity * dt;
        transform.position = transform.position + ctrl.velocity * dt;

        // Re-derive localUp from gravity
        if (gravLen > 0.001f) {
            ctrl.localUp = gravity * (-1.0f / gravLen);
        }

        // Check if we've reached ANY planet surface (allows transfer between bodies)
        if (m_World) {
            for (Entity zone : m_World->GetEntitiesWithComponent<GravityZoneComponent>()) {
                auto* gz = m_World->GetComponent<GravityZoneComponent>(zone);
                if (!gz || !gz->isActive || gz->mode != GravityZoneMode::Point) continue;
                auto* zt = m_World->GetComponent<TransformComponent>(zone);
                if (!zt) continue;

                f32 zoneRadius = gz->halfExtents.x * 0.1f;
                auto* sphereCol = m_World->GetComponent<SphereColliderComponent>(zone);
                if (sphereCol) zoneRadius = sphereCol->radius;

                f32 zoneStandRadius = zoneRadius + capsuleOffset;
                Math::Vector3 toCenter = zt->position - transform.position;
                f32 dist = toCenter.Length();
                if (dist <= zoneStandRadius) {
                    // Land on this planet
                    Math::Vector3 outward = (transform.position - zt->position).Normalized();
                    transform.position = zt->position + outward * zoneStandRadius;
                    ctrl.localUp = outward;
                    ctrl.velocity = Math::Vector3(0, 0, 0);
                    ctrl.isGrounded = true;
                    ctrl.isJumping = false;
                    ctrl.isFalling = false;
                    break;
                }
            }
        }

        if (!ctrl.isGrounded) ctrl.isFalling = true;
    }

    // Orient entity: snap rotation to surface on first frame, slerp after
    transform.rotation = ctrl.surfaceRotation;

    // Camera: orbit around player, using localUp as the up vector
    {
        f32 pitchRad = Math::Radians(ctrl.cameraPitch);
        f32 yawRad2 = Math::Radians(ctrl.cameraYaw);

        // Camera offset in surface-local space
        Math::Vector3 localOffset;
        localOffset.x = Math::Cos(pitchRad) * Math::Sin(yawRad2) * ctrl.cameraDistance;
        localOffset.y = Math::Sin(pitchRad) * ctrl.cameraDistance + ctrl.cameraHeight;
        localOffset.z = Math::Cos(pitchRad) * Math::Cos(yawRad2) * ctrl.cameraDistance;

        // Transform offset to world space using surface rotation (Rotate() avoids full ToMatrix)
        Math::Vector3 worldOffset = ctrl.surfaceRotation.Rotate(localOffset);

        Math::Vector3 cameraPos = transform.position + worldOffset;
        Math::Vector3 lookTarget = transform.position + ctrl.localUp * ctrl.cameraHeight * 0.5f;

        UpdateGameCameraTransform(cameraPos, lookTarget, ctrl.localUp);
    }
}

} // namespace ECS
} // namespace Enjin
