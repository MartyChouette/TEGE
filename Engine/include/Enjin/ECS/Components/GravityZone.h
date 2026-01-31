#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin {
namespace ECS {

// Gravity zone shape
enum class GravityZoneShape : u32 {
    Box = 0,     // Axis-aligned bounding box
    Sphere = 1   // Spherical region (radius = halfExtents.x)
};

// Gravity zone mode
enum class GravityZoneMode : u32 {
    Directional = 0,  // Uniform direction (e.g., room with different gravity)
    Point = 1         // Pull toward zone center (planetary gravity, Mario Galaxy style)
};

// Gravity zone component - defines a region with custom gravity
// Entities inside this zone experience a different gravity direction and strength.
// Multiple zones can overlap; the highest priority zone wins.
struct ENJIN_API GravityZoneComponent {
    // Zone shape and mode
    GravityZoneShape shape = GravityZoneShape::Box;
    GravityZoneMode mode = GravityZoneMode::Directional;

    // Bounding box half-extents (or radius for sphere, using halfExtents.x)
    Math::Vector3 halfExtents = Math::Vector3(10.0f, 10.0f, 10.0f);

    // Directional mode: gravity direction (normalized) and strength (magnitude)
    Math::Vector3 gravityDirection = Math::Vector3(0.0f, -1.0f, 0.0f);
    f32 gravityStrength = 9.81f;

    // Higher priority zones override lower ones when overlapping
    i32 priority = 0;

    // Whether this zone is active
    bool isActive = true;

    // Get the effective gravity vector for a given world-space position
    // For Directional mode: returns gravityDirection * gravityStrength
    // For Point mode: returns direction toward zone center * gravityStrength
    Math::Vector3 GetGravityAt(const Math::Vector3& zoneCenter, const Math::Vector3& position) const {
        if (mode == GravityZoneMode::Point) {
            Math::Vector3 toCenter = zoneCenter - position;
            f32 dist = toCenter.Length();
            if (dist > 0.001f) {
                return toCenter * (1.0f / dist) * gravityStrength;
            }
            return Math::Vector3(0.0f, 0.0f, 0.0f);
        }
        return gravityDirection * gravityStrength;
    }

    // Non-positional version for backward compatibility
    Math::Vector3 GetGravity() const {
        return gravityDirection * gravityStrength;
    }

    // Check if a point is inside this zone
    bool ContainsPoint(const Math::Vector3& center, const Math::Vector3& point) const {
        if (shape == GravityZoneShape::Sphere) {
            Math::Vector3 diff = point - center;
            f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            f32 radius = halfExtents.x;
            return distSq <= radius * radius;
        }
        // Box
        return std::abs(point.x - center.x) <= halfExtents.x &&
               std::abs(point.y - center.y) <= halfExtents.y &&
               std::abs(point.z - center.z) <= halfExtents.z;
    }
};

} // namespace ECS
} // namespace Enjin
