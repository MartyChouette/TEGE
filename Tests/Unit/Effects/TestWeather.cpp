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

// The setters take a TARGET that Update ramps toward, so these assert the
// clamp on what was asked for. RampReachesItsTarget below covers the ramp.
ENJIN_TEST(WeatherSetter, RainIntensityClamped) {
    WeatherSystem ws;
    ws.SetRainIntensity(1.5f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetTargetRainIntensity(), 1.0f);
    ws.SetRainIntensity(-0.5f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetTargetRainIntensity(), 0.0f);
    ws.SetRainIntensity(0.7f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetTargetRainIntensity(), 0.7f);
}

ENJIN_TEST(WeatherSetter, SnowIntensityClamped) {
    WeatherSystem ws;
    ws.SetSnowIntensity(2.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetTargetSnowIntensity(), 1.0f);
}

ENJIN_TEST(WeatherSetter, FogDensityClamped) {
    WeatherSystem ws;
    ws.SetFogDensity(0.5f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetTargetFogDensity(), 0.5f);
    ws.SetFogDensity(5.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetTargetFogDensity(), 1.0f);
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
// 2D scene mode: particles spawn in an XY sheet, no Z motion
// ===========================================================================

ENJIN_TEST(WeatherMode2D, DefaultOff) {
    WeatherSystem ws;
    ENJIN_EXPECT_FALSE(ws.GetMode2D());
}

ENJIN_TEST(WeatherMode2D, RainParticlesHaveNoZVelocity) {
    WeatherSystem ws;
    ws.Initialize(200);
    ws.SetMode2D(true);
    ws.SetWindDirection(Vector3(1.0f, 0.0f, 1.0f));  // Z wind must be ignored in 2D
    ws.SetWindStrength(2.0f);
    ws.SetWeather(WeatherType::HeavyRain, 0.0f);

    Vector3 camPos(5.0f, 3.0f, 10.0f);
    for (int i = 0; i < 60; ++i) ws.Update(1.0f / 60.0f, camPos);

    ENJIN_ASSERT_TRUE(ws.GetActiveParticleCount() > 0);
    const auto& particles = ws.GetParticles();
    for (u32 i = 0; i < ws.GetActiveParticleCount(); ++i) {
        ENJIN_EXPECT_FLOAT_EQ(particles[i].velocity.z, 0.0f);
    }
}

ENJIN_TEST(WeatherMode2D, ParticlesSpawnInFrontOfCamera) {
    WeatherSystem ws;
    ws.Initialize(200);
    ws.SetMode2D(true);
    ws.SetWeather(WeatherType::Snow, 0.0f);

    Vector3 camPos(0.0f, 0.0f, 10.0f);
    for (int i = 0; i < 60; ++i) ws.Update(1.0f / 60.0f, camPos);

    // 2D camera looks down -Z: every particle must sit in front of it (z < camZ)
    ENJIN_ASSERT_TRUE(ws.GetActiveParticleCount() > 0);
    const auto& particles = ws.GetParticles();
    for (u32 i = 0; i < ws.GetActiveParticleCount(); ++i) {
        ENJIN_EXPECT_TRUE(particles[i].position.z < camPos.z);
    }
}

ENJIN_TEST(WeatherMode2D, Mode3DStillSpreadsInZ) {
    WeatherSystem ws;
    ws.Initialize(200);
    ws.SetWeather(WeatherType::Snow, 0.0f);

    Vector3 camPos(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 60; ++i) ws.Update(1.0f / 60.0f, camPos);

    // 3D mode spawns in a cylinder around the camera — some particles behind it
    ENJIN_ASSERT_TRUE(ws.GetActiveParticleCount() > 0);
    const auto& particles = ws.GetParticles();
    bool anyBehind = false;
    for (u32 i = 0; i < ws.GetActiveParticleCount(); ++i) {
        if (particles[i].position.z > 0.0f) { anyBehind = true; break; }
    }
    ENJIN_EXPECT_TRUE(anyBehind);
}

// ===========================================================================
// Custom precipitation sprites
// ===========================================================================

ENJIN_TEST(WeatherSprites, DefaultProceduralIndices) {
    WeatherSystem ws;
    ENJIN_EXPECT_EQ(ws.GetRainTextureIndex(), -1);
    ENJIN_EXPECT_EQ(ws.GetSnowTextureIndex(), -1);
}

ENJIN_TEST(WeatherSprites, IndicesStored) {
    WeatherSystem ws;
    ws.SetRainTextureIndex(7);
    ws.SetSnowTextureIndex(12);
    ENJIN_EXPECT_EQ(ws.GetRainTextureIndex(), 7);
    ENJIN_EXPECT_EQ(ws.GetSnowTextureIndex(), 12);
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

// ===========================================================================
// Intensity ramp
// ===========================================================================

// The engine has always had a transition time; until now nothing ramped toward
// it, because the intensity setters wrote the live value and every per-frame
// caller (SeasonalWeather, weather zones) overwrote the transition on the same
// frame it was computed. Weather changed in one frame whatever was asked for.
ENJIN_TEST(WeatherRamp, RampReachesItsTargetAndNotBefore) {
    WeatherSystem ws;
    ws.Initialize(64);
    ws.SetIntensityBlendSeconds(1.0f);
    ws.SetRainIntensity(1.0f);

    // Not there yet: a single short frame must not arrive.
    ws.Update(0.1f, Math::Vector3(0.0f, 0.0f, 0.0f));
    const f32 afterOneFrame = ws.GetRainIntensity();
    ENJIN_EXPECT_TRUE(afterOneFrame > 0.0f);
    ENJIN_EXPECT_TRUE(afterOneFrame < 1.0f);

    // And it does arrive, exactly, rather than approaching forever.
    for (int i = 0; i < 200; ++i) ws.Update(0.05f, Math::Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_FLOAT_EQ(ws.GetRainIntensity(), 1.0f);
}

// Teardown has to be immediate: nothing updates the weather after a scene
// stops, so a ramped zero would freeze part-way and strand snow on the ground.
ENJIN_TEST(WeatherRamp, ResetPrecipitationIsImmediate) {
    WeatherSystem ws;
    ws.Initialize(64);
    ws.SetSnowIntensity(1.0f);
    for (int i = 0; i < 200; ++i) ws.Update(0.05f, Math::Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(ws.GetSnowAccumulation() > 0.0f);

    ws.ResetPrecipitation();
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSnowIntensity(), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetTargetSnowIntensity(), 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(ws.GetSnowAccumulation(), 0.0f);
}
