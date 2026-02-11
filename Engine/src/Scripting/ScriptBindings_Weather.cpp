#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Effects/Weather.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

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
        asFUNCTION(Weather_Set), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Weather_Get()",
        asFUNCTION(Weather_Get), asCALL_CDECL));

    // Intensity
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetRainIntensity(float)",
        asFUNCTION(Weather_SetRainIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Weather_GetRainIntensity()",
        asFUNCTION(Weather_GetRainIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetSnowIntensity(float)",
        asFUNCTION(Weather_SetSnowIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Weather_GetSnowIntensity()",
        asFUNCTION(Weather_GetSnowIntensity), asCALL_CDECL));

    // Fog
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetFogDensity(float)",
        asFUNCTION(Weather_SetFogDensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Weather_GetFogDensity()",
        asFUNCTION(Weather_GetFogDensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetFogColor(float, float, float)",
        asFUNCTION(Weather_SetFogColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetFogRange(float, float)",
        asFUNCTION(Weather_SetFogRange), asCALL_CDECL));

    // Wind
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetWind(float, float, float, float)",
        asFUNCTION(Weather_SetWind), asCALL_CDECL));

    // Lightning
    AS_CHECK(engine->RegisterGlobalFunction("bool Weather_IsLightning()",
        asFUNCTION(Weather_IsLightning), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Weather_LightningJustFired()",
        asFUNCTION(Weather_LightningJustFired), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Weather_SetLightningInterval(float, float)",
        asFUNCTION(Weather_SetLightningInterval), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
