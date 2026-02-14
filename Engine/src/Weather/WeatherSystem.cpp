// =============================================================================
// DEAD CODE: This file implemented the unused Enjin::Weather::WeatherSystem class.
// The active weather system is Enjin::Effects::WeatherSystem in Effects/Weather.cpp.
// This file is not compiled via CMakeLists.txt and has no consumers.
// Retained as a commented-out stub for reference. Safe to delete entirely.
// =============================================================================

#if 0  // Dead code — superseded by Effects::WeatherSystem

#include "Enjin/Weather/WeatherSystem.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

namespace Enjin {
namespace Weather {

WeatherSystem::WeatherSystem() {}
WeatherSystem::~WeatherSystem() { Shutdown(); }

bool WeatherSystem::Initialize() {
    ENJIN_LOG_INFO(Renderer, "Initializing Weather System...");
    return true;
}

void WeatherSystem::Shutdown() {}

void WeatherSystem::SetWeather(WeatherType type, f32 intensity) {
    m_CurrentWeather = type;
    m_TargetIntensity = Math::Clamp(intensity, 0.0f, 1.0f);
    ENJIN_LOG_INFO(Renderer, "Weather changed to type %d with intensity %.2f",
        static_cast<int>(type), intensity);
}

void WeatherSystem::SetIntensity(f32 intensity) {
    m_TargetIntensity = Math::Clamp(intensity, 0.0f, 1.0f);
}

void WeatherSystem::Update(f32 deltaTime) {
    f32 diff = m_TargetIntensity - m_Intensity;
    if (Math::Abs(diff) > 0.01f) {
        f32 change = Math::Sign(diff) * m_TransitionSpeed * deltaTime;
        if (Math::Abs(change) > Math::Abs(diff)) {
            m_Intensity = m_TargetIntensity;
        } else {
            m_Intensity += change;
        }
    } else {
        m_Intensity = m_TargetIntensity;
    }
}

void WeatherSystem::SetWindDirection(const Math::Vector3& direction) {
    m_WindDirection = direction.Normalized();
}

f32 WeatherSystem::GetFogDensity() const {
    if (m_CurrentWeather == WeatherType::Fog) return m_Intensity * 0.05f;
    return 0.0f;
}

Math::Vector3 WeatherSystem::GetFogColor() const {
    switch (m_CurrentWeather) {
        case WeatherType::Fog: return Math::Vector3(0.7f, 0.7f, 0.8f);
        case WeatherType::Rain:
        case WeatherType::Storm: return Math::Vector3(0.5f, 0.5f, 0.6f);
        default: return Math::Vector3(1.0f, 1.0f, 1.0f);
    }
}

} // namespace Weather
} // namespace Enjin

#endif // Dead code
