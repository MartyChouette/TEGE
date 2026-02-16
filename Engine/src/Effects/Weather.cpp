#include "Enjin/Effects/Weather.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin {
namespace Effects {

// S14: Fast xorshift32 PRNG replaces weak rand()
static u32 s_WeatherRandState = 1840271613u;
static f32 RandomFloat(f32 min, f32 max) {
    s_WeatherRandState ^= s_WeatherRandState << 13;
    s_WeatherRandState ^= s_WeatherRandState >> 17;
    s_WeatherRandState ^= s_WeatherRandState << 5;
    f32 r = static_cast<f32>(s_WeatherRandState & 0x7FFFFFFF) * (1.0f / 2147483647.0f);
    return min + r * (max - min);
}

void WeatherSystem::Initialize(u32 maxParticles) {
    m_MaxParticles = maxParticles;
    m_Particles.resize(maxParticles);
    m_ActiveParticles = 0;
    m_Initialized = true;
}

void WeatherSystem::Shutdown() {
    m_Particles.clear();
    m_ActiveParticles = 0;
    m_Initialized = false;
}

void WeatherSystem::SetWeather(WeatherType type, f32 transitionTime) {
    if (type == m_CurrentWeather) return;

    m_TargetWeather = type;
    m_TransitionDuration = transitionTime;
    m_TransitionProgress = 0.0f;

    // Cap existing particle lifetimes so old weather fades out quickly
    // instead of lingering (e.g., snow particles living 10s during rain transition)
    constexpr f32 kFadeOutTime = 0.5f;
    for (u32 i = 0; i < m_ActiveParticles; ++i) {
        if (m_Particles[i].lifetime > kFadeOutTime) {
            m_Particles[i].lifetime = kFadeOutTime;
        }
    }
}

void WeatherSystem::Update(f32 deltaTime, const Math::Vector3& cameraPos) {
    if (!m_Initialized) return;
    if (!std::isfinite(cameraPos.x) || !std::isfinite(cameraPos.y) || !std::isfinite(cameraPos.z)) return;

    // Update weather transition
    if (m_TransitionProgress < 1.0f) {
        if (m_TransitionDuration <= 0.0f) {
            m_TransitionProgress = 1.0f;
            m_CurrentWeather = m_TargetWeather;
        } else {
            m_TransitionProgress += deltaTime / m_TransitionDuration;
        }
        if (m_TransitionProgress >= 1.0f) {
            m_TransitionProgress = 1.0f;
            m_CurrentWeather = m_TargetWeather;
        }

        // Interpolate weather properties
        f32 t = m_TransitionProgress;
        // Smooth step for nicer transitions
        t = t * t * (3.0f - 2.0f * t);

        // Update intensities based on target weather
        f32 targetRain = 0.0f, targetSnow = 0.0f, targetFog = 0.0f;

        switch (m_TargetWeather) {
            case WeatherType::Clear:
                break;
            case WeatherType::Cloudy:
                targetFog = 0.2f;
                break;
            case WeatherType::Rain:
                targetRain = 0.6f;
                targetFog = 0.3f;
                break;
            case WeatherType::HeavyRain:
                targetRain = 1.0f;
                targetFog = 0.6f;
                break;
            case WeatherType::Snow:
                targetSnow = 1.0f;
                targetFog = 0.4f;
                break;
            case WeatherType::Fog:
                targetFog = 0.8f;
                break;
            case WeatherType::Storm:
                targetRain = 1.0f;
                targetFog = 0.7f;
                break;
        }

        m_RainIntensity = Math::Lerp(m_RainIntensity, targetRain, t);
        m_SnowIntensity = Math::Lerp(m_SnowIntensity, targetSnow, t);
        m_FogDensity = Math::Lerp(m_FogDensity, targetFog, t);
    }

    // Update particles
    UpdateParticles(deltaTime, cameraPos);

    // Spawn new particles based on intensity (rates tuned for visual quality vs performance)
    f32 rainRate = m_RainIntensity * 400.0f;  // Particles per second
    f32 snowRate = m_SnowIntensity * 200.0f;

    m_SpawnAccumulator += deltaTime;
    f32 spawnInterval = 1.0f / Math::Max(rainRate + snowRate, 1.0f);

    while (m_SpawnAccumulator >= spawnInterval && m_ActiveParticles < m_MaxParticles) {
        m_SpawnAccumulator -= spawnInterval;

        if (m_RainIntensity > 0.01f && RandomFloat(0, 1) < m_RainIntensity / (m_RainIntensity + m_SnowIntensity + 0.001f)) {
            SpawnRainParticle(cameraPos);
        } else if (m_SnowIntensity > 0.01f) {
            SpawnSnowParticle(cameraPos);
        }
    }

    // Clear the one-frame fire flag at the start of each update
    m_LightningJustFired = false;

    // Update lightning
    if (m_CurrentWeather == WeatherType::Storm || m_TargetWeather == WeatherType::Storm) {
        UpdateLightning(deltaTime);
    } else {
        m_LightningActive = false;
        m_LightningIntensity = 0.0f;
    }
}

void WeatherSystem::SpawnRainParticle(const Math::Vector3& cameraPos) {
    if (m_ActiveParticles >= m_MaxParticles) return;

    WeatherParticle& p = m_Particles[m_ActiveParticles++];

    // Spawn in a cylinder around camera
    f32 angle = RandomFloat(0, Math::PI * 2.0f);
    f32 dist = RandomFloat(0, m_SpawnRadius);

    p.position.x = cameraPos.x + Math::Cos(angle) * dist;
    p.position.z = cameraPos.z + Math::Sin(angle) * dist;

    // Distribute vertically so the volume is pre-filled with falling rain.
    // Bias toward the top so most drops enter naturally from above.
    f32 t = RandomFloat(0.0f, 1.0f);
    t = t * t;  // Quadratic bias — ~75% spawn in upper half
    p.position.y = cameraPos.y + m_SpawnHeight - t * m_SpawnHeight * 2.0f;

    // Rain falls fast with slight wind influence
    p.velocity.x = m_WindDirection.x * m_WindStrength * 2.0f;
    p.velocity.y = -15.0f - RandomFloat(0, 5.0f);  // Fast fall
    p.velocity.z = m_WindDirection.z * m_WindStrength * 2.0f;

    p.lifetime = 3.0f;
    p.size = RandomFloat(0.02f, 0.05f);  // Thin streaks
    p.alpha = 0.6f;
}

void WeatherSystem::SpawnSnowParticle(const Math::Vector3& cameraPos) {
    if (m_ActiveParticles >= m_MaxParticles) return;

    WeatherParticle& p = m_Particles[m_ActiveParticles++];

    f32 angle = RandomFloat(0, Math::PI * 2.0f);
    f32 dist = RandomFloat(0, m_SpawnRadius);

    p.position.x = cameraPos.x + Math::Cos(angle) * dist;
    p.position.z = cameraPos.z + Math::Sin(angle) * dist;

    // Distribute vertically — gentler bias for snow since it falls slower
    f32 t = RandomFloat(0.0f, 1.0f);
    t = t * t;
    p.position.y = cameraPos.y + m_SpawnHeight - t * m_SpawnHeight * 2.0f;

    // Snow falls slowly, drifts more with wind
    p.velocity.x = m_WindDirection.x * m_WindStrength * 3.0f + RandomFloat(-0.5f, 0.5f);
    p.velocity.y = -1.5f - RandomFloat(0, 1.0f);  // Slow fall
    p.velocity.z = m_WindDirection.z * m_WindStrength * 3.0f + RandomFloat(-0.5f, 0.5f);

    p.lifetime = 10.0f;
    p.size = RandomFloat(0.05f, 0.15f);  // Fluffy flakes
    p.alpha = 0.8f;
}

void WeatherSystem::UpdateParticles(f32 deltaTime, const Math::Vector3& cameraPos) {
    u32 i = 0;
    while (i < m_ActiveParticles) {
        WeatherParticle& p = m_Particles[i];

        // Update position
        p.position = p.position + p.velocity * deltaTime;
        p.lifetime -= deltaTime;

        // Add some drift to snow
        if (p.velocity.y > -5.0f) {  // Snow (slower fall)
            p.velocity.x += RandomFloat(-0.1f, 0.1f) * deltaTime;
            p.velocity.z += RandomFloat(-0.1f, 0.1f) * deltaTime;
        }

        // Check if particle should be removed
        bool remove = p.lifetime <= 0.0f ||
                      p.position.y < cameraPos.y - m_SpawnHeight ||  // Below camera (full fall distance)
                      Math::Abs(p.position.x - cameraPos.x) > m_SpawnRadius * 3.0f ||
                      Math::Abs(p.position.z - cameraPos.z) > m_SpawnRadius * 3.0f;

        if (remove) {
            // Swap with last and decrement
            m_Particles[i] = m_Particles[m_ActiveParticles - 1];
            m_ActiveParticles--;
        } else {
            i++;
        }
    }
}

void WeatherSystem::UpdateLightning(f32 deltaTime) {
    m_LightningTimer += deltaTime;

    if (m_LightningActive) {
        // Flash decay
        m_LightningIntensity -= deltaTime * 5.0f;
        if (m_LightningIntensity <= 0.0f) {
            m_LightningActive = false;
            m_LightningIntensity = 0.0f;
            m_NextLightningTime = RandomFloat(m_LightningMinInterval, m_LightningMaxInterval);
            m_LightningTimer = 0.0f;
        }
    } else if (m_LightningTimer >= m_NextLightningTime) {
        // Trigger lightning
        m_LightningActive = true;
        m_LightningJustFired = true;  // One-frame flag for sound/event hookup
        m_LightningIntensity = RandomFloat(0.5f, 1.0f);
    }
}

// 2D Weather Implementation
void Weather2D::Initialize(u32 maxParticles) {
    m_Particles.resize(maxParticles);
}

void Weather2D::Update(f32 deltaTime, const Math::Vector2& cameraPos, const Math::Vector2& screenSize) {
    (void)cameraPos;
    m_ScreenSize = screenSize;

    // Early-out for weather types that produce no visible particles
    if (m_Weather != WeatherType::Rain && m_Weather != WeatherType::HeavyRain &&
        m_Weather != WeatherType::Snow && m_Weather != WeatherType::Storm) {
        return;
    }

    // P-17: Combined position update + velocity update in a single pass
    f32 rainSpeed = 300.0f;
    f32 snowSpeed = 50.0f;

    for (auto& p : m_Particles) {
        // Update velocity based on weather type
        switch (m_Weather) {
            case WeatherType::Rain:
            case WeatherType::HeavyRain:
                p.velocity.y = rainSpeed * m_Intensity;
                p.velocity.x = 20.0f;  // Slight angle
                break;
            case WeatherType::Snow:
                p.velocity.y = snowSpeed * m_Intensity;
                p.velocity.x = Math::Sin(p.position.y * 0.01f) * 30.0f;  // Drift
                break;
            default:
                p.velocity = Math::Vector2(0, 0);
                break;
        }

        // Update position
        p.position = p.position + p.velocity * deltaTime;

        // Wrap around screen
        if (p.position.y > screenSize.y) {
            p.position.y = 0;
            p.position.x = RandomFloat(0, screenSize.x);
        }
        if (p.position.x < 0) p.position.x = screenSize.x;
        if (p.position.x > screenSize.x) p.position.x = 0;
    }
}

void Weather2D::SetWeather(WeatherType type) {
    m_Weather = type;

    // Reset particles for new weather
    for (usize i = 0; i < m_Particles.size(); ++i) {
        m_Particles[i].position.x = RandomFloat(0, m_ScreenSize.x);
        m_Particles[i].position.y = RandomFloat(0, m_ScreenSize.y);
        m_Particles[i].size = (type == WeatherType::Snow) ? RandomFloat(2, 6) : RandomFloat(1, 3);
        m_Particles[i].alpha = RandomFloat(0.3f, 0.8f);
    }
}

} // namespace Effects
} // namespace Enjin
