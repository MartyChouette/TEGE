// Seasons drive the weather when they are on, and get out of the way when off.
//
// SeasonalWeatherSystem picked new weather on an interval but WROTE it every
// frame. So anything else that set weather -- a script, a trigger, a designer
// calling SetWeather -- was silently overwritten within one frame, and the only
// defence the engine had was shipping the whole system disabled by default with
// a comment saying it would otherwise stomp script weather.
//
// The rule now: seasonal owns the ambient weather and writes at its transitions,
// because a transition is the only moment it has actually decided anything.
// Between transitions whatever set the weather last keeps it. A game with
// weather and no seasons never enables this and is untouched.
#include "EnjinTest.h"
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/WorldTime.h"

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

// A world-time state in the given season, which is all the seasonal system reads.
WorldTimeState StateFor(Season s) {
    WorldTimeState t;
    t.season = s;
    t.timeOfDay = 12.0f;
    t.normalizedTimeOfDay = 0.5f;
    t.daylightHours = 14.0f;
    return t;
}

// SetWeather only sets a TARGET; WeatherSystem::Update walks the current
// weather toward it. A runtime ticks both, so these tests do too.
void Settle(WeatherSystem& w) {
    for (int i = 0; i < 40; ++i) w.Update(0.05f, Math::Vector3(0.0f, 0.0f, 0.0f));
}

} // namespace

ENJIN_TEST(SeasonalWeather, DisabledSeasonsNeverTouchTheWeather) {
    // Arrange: a game with weather and no seasons. This is the common case and
    // it must be exactly as if the system did not exist.
    WeatherSystem weather;
    weather.Initialize(64);
    weather.SetWeather(WeatherType::Rain, 0.0f);
    weather.SetRainIntensity(0.85f);

    SeasonalWeatherSystem seasonal;
    seasonal.GetConfig().enabled = false;

    // Act
    const WorldTimeState t = StateFor(Season::Winter);
    for (int i = 0; i < 600; ++i) seasonal.Update(0.1f, t, weather);

    // Assert
    ENJIN_EXPECT_TRUE(weather.GetRainIntensity() > 0.8f);
}

ENJIN_TEST(SeasonalWeather, EnabledSeasonsTakeOverImmediately) {
    // Arrange: turning seasons on should show, not wait out a whole interval
    // first. weatherChangeInterval defaults to 300 game seconds.
    WeatherSystem weather;
    weather.Initialize(64);

    SeasonalWeatherSystem seasonal;
    seasonal.GetConfig().enabled = true;

    // Act: a single short tick, then let the weather transition land.
    seasonal.Update(0.016f, StateFor(Season::Winter), weather);
    Settle(weather);

    // Assert: it has committed to a weather type rather than sitting inert.
    ENJIN_EXPECT_TRUE(seasonal.GetCurrentWeatherType() == weather.GetWeather());
}

ENJIN_TEST(SeasonalWeather, BetweenTransitionsAScriptKeepsWhatItSet) {
    // Arrange: this is the assertion the old per-frame write failed. A script
    // sets weather; seasonal must not overwrite it on the very next frame.
    WeatherSystem weather;
    weather.Initialize(64);

    SeasonalWeatherSystem seasonal;
    seasonal.GetConfig().enabled = true;
    seasonal.GetConfig().weatherChangeInterval = 300.0f;
    seasonal.Update(0.016f, StateFor(Season::Summer), weather);   // seasonal's first pick

    // Act: a script takes the weather deliberately, then time passes -- but not
    // enough for a seasonal transition.
    weather.SetWeather(WeatherType::Snow, 0.0f);
    weather.SetSnowIntensity(0.9f);
    weather.SetRainIntensity(0.0f);
    Settle(weather);
    const WorldTimeState t = StateFor(Season::Summer);
    for (int i = 0; i < 100; ++i) seasonal.Update(0.1f, t, weather);   // 10s of 300

    // Assert: the script's weather survived.
    ENJIN_EXPECT_TRUE(weather.GetSnowIntensity() > 0.8f);
}

ENJIN_TEST(SeasonalWeather, AtTheNextTransitionSeasonsTakeItBack) {
    // Arrange: the other half of the rule. Seasons OWN the ambient weather, so
    // once the interval elapses they decide again regardless of what set it.
    WeatherSystem weather;
    weather.Initialize(64);

    SeasonalWeatherSystem seasonal;
    seasonal.GetConfig().enabled = true;
    seasonal.GetConfig().weatherChangeInterval = 5.0f;
    const WorldTimeState t = StateFor(Season::Winter);
    seasonal.Update(0.016f, t, weather);

    // Act: a script sets something, then a full interval passes.
    weather.SetWeather(WeatherType::Clear, 0.0f);
    Settle(weather);
    for (int i = 0; i < 100; ++i) seasonal.Update(0.1f, t, weather);   // 10s > 5s
    Settle(weather);

    // Assert: the live weather is the seasonal system's choice again.
    ENJIN_EXPECT_TRUE(weather.GetWeather() == seasonal.GetCurrentWeatherType());
}

ENJIN_TEST(SeasonalWeather, TurningSeasonsOffAndOnReapplies) {
    // Arrange: the editor checkbox and a scene reload both toggle this, and it
    // must not leave the system waiting out an interval before it shows.
    WeatherSystem weather;
    weather.Initialize(64);

    SeasonalWeatherSystem seasonal;
    seasonal.GetConfig().enabled = true;
    seasonal.GetConfig().weatherChangeInterval = 300.0f;
    const WorldTimeState t = StateFor(Season::Fall);
    seasonal.Update(0.016f, t, weather);

    // Act
    seasonal.GetConfig().enabled = false;
    seasonal.Update(0.016f, t, weather);
    weather.SetWeather(WeatherType::Clear, 0.0f);
    seasonal.GetConfig().enabled = true;
    seasonal.Update(0.016f, t, weather);
    Settle(weather);

    // Assert
    ENJIN_EXPECT_TRUE(weather.GetWeather() == seasonal.GetCurrentWeatherType());
}

ENJIN_TEST(SeasonalWeather, TemperatureTracksEveryFrameEvenBetweenTransitions) {
    // Arrange: temperature is a read-only output other systems sample, so it
    // must keep updating even though the weather write does not.
    WeatherSystem weather;
    weather.Initialize(64);

    SeasonalWeatherSystem seasonal;
    seasonal.GetConfig().enabled = true;
    seasonal.GetConfig().weatherChangeInterval = 1000.0f;

    // Act
    seasonal.Update(0.016f, StateFor(Season::Summer), weather);
    const f32 summer = seasonal.GetCurrentTemperature();
    seasonal.Update(0.016f, StateFor(Season::Winter), weather);
    const f32 winter = seasonal.GetCurrentTemperature();

    // Assert: no transition happened, and temperature still followed the season.
    ENJIN_EXPECT_TRUE(winter < summer);
}

ENJIN_TEST_MAIN()
