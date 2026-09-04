#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Effects/WorldTime.h"

namespace Enjin {

namespace ECS { class RenderSystem; }

namespace Effects {

class SeasonalWeatherSystem;
class WeatherSystem;

/**
 * @brief Advance the world clock and apply it to the scene. The one way to do it.
 *
 * Ticking WorldTimeSystem is not enough: something has to take its output and
 * put it somewhere the renderer reads. That code lived only in EditorLayer, so
 * day and night worked in the editor and did nothing in a shipped game --
 * Player and web_main called Update() and passed the state to seasonal weather,
 * and never touched the sun, the ambient colour or the tree season.
 *
 * What it applies:
 *   - the first directional light's rotation, from the sun's position
 *   - that light's colour and intensity, dimmed and cooled at night
 *   - the renderer's ambient colour and intensity
 *   - the season and its progress, for the tree renderer's foliage
 *   - seasonal weather, when its config is enabled
 *
 * @param world     Scene world. Required.
 * @param time      The clock. Advanced by deltaTime.
 * @param render    Renderer, for ambient and the tree season. May be null.
 * @param seasonal  Seasonal weather. May be null; skipped when its config is off.
 * @param weather   Weather system seasonal weather drives. May be null.
 * @param deltaTime Seconds, already scaled by the caller's time scale.
 */
ENJIN_API void UpdateAndApplyWorldTime(ECS::World* world,
                                       WorldTimeSystem& time,
                                       ECS::RenderSystem* render,
                                       SeasonalWeatherSystem* seasonal,
                                       WeatherSystem* weather,
                                       f32 deltaTime);

} // namespace Effects
} // namespace Enjin
