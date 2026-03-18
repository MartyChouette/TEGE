#include "Enjin/Renderer/ReflectionProbeSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin {
namespace Renderer {

ReflectionProbeData ReflectionProbeSystem::FindNearestProbe(
    ECS::World* world, const Math::Vector3& position) const {

    ReflectionProbeData result{};
    if (!world) return result;

    i32 bestPriority = -1;
    f32 bestWeight = 0.0f;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ReflectionProbeComponent>()) {
        auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!probe || !transform || !probe->isActive) continue;

        // Compute world-space AABB from probe center + box offsets
        Math::Vector3 center = transform->position;
        Math::Vector3 worldMin(center.x + probe->boxMin.x,
                               center.y + probe->boxMin.y,
                               center.z + probe->boxMin.z);
        Math::Vector3 worldMax(center.x + probe->boxMax.x,
                               center.y + probe->boxMax.y,
                               center.z + probe->boxMax.z);

        // Check if position is inside the probe's AABB
        if (position.x < worldMin.x || position.x > worldMax.x ||
            position.y < worldMin.y || position.y > worldMax.y ||
            position.z < worldMin.z || position.z > worldMax.z) {
            continue;
        }

        // Compute blend weight based on distance from box edges
        f32 blend = probe->blendDistance;
        f32 weight = 1.0f;
        if (blend > 0.001f) {
            // Find minimum distance from any face (positive = inside)
            f32 dx0 = position.x - worldMin.x;
            f32 dx1 = worldMax.x - position.x;
            f32 dy0 = position.y - worldMin.y;
            f32 dy1 = worldMax.y - position.y;
            f32 dz0 = position.z - worldMin.z;
            f32 dz1 = worldMax.z - position.z;

            f32 minDist = Math::Min(dx0, Math::Min(dx1,
                          Math::Min(dy0, Math::Min(dy1,
                          Math::Min(dz0, dz1)))));

            weight = Math::Clamp(minDist / blend, 0.0f, 1.0f);
        }

        // Pick the probe with highest priority, then best blend weight
        i32 probePriority = static_cast<i32>(probe->priority);
        if (probePriority > bestPriority ||
            (probePriority == bestPriority && weight > bestWeight)) {
            bestPriority = probePriority;
            bestWeight = weight;

            result.probePosition = center;
            result.intensity = probe->intensity * weight;
            result.boxMin = worldMin;
            result.boxMax = worldMax;
            result.blendDistance = probe->blendDistance;
        }
    }

    return result;
}

} // namespace Renderer
} // namespace Enjin
