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

// Spring connection via physics joints. Breaks when SpringJointComponent is destroyed.
struct TetherComponent {
    // Authored parameters
    Entity stemEntity = INVALID_ENTITY;         // Stem entity for scoring
    Entity connectedEntity = INVALID_ENTITY;    // Direct connection target (crown for petals, stem for leaves/crown)
    Math::Vector3 attachLocalPos = Math::Vector3(0, 0, 0);
    f32 breakDistance = 1.5f;        // Informational (break now via SpringJoint breakForce)
    f32 tensionRamp = 2.0f;          // Informational

    // Runtime state
    bool isBroken = false;
    bool justBroke = false;          // One-frame flag for particle spawn
    f32 currentTension = 0.0f;       // 0..1 normalized for feedback
    bool hadJoint = false;           // True if SpringJointComponent existed last frame
    Math::Vector3 lastAnchorWorldA = Math::Vector3(0, 0, 0);  // Cached joint anchor A world pos
    Math::Vector3 lastAnchorWorldB = Math::Vector3(0, 0, 0);  // Cached joint anchor B world pos
    Math::Vector3 junctionWorldPos = Math::Vector3(0, 0, 0);  // Where part meets connected entity (for particles)
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
    f32 liquidIntensity = 1.0f;  // 0 = off, 0.5 = mild, 1.0 = full gush

    // Runtime state
    i32 partsRemoved = 0;
    i32 healthyRemoved = 0;
    i32 witheredRemoved = 0;
    f32 score = 0.0f;
    bool evaluated = false;
};

} // namespace ECS
} // namespace Enjin
