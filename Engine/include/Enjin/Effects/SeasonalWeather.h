#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/Weather.h"

namespace Enjin {
namespace Effects {

struct SeasonalTemperatureRange {
    f32 minTemp = 5.0f, maxTemp = 18.0f;
    f32 coldestHour = 4.0f, warmestHour = 14.0f;
};

struct WeatherProbability {
    f32 clear = 0.3f, cloudy = 0.25f, rain = 0.2f, heavyRain = 0.1f;
    f32 snow = 0.0f, fog = 0.1f, storm = 0.05f;
};

struct SeasonalConfig {
    SeasonalTemperatureRange spring = {5.0f, 18.0f, 4.0f, 14.0f};
    SeasonalTemperatureRange summer = {15.0f, 32.0f, 4.0f, 14.0f};
    SeasonalTemperatureRange fall = {2.0f, 15.0f, 4.0f, 14.0f};
    SeasonalTemperatureRange winter = {-10.0f, 5.0f, 4.0f, 14.0f};

    WeatherProbability springWeather  = {0.30f, 0.25f, 0.25f, 0.10f, 0.00f, 0.05f, 0.05f};
    WeatherProbability summerWeather  = {0.45f, 0.20f, 0.15f, 0.05f, 0.00f, 0.05f, 0.10f};
    WeatherProbability fallWeather    = {0.20f, 0.30f, 0.20f, 0.10f, 0.05f, 0.10f, 0.05f};
    WeatherProbability winterWeather  = {0.15f, 0.20f, 0.05f, 0.00f, 0.35f, 0.15f, 0.10f};

    f32 weatherChangeInterval = 300.0f;  // game seconds between weather transitions

    // Master switch, default OFF (matches the editor's Seasonal Weather
    // checkbox default). Update() early-returns when disabled — CRITICAL:
    // its tail calls weather.SetWeather() every frame, which stomps script-
    // driven weather (Weather_Set) in any host that ticks this unconditionally
    // (the desktop player did exactly that — "no rain" in the Playground).
    bool enabled = false;
};

class ENJIN_API SeasonalWeatherSystem {
public:
    SeasonalWeatherSystem() = default;
    ~SeasonalWeatherSystem() = default;

    void Update(f32 dt, const WorldTimeState& time, WeatherSystem& weather);
    f32 GetCurrentTemperature() const { return m_CurrentTemperature; }
    SeasonalConfig& GetConfig() { return m_Config; }
    const SeasonalConfig& GetConfig() const { return m_Config; }

    // Current weather type picked by the seasonal system
    WeatherType GetCurrentWeatherType() const { return m_CurrentWeatherType; }

private:
    f32 ComputeTemperature(const WorldTimeState& time) const;
    WeatherType PickWeather(Season season);
    const SeasonalTemperatureRange& GetTempRange(Season season) const;
    const WeatherProbability& GetWeatherProb(Season season) const;

    SeasonalConfig m_Config;
    f32 m_CurrentTemperature = 20.0f;
    f32 m_WeatherTimer = 0.0f;
    WeatherType m_CurrentWeatherType = WeatherType::Clear;
    // Seasonal picks weather on an interval but used to WRITE it every frame,
    // so anything else that set weather was overwritten within one frame. This
    // makes the write happen at the transition, which is the only moment
    // seasonal has actually decided anything.
    bool m_HasAppliedOnce = false;
    u32 m_RandomSeed = 12345;
};

} // namespace Effects
} // namespace Enjin
