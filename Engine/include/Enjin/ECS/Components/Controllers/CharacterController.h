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

    // Swimming. Entering a water volume below its surface turns gravity off and
    // hands vertical motion to these: each JUMP PRESS is a stroke that kicks you
    // up, and you sink gently between strokes, so staying afloat means keeping a
    // rhythm. Tunable per controller rather than baked into the system, so a
    // heavy character can be given a weaker stroke and a faster sink without
    // touching engine code.
    f32 swimSpeedScale = 0.6f;      // fraction of moveSpeed while swimming
    f32 swimStrokeImpulse = 3.2f;   // upward kick per jump press
    f32 swimSinkRate = -0.5f;       // drift while not stroking (negative = down)
    f32 swimDrag = 4.0f;            // how fast motion converges on the target
    f32 swimSurfaceBand = 0.25f;    // depth below the surface that counts as "at the top"
    f32 swimSurfaceStrokeScale = 0.28f;  // stroke strength there, so you bob instead of launching
    // Runtime: true while the chest probe is under a water surface.
    bool isSwimming = false;

    // State
    bool isEnabled = true;
    // Bullet time: run THIS controller at wall-clock rate while Time_SetScale
    // slows the world. The runtime updates flagged controllers per rendered
    // frame with dt divided by the global scale, so movement, jumps, and
    // gravity all integrate at normal speed - no compensation math needed.
    bool ignoreGlobalTimeScale = false;
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

    // When false, the game never captures/hides the cursor for this controller:
    // the cursor stays visible, camera look requires holding RMB, and on-screen
    // UI stays clickable while moving (the Web Demo template's mode).
    bool captureMouseOnClick = true;

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

    // Stomp: landing on an enemy from above kills it and bounces the player.
    // These were literals written into two separate code paths that both have
    // to agree - the damage-event route and the direct overlap sweep - so the
    // feel could not be tuned and the two could drift apart.
    f32 stompMinFallSpeed = 1.0f;   // downward speed required, world units/sec
    f32 stompMinHeight = 0.3f;      // how far above the enemy the player must be
    f32 stompBounceScale = 0.6f;    // bounce height as a fraction of jumpForce

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
    // G1 ladder climb state (runtime)
    bool isClimbing = false;
    // Height above the feet at which this character grabs a ladder rung. Was a
    // literal in the shared climb step, so every character reached the same way.
    f32 ladderGrabHeight = 1.0f;
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
    f32 prevPositionY = 0.0f;   // for WASM ground detection fallback
    bool jumpKeyWasDown = false;  // for WASM edge detection
    i32 fallFrameCount = 0;       // frames of continuous Y decrease
};

// 3D First Person Controller (camera is the player's eyes)
struct FirstPersonController : public CharacterControllerBase {
    // G1 ladder climb state (runtime)
    bool isClimbing = false;
    // Height above the feet at which this character grabs a ladder rung. Was a
    // literal in the shared climb step, so every character reached the same way.
    f32 ladderGrabHeight = 1.0f;
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

    // Dash/dodge
    bool enableDash = false;
    f32 dashSpeed = 15.0f;
    f32 dashDuration = 0.2f;
    f32 dashCooldown = 1.0f;
    f32 dashTimer = 0.0f;
    f32 dashCooldownTimer = 0.0f;
    bool isDashing = false;

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

    // Handling numbers that were literals in the update. Everything else about
    // this vehicle was authored; these four decide how it actually drives.
    f32 handbrakeScale = 1.5f;       // handbrake deceleration, as a multiple of brakeForce
    f32 reverseAccelScale = 0.5f;    // reverse acceleration, as a fraction of acceleration
    f32 reverseSpeedThreshold = 0.5f;// below this forward speed, reverse input reverses
                                     // instead of braking (units/sec)
    f32 highSpeedSteerReduction = 0.5f; // how much steering is taken away at top
                                        // speed: 0 = none, 1 = no steering at all

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

    // Model alignment: the engine's forward is -Z (right-handed, +Y up, +X
    // right — the glTF/OpenGL convention). If a car model was authored facing a
    // different local axis, spin the VISUAL body by this yaw (degrees) so its
    // nose lines up with the drive direction, without touching the physics.
    // 0 = model already faces -Z; 180 = model faces +Z; -90/+90 = faces +X/-X.
    f32 modelForwardYaw = 0.0f;

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
// ============================================================================
// Water Vehicle Controller
//
// Anything that floats and is steered by a rudder: powered boats, jet skis,
// paddled craft, sailing dinghies. This is NOT the car controller with the
// wheels taken off, and the difference is the rudder. A wheel grips the road
// whether or not you are moving; a rudder is a foil in flowing water, so a
// stopped boat does not steer at all. That single fact is most of what makes a
// boat feel like a boat, and it is why lateral grip here is low and deliberate
// (a boat always slips sideways a little) where a car's is near total.
//
// Propulsion is a mode rather than three components:
//   Motor    - throttle and reverse, optionally planing
//   Paddle   - impulse per stroke, no reverse gear
//   External - a script writes externalDrive/externalSide each frame
//
// External exists for sails. A sail model is all apparent wind, trim angle and
// point-of-sail curves, and it wants tuning in seconds. Compiled into the engine
// every tweak costs a rebuild, so the hull lives here (buoyancy, drag, the hull
// speed wall, rudder authority, leeway, heel, wake) and the wind stays in script
// where it can be hot-reloaded. See Whistland's SailingProto.as.
// ============================================================================

enum class WaterPropulsion : u8 { Motor = 0, Paddle = 1, External = 2 };

struct WaterVehicleController : public CharacterControllerBase {
    WaterPropulsion propulsion = WaterPropulsion::Motor;

