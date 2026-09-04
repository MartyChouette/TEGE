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
    //
    // GetSunDirection returns the direction light TRAVELS, so above the horizon
    // its y is negative (downward). Below the horizon it flips positive and the
    // light shines UP through the ground, lighting the underside of every
    // object and the floor itself -- light bleeding out from under things.
    //
    // Night is lit by a moon, and a moon is in the sky. Mirror the direction
    // back below the horizon so it always arrives from above; the colour and
    // intensity below already make it read as moonlight rather than sun.
    Math::Vector3 sunDir = time.GetSunDirection();
    if (sunDir.y > -0.15f) {
        sunDir.y = -0.15f - std::fabs(sunDir.y) * 0.5f;
        const f32 len = std::sqrt(sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z);
        if (len > 1e-5f) sunDir = sunDir / len;
    }
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::LightComponent>()) {
        auto* light = world->GetComponent<ECS::LightComponent>(e);
        auto* xf = world->GetComponent<ECS::TransformComponent>(e);
        if (!light || !xf || light->type != ECS::LightType::Directional) continue;

        // Set the rotation so GetForward() IS the sun direction. This was a
        // hand-rolled asin/atan2 euler, which inverted the pitch: the clamp
        // above kept the direction pointing down and the euler turned it back
        // up again. LookRotation puts its argument on local +Z and GetForward
        // reads local -Z, hence the negation -- the same convention
        // ApplyCameraPose uses.
        xf->rotation = Math::Quaternion::LookRotation(sunDir * -1.0f,
                                                      Math::Vector3(0.0f, 1.0f, 0.0f));

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
