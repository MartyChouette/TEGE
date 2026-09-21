#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/Effects/Weather.h"

#include <string>
#include <unordered_map>

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

    // The world seed. Weather is a pure function of this plus the date, so the
    // same seed and the same day give the same sky on any machine, from any
    // save, forever. An empty seed still works; it just means every project
    // that leaves it empty shares one year of weather.
    std::string worldSeed;

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

    // What the sky is on a given date, without ticking anything and without
    // touching the live weather. This is what a forecast board asks: a player
    // can be told on Monday that Thursday is a snow day, because Thursday's
    // weather is already decided by the seed and the date.
    WeatherType WeatherOn(const WorldTimeState& time) const { return PickWeather(time); }

    // --- authored days ----------------------------------------------------
    // A day can be written down instead of rolled. An authored day always
    // happens; a day left blank is rolled from the seed. That is one rule, and
    // it is what lets a designer pin the weather for a holiday without giving
    // up a deterministic sky for every other day of the year.
    //
    // The engine deliberately does not know where these came from. Reading a
    // game's own calendar file is the game's business; this is only the place
    // the answer lands, so the format can change without the engine moving.
    void SetAuthoredWeather(u32 dayOfYear, WeatherType w) { m_Authored[dayOfYear] = w; }
    void ClearAuthoredWeather() { m_Authored.clear(); }
    void ClearAuthoredWeather(u32 dayOfYear) { m_Authored.erase(dayOfYear); }
    bool IsAuthored(u32 dayOfYear) const { return m_Authored.count(dayOfYear) != 0; }
    size_t GetAuthoredDayCount() const { return m_Authored.size(); }

private:
    f32 ComputeTemperature(const WorldTimeState& time) const;
    WeatherType PickWeather(const WorldTimeState& time) const;
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
    // dayOfYear -> the sky somebody wrote down for it
    std::unordered_map<u32, WeatherType> m_Authored;
};

// FNV-1a over the seed string, the literal 'w' and the decimal day-of-year,
// finished with an avalanche mix.
//
// THE FINALIZER IS NOT OPTIONAL. Plain FNV-1a over sequential days leaves the
// low bits correlated, and the mean still comes out right, which is exactly why
// it survives a glance. The spread does not: over 200 seeds the unfinished hash
// produced between 0 and 35 snow days in a year that should hold 12, and 20 of
// those 200 seeds gave a year with no snow in it at all. Deterministic and
// wrong is the worst outcome available here, because nothing about it looks
// broken. With the finalizer the same 200 seeds run 5 to 21.
//
// This must stay byte-identical to hash32() in the Shells year planner
// (tools/shells_year.py), because that tool previews the sky for days a
// designer left blank. If the two ever disagree, the preview is a lie.
ENJIN_API u32 DateHash(const std::string& seed, u32 dayOfYear);

} // namespace Effects
} // namespace Enjin
