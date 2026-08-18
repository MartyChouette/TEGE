#pragma once

// Backend-agnostic collider shapes for GPU particle collision. Gathered from the
// scene's Box/Sphere/Capsule collider components each frame and uploaded to the
// particle sim (GLSL binding 3 / WGSL binding 2). Layout matches the shader
// struct exactly: three vec4s, 48 bytes.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace ECS { class World; }
namespace Effects {

struct ParticleColliderShape {
    // xyz = world center, w = kind (0 = box, 1 = sphere, 2 = capsule)
    Math::Vector4 posKind;
    // entity rotation quaternion (box axes / capsule axis)
    Math::Vector4 rot;
    // box: half extents; sphere: x = radius; capsule: x = radius, y = half height (cylinder)
    Math::Vector4 dims;
};
static_assert(sizeof(ParticleColliderShape) == 48, "must match the GLSL/WGSL shape struct");

// Max shapes uploaded to the sim (matches the shader-side cap).
constexpr u32 kMaxParticleColliders = 32;

// Gather every non-trigger Box/Sphere/Capsule collider in the world (capped).
// Collider sizes are WORLD SPACE per engine convention; capsule height is the
// cylinder section only.
void GatherParticleColliders(ECS::World* world, std::vector<ParticleColliderShape>& out);

} // namespace Effects
} // namespace Enjin
