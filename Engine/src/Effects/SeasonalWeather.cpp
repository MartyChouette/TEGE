#include "Enjin/Effects/SeasonalWeather.h"
#include <cmath>

namespace Enjin {
namespace Effects {

static constexpr f32 PI = 3.14159265358979323846f;

const SeasonalTemperatureRange& SeasonalWeatherSystem::GetTempRange(Season season) const {
    switch (season) {
        case Season::Spring: return m_Config.spring;
        case Season::Summer: return m_Config.summer;
        case Season::Fall:   return m_Config.fall;
        case Season::Winter: return m_Config.winter;
    }
    return m_Config.summer;
}

const WeatherProbability& SeasonalWeatherSystem::GetWeatherProb(Season season) const {
    switch (season) {
        case Season::Spring: return m_Config.springWeather;
        case Season::Summer: return m_Config.summerWeather;
        case Season::Fall:   return m_Config.fallWeather;
        case Season::Winter: return m_Config.winterWeather;
    }
    return m_Config.summerWeather;
}

f32 SeasonalWeatherSystem::ComputeTemperature(const WorldTimeState& time) const {
    const auto& range = GetTempRange(time.season);

    // Cosine interpolation between coldest and warmest hour
    f32 hourOffset = time.timeOfDay - range.coldestHour;
    if (hourOffset < 0.0f) hourOffset += 24.0f;
    f32 warmHourOffset = range.warmestHour - range.coldestHour;
    if (warmHourOffset < 0.0f) warmHourOffset += 24.0f;

    f32 t;
    if (hourOffset <= warmHourOffset) {
        // Rising from coldest to warmest
        t = hourOffset / warmHourOffset;
    } else {
        // Falling from warmest to coldest
        f32 coolDuration = 24.0f - warmHourOffset;
        t = 1.0f - (hourOffset - warmHourOffset) / coolDuration;
    }

    // Smooth cosine interpolation
    f32 cosT = (1.0f - std::cos(t * PI)) * 0.5f;
    return range.minTemp + (range.maxTemp - range.minTemp) * cosT;
}

WeatherType SeasonalWeatherSystem::PickWeather(Season season) {
    // Simple xorshift PRNG
    m_RandomSeed ^= m_RandomSeed << 13;
    m_RandomSeed ^= m_RandomSeed >> 17;
    m_RandomSeed ^= m_RandomSeed << 5;
    f32 roll = static_cast<f32>(m_RandomSeed & 0xFFFF) / 65535.0f;

    const auto& prob = GetWeatherProb(season);

    f32 cumulative = 0.0f;
    cumulative += prob.clear;
    if (roll < cumulative) return WeatherType::Clear;
    cumulative += prob.cloudy;
    if (roll < cumulative) return WeatherType::Clear;  // Cloudy mapped to Clear with fog
    cumulative += prob.rain;
    if (roll < cumulative) return WeatherType::Rain;
    cumulative += prob.heavyRain;
    if (roll < cumulative) return WeatherType::HeavyRain;
    cumulative += prob.snow;
    if (roll < cumulative) return WeatherType::Snow;
    cumulative += prob.fog;
    if (roll < cumulative) return WeatherType::Fog;
    return WeatherType::Storm;
}

void SeasonalWeatherSystem::Update(f32 dt, const WorldTimeState& time, WeatherSystem& weather) {
    if (!m_Config.enabled) {
        // Re-arm, so switching seasons back on applies immediately rather than
        // waiting out a whole interval first.
        m_HasAppliedOnce = false;
        return;
    }

    // Temperature is a read-only output others sample, so it tracks every frame.
    m_CurrentTemperature = ComputeTemperature(time);

    // Seasonal owns the AMBIENT weather, and it decides on an interval -- so it
    // WRITES on the interval too, not every frame. Writing every frame is what
    // made this system unusable beside anything else: a script, a trigger or a
    // designer's own SetWeather was silently overwritten within one frame, and
    // the only defence the engine had was leaving the whole system switched off
    // by default.
    //
    // Between transitions, whatever set the weather last keeps it. A game with
    // weather and no seasons simply never enables this.
    m_WeatherTimer += dt;
    const bool transition = !m_HasAppliedOnce ||
                            m_WeatherTimer >= m_Config.weatherChangeInterval;
    if (!transition) return;

    m_WeatherTimer = 0.0f;
    m_HasAppliedOnce = true;

    // Pick weather from this season's probabilities.
    m_CurrentWeatherType = PickWeather(time.season);

    // Below freezing, rain falls as snow.
    if (m_CurrentTemperature < 0.0f) {
        if (m_CurrentWeatherType == WeatherType::Rain ||
            m_CurrentWeatherType == WeatherType::HeavyRain) {
            m_CurrentWeatherType = WeatherType::Snow;
        }
    }

    // Apply weather type to the weather system
    weather.SetWeather(m_CurrentWeatherType, 0.3f);

    switch (m_CurrentWeatherType) {
        case WeatherType::Rain:
            weather.SetRainIntensity(0.5f);
            weather.SetSnowIntensity(0.0f);
            break;
        case WeatherType::HeavyRain:
            weather.SetRainIntensity(1.0f);
            weather.SetSnowIntensity(0.0f);
            break;
        case WeatherType::Snow:
            weather.SetRainIntensity(0.0f);
            weather.SetSnowIntensity(0.7f);
            break;
        case WeatherType::Fog:
            weather.SetRainIntensity(0.0f);
            weather.SetSnowIntensity(0.0f);
            weather.SetFogDensity(0.8f);
            break;
        case WeatherType::Storm:
            weather.SetRainIntensity(1.0f);
            weather.SetSnowIntensity(0.0f);
            break;
        default:
            weather.SetRainIntensity(0.0f);
            weather.SetSnowIntensity(0.0f);
            break;
    }
}

} // namespace Effects
} // namespace Enjin
