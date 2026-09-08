#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// Reflection probe component - defines a volume with box-projected cubemap reflections.
// Position comes from TransformComponent; the box extents define the influence volume.
// Supports baked cubemaps: click "Bake" in the editor to render 6 faces from the probe
// position into a cubemap texture. The fragment shader samples from the baked cubemap
// (binding 19) when available, falling back to a sky gradient approximation when not baked.
struct ENJIN_API ReflectionProbeComponent {
    // Box extents (world-space half-sizes from the probe center)
    Math::Vector3 boxMin = Math::Vector3(-5.0f, -3.0f, -5.0f);
    Math::Vector3 boxMax = Math::Vector3(5.0f, 3.0f, 5.0f);

    // Cubemap resolution per face for baking (128, 256, 512)
    u32 resolution = 256;

    // Reflection intensity multiplier (0 = no contribution, 1 = full)
    f32 intensity = 1.0f;

    // Priority for overlapping probes (higher wins)
    u32 priority = 0;

    // RUNTIME ONLY, and deliberately not serialized -- the cubemap itself is a
    // GPU resource that does not survive a scene save, so a `baked` flag that
    // DID survive would be a lie the moment the scene reloaded. It was
    // serialized until 2026-09-08, and the effect was that every probe came back
    // claiming to be baked with no cubemap behind it: the shader fell through to
    // the sky approximation while the inspector said "Baked", and the only way
    // to find out was to notice your reflections were wrong. Probes now re-bake
    // themselves instead (ReflectionProbeSystem::Update), so nothing needs to
    // persist here.
    bool baked = false;
    i32 cubemapTextureId = -1;

    // Whether this probe is active
    bool isActive = true;

    // Blend distance: how far inside the box before reaching full influence (world units)
    // Creates a smooth falloff at the edges of the probe volume
    f32 blendDistance = 1.0f;
};

} // namespace ECS
} // namespace Enjin
