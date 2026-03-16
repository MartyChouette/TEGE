#include "EnjinTest.h"
#include "Enjin/Effects/WorldTime.h"

using namespace Enjin;
using namespace Enjin::Effects;

// ===========================================================================
// WorldTimeState Defaults
// ===========================================================================

ENJIN_TEST(WorldTimeState, DefaultTime) {
    WorldTimeState state;
    ENJIN_EXPECT_FLOAT_EQ(state.timeOfDay, 12.0f);
}

ENJIN_TEST(WorldTimeState, DefaultDate) {
    WorldTimeState state;
    ENJIN_EXPECT_EQ(state.day, 1u);
    ENJIN_EXPECT_EQ(state.month, 6u);
    ENJIN_EXPECT_EQ(state.year, 1u);
}

ENJIN_TEST(WorldTimeState, DefaultSeason) {
    WorldTimeState state;
    ENJIN_EXPECT_EQ((int)state.season, (int)Season::Summer);
}

ENJIN_TEST(WorldTimeState, DefaultNight) {
    WorldTimeState state;
    ENJIN_EXPECT_FALSE(state.isNight);
}

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

ENJIN_TEST(DaylightConfig, SpringDaylight) {
    DaylightConfig cfg;
    ENJIN_EXPECT_FLOAT_EQ(cfg.springDaylight, 13.0f);
}

ENJIN_TEST(DaylightConfig, SummerDaylight) {
    DaylightConfig cfg;
    ENJIN_EXPECT_FLOAT_EQ(cfg.summerDaylight, 16.0f);
}

ENJIN_TEST(DaylightConfig, FallDaylight) {
    DaylightConfig cfg;
    ENJIN_EXPECT_FLOAT_EQ(cfg.fallDaylight, 11.0f);
}

ENJIN_TEST(DaylightConfig, WinterDaylight) {
    DaylightConfig cfg;
    ENJIN_EXPECT_FLOAT_EQ(cfg.winterDaylight, 8.0f);
}

// ===========================================================================
// Season Enum
// ===========================================================================

ENJIN_TEST(SeasonEnum, Values) {
    ENJIN_EXPECT_EQ((int)Season::Spring, 0);
    ENJIN_EXPECT_EQ((int)Season::Summer, 1);
    ENJIN_EXPECT_EQ((int)Season::Fall, 2);
    ENJIN_EXPECT_EQ((int)Season::Winter, 3);
}

// ===========================================================================
// WorldTimeSystem
// ===========================================================================

ENJIN_TEST(WorldTimeSystem, InitialState) {
    WorldTimeSystem wts;
    const auto& state = wts.GetState();
    ENJIN_EXPECT_FLOAT_EQ(state.timeOfDay, 12.0f);
}

ENJIN_TEST(WorldTimeSystem, SetTime) {
    WorldTimeSystem wts;
    wts.SetTime(6.0f, 15, 3, 2);
    const auto& state = wts.GetState();
    ENJIN_EXPECT_FLOAT_EQ(state.timeOfDay, 6.0f);
    ENJIN_EXPECT_EQ(state.day, 15u);
    ENJIN_EXPECT_EQ(state.month, 3u);
    ENJIN_EXPECT_EQ(state.year, 2u);
}

ENJIN_TEST(WorldTimeSystem, CalendarConfigAccess) {
    WorldTimeSystem wts;
    auto& cal = wts.GetCalendarConfig();
    cal.daysPerMonth = 28;
    ENJIN_EXPECT_EQ(wts.GetCalendarConfig().daysPerMonth, 28u);
}

ENJIN_TEST(WorldTimeSystem, DaylightConfigAccess) {
    WorldTimeSystem wts;
    auto& dl = wts.GetDaylightConfig();
    dl.summerDaylight = 18.0f;
    ENJIN_EXPECT_FLOAT_EQ(wts.GetDaylightConfig().summerDaylight, 18.0f);
}

ENJIN_TEST(WorldTimeSystem, PauseStopsUpdate) {
    WorldTimeSystem wts;
    wts.SetTime(12.0f, 1, 1, 1);
    wts.GetCalendarConfig().paused = true;
    wts.Update(100.0f); // Large dt
    // Time should not advance when paused
    ENJIN_EXPECT_FLOAT_EQ(wts.GetState().timeOfDay, 12.0f);
}

ENJIN_TEST_MAIN()
