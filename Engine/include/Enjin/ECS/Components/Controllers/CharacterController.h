#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"

namespace Enjin {
namespace ECS {

// Base settings shared by all character controllers
struct CharacterControllerBase {
    // Movement
    f32 moveSpeed = 5.0f;
    f32 sprintMultiplier = 2.0f;

    // State
    bool isEnabled = true;
    bool isGrounded = true;
    Math::Vector3 velocity = Math::Vector3(0.0f, 0.0f, 0.0f);

    // Input mapping (can be remapped)
    bool useWASD = true;       // Use WASD keys
    bool useArrowKeys = false; // Use arrow keys
    bool useGamepad = false;   // Use gamepad input
    i32 gamepadIndex = 0;      // Which gamepad (0-3)

    // Gamepad settings
    f32 gamepadLookSensitivity = 2.0f;  // Right stick look speed

    // Grid/tile-based movement (alternative to free movement)
    bool gridMovement = false;          // Snap to grid cells instead of free movement
    f32 gridCellSize = 1.0f;            // Size of each grid cell in world units
    f32 gridMoveSpeed = 8.0f;           // Speed of lerp between cells (cells/sec)
    f32 gridMoveProgress = 0.0f;        // 0-1 interpolation between cells
    Math::Vector3 gridOrigin = Math::Vector3(0.0f, 0.0f, 0.0f);    // Grid origin offset
    Math::Vector3 gridMoveStart = Math::Vector3(0.0f, 0.0f, 0.0f); // Start position of current move
    Math::Vector3 gridMoveTarget = Math::Vector3(0.0f, 0.0f, 0.0f);// Target position of current move
    bool gridMoving = false;            // Currently transitioning between cells
};

// 2D Platformer Controller (side-scrolling with gravity and jumping)
struct Platformer2DController : public CharacterControllerBase {
    // Jumping
    f32 jumpForce = 8.0f;
    f32 gravity = 20.0f;
    i32 maxJumps = 2;          // Double jump support
    i32 currentJumps = 0;

    // Ground detection
    f32 groundCheckDistance = 0.1f;

    // Movement feel
    f32 acceleration = 50.0f;
    f32 deceleration = 40.0f;
    f32 airControl = 0.5f;     // Reduced control in air

    // Facing direction (1 = right, -1 = left)
    f32 facingDirection = 1.0f;

    // Coyote time (grace period after leaving platform)
    f32 coyoteTime = 0.1f;
    f32 coyoteTimer = 0.0f;

    // Jump buffer (press jump slightly before landing)
    f32 jumpBufferTime = 0.1f;
    f32 jumpBufferTimer = 0.0f;

    // Wall mechanics (optional)
    bool enableWallJump = false;
    bool enableWallSlide = false;
    f32 wallSlideSpeed = 2.0f;
    f32 wallJumpForce = 6.0f;

    // State
    bool isJumping = false;
    bool isFalling = false;
    bool isWallSliding = false;
};

// 2D Top-Down Controller (overhead view, 8-directional movement)
struct TopDown2DController : public CharacterControllerBase {
    // Movement
    f32 acceleration = 30.0f;
    f32 deceleration = 25.0f;

    // Rotation
    bool rotateToFaceMovement = true;
    f32 rotationSpeed = 720.0f;  // Degrees per second

    // Dash/dodge
    bool enableDash = false;
    f32 dashSpeed = 15.0f;
    f32 dashDuration = 0.2f;
    f32 dashCooldown = 1.0f;
    f32 dashTimer = 0.0f;
    f32 dashCooldownTimer = 0.0f;
    bool isDashing = false;

    // Current facing angle (in degrees, 0 = right)
    f32 facingAngle = 0.0f;
};

// 3D Top-Down Controller (isometric/overhead 3D, like Diablo)
struct TopDown3DController : public CharacterControllerBase {
    // Movement
    f32 acceleration = 30.0f;
    f32 deceleration = 25.0f;

    // Rotation
    bool rotateToFaceMovement = true;
    f32 rotationSpeed = 720.0f;

