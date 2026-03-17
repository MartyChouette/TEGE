#include "EnjinTest.h"
#include "Enjin/Effects/Weather.h"

using namespace Enjin;
using namespace Enjin::Effects;
using namespace Enjin::Math;

// ===========================================================================
// WeatherSystem Initial State
// ===========================================================================

ENJIN_TEST(WeatherInit, DefaultWeatherIsClear) {
    WeatherSystem ws;
    ENJIN_EXPECT_EQ((int)ws.GetWeather(), (int)WeatherType::Clear);
}

ENJIN_TEST(WeatherInit, DefaultRainZero) {
    WeatherSystem ws;
    ENJIN_EXPECT_FLOAT_EQ(ws.GetRainIntensity(), 0.0f);
}

ENJIN_TEST(WeatherInit, DefaultSnowZero) {
    WeatherSystem ws;
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSnowIntensity(), 0.0f);
}

ENJIN_TEST(WeatherInit, DefaultFog) {
    WeatherSystem ws;
    ENJIN_EXPECT_FLOAT_EQ(ws.GetFogDensity(), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetFogStart(), 20.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetFogEnd(), 100.0f);
}

ENJIN_TEST(WeatherInit, DefaultWind) {
    WeatherSystem ws;
    ENJIN_EXPECT_FLOAT_EQ(ws.GetWindStrength(), 1.0f);
}

ENJIN_TEST(WeatherInit, DefaultSpawnArea) {
    WeatherSystem ws;
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSpawnRadius(), 50.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSpawnHeight(), 20.0f);
}

ENJIN_TEST(WeatherInit, NoLightning) {
    WeatherSystem ws;
    ENJIN_EXPECT_FALSE(ws.IsLightningActive());
    ENJIN_EXPECT_FLOAT_EQ(ws.GetLightningIntensity(), 0.0f);
}

ENJIN_TEST(WeatherInit, NoParticles) {
    WeatherSystem ws;
    ENJIN_EXPECT_EQ(ws.GetActiveParticleCount(), 0u);
}

// ===========================================================================
// WeatherSystem Setters
// ===========================================================================

ENJIN_TEST(WeatherSetter, RainIntensityClamped) {
    WeatherSystem ws;
    ws.SetRainIntensity(1.5f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetRainIntensity(), 1.0f);
    ws.SetRainIntensity(-0.5f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetRainIntensity(), 0.0f);
    ws.SetRainIntensity(0.7f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetRainIntensity(), 0.7f);
}

ENJIN_TEST(WeatherSetter, SnowIntensityClamped) {
    WeatherSystem ws;
    ws.SetSnowIntensity(2.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSnowIntensity(), 1.0f);
}

ENJIN_TEST(WeatherSetter, FogDensityClamped) {
    WeatherSystem ws;
    ws.SetFogDensity(0.5f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetFogDensity(), 0.5f);
    ws.SetFogDensity(5.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetFogDensity(), 1.0f);
}

ENJIN_TEST(WeatherSetter, FogColor) {
    WeatherSystem ws;
    ws.SetFogColor(Vector3(0.8f, 0.8f, 0.9f));
    Vector3 color = ws.GetFogColor();
    ENJIN_EXPECT_FLOAT_EQ(color.x, 0.8f);
    ENJIN_EXPECT_FLOAT_EQ(color.z, 0.9f);
}

ENJIN_TEST(WeatherSetter, FogStartEnd) {
    WeatherSystem ws;
    ws.SetFogStart(10.0f);
    ws.SetFogEnd(50.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetFogStart(), 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetFogEnd(), 50.0f);
}

ENJIN_TEST(WeatherSetter, WindDirection) {
    WeatherSystem ws;
    ws.SetWindDirection(Vector3(1.0f, 0.0f, 0.0f));
    Vector3 wind = ws.GetWindDirection();
    ENJIN_EXPECT_FLOAT_EQ(wind.x, 1.0f);
}

ENJIN_TEST(WeatherSetter, WindStrength) {
    WeatherSystem ws;
    ws.SetWindStrength(3.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetWindStrength(), 3.0f);
}

ENJIN_TEST(WeatherSetter, SpawnRadius) {
    WeatherSystem ws;
    ws.SetSpawnRadius(100.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSpawnRadius(), 100.0f);
}

ENJIN_TEST(WeatherSetter, SpawnHeight) {
    WeatherSystem ws;
    ws.SetSpawnHeight(30.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSpawnHeight(), 30.0f);
}

// ===========================================================================
// Weather2D
// ===========================================================================

ENJIN_TEST(Weather2D, DefaultState) {
    Weather2D w2d;
    ENJIN_EXPECT_EQ(w2d.GetParticles().size(), (size_t)0);
}

// ===========================================================================
// WeatherType Enum Coverage
// ===========================================================================

ENJIN_TEST(WeatherEnum, AllTypes) {
    ENJIN_EXPECT_EQ((int)WeatherType::Clear, 0);
    ENJIN_EXPECT_EQ((int)WeatherType::Cloudy, 1);
    ENJIN_EXPECT_EQ((int)WeatherType::Rain, 2);
    ENJIN_EXPECT_EQ((int)WeatherType::HeavyRain, 3);
    ENJIN_EXPECT_EQ((int)WeatherType::Snow, 4);
    ENJIN_EXPECT_EQ((int)WeatherType::Fog, 5);
    ENJIN_EXPECT_EQ((int)WeatherType::Storm, 6);
}

ENJIN_TEST_MAIN()
