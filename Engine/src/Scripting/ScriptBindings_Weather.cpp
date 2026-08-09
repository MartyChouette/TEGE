#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Effects/Weather.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static Effects::WeatherSystem* s_BindingsWeather = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsWeather(Effects::WeatherSystem* weather) {
    s_BindingsWeather = weather;
}

} // namespace Scripting
} // namespace Enjin

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
