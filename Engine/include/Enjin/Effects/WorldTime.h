#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Effects {

enum class Season : u8 { Spring = 0, Summer, Fall, Winter };

struct CalendarConfig {
    u32 daysPerMonth = 30;
    u32 monthsPerYear = 12;
    f32 secondsPerGameHour = 60.0f;  // real seconds per game hour
    bool paused = false;
};

struct DaylightConfig {
    f32 springDaylight = 13.0f;   // hours of sunlight
    f32 summerDaylight = 16.0f;
    f32 fallDaylight = 11.0f;
    f32 winterDaylight = 8.0f;
};

struct WorldTimeState {
    f32 timeOfDay = 12.0f;    // 0-24
    u32 day = 1, month = 6, year = 1;
    Season season = Season::Summer;
    f32 daylightHours = 14.0f;
    f32 sunElevation = 0.0f;  // -1 to 1
    f32 sunAzimuth = 0.0f;    // 0 to 2*PI
    bool isNight = false;
    f32 normalizedTimeOfDay = 0.5f;
};

class ENJIN_API WorldTimeSystem {
public:
    WorldTimeSystem() = default;
    ~WorldTimeSystem() = default;

    void Update(f32 deltaTime);
    void SetTime(f32 hour, u32 day, u32 month, u32 year);
    const WorldTimeState& GetState() const { return m_State; }
    CalendarConfig& GetCalendarConfig() { return m_CalendarConfig; }
    DaylightConfig& GetDaylightConfig() { return m_DaylightConfig; }

    // Computed outputs for other systems
    Math::Vector3 GetSunDirection() const;
    Math::Vector3 GetAmbientColor() const;
    f32 GetAmbientIntensity() const;
    Math::Vector3 GetSkyColor() const;
    f32 GetExposureOffset() const;
    Season GetCurrentSeason() const { return m_State.season; }
    f32 GetSeasonProgress() const;  // 0-1 within season

private:
    void AdvanceCalendar();
    void ComputeSunPosition();
    void ComputeDaylightHours();

    WorldTimeState m_State;
    CalendarConfig m_CalendarConfig;
    DaylightConfig m_DaylightConfig;
};

} // namespace Effects
} // namespace Enjin
