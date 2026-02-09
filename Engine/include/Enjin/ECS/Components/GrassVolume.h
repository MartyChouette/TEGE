#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace ECS {

// Grass volume component - defines a region filled with GPU-instanced grass blades
// The entity's TransformComponent position is the center of the grass volume
struct ENJIN_API GrassVolumeComponent {
    // Bounding box half-extents (local space, Y is ignored — grass sits on XZ plane)
    Math::Vector3 halfExtents = Math::Vector3(10.0f, 0.0f, 10.0f);

    // Number of grass blade instances
    u32 density = 5000;

    // Blade geometry
    f32 bladeHeight = 0.3f;
    f32 bladeHeightVariance = 0.1f;
    f32 bladeWidth = 0.03f;

    // Colors (lerped base-to-tip)
    Math::Vector3 baseColor = Math::Vector3(0.2f, 0.5f, 0.1f);
    Math::Vector3 tipColor = Math::Vector3(0.4f, 0.7f, 0.2f);

    // Wind response
    f32 windSwayStrength = 1.0f;

    // Custom asset path (texture or model) — overrides procedural grass when non-empty
    std::string customAssetPath;
};

} // namespace ECS
} // namespace Enjin
