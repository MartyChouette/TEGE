#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <cmath>

namespace Enjin {
namespace ECS {

// Water volume component - attach to an empty game object to define a water area
// The entity's TransformComponent position is the origin of the water surface
// Y position = water surface level, halfExtents define the horizontal area and depth
struct ENJIN_API WaterVolumeComponent {
    // Bounding box half-extents (local space)
    // X/Z define horizontal area, Y defines depth below surface
    Math::Vector3 halfExtents = Math::Vector3(50.0f, 5.0f, 50.0f);

    // Water visual settings
    Math::Vector3 waterColor = Math::Vector3(0.1f, 0.3f, 0.5f);
    f32 opacity = 0.7f;

    // Wave animation
    f32 waveSpeed = 1.0f;
    f32 waveHeight = 0.2f;

    // Higher priority volumes override lower ones when overlapping
    i32 priority = 0;

    // Check if a point is inside this volume's bounding box
    // center = entity's world position (from TransformComponent)
    bool ContainsPoint(const Math::Vector3& center, const Math::Vector3& point) const {
        return std::abs(point.x - center.x) <= halfExtents.x &&
               std::abs(point.y - center.y) <= halfExtents.y &&
               std::abs(point.z - center.z) <= halfExtents.z;
    }
};

} // namespace ECS
} // namespace Enjin
