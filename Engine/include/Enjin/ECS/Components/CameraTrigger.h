#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include <cmath>

namespace Enjin {
namespace ECS {

// Camera trigger zone - attach to an entity to define an area that activates a specific camera.
// When the player entity enters this zone, the target camera becomes the active game camera.
// The entity's TransformComponent position is the center of the bounding box.
struct ENJIN_API CameraTriggerComponent {
    // Bounding box half-extents (local space, centered on entity position)
    Math::Vector3 halfExtents = Math::Vector3(10.0f, 10.0f, 10.0f);

    // Target camera entity to activate when player enters this zone
    Entity targetCamera = INVALID_ENTITY;

    // Higher priority zones override lower ones when overlapping
    i32 priority = 0;

    // Blend time in seconds for smooth camera transition
    f32 blendTime = 0.5f;

    // Check if a point is inside this zone's bounding box
    // center = entity's world position (from TransformComponent)
    bool ContainsPoint(const Math::Vector3& center, const Math::Vector3& point) const {
        return std::abs(point.x - center.x) <= halfExtents.x &&
               std::abs(point.y - center.y) <= halfExtents.y &&
               std::abs(point.z - center.z) <= halfExtents.z;
    }
};

} // namespace ECS
} // namespace Enjin
