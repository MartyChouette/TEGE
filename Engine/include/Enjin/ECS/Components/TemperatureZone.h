#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <cmath>

namespace Enjin {
namespace ECS {

// Temperature zone component - defines a region with a specific temperature
// Used to determine weather behavior: below freezing = snow, above = rain
// Also provides a foundation for future ice/melting mechanics
struct ENJIN_API TemperatureZoneComponent {
    // Bounding box half-extents (local space, centered on entity position)
    Math::Vector3 halfExtents = Math::Vector3(20.0f, 20.0f, 20.0f);

    // Temperature in Celsius
    // Below 0: freezing (snow, ice)
    // 0-5: near-freezing (sleet mix)
    // Above 5: warm (rain)
    f32 temperature = 15.0f;

    // Higher priority zones override lower ones when overlapping
    i32 priority = 0;

    // Check if a point is inside this zone's bounding box
    bool ContainsPoint(const Math::Vector3& center, const Math::Vector3& point) const {
        return std::abs(point.x - center.x) <= halfExtents.x &&
               std::abs(point.y - center.y) <= halfExtents.y &&
               std::abs(point.z - center.z) <= halfExtents.z;
    }

    // Whether temperature indicates freezing conditions
    bool IsFreezing() const { return temperature <= 0.0f; }

    // Whether temperature is near-freezing (sleet/mix zone)
    bool IsNearFreezing() const { return temperature > 0.0f && temperature <= 5.0f; }
};

} // namespace ECS
} // namespace Enjin
