#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <cmath>

namespace Enjin {
namespace ECS {

// Water type presets
enum class WaterType : u32 {
    Lake = 0,
    Ocean,
    River,
    Pond
};

// Water volume component - attach to an empty game object to define a water area
// The entity's TransformComponent position is the origin of the water surface
// Y position = water surface level, halfExtents define the horizontal area and depth
struct ENJIN_API WaterVolumeComponent {
    // Bounding box half-extents (local space)
    // X/Z define horizontal area, Y defines depth below surface
    Math::Vector3 halfExtents = Math::Vector3(50.0f, 5.0f, 50.0f);

    // Water type preset
    WaterType waterType = WaterType::Lake;

    // Water visual settings
    Math::Vector3 waterColor = Math::Vector3(0.1f, 0.3f, 0.5f);
    f32 opacity = 0.7f;

    // Wave animation
    f32 waveSpeed = 1.0f;
    f32 waveHeight = 0.2f;

    // Shore foam settings
    bool enableShore = true;
    f32 shoreWidth = 0.15f;       // 0-0.5, normalized edge distance for foam
    f32 foamIntensity = 0.6f;     // 0-1
    f32 foamScale = 8.0f;         // Noise scale for foam pattern
    Math::Vector3 shoreColor = Math::Vector3(0.3f, 0.6f, 0.7f);  // Shallow water tint

    // Higher priority volumes override lower ones when overlapping
    i32 priority = 0;

    // Freeze state (runtime — driven by temperature zones each frame)
    f32 freezeProgress = 0.0f;      // 0 = liquid, 1 = frozen solid
    bool isFrozen = false;           // Convenience: true when freezeProgress >= 0.99

    // Freeze tuning (authored, serialized)
    f32 freezeRate = 0.3f;          // How fast it freezes (per second)
    f32 thawRate = 0.5f;            // How fast it thaws (per second)
    Math::Vector3 iceColor = Math::Vector3(0.7f, 0.85f, 0.95f);  // Frozen surface color
    f32 iceOpacity = 0.95f;         // Frozen surface opacity

    // Dirty flag: set to false to force mesh regeneration
    bool meshCreated = false;

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
