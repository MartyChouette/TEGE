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

    return baseWind + dir * (gust + turb);
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
