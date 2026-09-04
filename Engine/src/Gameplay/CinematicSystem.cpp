#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Gameplay/CameraPose.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include <cmath>

namespace Enjin {
namespace Gameplay {

f32 CinematicSystem::ApplyEasing(f32 t, ECS::CinematicCameraComponent::Waypoint::Easing easing) {
    switch (easing) {
        case ECS::CinematicCameraComponent::Waypoint::Easing::Linear:
            return t;
        case ECS::CinematicCameraComponent::Waypoint::Easing::EaseIn:
            return t * t;
        case ECS::CinematicCameraComponent::Waypoint::Easing::EaseOut:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case ECS::CinematicCameraComponent::Waypoint::Easing::EaseInOut:
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        case ECS::CinematicCameraComponent::Waypoint::Easing::SmashCut:
            return t >= 1.0f ? 1.0f : 0.0f;
    }
    return t;
}

void CinematicSystem::Update(ECS::World* world, Renderer::Camera* gameCamera, f32 deltaTime) {
    if (!m_Enabled || !world) return;

    auto entities = world->GetEntitiesWithComponent<ECS::CinematicCameraComponent>();
    for (auto entity : entities) {
        auto* cine = world->GetComponent<ECS::CinematicCameraComponent>(entity);
        if (!cine || !cine->isPlaying || cine->waypoints.size() < 2) continue;

        i32 numSegments = static_cast<i32>(cine->waypoints.size()) - 1;
        if (cine->currentSegment < 0) cine->currentSegment = 0;
        if (cine->currentSegment >= numSegments) {
            // Sequence complete
            if (cine->loop) {
                cine->currentSegment = 0;
                cine->segmentProgress = 0.0f;
                cine->currentTime = 0.0f;
            } else {
                cine->isPlaying = false;
                cine->isComplete = true;
            }
            continue;
        }

        const auto& fromWP = cine->waypoints[cine->currentSegment];
        const auto& toWP = cine->waypoints[cine->currentSegment + 1];

        // Handle hold time at current waypoint
        f32 holdRemaining = fromWP.holdTime - (cine->segmentProgress < 0.0f ? -cine->segmentProgress : 0.0f);
        if (cine->segmentProgress < 0.0f) {
            // In hold phase (negative progress = hold)
            cine->segmentProgress += deltaTime;
            if (cine->segmentProgress >= 0.0f) {
                cine->segmentProgress = 0.0f;
            }
            // Use fromWP position during hold
            ApplyCameraPose(world, gameCamera, fromWP.position, fromWP.lookAt, fromWP.fov);
            continue;
        }

        // Advance progress
        f32 duration = toWP.duration;
        if (duration <= 0.0f) duration = 0.001f;
        cine->segmentProgress += deltaTime / duration;
        cine->currentTime += deltaTime;

        f32 t = cine->segmentProgress;
        if (t >= 1.0f) {
            t = 1.0f;
            cine->currentSegment++;
            cine->segmentProgress = toWP.holdTime > 0.0f ? -toWP.holdTime : 0.0f;
        }

        // Apply easing
        f32 easedT = ApplyEasing(t, toWP.easing);

        // Interpolate position, lookAt, FOV
        Math::Vector3 pos = fromWP.position + (toWP.position - fromWP.position) * easedT;
        Math::Vector3 lookAt = fromWP.lookAt + (toWP.lookAt - fromWP.lookAt) * easedT;
        f32 fov = fromWP.fov + (toWP.fov - fromWP.fov) * easedT;

        // Apply to the camera. This used to write only the Renderer::Camera and
        // then the CINEMATIC entity's own transform -- which nothing renders
        // from -- so every runtime rebuilt the view from the active camera
        // entity immediately afterwards and the shot never moved.
        ApplyCameraPose(world, gameCamera, pos, lookAt, fov);

        // Keep the cinematic entity itself on the path too, so its gizmo and
        // any child rig follow the shot in the viewport.
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (transform) {
            transform->position = pos;
        }
    }
}

void CinematicSystem::Play(ECS::World* world, ECS::Entity cinematicEntity) {
    if (!world) return;
    auto* cine = world->GetComponent<ECS::CinematicCameraComponent>(cinematicEntity);
    if (!cine || cine->waypoints.size() < 2) return;

    cine->isPlaying = true;
    cine->isComplete = false;
    cine->currentSegment = 0;
    cine->segmentProgress = 0.0f;
    cine->currentTime = 0.0f;
}

void CinematicSystem::Stop(ECS::World* world, ECS::Entity cinematicEntity) {
    if (!world) return;
    auto* cine = world->GetComponent<ECS::CinematicCameraComponent>(cinematicEntity);
    if (cine) {
        cine->isPlaying = false;
        cine->currentSegment = 0;
        cine->segmentProgress = 0.0f;
    }
}

bool CinematicSystem::IsAnyCinematicPlaying(ECS::World* world) const {
    if (!world) return false;
    auto entities = world->GetEntitiesWithComponent<ECS::CinematicCameraComponent>();
    for (auto entity : entities) {
        auto* cine = world->GetComponent<ECS::CinematicCameraComponent>(entity);
        if (cine && cine->isPlaying) return true;
    }
    return false;
}

} // namespace Gameplay
} // namespace Enjin
