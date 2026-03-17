#include "EnjinTest.h"
#include "Enjin/Effects/WorldTime.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Effects;

// ===========================================================================
// CalendarConfig Defaults
// ===========================================================================

ENJIN_TEST(CalendarConfig, Defaults) {
    CalendarConfig cfg;
    ENJIN_EXPECT_EQ(cfg.daysPerMonth, 30u);
    ENJIN_EXPECT_EQ(cfg.monthsPerYear, 12u);
    ENJIN_EXPECT_FLOAT_EQ(cfg.secondsPerGameHour, 60.0f);
    ENJIN_EXPECT_FALSE(cfg.paused);
}

// ===========================================================================
// DaylightConfig Defaults
// ===========================================================================

ENJIN_TEST(DaylightConfig, Defaults) {
    DaylightConfig cfg;
    ENJIN_EXPECT_FLOAT_EQ(cfg.springDaylight, 13.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.summerDaylight, 16.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.fallDaylight, 11.0f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.winterDaylight, 8.0f);
}

// ===========================================================================
// WorldTimeState Defaults
// ===========================================================================

ENJIN_TEST(WorldTimeState, Defaults) {
    WorldTimeState state;
    ENJIN_EXPECT_FLOAT_EQ(state.timeOfDay, 12.0f);
    ENJIN_EXPECT_EQ(state.day, 1u);
    ENJIN_EXPECT_EQ(state.month, 6u);
    ENJIN_EXPECT_EQ(state.year, 1u);
    ENJIN_EXPECT_EQ((int)state.season, (int)Season::Summer);
    ENJIN_EXPECT_FALSE(state.isNight);
}

// ===========================================================================
// Season Enum
// ===========================================================================

ENJIN_TEST(Season, Values) {
    ENJIN_EXPECT_EQ((int)Season::Spring, 0);
    ENJIN_EXPECT_EQ((int)Season::Summer, 1);
    ENJIN_EXPECT_EQ((int)Season::Fall, 2);
    ENJIN_EXPECT_EQ((int)Season::Winter, 3);
}

// ===========================================================================
// WorldTimeSystem
// ===========================================================================

ENJIN_TEST(System, InitialState) {
    WorldTimeSystem sys;
    const WorldTimeState& state = sys.GetState();
    ENJIN_EXPECT_FLOAT_EQ(state.timeOfDay, 12.0f);
}

ENJIN_TEST(System, SetTime) {
    WorldTimeSystem sys;
    sys.SetTime(6.0f, 15, 3, 2);
    const WorldTimeState& state = sys.GetState();
    ENJIN_EXPECT_FLOAT_EQ(state.timeOfDay, 6.0f);
    ENJIN_EXPECT_EQ(state.day, 15u);
    ENJIN_EXPECT_EQ(state.month, 3u);
    ENJIN_EXPECT_EQ(state.year, 2u);
}

ENJIN_TEST(System, TimeAdvances) {
    WorldTimeSystem sys;
    sys.SetTime(12.0f, 1, 1, 1);
    // With default 60 seconds per game hour, 60 real seconds = 1 game hour
    sys.Update(60.0f);
    const WorldTimeState& state = sys.GetState();
    ENJIN_EXPECT_FLOAT_NEAR(state.timeOfDay, 13.0f, 0.1f);
}

ENJIN_TEST(System, TimeWrapsAt24) {
    WorldTimeSystem sys;
    sys.SetTime(23.0f, 1, 1, 1);
    // Advance 2 game hours
    sys.Update(120.0f);
    const WorldTimeState& state = sys.GetState();
    // Should wrap past 24 → small value
    ENJIN_EXPECT_TRUE(state.timeOfDay < 24.0f);
    ENJIN_EXPECT_FLOAT_NEAR(state.timeOfDay, 1.0f, 0.5f);
}

ENJIN_TEST(System, DayIncrements) {
    WorldTimeSystem sys;
    sys.SetTime(23.5f, 1, 1, 1);
    // Advance past midnight
    sys.Update(60.0f); // 1 game hour → 0.5h past midnight
    const WorldTimeState& state = sys.GetState();
    ENJIN_EXPECT_GE(state.day, 2u);
}

ENJIN_TEST(System, PausedTimeDoesNotAdvance) {
    WorldTimeSystem sys;
    sys.SetTime(12.0f, 1, 1, 1);
    sys.GetCalendarConfig().paused = true;
    sys.Update(1000.0f);
    ENJIN_EXPECT_FLOAT_EQ(sys.GetState().timeOfDay, 12.0f);
}

ENJIN_TEST(System, SunDirectionNonZero) {
    WorldTimeSystem sys;
    sys.SetTime(12.0f, 1, 6, 1);
    sys.Update(0.01f);
    Math::Vector3 dir = sys.GetSunDirection();
    f32 len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    ENJIN_EXPECT_GT(len, 0.01f);
}

ENJIN_TEST(System, NightDetection) {
    WorldTimeSystem sys;
    sys.SetTime(2.0f, 1, 1, 1);
    sys.Update(0.01f);
    ENJIN_EXPECT_TRUE(sys.GetState().isNight);
}

ENJIN_TEST(System, DayDetection) {
    WorldTimeSystem sys;
    sys.SetTime(12.0f, 1, 6, 1);
    sys.Update(0.01f);
    ENJIN_EXPECT_FALSE(sys.GetState().isNight);
}

ENJIN_TEST(System, SeasonFromMonth) {
    WorldTimeSystem sys;
    // Month 6 = Summer (months 4-6)
    sys.SetTime(12.0f, 1, 6, 1);
    sys.Update(0.01f);
    ENJIN_EXPECT_EQ((int)sys.GetCurrentSeason(), (int)Season::Summer);
}

ENJIN_TEST(System, SeasonProgressRange) {
    WorldTimeSystem sys;
    sys.SetTime(12.0f, 15, 5, 1);
    sys.Update(0.01f);
    f32 progress = sys.GetSeasonProgress();
    ENJIN_EXPECT_GE(progress, 0.0f);
    ENJIN_EXPECT_TRUE(progress <= 1.0f);
}

ENJIN_TEST_MAIN()
