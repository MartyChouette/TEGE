#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <cmath>
#include <string>

namespace Enjin {
namespace ECS {

// Weather zone component - attach to an entity to define a weather effect area
// The entity's TransformComponent position is the center of the bounding box
// Can be parented to a camera entity to follow it, or placed anywhere in the world
struct ENJIN_API WeatherZoneComponent {
    // Bounding box half-extents (local space, centered on entity position)
    Math::Vector3 halfExtents = Math::Vector3(20.0f, 20.0f, 20.0f);

    // Weather type: 0=Clear, 1=Cloudy, 2=Rain, 3=HeavyRain, 4=Snow, 5=Fog, 6=Storm
    u32 weatherType = 0;

    // Intensity settings
    f32 rainIntensity = 0.7f;
    f32 snowIntensity = 0.7f;

    // Fog settings
    f32 fogDensity = 0.0f;
    Math::Vector3 fogColor = Math::Vector3(0.5f, 0.5f, 0.6f);
    f32 fogStart = 20.0f;
    f32 fogEnd = 100.0f;

    // Wind
    Math::Vector3 windDirection = Math::Vector3(0.2f, 0.0f, 0.1f);
    f32 windStrength = 1.0f;

    // Custom precipitation sprites (image paths). Empty = built-in procedural
    // look (stretched streak for rain, radial glow for snow).
    std::string rainTexturePath;
    std::string snowTexturePath;

    // Runtime bindless texture indices for the paths above (-2 = unresolved,
    // -1 = none/failed). Not serialized; reset to -2 when a path changes.
    i32 cachedRainTexIndex = -2;
    i32 cachedSnowTexIndex = -2;

    // Lightning (for storms)
    bool lightningEnabled = true;
    f32 lightningMinInterval = 2.0f;   // Seconds between lightning (min)
    f32 lightningMaxInterval = 10.0f;  // Seconds between lightning (max)

    // Higher priority zones override lower ones when overlapping
    i32 priority = 0;

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
