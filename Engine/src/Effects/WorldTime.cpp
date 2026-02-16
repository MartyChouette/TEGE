#include "Enjin/Effects/WorldTime.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Effects {

static constexpr f32 PI = 3.14159265358979323846f;
static constexpr f32 TWO_PI = 6.28318530717958647692f;

void WorldTimeSystem::Update(f32 deltaTime) {
    if (m_CalendarConfig.paused) return;
    if (m_CalendarConfig.secondsPerGameHour <= 0.0f) return;

    // Accumulate real time -> game hours
    f32 gameHoursElapsed = deltaTime / m_CalendarConfig.secondsPerGameHour;
    m_State.timeOfDay += gameHoursElapsed;

    // Wrap at 24h -> advance calendar
    while (m_State.timeOfDay >= 24.0f) {
        m_State.timeOfDay -= 24.0f;
        AdvanceCalendar();
    }

    m_State.normalizedTimeOfDay = m_State.timeOfDay / 24.0f;

    ComputeDaylightHours();
    ComputeSunPosition();

    // Determine night
    f32 sunrise = 12.0f - m_State.daylightHours * 0.5f;
    f32 sunset = 12.0f + m_State.daylightHours * 0.5f;
    m_State.isNight = (m_State.timeOfDay < sunrise || m_State.timeOfDay > sunset);
}

void WorldTimeSystem::SetTime(f32 hour, u32 day, u32 month, u32 year) {
    m_State.timeOfDay = hour;
    m_State.day = day;
    m_State.month = month;
    m_State.year = year;
    m_State.normalizedTimeOfDay = hour / 24.0f;

    // Derive season from month
    u32 monthsPerSeason = m_CalendarConfig.monthsPerYear / 4;
    if (monthsPerSeason == 0) monthsPerSeason = 1;
    u32 seasonIndex = ((month - 1) / monthsPerSeason) % 4;
    m_State.season = static_cast<Season>(seasonIndex);

    ComputeDaylightHours();
    ComputeSunPosition();
}

void WorldTimeSystem::AdvanceCalendar() {
    m_State.day++;
    if (m_State.day > m_CalendarConfig.daysPerMonth) {
        m_State.day = 1;
        m_State.month++;
        if (m_State.month > m_CalendarConfig.monthsPerYear) {
            m_State.month = 1;
            m_State.year++;
        }

        // Derive season from month
        u32 monthsPerSeason = m_CalendarConfig.monthsPerYear / 4;
        if (monthsPerSeason == 0) monthsPerSeason = 1;
        u32 seasonIndex = ((m_State.month - 1) / monthsPerSeason) % 4;
        m_State.season = static_cast<Season>(seasonIndex);
    }
}

void WorldTimeSystem::ComputeDaylightHours() {
    f32 progress = GetSeasonProgress();

    // Interpolate daylight between seasons
    f32 current = 0.0f, next = 0.0f;
    switch (m_State.season) {
        case Season::Spring:
            current = m_DaylightConfig.springDaylight;
            next = m_DaylightConfig.summerDaylight;
            break;
        case Season::Summer:
            current = m_DaylightConfig.summerDaylight;
            next = m_DaylightConfig.fallDaylight;
            break;
        case Season::Fall:
            current = m_DaylightConfig.fallDaylight;
            next = m_DaylightConfig.winterDaylight;
            break;
        case Season::Winter:
            current = m_DaylightConfig.winterDaylight;
            next = m_DaylightConfig.springDaylight;
            break;
    }

    // Smooth cosine interpolation
    f32 t = (1.0f - std::cos(progress * PI)) * 0.5f;
    m_State.daylightHours = current + (next - current) * t;
}

void WorldTimeSystem::ComputeSunPosition() {
    f32 sunrise = 12.0f - m_State.daylightHours * 0.5f;
    f32 sunset = 12.0f + m_State.daylightHours * 0.5f;

    if (m_State.timeOfDay >= sunrise && m_State.timeOfDay <= sunset) {
        // Daytime: sun traces arc from east to west
        f32 dayProgress = (m_State.timeOfDay - sunrise) / (sunset - sunrise);
        m_State.sunElevation = std::sin(dayProgress * PI);  // 0 at horizon, 1 at zenith
        m_State.sunAzimuth = dayProgress * PI;  // East (0) to West (PI)
    } else {
        // Nighttime: sun below horizon
        f32 nightDuration = 24.0f - (sunset - sunrise);
        f32 nightProgress;
        if (m_State.timeOfDay > sunset) {
            nightProgress = (m_State.timeOfDay - sunset) / nightDuration;
        } else {
            nightProgress = (m_State.timeOfDay + 24.0f - sunset) / nightDuration;
        }
        m_State.sunElevation = -std::sin(nightProgress * PI) * 0.5f;
        m_State.sunAzimuth = PI + nightProgress * PI;
    }
}

