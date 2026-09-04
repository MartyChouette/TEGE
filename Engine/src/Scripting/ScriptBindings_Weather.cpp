#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Wind.h"
#include <string>
#include "Enjin/Effects/SeasonalWeather.h"
#include "Enjin/Effects/WorldTime.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static Effects::WeatherSystem* s_BindingsWeather = nullptr;
// The WindSystem is what actually drives foliage sway (its wind vector feeds the
// shader windData). Weather_SetWind only slants precipitation; Wind_* here gusts
// the trees/grass/vegetation-meshes.
static Effects::WindSystem* s_BindingsWind = nullptr;
// The world clock. Day/night and the seasons had NO script access at all, so a
// game could run them but never set the hour, skip to a season, or tell the
// player what season it was in.
static Effects::WorldTimeSystem* s_BindingsWorldTime = nullptr;
static Effects::SeasonalWeatherSystem* s_BindingsSeasonal = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsWeather(Effects::WeatherSystem* weather) {
    s_BindingsWeather = weather;
}

void SetBindingsWind(Effects::WindSystem* wind) {
    s_BindingsWind = wind;
}

void SetBindingsWorldTime(Effects::WorldTimeSystem* time,
                          Effects::SeasonalWeatherSystem* seasonal) {
    s_BindingsWorldTime = time;
    s_BindingsSeasonal = seasonal;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Wind control (drives foliage sway via the WindSystem's global wind)
// ============================================================================

static void Wind_SetStrength(float strength) {
    if (!s_BindingsWind) return;
    Effects::WindParams p = s_BindingsWind->GetGlobalParams();
    p.strength = strength;
    s_BindingsWind->SetGlobalWind(p);
}

static void Wind_SetDirection(float x, float y, float z) {
    if (!s_BindingsWind) return;
    Effects::WindParams p = s_BindingsWind->GetGlobalParams();
    p.direction = Math::Vector3(x, y, z);
    s_BindingsWind->SetGlobalWind(p);
}

// ============================================================================
// Weather control
// ============================================================================

static void Weather_Set(int type, float transitionTime) {
    if (!s_BindingsWeather) return;
    if (type < 0 || type > 6) return;
    s_BindingsWeather->SetWeather(static_cast<Effects::WeatherType>(type), transitionTime);
}

static int Weather_Get() {
    return s_BindingsWeather ? static_cast<int>(s_BindingsWeather->GetWeather()) : 0;
}

static void Weather_SetRainIntensity(float v) {
    if (s_BindingsWeather) s_BindingsWeather->SetRainIntensity(v);
}

static float Weather_GetRainIntensity() {
    return s_BindingsWeather ? s_BindingsWeather->GetRainIntensity() : 0.0f;
}

static void Weather_SetSnowIntensity(float v) {
    if (s_BindingsWeather) s_BindingsWeather->SetSnowIntensity(v);
}

static float Weather_GetSnowIntensity() {
    return s_BindingsWeather ? s_BindingsWeather->GetSnowIntensity() : 0.0f;
}

static void Weather_SetFogDensity(float v) {
    if (s_BindingsWeather) s_BindingsWeather->SetFogDensity(v);
}

static float Weather_GetFogDensity() {
    return s_BindingsWeather ? s_BindingsWeather->GetFogDensity() : 0.0f;
}

static void Weather_SetFogColor(float r, float g, float b) {
    if (s_BindingsWeather) s_BindingsWeather->SetFogColor(Math::Vector3(r, g, b));
}

static void Weather_SetFogRange(float start, float end) {
    if (!s_BindingsWeather) return;
    s_BindingsWeather->SetFogStart(start);
    s_BindingsWeather->SetFogEnd(end);
}

static void Weather_SetWind(float dirX, float dirY, float dirZ, float strength) {
    if (!s_BindingsWeather) return;
    s_BindingsWeather->SetWindDirection(Math::Vector3(dirX, dirY, dirZ));
    s_BindingsWeather->SetWindStrength(strength);
}

static bool Weather_IsLightning() {
    return s_BindingsWeather ? s_BindingsWeather->IsLightningActive() : false;
}

static bool Weather_LightningJustFired() {
    return s_BindingsWeather ? s_BindingsWeather->LightningJustFired() : false;
}

static void Weather_SetLightningInterval(float minSec, float maxSec) {
    if (s_BindingsWeather) s_BindingsWeather->SetLightningInterval(minSec, maxSec);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

// --- World time: the hour, the season, and what to call them ---------------

static const char* kSeasonNames[4] = { "Spring", "Summer", "Fall", "Winter" };

static f32 WorldTime_GetTimeOfDay() {
    return s_BindingsWorldTime ? s_BindingsWorldTime->GetState().timeOfDay : 12.0f;
}
static void WorldTime_SetTimeOfDay(f32 hour) {
    if (!s_BindingsWorldTime) return;
    const auto& st = s_BindingsWorldTime->GetState();
    s_BindingsWorldTime->SetTime(hour, st.day, st.month, st.year);
}
static bool WorldTime_IsNight() {
    return s_BindingsWorldTime ? s_BindingsWorldTime->GetState().isNight : false;
}
static int WorldTime_GetSeason() {
    return s_BindingsWorldTime ? static_cast<int>(s_BindingsWorldTime->GetState().season) : 1;
}
static std::string WorldTime_GetSeasonName() {
    const int i = WorldTime_GetSeason();
    return kSeasonNames[(i < 0 || i > 3) ? 1 : i];
}
// Seasons follow the calendar, so moving to one means moving the month. Each
// season is three months starting at March.
static void WorldTime_SetSeason(int season) {
    if (!s_BindingsWorldTime) return;
    const int s = (season % 4 + 4) % 4;
    const u32 month = static_cast<u32>(3 + s * 3);   // Spring=3, Summer=6, Fall=9, Winter=12
    const auto& st = s_BindingsWorldTime->GetState();
    s_BindingsWorldTime->SetTime(st.timeOfDay, 1, month, st.year);
}
static void WorldTime_AdvanceSeason() {
    WorldTime_SetSeason(WorldTime_GetSeason() + 1);
}
static void WorldTime_SetSecondsPerHour(f32 seconds) {
    if (!s_BindingsWorldTime) return;
    s_BindingsWorldTime->GetCalendarConfig().secondsPerGameHour = seconds;
}
static bool WorldTime_GetSeasonalWeather() {
    return s_BindingsSeasonal ? s_BindingsSeasonal->GetConfig().enabled : false;
}
static void WorldTime_SetSeasonalWeather(bool on) {
    if (s_BindingsSeasonal) s_BindingsSeasonal->GetConfig().enabled = on;
}

void RegisterWeatherBindings(asIScriptEngine* engine) {
    // Weather type enum constants
    AS_CHECK(engine->RegisterEnum("WeatherType"));
    AS_CHECK(engine->RegisterEnumValue("WeatherType", "WEATHER_CLEAR", 0));
    AS_CHECK(engine->RegisterEnumValue("WeatherType", "WEATHER_CLOUDY", 1));
    AS_CHECK(engine->RegisterEnumValue("WeatherType", "WEATHER_RAIN", 2));
    AS_CHECK(engine->RegisterEnumValue("WeatherType", "WEATHER_HEAVY_RAIN", 3));
    AS_CHECK(engine->RegisterEnumValue("WeatherType", "WEATHER_SNOW", 4));
    AS_CHECK(engine->RegisterEnumValue("WeatherType", "WEATHER_FOG", 5));
    AS_CHECK(engine->RegisterEnumValue("WeatherType", "WEATHER_STORM", 6));

    // Weather control
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_Set(int, float = 2.0)",
        ENJIN_AS_FN(Weather_Set), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Weather_Get()",
        ENJIN_AS_FN(Weather_Get), ENJIN_AS_CALL_CDECL));

    // World time. Day/night and seasons ran with no script access at all, so
    // a game could not set the hour, jump to a season, or even tell the
    // player which season it was in.
    AS_CHECK(engine->RegisterGlobalFunction("float WorldTime_GetTimeOfDay()",
        ENJIN_AS_FN(WorldTime_GetTimeOfDay), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void WorldTime_SetTimeOfDay(float)",
        ENJIN_AS_FN(WorldTime_SetTimeOfDay), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool WorldTime_IsNight()",
        ENJIN_AS_FN(WorldTime_IsNight), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int WorldTime_GetSeason()",
        ENJIN_AS_FN(WorldTime_GetSeason), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string WorldTime_GetSeasonName()",
        ENJIN_AS_FN(WorldTime_GetSeasonName), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void WorldTime_SetSeason(int)",
        ENJIN_AS_FN(WorldTime_SetSeason), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void WorldTime_AdvanceSeason()",
        ENJIN_AS_FN(WorldTime_AdvanceSeason), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void WorldTime_SetSecondsPerHour(float)",
        ENJIN_AS_FN(WorldTime_SetSecondsPerHour), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool WorldTime_GetSeasonalWeather()",
        ENJIN_AS_FN(WorldTime_GetSeasonalWeather), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void WorldTime_SetSeasonalWeather(bool)",
        ENJIN_AS_FN(WorldTime_SetSeasonalWeather), ENJIN_AS_CALL_CDECL));

    // Intensity
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetRainIntensity(float)",
        ENJIN_AS_FN(Weather_SetRainIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Weather_GetRainIntensity()",
        ENJIN_AS_FN(Weather_GetRainIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetSnowIntensity(float)",
        ENJIN_AS_FN(Weather_SetSnowIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Weather_GetSnowIntensity()",
        ENJIN_AS_FN(Weather_GetSnowIntensity), ENJIN_AS_CALL_CDECL));

    // Fog
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetFogDensity(float)",
        ENJIN_AS_FN(Weather_SetFogDensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Weather_GetFogDensity()",
        ENJIN_AS_FN(Weather_GetFogDensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetFogColor(float, float, float)",
        ENJIN_AS_FN(Weather_SetFogColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetFogRange(float, float)",
        ENJIN_AS_FN(Weather_SetFogRange), ENJIN_AS_CALL_CDECL));

    // Wind
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetWind(float, float, float, float)",
        ENJIN_AS_FN(Weather_SetWind), ENJIN_AS_CALL_CDECL));
    // Wind that actually moves foliage (the WindSystem's global wind).
    AS_CHECK(engine->RegisterGlobalFunction("void Wind_SetStrength(float)",
        ENJIN_AS_FN(Wind_SetStrength), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Wind_SetDirection(float, float, float)",
        ENJIN_AS_FN(Wind_SetDirection), ENJIN_AS_CALL_CDECL));

    // Lightning
    AS_CHECK(engine->RegisterGlobalFunction("bool Weather_IsLightning()",
        ENJIN_AS_FN(Weather_IsLightning), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Weather_LightningJustFired()",
        ENJIN_AS_FN(Weather_LightningJustFired), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetLightningInterval(float, float)",
        ENJIN_AS_FN(Weather_SetLightningInterval), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
