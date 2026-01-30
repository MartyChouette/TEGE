#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/Camera.h"
#include <vector>

namespace Enjin {
namespace Effects {

// Weather type
enum class WeatherType : u8 {
    Clear,
    Cloudy,
    Rain,
    HeavyRain,
    Snow,
    Fog,
    Storm
};

// Simple particle for weather effects
struct WeatherParticle {
    Math::Vector3 position;
    Math::Vector3 velocity;
    f32 lifetime;
    f32 size;
    f32 alpha;
};

// Lightweight weather system (PS1/PS2 style)
// Uses simple particle spawning around camera for rain/snow
// Distance fog for atmosphere
// No volumetric rendering - just simple, effective techniques
class ENJIN_API WeatherSystem {
public:
    WeatherSystem() = default;
    ~WeatherSystem() = default;

    void Initialize(u32 maxParticles = 500);
    void Shutdown();

    void Update(f32 deltaTime, const Math::Vector3& cameraPos);

    // Weather control
    void SetWeather(WeatherType type, f32 transitionTime = 2.0f);
    WeatherType GetWeather() const { return m_CurrentWeather; }

    // Individual settings
    void SetRainIntensity(f32 intensity) { m_RainIntensity = Math::Clamp(intensity, 0.0f, 1.0f); }
    void SetSnowIntensity(f32 intensity) { m_SnowIntensity = Math::Clamp(intensity, 0.0f, 1.0f); }
    void SetFogDensity(f32 density) { m_FogDensity = Math::Clamp(density, 0.0f, 1.0f); }
    void SetFogColor(const Math::Vector3& color) { m_FogColor = color; }
    void SetFogStart(f32 start) { m_FogStart = start; }
    void SetFogEnd(f32 end) { m_FogEnd = end; }

    f32 GetRainIntensity() const { return m_RainIntensity; }
    f32 GetSnowIntensity() const { return m_SnowIntensity; }
    f32 GetFogDensity() const { return m_FogDensity; }
    Math::Vector3 GetFogColor() const { return m_FogColor; }
    f32 GetFogStart() const { return m_FogStart; }
    f32 GetFogEnd() const { return m_FogEnd; }

    // Wind affects rain/snow direction
    void SetWindDirection(const Math::Vector3& dir) { m_WindDirection = dir; }
    void SetWindStrength(f32 strength) { m_WindStrength = strength; }

    // Get particles for rendering
    const std::vector<WeatherParticle>& GetParticles() const { return m_Particles; }
    u32 GetActiveParticleCount() const { return m_ActiveParticles; }

    // Area settings (how far from camera to spawn)
    void SetSpawnRadius(f32 radius) { m_SpawnRadius = radius; }
    void SetSpawnHeight(f32 height) { m_SpawnHeight = height; }

    // Lightning (for storms)
    bool IsLightningActive() const { return m_LightningActive; }
    f32 GetLightningIntensity() const { return m_LightningIntensity; }

    // Lightning frequency control
    void SetLightningInterval(f32 minSeconds, f32 maxSeconds) {
        m_LightningMinInterval = Math::Max(0.1f, minSeconds);
        m_LightningMaxInterval = Math::Max(m_LightningMinInterval, maxSeconds);
    }

    // Returns true for exactly one frame when lightning fires.
    // Game code can poll this to trigger a sound effect or other event.
    bool LightningJustFired() const { return m_LightningJustFired; }

private:
    void SpawnRainParticle(const Math::Vector3& cameraPos);
    void SpawnSnowParticle(const Math::Vector3& cameraPos);
    void UpdateParticles(f32 deltaTime, const Math::Vector3& cameraPos);
    void UpdateLightning(f32 deltaTime);

    WeatherType m_CurrentWeather = WeatherType::Clear;
    WeatherType m_TargetWeather = WeatherType::Clear;
    f32 m_TransitionProgress = 1.0f;
    f32 m_TransitionDuration = 2.0f;

    // Particle pool
    std::vector<WeatherParticle> m_Particles;
    u32 m_MaxParticles = 500;
    u32 m_ActiveParticles = 0;

    // Weather intensities
    f32 m_RainIntensity = 0.0f;
    f32 m_SnowIntensity = 0.0f;

    // Fog settings (linear fog, very PS1/N64)
    f32 m_FogDensity = 0.0f;
    Math::Vector3 m_FogColor = Math::Vector3(0.5f, 0.5f, 0.6f);
    f32 m_FogStart = 20.0f;
    f32 m_FogEnd = 100.0f;

    // Wind
    Math::Vector3 m_WindDirection = Math::Vector3(0.2f, 0.0f, 0.1f);
    f32 m_WindStrength = 1.0f;

    // Spawn area
    f32 m_SpawnRadius = 30.0f;
    f32 m_SpawnHeight = 20.0f;

    // Lightning
    bool m_LightningActive = false;
    bool m_LightningJustFired = false;  // True for one frame when lightning triggers
    f32 m_LightningIntensity = 0.0f;
    f32 m_LightningTimer = 0.0f;
    f32 m_NextLightningTime = 5.0f;
    f32 m_LightningMinInterval = 2.0f;
    f32 m_LightningMaxInterval = 10.0f;

    // Spawn timing
    f32 m_SpawnAccumulator = 0.0f;

    bool m_Initialized = false;
};

// 2D Weather variant (for side-scrollers, top-down)
class ENJIN_API Weather2D {
public:
    void Initialize(u32 maxParticles = 200);
    void Update(f32 deltaTime, const Math::Vector2& cameraPos, const Math::Vector2& screenSize);

    void SetWeather(WeatherType type);
    void SetIntensity(f32 intensity) { m_Intensity = intensity; }

    // For 2D, weather is screen-space particles
    struct Particle2D {
        Math::Vector2 position;  // Screen space
        Math::Vector2 velocity;
        f32 size;
        f32 alpha;
    };

    const std::vector<Particle2D>& GetParticles() const { return m_Particles; }

    // Parallax layers for depth (very SNES/Genesis)
    void SetParallaxLayers(u32 layers) { m_ParallaxLayers = layers; }

private:
    std::vector<Particle2D> m_Particles;
    WeatherType m_Weather = WeatherType::Clear;
    f32 m_Intensity = 0.5f;
    u32 m_ParallaxLayers = 3;
    Math::Vector2 m_ScreenSize;
};

} // namespace Effects
} // namespace Enjin
