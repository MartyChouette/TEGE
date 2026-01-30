#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

void ControllerSystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World) {
        return;
    }

    // Update all 2D Platformer controllers
    for (Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<Platformer2DController>(entity) &&
            m_World->HasComponent<TransformComponent>(entity)) {
            auto* controller = m_World->GetComponent<Platformer2DController>(entity);
            auto* transform = m_World->GetComponent<TransformComponent>(entity);
            if (controller->isEnabled) {
                UpdatePlatformer2D(entity, *controller, *transform, deltaTime);
            }
        }
    }

    // Update all 2D Top-Down controllers
    for (Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<TopDown2DController>(entity) &&
            m_World->HasComponent<TransformComponent>(entity)) {
            auto* controller = m_World->GetComponent<TopDown2DController>(entity);
            auto* transform = m_World->GetComponent<TransformComponent>(entity);
            if (controller->isEnabled) {
                UpdateTopDown2D(entity, *controller, *transform, deltaTime);
            }
        }
    }

    // Update all 3D Top-Down controllers
    for (Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<TopDown3DController>(entity) &&
            m_World->HasComponent<TransformComponent>(entity)) {
            auto* controller = m_World->GetComponent<TopDown3DController>(entity);
            auto* transform = m_World->GetComponent<TransformComponent>(entity);
            if (controller->isEnabled) {
                UpdateTopDown3D(entity, *controller, *transform, deltaTime);
            }
        }
    }

    // Update all Third Person controllers
    for (Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<ThirdPersonController>(entity) &&
            m_World->HasComponent<TransformComponent>(entity)) {
            auto* controller = m_World->GetComponent<ThirdPersonController>(entity);
            auto* transform = m_World->GetComponent<TransformComponent>(entity);
            if (controller->isEnabled) {
                UpdateThirdPerson(entity, *controller, *transform, deltaTime);
            }
        }
    }

    // Update all First Person controllers
    for (Entity entity : m_World->GetAllEntities()) {
        if (m_World->HasComponent<FirstPersonController>(entity) &&
            m_World->HasComponent<TransformComponent>(entity)) {
            auto* controller = m_World->GetComponent<FirstPersonController>(entity);
            auto* transform = m_World->GetComponent<TransformComponent>(entity);
            if (controller->isEnabled) {
                UpdateFirstPerson(entity, *controller, *transform, deltaTime);
            }
        }
    }
}

Math::Vector2 ControllerSystem::GetMovementInput(const CharacterControllerBase& controller) {
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

    // Normalize diagonal movement
    f32 length = input.Length();
    if (length > 1.0f) {
        input = input * (1.0f / length);
    }

    return input;
}

bool ControllerSystem::IsJumpPressed() {
    return Input::IsKeyPressed(KeyCode::Space);
}

bool ControllerSystem::IsSprintHeld() {
    return Input::IsKeyDown(KeyCode::LeftShift) || Input::IsKeyDown(KeyCode::RightShift);
}

bool ControllerSystem::IsCrouchPressed() {
    return Input::IsKeyPressed(KeyCode::LeftControl) || Input::IsKeyPressed(KeyCode::C);
}

