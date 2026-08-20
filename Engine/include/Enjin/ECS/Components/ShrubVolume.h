#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace ECS {

// Shrub volume component - defines a region filled with GPU-instanced procedural shrubs
struct ENJIN_API ShrubVolumeComponent {
    // Bounding box half-extents (local space, Y ignored - shrubs sit on XZ plane)
    Math::Vector3 halfExtents = Math::Vector3(10.0f, 0.0f, 10.0f);

    // Number of shrub instances
    u32 density = 500;

    // Shrub geometry
    f32 shrubHeight = 0.6f;
    f32 heightVariance = 0.2f;
    f32 width = 0.4f;

    // Colors (lerped base-to-tip). Shared palette: deep ground base (shared with
    // grass for ground unity) -> light green tip.
    Math::Vector3 baseColor = Math::Vector3(0.11f, 0.18f, 0.09f);  // deep ground
    Math::Vector3 tipColor = Math::Vector3(0.42f, 0.58f, 0.27f);   // light green

    // Wind response (bushier, less flexible than grass)
    f32 windSwayStrength = 0.5f;

    // Number of intersecting quads per shrub (star pattern)
    u32 quadsPerShrub = 3;

    // Custom texture (image) — sampled onto the procedural quads when non-empty,
    // with alpha-cutout so foliage sheets read as shaped leaves
    std::string customAssetPath;

    // Runtime bindless texture index for customAssetPath (-2 = unresolved,
    // -1 = none/failed). Not serialized; reset to -2 when the path changes.
    i32 cachedTexIndex = -2;
};

} // namespace ECS
} // namespace Enjin
