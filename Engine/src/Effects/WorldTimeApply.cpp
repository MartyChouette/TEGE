#include "Enjin/Effects/WorldTimeApply.h"

#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Effects/TreeRenderer.h"
#endif

#include <cmath>

namespace Enjin {
namespace Effects {

namespace {
constexpr f32 kRadToDeg = 57.29578f;
}

void UpdateAndApplyWorldTime(ECS::World* world,
                             WorldTimeSystem& time,
                             ECS::RenderSystem* render,
                             SeasonalWeatherSystem* seasonal,
                             WeatherSystem* weather,
                             f32 deltaTime) {
    if (!world) return;

    time.Update(deltaTime);
    const WorldTimeState& state = time.GetState();

    // Sun: drive the first directional light. The light has no direction field
    // of its own -- direction comes from the transform rotation -- so the sun
    // vector is encoded there.
    const Math::Vector3 sunDir = time.GetSunDirection();
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::LightComponent>()) {
        auto* light = world->GetComponent<ECS::LightComponent>(e);
        auto* xf = world->GetComponent<ECS::TransformComponent>(e);
        if (!light || !xf || light->type != ECS::LightType::Directional) continue;

        xf->rotation = Math::Quaternion::FromEuler(Math::Vector3(
            std::asin(-sunDir.y) * kRadToDeg,
            std::atan2(-sunDir.x, -sunDir.z) * kRadToDeg,
            0.0f));

        if (state.isNight) {
            light->color = Math::Vector3(0.3f, 0.35f, 0.5f);   // cool moonlight
            light->intensity = 0.3f;
        } else {
            light->color = Math::Vector3(1.0f, 0.95f, 0.9f);
            light->intensity = time.GetAmbientIntensity() * 1.5f;
        }
        break;   // the first directional light is the sun
    }

    if (render) {
        render->SetAmbientColor(time.GetAmbientColor());
        render->SetAmbientIntensity(time.GetAmbientIntensity());

        // Foliage colour follows the season (bare in winter, full in summer).
        // The Vulkan TreeRenderer is the only consumer; on web the vegetation
        // system draws trees and does not read season yet.
#if !ENJIN_RENDERER_WEBGPU
        if (auto* trees = render->GetTreeRenderer()) {
            trees->SetSeasonState(time.GetCurrentSeason(), time.GetSeasonProgress());
        }
#endif
    }

    // Seasonal weather drives the weather system from temperature and season.
    // Its config defaults OFF so an ungated host cannot stomp script-driven
    // weather every frame -- the desktop player used to.
    if (seasonal && weather && seasonal->GetConfig().enabled) {
        seasonal->Update(deltaTime, state, *weather);
    }
}

} // namespace Effects
} // namespace Enjin