bool ControllerSystem::IsDashPressed() {
    return Input::IsKeyPressed(KeyCode::LeftShift) || Input::IsKeyPressed(KeyCode::E);
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

void ControllerSystem::UpdatePlatformer2D(Entity entity, Platformer2DController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    Math::Vector2 input = GetMovementInput(ctrl);

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
    if (m_Camera && ctrl.lockCameraToPlayer) {
        f32 camAngleRad = Math::Radians(ctrl.cameraAngle);
        Math::Vector3 cameraOffset(
            0.0f,
            ctrl.cameraHeight,
            ctrl.cameraDistance
        );

        Math::Vector3 cameraPos = transform.position + cameraOffset;
        m_Camera->SetPosition(cameraPos);
        m_Camera->SetLookAt(cameraPos, transform.position, Math::Vector3(0, 1, 0));
    }
}

void ControllerSystem::UpdateThirdPerson(Entity entity, ThirdPersonController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Mouse look for camera orbit
    if (Input::IsMouseButtonDown(MouseButton::Right)) {
        Math::Vector2 mouseDelta = Input::GetMouseDelta();
        ctrl.cameraYaw += mouseDelta.x * ctrl.cameraSensitivity;
        ctrl.cameraPitch -= mouseDelta.y * ctrl.cameraSensitivity;
        ctrl.cameraPitch = Math::Clamp(ctrl.cameraPitch, ctrl.cameraMinPitch, ctrl.cameraMaxPitch);
    }

    // Scroll to adjust camera distance
    Math::Vector2 scroll = Input::GetScrollDelta();
    if (scroll.y != 0.0f) {
        ctrl.cameraDistance -= scroll.y * 0.5f;
        ctrl.cameraDistance = Math::Clamp(ctrl.cameraDistance, ctrl.cameraMinDistance, ctrl.cameraMaxDistance);
    }

    // Get input relative to camera
    Math::Vector2 input = GetMovementInput(ctrl);

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

    // Calculate speed
    ctrl.isSprinting = IsSprintHeld() && moveMag > 0.1f;
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

    // Update camera position
    if (m_Camera) {
        f32 pitchRad = Math::Radians(ctrl.cameraPitch);
        f32 yawRad2 = Math::Radians(ctrl.cameraYaw);

        Math::Vector3 cameraOffset;
        cameraOffset.x = Math::Cos(pitchRad) * Math::Sin(yawRad2) * ctrl.cameraDistance;
        cameraOffset.y = Math::Sin(pitchRad) * ctrl.cameraDistance + ctrl.cameraHeight;
        cameraOffset.z = Math::Cos(pitchRad) * Math::Cos(yawRad2) * ctrl.cameraDistance;

        Math::Vector3 targetCameraPos = transform.position + cameraOffset;
        Math::Vector3 lookTarget = transform.position + Math::Vector3(0, ctrl.cameraHeight * 0.5f, 0);

        // Smooth camera follow
        Math::Vector3 currentPos = m_Camera->GetPosition();
        Math::Vector3 newPos = currentPos + (targetCameraPos - currentPos) * Math::Min(ctrl.cameraLerpSpeed * dt, 1.0f);

        m_Camera->SetPosition(newPos);
        m_Camera->SetLookAt(newPos, lookTarget, Math::Vector3(0, 1, 0));
    }
}

void ControllerSystem::UpdateFirstPerson(Entity entity, FirstPersonController& ctrl, TransformComponent& transform, f32 dt) {
    (void)entity;

    // Mouse look
    if (Input::IsMouseCaptured() || Input::IsMouseButtonDown(MouseButton::Left)) {
        Math::Vector2 mouseDelta = Input::GetMouseDelta();

        ctrl.yaw -= mouseDelta.x * ctrl.mouseSensitivity;
        if (ctrl.invertY) {
            ctrl.pitch -= mouseDelta.y * ctrl.mouseSensitivity;
        } else {
            ctrl.pitch += mouseDelta.y * ctrl.mouseSensitivity;
        }
        ctrl.pitch = Math::Clamp(ctrl.pitch, ctrl.minPitch, ctrl.maxPitch);
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

    // Calculate speed
    ctrl.isSprinting = IsSprintHeld() && moveMag > 0.1f && !ctrl.isCrouching;
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

    // Jumping
    if (IsJumpPressed() && ctrl.isGrounded && !ctrl.isCrouching) {
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

    // Head bob
    if (ctrl.enableHeadBob && ctrl.isGrounded && moveMag > 0.1f) {
        ctrl.headBobTimer += dt * ctrl.headBobFrequency * (ctrl.isSprinting ? 1.5f : 1.0f);
    }

    // Update camera (first person camera IS the player's eyes)
    if (m_Camera) {
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

        m_Camera->SetPosition(eyePos);
        m_Camera->SetLookAt(eyePos, eyePos + lookDir, Math::Vector3(0, 1, 0));
    }

    // Update entity rotation to match yaw (body rotation)
    transform.rotation = Math::Quaternion(Math::Vector3(0, 1, 0), Math::Radians(ctrl.yaw));
}

} // namespace ECS
} // namespace Enjin
