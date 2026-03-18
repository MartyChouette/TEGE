#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// Reflection probe component - defines a volume with box-projected cubemap reflections.
// Position comes from TransformComponent; the box extents define the influence volume.
// Currently uses the scene skybox cubemap as the reflection source with box projection
// correction, so reflections appear correct in enclosed spaces. Per-probe baked cubemaps
// can be added as a follow-up.
struct ENJIN_API ReflectionProbeComponent {
    // Box extents (world-space half-sizes from the probe center)
    Math::Vector3 boxMin = Math::Vector3(-5.0f, -3.0f, -5.0f);
    Math::Vector3 boxMax = Math::Vector3(5.0f, 3.0f, 5.0f);

    // Cubemap resolution for future baking (128, 256, 512)
    u32 resolution = 256;

    // Reflection intensity multiplier (0 = no contribution, 1 = full)
    f32 intensity = 1.0f;

    // Priority for overlapping probes (higher wins)
    u32 priority = 0;

    // Whether this probe has been baked (future: triggers cubemap capture)
    bool baked = false;

    // Runtime: cubemap texture index (-1 = not baked, uses skybox fallback)
    i32 cubemapTextureId = -1;

    // Whether this probe is active
    bool isActive = true;

    // Blend distance: how far inside the box before reaching full influence (world units)
    // Creates a smooth falloff at the edges of the probe volume
    f32 blendDistance = 1.0f;
};

} // namespace ECS
} // namespace Enjin
