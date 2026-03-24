#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <string>

namespace Enjin {
namespace ECS {

struct LookAtIKComponent {
    std::string headBoneName;
    std::string neckBoneName;  // Optional (empty = head only)

    // Target: entity ID or world position
    Entity targetEntity = INVALID_ENTITY;
    Math::Vector3 targetWorldPos = Math::Vector3(0.0f);
    bool useEntityTarget = false;

    // Parameters
    f32 maxRotation = 45.0f;      // Max angle in degrees
    f32 smoothSpeed = 5.0f;       // Interpolation speed
    f32 lookWeight = 1.0f;        // 0..1 blend weight

    // Runtime state (not serialized)
    Math::Quaternion currentHeadRotation;
    Math::Quaternion currentNeckRotation;
};

struct InteractionIKComponent {
    std::string handBoneName;
    std::string elbowBoneName;
    std::string shoulderBoneName;

    f32 interactionRadius = 1.5f;  // Meters
    f32 ikWeight = 1.0f;          // 0..1 blend weight
    f32 smoothSpeed = 3.0f;       // Interpolation speed
    std::string interactionTag;    // Tag to match nearby interactables

    // Runtime state (not serialized)
    Entity currentTarget = INVALID_ENTITY;
    Math::Vector3 currentHandTarget = Math::Vector3(0.0f);
};

// Two-Bone IK component - analytic IK for arms and legs (law of cosines)
struct TwoBoneIKComponent {
    std::string rootBoneName;   // e.g., LeftUpperArm or LeftUpperLeg
    std::string midBoneName;    // e.g., LeftForeArm or LeftLowerLeg
    std::string endBoneName;    // e.g., LeftHand or LeftFoot

    // Target: world-space position or entity to track
    Math::Vector3 targetPosition = Math::Vector3(0.0f);
    Entity targetEntity = INVALID_ENTITY;
    bool useEntityTarget = false;

    // IK parameters
    f32 weight = 1.0f;                                       // 0 = animation only, 1 = full IK
    Math::Vector3 poleVector = Math::Vector3(0.0f, 0.0f, 1.0f); // Elbow/knee direction hint
};

} // namespace ECS
} // namespace Enjin
