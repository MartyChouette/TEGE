#include "Enjin/Effects/WaterFreeze.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include <climits>

namespace Enjin {
namespace Effects {

void UpdateWaterFreeze(ECS::World* world, f32 snowIntensity, f32 dt) {
    if (!world) return;
    for (ECS::Entity waterEntity : world->GetEntitiesWithComponent<ECS::WaterVolumeComponent>()) {
        auto* waterVol = world->GetComponent<ECS::WaterVolumeComponent>(waterEntity);
        auto* waterTransform = world->GetComponent<ECS::TransformComponent>(waterEntity);
        if (!waterVol || !waterTransform) continue;

        // The highest-priority temperature zone containing this water
        ECS::TemperatureZoneComponent* zone = nullptr;
        i32 bestPriority = INT_MIN;
        for (ECS::Entity tzEntity : world->GetEntitiesWithComponent<ECS::TemperatureZoneComponent>()) {
            auto* tz = world->GetComponent<ECS::TemperatureZoneComponent>(tzEntity);
            auto* tzTransform = world->GetComponent<ECS::TransformComponent>(tzEntity);
            if (tz && tzTransform && tz->priority > bestPriority &&
                tz->ContainsPoint(tzTransform->position, waterTransform->position)) {
                zone = tz;
                bestPriority = tz->priority;
            }
        }

        // Snow freezes water even without a temperature zone (Marty: water
        // should freeze in snow); a zone still overrides
        const bool snowFreeze = snowIntensity > 0.25f;
        if ((zone && zone->IsFreezing()) || snowFreeze) {
            waterVol->freezeProgress += waterVol->freezeRate * dt;
            if (waterVol->freezeProgress > 1.0f) waterVol->freezeProgress = 1.0f;
        } else if (zone && zone->IsNearFreezing()) {
            // Near freezing (0-5 C): settle at a partial freeze
            constexpr f32 kNearFreezingTarget = 0.3f;
            if (waterVol->freezeProgress < kNearFreezingTarget) {
                waterVol->freezeProgress += waterVol->freezeRate * 0.5f * dt;
                if (waterVol->freezeProgress > kNearFreezingTarget) waterVol->freezeProgress = kNearFreezingTarget;
            } else {
                waterVol->freezeProgress -= waterVol->thawRate * 0.5f * dt;
                if (waterVol->freezeProgress < kNearFreezingTarget) waterVol->freezeProgress = kNearFreezingTarget;
            }
        } else {
            waterVol->freezeProgress -= waterVol->thawRate * dt;
            if (waterVol->freezeProgress < 0.0f) waterVol->freezeProgress = 0.0f;
        }
        waterVol->isFrozen = (waterVol->freezeProgress >= 0.99f);
    }
}

} // namespace Effects
} // namespace Enjin
