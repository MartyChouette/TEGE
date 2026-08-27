#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// Reflective plane component — a hand-crafted planar reflection (the PS2/GameCube
// "wet floor" look). The plane is horizontal (XZ) at the entity's Y. Each frame the
// scene is re-rendered from a camera mirrored across the plane into a reflection
// texture, and the floor samples that texture back, so real on-screen geometry is
// mirrored below the surface. Deterministic and authored: the maker places the
// plane and dials the look; it renders the same every frame on every machine.
//
// This is not screen-space and not a probe — it is a genuine second view of the
// scene, the classic mirror-floor technique. Cost is one extra scene render per
// active plane per frame, which is the opt-in price of the effect.
struct ENJIN_API ReflectivePlaneComponent {
    // How strongly the reflection shows on the floor (0 = off, 1 = full mirror).
    f32 reflectionStrength = 0.6f;

    // Reflection tint, multiplied into the mirrored image. Use it for wet-asphalt
    // blue, gold-sheen floors, murky water, and so on.
    Math::Vector3 tint = Math::Vector3(1.0f, 1.0f, 1.0f);

    // 0 = sharp mirror, 1 = fully blurred (the soft showroom / wet-floor sheen).
    f32 blur = 0.0f;

    // Grazing-angle falloff: higher = reflection concentrates toward glancing views
    // (more reflective at the horizon, less looking straight down).
    f32 fresnelPower = 4.0f;

    // Resolution of the per-plane reflection render target (256/512/1024).
    u32 resolution = 512;

    // Clip a hair above the plane so the surface itself is not caught in its own
    // reflection (world units).
    f32 clipBias = 0.02f;

    // Whether this plane renders its reflection.
    bool active = true;

    // Runtime: index of the reflection texture owned by the planar-reflection
    // system (-1 = not yet rendered). Not serialized.
    i32 reflectionTexId = -1;
};

} // namespace ECS
} // namespace Enjin