Math::Vector3 WorldTimeSystem::GetSunDirection() const {
    // Convert elevation + azimuth to direction vector
    f32 elevation = m_State.sunElevation * PI * 0.5f;  // Map to actual angle
    f32 cosElev = std::cos(elevation);
    f32 sinElev = std::sin(elevation);
    f32 cosAzi = std::cos(m_State.sunAzimuth);
    f32 sinAzi = std::sin(m_State.sunAzimuth);

    // Direction FROM the sun toward the scene (for directional light)
    return Math::Vector3(-sinAzi * cosElev, -sinElev, -cosAzi * cosElev).Normalized();
}

Math::Vector3 WorldTimeSystem::GetAmbientColor() const {
    f32 elev = m_State.sunElevation;

    if (elev > 0.3f) {
        // Full day: neutral white
        return Math::Vector3(0.15f, 0.15f, 0.18f);
    } else if (elev > 0.0f) {
        // Dawn/dusk: warm orange tint
        f32 t = elev / 0.3f;  // 0 at horizon, 1 at full day
        Math::Vector3 dawnColor(0.2f, 0.12f, 0.06f);
        Math::Vector3 dayColor(0.15f, 0.15f, 0.18f);
        return Math::Vector3(
            dawnColor.x + (dayColor.x - dawnColor.x) * t,
            dawnColor.y + (dayColor.y - dawnColor.y) * t,
            dawnColor.z + (dayColor.z - dawnColor.z) * t
        );
    } else {
        // Night: dark blue
        f32 t = std::min(-elev * 4.0f, 1.0f);  // Quick transition
        Math::Vector3 duskColor(0.2f, 0.12f, 0.06f);
        Math::Vector3 nightColor(0.02f, 0.02f, 0.06f);
        return Math::Vector3(
            duskColor.x + (nightColor.x - duskColor.x) * t,
            duskColor.y + (nightColor.y - duskColor.y) * t,
            duskColor.z + (nightColor.z - duskColor.z) * t
        );
    }
}

f32 WorldTimeSystem::GetAmbientIntensity() const {
    f32 elev = m_State.sunElevation;
    if (elev > 0.3f) return 1.0f;
    if (elev > 0.0f) return 0.3f + 0.7f * (elev / 0.3f);
    return 0.1f + 0.2f * std::max(0.0f, 1.0f + elev * 4.0f);
}

Math::Vector3 WorldTimeSystem::GetSkyColor() const {
    f32 elev = m_State.sunElevation;

    if (elev > 0.3f) {
        // Day: blue sky
        return Math::Vector3(0.4f, 0.6f, 0.9f);
    } else if (elev > 0.0f) {
        // Sunrise/sunset: orange-pink gradient
        f32 t = elev / 0.3f;
        Math::Vector3 horizonColor(0.9f, 0.5f, 0.2f);
        Math::Vector3 skyColor(0.4f, 0.6f, 0.9f);
        return Math::Vector3(
            horizonColor.x + (skyColor.x - horizonColor.x) * t,
            horizonColor.y + (skyColor.y - horizonColor.y) * t,
            horizonColor.z + (skyColor.z - horizonColor.z) * t
        );
    } else {
        // Night: dark blue-black
        f32 t = std::min(-elev * 4.0f, 1.0f);
        Math::Vector3 duskColor(0.3f, 0.2f, 0.3f);
        Math::Vector3 nightColor(0.01f, 0.01f, 0.03f);
        return Math::Vector3(
            duskColor.x + (nightColor.x - duskColor.x) * t,
            duskColor.y + (nightColor.y - duskColor.y) * t,
            duskColor.z + (nightColor.z - duskColor.z) * t
        );
    }
}

f32 WorldTimeSystem::GetExposureOffset() const {
    f32 elev = m_State.sunElevation;
    if (elev > 0.3f) return 0.0f;      // Normal exposure
    if (elev > 0.0f) return -0.5f * (1.0f - elev / 0.3f);  // Slight darkening at dawn
    return -1.0f;                         // Dark at night
}

f32 WorldTimeSystem::GetSeasonProgress() const {
    u32 monthsPerSeason = m_CalendarConfig.monthsPerYear / 4;
    if (monthsPerSeason == 0) monthsPerSeason = 1;

    u32 monthInSeason = ((m_State.month - 1) % monthsPerSeason);
    f32 monthProgress = static_cast<f32>(monthInSeason) / static_cast<f32>(monthsPerSeason);
    f32 dayProgress = static_cast<f32>(m_State.day - 1) / static_cast<f32>(m_CalendarConfig.daysPerMonth);

    return monthProgress + dayProgress / static_cast<f32>(monthsPerSeason);
}

} // namespace Effects
} // namespace Enjin
