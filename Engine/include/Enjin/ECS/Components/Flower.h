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

// Spring connection to a stem entity. Breaks at distance threshold.
struct TetherComponent {
    // Authored parameters
    Entity stemEntity = INVALID_ENTITY;
    Math::Vector3 attachLocalPos = Math::Vector3(0, 0, 0);
    f32 restLength = 0.0f;           // 0 = auto-compute from initial distance
    f32 tetherStiffness = 40.0f;
    f32 tetherDamping = 3.0f;
    f32 breakDistance = 1.5f;
    f32 tensionRamp = 2.0f;          // Exponent for non-linear resistance

    // Runtime state
    bool isBroken = false;
    bool justBroke = false;          // One-frame flag for particle spawn
    f32 currentTension = 0.0f;       // 0..1 normalized for feedback
    f32 computedRestLength = 0.0f;   // Auto-computed rest length
    bool restLengthInitialized = false;
};

// Marks entity as click-draggable in play mode
struct GrabbableComponent {
    // Authored parameters
    f32 pullForce = 15.0f;
    f32 grabRadius = 0.5f;

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

    // Runtime state
    i32 partsRemoved = 0;
    i32 healthyRemoved = 0;
    i32 witheredRemoved = 0;
    f32 score = 0.0f;
    bool evaluated = false;
};

} // namespace ECS
} // namespace Enjin
