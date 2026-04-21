#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Entity.h"
#include <string>

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

    // Mouse look
    bool disableMouseLook = false;      // Disable mouse/stick camera control

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

    // Collision capsule (used for multi-ray ground/wall checks)
    f32 collisionRadius = 0.3f;    // Capsule radius (half-width)
    f32 collisionHeight = 1.0f;    // Total capsule height including caps

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

    // Moving platform carry (Celeste-style: track ground entity position delta)
    Entity ridingEntity = INVALID_ENTITY;
    Math::Vector3 ridingEntityLastPos = Math::Vector3(0.0f);
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

    // Camera framing — horizontal offset for character placement in frame
    // -1 = character on left, 0 = center, 1 = character on right
    enum class FrameSide : u8 { Left, Center, Right };
    FrameSide frameSide = FrameSide::Right;  // Over-the-shoulder default
    f32 frameHorizontalBias = 1.5f;         // How far to offset (world units)

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
    f32 prevPositionY = 0.0f;  // for WASM ground detection fallback
    bool jumpKeyWasDown = false;  // for WASM edge detection
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
    f32 sprintFOVCurrent = 0.0f;    // Current interpolated FOV offset (runtime state, not serialized)

    // Dungeon crawler mode (SMT-style)
    bool dungeonCrawlerMode = false; // Snap turns + facing-relative movement
    f32 snapTurnAngle = 90.0f;       // Degrees per snap turn (A/D)
    bool snapTurnPending = false;    // Prevents holding A/D from spinning

    // State
    bool isJumping = false;
    bool isFalling = false;
    bool isSprinting = false;
};

// Vehicle Controller (car-like physics with steering, acceleration, braking)
struct VehicleController : public CharacterControllerBase {
    // Acceleration / braking
    f32 maxSpeed = 30.0f;           // Top speed (units/sec)
    f32 reverseMaxSpeed = 10.0f;    // Top reverse speed
    f32 acceleration = 15.0f;       // Forward acceleration force
    f32 brakeForce = 25.0f;         // Brake deceleration
    f32 engineBrake = 5.0f;         // Deceleration when no input (engine drag)
    f32 currentSpeed = 0.0f;        // Current forward speed

    // Steering
    f32 maxSteerAngle = 35.0f;      // Max wheel turn angle (degrees)
    f32 steerSpeed = 120.0f;        // Steering input speed (degrees/sec)
    f32 steerReturnSpeed = 200.0f;  // Auto-center speed (degrees/sec)
    f32 currentSteerAngle = 0.0f;   // Current steering angle
    f32 wheelBase = 2.5f;           // Distance between front/rear axles

    // Physics feel
    f32 grip = 1.0f;                // Tire grip multiplier (lower = slidey)
    f32 driftFactor = 0.9f;         // Lateral velocity retention (1 = no drift, 0 = full drift)
    f32 downforceMultiplier = 0.5f; // Speed-dependent downforce
    f32 mass = 1000.0f;             // Vehicle mass (kg)

    // Camera
    f32 cameraDistance = 8.0f;
    f32 cameraHeight = 3.5f;
    f32 cameraLerpSpeed = 5.0f;     // Smooth follow speed
    f32 cameraLookAhead = 2.0f;     // Look ahead based on velocity
    f32 cameraYaw = 0.0f;
    f32 cameraPitch = 15.0f;

    // Visuals
    f32 bodyRollAmount = 5.0f;      // Degrees of body roll in turns
    f32 bodyPitchAmount = 3.0f;     // Degrees of body pitch on accel/brake

    // State
    f32 currentRPM = 0.0f;          // For engine sound/visual
    f32 heading = 0.0f;             // Current facing (yaw degrees)
    Math::Vector3 forwardDir = Math::Vector3(0.0f, 0.0f, -1.0f);
    Math::Vector3 lateralVelocity = Math::Vector3(0.0f, 0.0f, 0.0f);
    bool isBraking = false;
    bool isReversing = false;
    bool isDrifting = false;
    bool handbrake = false;
};

// Surface Aligned Controller (spherical/planet gravity, Super Mario Galaxy style)
struct SurfaceAlignedController : public CharacterControllerBase {
    // Movement
    f32 acceleration = 30.0f;
    f32 deceleration = 25.0f;
    f32 jumpForce = 10.0f;

    // Camera orbit
    f32 cameraDistance = 8.0f;
    f32 cameraHeight = 3.0f;
    f32 cameraPitch = 20.0f;
    f32 cameraYaw = 0.0f;
    f32 cameraMinPitch = -30.0f;
    f32 cameraMaxPitch = 60.0f;
    f32 cameraSensitivity = 0.15f;
    f32 cameraLerpSpeed = 15.0f;

    // Surface alignment
    f32 alignSpeed = 8.0f;             // Slerp rate toward surface normal
    f32 groundCheckDistance = 1.5f;     // Distance to check for ground

    // State
    Math::Vector3 localUp = Math::Vector3(0.0f, 1.0f, 0.0f);
    Math::Quaternion surfaceRotation;
    bool isJumping = false;
    bool isFalling = false;
};

// Possessable component — allows the player to switch which entity they control
struct PossessableComponent {
    bool isPossessed = false;        // Currently controlled by the player
    bool autoDetect = true;          // Automatically detect controller type on possess
    i32 playerIndex = 0;             // Which player (0-3) can possess this entity
    f32 possessRange = 5.0f;         // Max distance to possess (0 = unlimited)
    std::string promptText = "Press E to enter"; // UI prompt when in range

    // Transition settings
    f32 transitionDuration = 0.3f;   // Camera blend time on possess/unpossess
    bool disableOnUnpossess = true;  // Disable controller when not possessed

    // State
    Entity previousPossessor = 0;    // Entity that was possessed before this one
};

} // namespace ECS
} // namespace Enjin
