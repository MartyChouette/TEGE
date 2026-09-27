#include "Enjin/ECS/CameraZones.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include <algorithm>
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
    return ResolveCameraZone(world, player, nullptr);
}

Entity ResolveCameraZone(World* world, Entity player, Entity* outTrigger) {
    if (outTrigger) *outTrigger = INVALID_ENTITY;
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
        if (outTrigger) *outTrigger = entity;
    }
    return best;
}

Entity ResolveGameCamera(World* world) {
    if (!world) return INVALID_ENTITY;
    const Entity zone = ResolveCameraZone(world, FindCameraZonePlayer(world));
    if (zone != INVALID_ENTITY) return zone;
    return static_cast<Entity>(CameraManager::GetActiveCamera(world));
}

bool BlendGameCamera(World* world, GameCameraBlend& state, Entity target,
                     f32 blendTime, f32 deltaTime, GameCameraPose& out) {
    if (!world || target == INVALID_ENTITY || !world->IsValid(target) ||
        !world->GetComponent<TransformComponent>(target)) return false;

    GameCameraPose to;
    to.entity = target;
    GetWorldTransform(world, target, to.position, to.rotation);
    if (const auto* cc = world->GetComponent<CameraComponent>(target)) to.fieldOfView = cc->fieldOfView;

    if (target != state.current) {
        // Start from the view as it stood, mid-blend included
        if (state.hasLast && state.current != INVALID_ENTITY && blendTime > 0.0f) {
            state.from = state.last;
            state.duration = blendTime;
            state.elapsed = 0.0f;
        } else {
            state.duration = 0.0f;
        }
        state.current = target;
    }

    out = to;
    if (state.duration > 0.0f && state.elapsed < state.duration) {
        state.elapsed = std::min(state.elapsed + std::max(deltaTime, 0.0f), state.duration);
        f32 t = state.elapsed / state.duration;
        t = t * t * (3.0f - 2.0f * t);   // smoothstep: eases out of the old view and into the new
        out.position = state.from.position + (to.position - state.from.position) * t;
        out.rotation = Math::Quaternion::Slerp(state.from.rotation, to.rotation, t).Normalized();
        out.fieldOfView = state.from.fieldOfView + (to.fieldOfView - state.from.fieldOfView) * t;
    }
    state.last = out;
    state.hasLast = true;
    return true;
}

bool ResolveBlendedGameCamera(World* world, GameCameraBlend& state, f32 deltaTime, GameCameraPose& out) {
    if (!world) return false;
    Entity trigger = INVALID_ENTITY;
    Entity target = ResolveCameraZone(world, FindCameraZonePlayer(world), &trigger);
    f32 blendTime = state.lastZoneBlend;
    if (target != INVALID_ENTITY) {
        if (const auto* t = world->GetComponent<CameraTriggerComponent>(trigger)) blendTime = t->blendTime;
        state.lastZoneBlend = blendTime;
    } else {
        target = static_cast<Entity>(CameraManager::GetActiveCamera(world));
    }
    const bool ok = BlendGameCamera(world, state, target, blendTime, deltaTime, out);
    if (trigger == INVALID_ENTITY) state.lastZoneBlend = 0.0f;   // used once, on the way out
    return ok;
}

} // namespace ECS
} // namespace Enjin
