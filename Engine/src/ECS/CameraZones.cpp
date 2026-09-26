#include "Enjin/ECS/CameraZones.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include <climits>

namespace Enjin {
namespace ECS {

Entity FindCameraZonePlayer(World* world) {
    if (!world) return INVALID_ENTITY;
    auto first = [&](const auto& entities) {
        return entities.empty() ? INVALID_ENTITY : static_cast<Entity>(entities.front());
    };
    Entity p = first(world->GetEntitiesWithComponent<Platformer2DController>());
    if (p == INVALID_ENTITY) p = first(world->GetEntitiesWithComponent<TopDown2DController>());
    if (p == INVALID_ENTITY) p = first(world->GetEntitiesWithComponent<TopDown3DController>());
    if (p == INVALID_ENTITY) p = first(world->GetEntitiesWithComponent<ThirdPersonController>());
    if (p == INVALID_ENTITY) p = first(world->GetEntitiesWithComponent<FirstPersonController>());
    return p;
}

Entity ResolveCameraZone(World* world, Entity player) {
    if (!world || player == INVALID_ENTITY || !world->IsValid(player)) return INVALID_ENTITY;
    const auto* playerTransform = world->GetComponent<TransformComponent>(player);
    if (!playerTransform) return INVALID_ENTITY;

    Entity best = INVALID_ENTITY;
    i32 bestPriority = INT_MIN;
    for (Entity entity : world->GetEntitiesWithComponent<CameraTriggerComponent>()) {
        const auto* trigger = world->GetComponent<CameraTriggerComponent>(entity);
        const auto* trigTransform = world->GetComponent<TransformComponent>(entity);
        if (!trigger || !trigTransform || trigger->priority <= bestPriority) continue;
        if (!trigger->ContainsPoint(trigTransform->position, playerTransform->position)) continue;
        if (trigger->targetCamera == INVALID_ENTITY ||
            !world->HasComponent<CameraComponent>(trigger->targetCamera)) continue;
        best = trigger->targetCamera;
        bestPriority = trigger->priority;
    }
    return best;
}

Entity ResolveGameCamera(World* world) {
    if (!world) return INVALID_ENTITY;
    const Entity zone = ResolveCameraZone(world, FindCameraZonePlayer(world));
    if (zone != INVALID_ENTITY) return zone;
    return static_cast<Entity>(CameraManager::GetActiveCamera(world));
}

} // namespace ECS
} // namespace Enjin
