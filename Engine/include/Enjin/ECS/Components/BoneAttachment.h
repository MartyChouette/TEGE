#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <string>

namespace Enjin {
namespace ECS {

// Attaches an entity's transform to a specific bone on a skeletal mesh.
// The entity will track the bone's world position + rotation each frame,
// with optional local-space offsets.  Useful for parenting weapons, hats,
// particle emitters, etc. to animated characters.
struct BoneAttachmentComponent {
    // The entity that has the AnimatorComponent / SkeletonComponent
    Entity targetEntity = INVALID_ENTITY;

    // Name of the bone to attach to (matched via Skeleton::FindBoneIndex)
    std::string targetBoneName;

    // Local-space offsets applied after the bone transform
    Math::Vector3 positionOffset = Math::Vector3(0.0f);
    Math::Quaternion rotationOffset = Math::Quaternion::Identity();
};

} // namespace ECS
} // namespace Enjin