    // Camera settings (for fixed camera angle)
    f32 cameraAngle = 45.0f;    // Angle from horizontal
    f32 cameraDistance = 15.0f;
    f32 cameraHeight = 10.0f;
    bool lockCameraToPlayer = true;

    // Click-to-move (optional, like Diablo)
    bool enableClickToMove = false;
    Math::Vector3 targetPosition = Math::Vector3(0.0f, 0.0f, 0.0f);
    bool hasTarget = false;
    f32 arrivalThreshold = 0.5f;

    // Dash/dodge
    bool enableDash = false;
    f32 dashSpeed = 15.0f;
    f32 dashDuration = 0.2f;
    f32 dashCooldown = 1.0f;
    f32 dashTimer = 0.0f;
    f32 dashCooldownTimer = 0.0f;
    bool isDashing = false;
};

// 3D Third Person Controller (camera follows behind player)
struct ThirdPersonController : public CharacterControllerBase {
    // Movement
    f32 acceleration = 30.0f;
    f32 deceleration = 25.0f;
    f32 jumpForce = 8.0f;
    f32 gravity = 20.0f;

    // Rotation
    bool rotateToFaceMovement = true;  // Character faces movement direction
    bool rotateToFaceCamera = false;   // Character always faces camera direction
    f32 rotationSpeed = 720.0f;

    // Camera orbit
    f32 cameraDistance = 5.0f;
    f32 cameraHeight = 2.0f;
    f32 cameraMinDistance = 2.0f;
    f32 cameraMaxDistance = 15.0f;
    f32 cameraPitch = 20.0f;       // Current pitch angle
    f32 cameraYaw = 0.0f;          // Current yaw angle
    f32 cameraMinPitch = -30.0f;
    f32 cameraMaxPitch = 60.0f;
    f32 cameraSensitivity = 0.15f;
    f32 cameraLerpSpeed = 20.0f;   // Smooth camera follow

    // Camera collision
    bool enableCameraCollision = true;
    f32 cameraCollisionRadius = 0.3f;

    // Lock-on targeting (optional)
    bool enableLockOn = false;
    Entity lockedTarget = 0;  // INVALID_ENTITY
    f32 lockOnRange = 20.0f;

    // State
    bool isJumping = false;
    bool isFalling = false;
    bool isSprinting = false;
};

// 3D First Person Controller (camera is the player's eyes)
struct FirstPersonController : public CharacterControllerBase {
    // Movement
    f32 acceleration = 50.0f;
    f32 deceleration = 40.0f;
    f32 jumpForce = 7.0f;
    f32 gravity = 20.0f;

    // Mouse look
    f32 mouseSensitivity = 2.0f;
    f32 pitch = 0.0f;           // Current pitch (up/down)
    f32 yaw = 0.0f;             // Current yaw (left/right)
    f32 minPitch = -89.0f;
    f32 maxPitch = 89.0f;
    bool invertY = false;

    // Head bob (optional)
    bool enableHeadBob = false;
    f32 headBobFrequency = 8.0f;
    f32 headBobAmplitude = 0.05f;
    f32 headBobTimer = 0.0f;

    // Crouching
    bool enableCrouch = true;
    f32 standingHeight = 1.8f;
    f32 crouchingHeight = 1.0f;
    f32 crouchSpeed = 0.5f;     // Movement speed multiplier when crouching
    bool isCrouching = false;
    f32 currentHeight = 1.8f;

    // Sprinting
    f32 sprintFOVIncrease = 10.0f;  // FOV increase when sprinting

    // Dungeon crawler mode (SMT-style)
    bool dungeonCrawlerMode = false; // Snap turns + facing-relative movement
    f32 snapTurnAngle = 90.0f;       // Degrees per snap turn (A/D)
    bool snapTurnPending = false;    // Prevents holding A/D from spinning

    // State
    bool isJumping = false;
    bool isFalling = false;
    bool isSprinting = false;
};

} // namespace ECS
} // namespace Enjin
