#include "Enjin/Effects/Wind.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin {
namespace Effects {

void WindSystem::Update(f32 deltaTime) {
    m_Time += deltaTime;
}

void WindSystem::SetZoneOverride(const Math::Vector3& dir, f32 strength) {
    m_HasZoneOverride = true;
    m_ZoneDirection = dir;
    m_ZoneStrength = strength;
}

void WindSystem::ClearZoneOverride() {
    m_HasZoneOverride = false;
}

Math::Vector3 WindSystem::GetWindAt(const Math::Vector3& pos) const {
    // Use zone override if active, otherwise global
    Math::Vector3 dir = m_HasZoneOverride ? m_ZoneDirection : m_GlobalParams.direction;
    f32 strength = m_HasZoneOverride ? m_ZoneStrength : m_GlobalParams.strength;

    Math::Vector3 baseWind = dir * strength;

    // Gust: sinusoidal variation based on time and spatial hash
    // Spatial hash adds variation so wind isn't identical everywhere
    f32 spatialHash = pos.x * 0.137f + pos.z * 0.271f;
    f32 gustPhase = m_Time * m_GlobalParams.gustFrequency * Math::PI * 2.0f + spatialHash;
    f32 gust = std::sin(gustPhase) * m_GlobalParams.gustStrength;

    // Turbulence: higher-frequency noise for small-scale variation
    f32 turbPhase = m_Time * 2.7f + spatialHash * 3.1f;
    f32 turb = std::sin(turbPhase) * m_GlobalParams.turbulence * 0.5f;

    Math::Vector3 result = baseWind + dir * (gust + turb);

    // Thermal convection from nearby heat sources
    for (u32 i = 0; i < m_HeatSourceCount; ++i) {
        Math::Vector3 diff = pos - m_HeatSources[i].position;
        f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        f32 heatRadius = 5.0f; // 5m influence radius
        if (distSq < heatRadius * heatRadius) {
            f32 falloff = 1.0f - distSq / (heatRadius * heatRadius);
            f32 heatIntensity = m_HeatSources[i].intensity * falloff;
            // Upward convection
            result.y += heatIntensity * 3.0f;
            // Slight outward push (horizontal)
            if (distSq > 0.01f) {
                f32 invDist = 1.0f / std::sqrt(distSq);
                result.x += diff.x * invDist * heatIntensity * 0.5f;
                result.z += diff.z * invDist * heatIntensity * 0.5f;
            }
        }
    }

    return result;
}

Math::Vector3 WindSystem::GetWindForce(const Math::Vector3& pos, f32 windSensitivity) const {
    return GetWindAt(pos) * windSensitivity;
}

void WindSystem::RegisterHeatSource(const Math::Vector3& pos, f32 intensity) {
    if (m_HeatSourceCount >= MAX_HEAT_SOURCES) {
        // Replace lowest intensity source
        u32 weakest = 0;
        for (u32 i = 1; i < MAX_HEAT_SOURCES; ++i) {
            if (m_HeatSources[i].intensity < m_HeatSources[weakest].intensity) weakest = i;
        }
        if (intensity > m_HeatSources[weakest].intensity) {
            m_HeatSources[weakest] = { pos, intensity };
        }
        return;
    }
    m_HeatSources[m_HeatSourceCount++] = { pos, intensity };
}

void WindSystem::ClearHeatSources() {
    m_HeatSourceCount = 0;
}

Math::Vector4 WindSystem::GetWindVector() const {
    // Packed for GPU: xyz = effective wind direction * strength, w = time
    Math::Vector3 dir = m_HasZoneOverride ? m_ZoneDirection : m_GlobalParams.direction;
    f32 strength = m_HasZoneOverride ? m_ZoneStrength : m_GlobalParams.strength;

    Math::Vector3 wind = dir * strength;
    return Math::Vector4(wind.x, wind.y, wind.z, m_Time);
}

} // namespace Effects
} // namespace Enjin
