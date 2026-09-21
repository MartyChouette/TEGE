#include "Enjin/Effects/SeasonalWeather.h"

#include <string>
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

u32 DateHash(const std::string& seed, u32 dayOfYear) {
    u32 h = 0x811C9DC5u;
    const auto feed = [&h](const char* bytes, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            h ^= static_cast<u32>(static_cast<u8>(bytes[i]));
            h *= 0x01000193u;
        }
    };

    feed(seed.data(), seed.size());
    const char tag = 'w';               // the stream this hash is for
    feed(&tag, 1);
    const std::string digits = std::to_string(dayOfYear);
    feed(digits.data(), digits.size());

    // The avalanche. Without it the low bits stay correlated across
    // consecutive days -- see the header for what that costs.
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// A pure function of the seed and the date. It used to advance a running
// xorshift, which made the weather on a date depend on how many rolls had
// happened since the process started: reload a save and the sky changed.
WeatherType SeasonalWeatherSystem::PickWeather(const WorldTimeState& time) const {
    // A day with weather written on it always happens. Only a blank day rolls.
    const auto authored = m_Authored.find(time.dayOfYear);
    if (authored != m_Authored.end()) return authored->second;

    const f64 roll = static_cast<f64>(DateHash(m_Config.worldSeed, time.dayOfYear)) /
                     4294967296.0;

    const auto& prob = GetWeatherProb(time.season);

    f64 cumulative = 0.0;
    cumulative += prob.clear;
    if (roll < cumulative) return WeatherType::Clear;
    cumulative += prob.cloudy;
    // Cloudy used to fall through to Clear, so the seasonal system could never
    // return a WeatherType the enum has had all along. Fall is 30% cloudy; all
    // of it was being reported as clear sky.
    if (roll < cumulative) return WeatherType::Cloudy;
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
    m_CurrentWeatherType = PickWeather(time);

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
