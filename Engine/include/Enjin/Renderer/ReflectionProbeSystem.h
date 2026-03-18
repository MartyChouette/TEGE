#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {

// Forward declarations
namespace ECS { class World; }

namespace Renderer {

// Result of a reflection probe query — contains the data needed for GPU upload
struct ReflectionProbeData {
    Math::Vector3 probePosition;    // World-space probe center
    f32 intensity = 0.0f;           // 0 = no probe found
    Math::Vector3 boxMin;           // World-space AABB min (probe center + component boxMin)
    f32 blendDistance = 1.0f;
    Math::Vector3 boxMax;           // World-space AABB max (probe center + component boxMax)
    f32 _pad0 = 0.0f;
};

// Reflection probe system — manages probe queries for the renderer.
// Currently provides box-projected skybox reflections; future versions will
// support per-probe baked cubemaps and multi-probe blending.
class ENJIN_API ReflectionProbeSystem {
public:
    ReflectionProbeSystem() = default;
    ~ReflectionProbeSystem() = default;

    // Find the highest-priority active probe that contains the given position.
    // Returns a ReflectionProbeData with intensity > 0 if a probe was found.
    ReflectionProbeData FindNearestProbe(ECS::World* world, const Math::Vector3& position) const;

    // Future: Bake a single probe's cubemap from its position
    // void BakeProbe(ECS::World* world, u64 entity);
    // void BakeAll(ECS::World* world);
};

} // namespace Renderer
} // namespace Enjin
