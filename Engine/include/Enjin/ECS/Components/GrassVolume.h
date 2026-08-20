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

    // Colors (lerped base-to-tip). Shared palette: deep ground base -> mid green
    // tip. See docs/art/PROCEDURAL_EFFECTS_DIRECTION.md.
    Math::Vector3 baseColor = Math::Vector3(0.11f, 0.18f, 0.09f);  // deep ground
    Math::Vector3 tipColor = Math::Vector3(0.24f, 0.37f, 0.16f);   // mid green

    // Wind response
    f32 windSwayStrength = 1.0f;

    // Custom texture (image) — sampled onto the procedural blades when non-empty,
    // with alpha-cutout so foliage sheets read as shaped leaves
    std::string customAssetPath;

    // Runtime bindless texture index for customAssetPath (-2 = unresolved,
    // -1 = none/failed). Not serialized; reset to -2 when the path changes.
    i32 cachedTexIndex = -2;
};

} // namespace ECS
} // namespace Enjin
