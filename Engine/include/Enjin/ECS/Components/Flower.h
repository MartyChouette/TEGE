#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include <vector>

namespace Enjin {
namespace ECS {

// Per-vertex spring deformation for petals/leaves
struct JellyMeshComponent {
    // Authored parameters
    f32 springStiffness = 80.0f;
    f32 damping = 5.0f;
    f32 maxStretch = 0.3f;

    // Runtime state (managed by FlowerSystem)
    std::vector<Math::Vector3> restPositions;
    std::vector<Math::Vector3> velocities;
    bool initialized = false;
    bool meshDirty = false;
};

// Tether connection from petal/leaf to stem/crown. Break detection is distance/speed-based
// (matching Iris XYTetherJoint reference), not dependent on physics backend joint breaking.
struct TetherComponent {
    // Authored parameters
    Entity stemEntity = INVALID_ENTITY;         // Stem entity for scoring
    Entity connectedEntity = INVALID_ENTITY;    // Direct connection target (crown for petals, stem for leaves/crown)
    Math::Vector3 attachLocalPos = Math::Vector3(0, 0, 0);

    // Break criteria (distance + speed + travel, ref: XYTetherJoint)
    f32 maxDistance = 1.2f;                      // Stretch limit before distance-based break
    f32 relativeSpeedThreshold = 8.0f;           // Separation speed limit (m/s)
    f32 ownSpeedThreshold = 10.0f;               // Petal absolute speed limit (m/s)
    f32 absoluteTravelThreshold = 8.0f;          // Total distance petal has traveled
    f32 relativeTravelThreshold = 8.0f;          // Total tether stretch accumulator
    f32 armDelay = 0.15f;                        // Grace period before break checks arm (seconds)

    // Spring/damper for tether pull-back (ref: XYTetherJoint spring/damper)
    f32 autoMass = 0.3f;
    f32 autoSpringK = 1200.0f;                   // Spring constant (pull-back force)
    f32 autoDamping = 60.0f;                     // Damping (opposing overshoot)
    f32 autoDrag = 1.5f;
    f32 driveMaxForce = 500.0f;                  // Cap on spring/damper force

    // Pluck dwell: hold above threshold tension for N seconds → auto-break (ref: XYTetherJoint)
    f32 pluckDwellThreshold = 0.85f;             // Tension (0..1) to start pluck timer
    f32 pluckDwellSeconds = 0.3f;                // Hold duration to auto-break (longer = harder)

    // Release pop: pull above high threshold, release below low → auto-break
    f32 releasePopHighThreshold = 0.8f;          // Must reach this tension
    f32 releasePopLowThreshold = 0.4f;           // Then drop below this to pop

    // Adaptive drive: tension-dependent spring/damper scaling (ref: XYTetherJoint ritual feel)
    f32 adaptiveMinSpringMult = 0.3f;            // Spring mult at zero stretch (soft start)
    f32 adaptiveMaxSpringMult = 2.0f;            // Spring mult at max stretch (stiff resistance)
    f32 adaptiveMinDamperMult = 0.5f;            // Damper mult at zero stretch
    f32 adaptiveMaxDamperMult = 1.5f;            // Damper mult at max stretch

    // Runtime state
    bool isBroken = false;
    bool justBroke = false;          // One-frame flag for particle spawn
    f32 currentTension = 0.0f;       // 0..1 normalized for feedback
    bool hadJoint = false;           // True if setup phase completed
    f32 restDistance = 0.0f;         // Captured at joint creation
    Math::Vector3 lastAnchorWorldA = Math::Vector3(0, 0, 0);
    Math::Vector3 lastAnchorWorldB = Math::Vector3(0, 0, 0);
    Math::Vector3 prevAnchorWorldA = Math::Vector3(0, 0, 0);  // Previous frame anchor A (for velocity)
    Math::Vector3 prevRelVec = Math::Vector3(0, 0, 0);        // Previous (A-B) vector
    Math::Vector3 junctionWorldPos = Math::Vector3(0, 0, 0);
    f32 absoluteTravel = 0.0f;       // Accumulated total distance of anchor A
    f32 relativeTravel = 0.0f;       // Accumulated change in (A-B) vector
    f32 aliveTime = 0.0f;            // Time since tether was armed
    f32 pluckDwellTimer = 0.0f;      // Accumulator for pluck dwell
    bool reachedHighTension = false;  // Flag for release pop detection
    Math::Vector3 velocity = Math::Vector3(0, 0, 0);  // FlowerSystem-owned velocity (no physics backend)
};

// Marks entity as click-draggable in play mode (ref: GrabPull.cs)
struct GrabbableComponent {
    // Authored parameters (ref: GrabPull spring physics)
    f32 grabSpring = 40.0f;      // How aggressively pulled toward cursor (lower = smoother)
    f32 grabDamper = 12.0f;      // Opposes overshoot, smooth deceleration
    f32 maxAccel = 20.0f;        // Safety cap on acceleration
    f32 maxSpeed = 5.0f;         // Velocity cap for grabbed parts
    f32 grabRadius = 0.5f;       // Hit sphere size for ray picking
    f32 windSwayScale = 0.15f;   // Wind influence multiplier

    // Runtime state
    bool isGrabbed = false;
    bool isBroken = false;  // True after tether breaks — petal follows cursor freely
    Math::Vector3 grabWorldPoint = Math::Vector3(0, 0, 0);
    Math::Vector3 cursorWorldPoint = Math::Vector3(0, 0, 0);
};

// Marker on the stem entity - evaluation target
struct FlowerStemComponent {
    // Authored parameters
    f32 healthyBonus = 10.0f;
    f32 witheredPenalty = 5.0f;
    f32 liquidIntensity = 1.0f;  // 0 = off, 0.5 = mild, 1.0 = full gush
    f32 groundLevel = 0.0f;                                    // Y position treated as ground
    Math::Vector3 sapColor = Math::Vector3(0.1f, 0.5f, 0.08f); // Color for sap particles
    f32 stemSwayAmplitude = 0.07f;                             // Wind sway strength on stem

    // Runtime state
    i32 partsRemoved = 0;
    i32 healthyRemoved = 0;
    i32 witheredRemoved = 0;
    f32 score = 0.0f;
    bool evaluated = false;
};

// Configures particle spawning for the flower system (placed on stem entity)
struct FlowerParticleConfigComponent {
    // Break burst particles — brief outward squirt, no upward kick
    i32 breakBurstCount = 10;
    f32 breakBurstSpeed = 2.5f;
    f32 breakBurstUpKick = -1.0f;
    f32 breakBurstLifetime = 0.3f;
    f32 breakBurstScale = 0.06f;

    // Break drip particles — heavy sap drops straight down
    i32 breakDripCount = 6;
    f32 breakDripSpeed = 3.0f;
    f32 breakDripLifetime = 0.5f;

    // Ground splash particles
    i32 splashCount = 8;
    f32 splashSpeed = 1.5f;
    f32 splashUpKick = 2.0f;
    f32 splashLifetime = 0.3f;

    // Tension drip particles
    f32 tensionDripRate = 3.0f;
    f32 tensionDripThreshold = 0.15f;
    f32 tensionSquirtSpeed = 2.0f;

    // Physics — strong gravity for quick arc and fall
    f32 particleGravity = 25.0f;
};

} // namespace ECS
} // namespace Enjin