    // --- Hull ---
    // Displacement hulls have a hard speed limit set by waterline length. Past
    // it the boat is climbing its own bow wave, which is what hullSpeedWall
    // models. Raising hullSpeed is the classic boat upgrade.
    f32 hullSpeed = 9.0f;
    f32 hullDragLinear = 0.09f;     // skin friction
    f32 hullDragQuad = 0.030f;      // wave making
    f32 hullSpeedWall = 0.90f;      // 0 = no wall, higher = flatter top end
    f32 lateralGrip = 6.0f;         // keel/skeg resistance to sideways slip
    f32 mass = 1.0f;                // forces are accelerations at 1

    // --- Planing ---
    // A motorboat lifts onto its bow wave and leaves hull speed behind. A
    // displacement dinghy never does. Off by default because most boats do not.
    bool canPlane = false;
    f32 planeThreshold = 0.75f;     // fraction of hullSpeed where the bow lifts
    f32 planeDragCut = 0.55f;       // drag multiplier once planing

    // --- Propulsion (Motor / Paddle) ---
    f32 maxThrust = 12.0f;
    f32 reverseThrustScale = 0.40f;
    f32 throttleRate = 2.0f;
    f32 paddleImpulse = 3.2f;       // Paddle: speed added per stroke
    f32 paddleCooldown = 0.65f;
    f32 currentThrottle = 0.0f;

    // --- Rudder ---
    f32 rudderAuthority = 62.0f;    // deg/sec at full steerage
    f32 rudderFullSpeed = 2.2f;     // water speed at which authority is reached
    f32 rudderMinAuthority = 0.16f; // floor, so dead in the water is a setback, not a softlock
    f32 rudderRate = 3.4f;          // how fast the blade swings to the input
    f32 rudderDrag = 0.022f;        // cost of steering hard; smooth is fast
    f32 currentRudder = 0.0f;

    // --- Heel and trim ---
    f32 righting = 11.0f;           // stability; low = tippy dinghy
    f32 heelDrag = 0.14f;           // a heeled hull drags its rail. This, not
                                    // spilled thrust, is what makes heel cost speed
    f32 maxHeel = 40.0f;
    f32 turnHeel = 0.55f;           // degrees of lean per deg/sec of turn
    f32 trimPitch = 1.4f;           // bow lift under thrust

    // --- Buoyancy ---
    // Rides the surface of a water volume rather than being a rigidbody, so the
    // motion is authored and predictable. Set followWaterSurface false to own Y.
    bool followWaterSurface = true;
    f32 waterLine = 0.0f;           // hull origin height relative to the surface
    f32 bobAmplitude = 0.10f;
    f32 bobRate = 1.35f;
    f32 buoyancyResponse = 6.0f;

    // --- Current ---
    // Moving water CARRIES a boat, it does not push it: drag is computed against
    // the water and this is added to position. That is what makes a tide read as
    // a tide instead of as a mysterious force, and it is why you can make hull
    // speed through the water and go backwards over the ground.
    Math::Vector3 current = Math::Vector3(0.0f, 0.0f, 0.0f);

    // --- External drive (WaterPropulsion::External) ---
    f32 externalDrive = 0.0f;       // force along forward
    f32 externalSide = 0.0f;        // force along right, + = to starboard
    f32 externalHeel = 0.0f;        // extra heeling moment, e.g. from a sail

    // --- Wake ---
    bool emitWake = true;
    f32 wakeWidth = 1.6f;
    f32 wakeMinSpeed = 0.6f;
    Entity waterEntity = 0;         // entity carrying interactiveWater; 0 = auto-find

    // --- Camera ---
    f32 cameraDistance = 9.0f;
    f32 cameraHeight = 3.4f;
    f32 cameraLerpSpeed = 6.5f;
    f32 cameraLookAhead = 5.0f;
    f32 cameraSpeedPull = 0.52f;    // extra distance per unit of speed

    // Model alignment: engine forward is -Z. Spin the visual body by this yaw if
    // the hull was authored facing another axis.
    f32 modelForwardYaw = 0.0f;

    // --- State ---
    f32 heading = 0.0f;
    f32 heel = 0.0f;
    f32 heelVel = 0.0f;
    f32 pitch = 0.0f;
    f32 speedThroughWater = 0.0f;   // what the hull and rudder feel
    f32 speedOverGround = 0.0f;     // what the racing line cares about
    f32 leeway = 0.0f;              // sideways slip
    f32 paddleTimer = 0.0f;
    f32 bobPhase = 0.0f;            // own clock, so bob is deterministic and needs no global time
    bool isPlaning = false;
    Math::Vector3 forwardDir = Math::Vector3(0.0f, 0.0f, -1.0f);
    Math::Vector3 groundVelocity = Math::Vector3(0.0f, 0.0f, 0.0f);
};

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
