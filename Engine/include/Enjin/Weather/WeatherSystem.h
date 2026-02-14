#pragma once

// =============================================================================
// DEAD CODE: This file contains an unused WeatherSystem class in the
// Enjin::Weather namespace. The active weather system is Enjin::Effects::WeatherSystem
// defined in "Enjin/Effects/Weather.h". This class is not referenced anywhere in
// the codebase and is not compiled via CMakeLists.txt.
//
// Retained as a commented-out stub for reference. Safe to delete entirely.
// =============================================================================

#if 0  // Dead code — superseded by Effects::WeatherSystem in Effects/Weather.h

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>

namespace Enjin {
namespace Weather {

enum class WeatherType {
    Clear,
    Rain,
    Snow,
    Fog,
    Storm,
    Custom
};

class ENJIN_API WeatherSystem {
public:
    WeatherSystem();
    ~WeatherSystem();

    bool Initialize();
    void Shutdown();
    void SetWeather(WeatherType type, f32 intensity = 1.0f);
    WeatherType GetCurrentWeather() const { return m_CurrentWeather; }
    f32 GetIntensity() const { return m_Intensity; }
    void SetIntensity(f32 intensity);
    void Update(f32 deltaTime);
    Math::Vector3 GetWindDirection() const { return m_WindDirection; }
    void SetWindDirection(const Math::Vector3& direction);
    f32 GetWindSpeed() const { return m_WindSpeed; }
    void SetWindSpeed(f32 speed) { m_WindSpeed = speed; }
    f32 GetFogDensity() const;
    Math::Vector3 GetFogColor() const;

private:
    WeatherType m_CurrentWeather = WeatherType::Clear;
    f32 m_Intensity = 0.0f;
    f32 m_TargetIntensity = 0.0f;
    f32 m_TransitionSpeed = 2.0f;
    Math::Vector3 m_WindDirection = Math::Vector3(1.0f, 0.0f, 0.0f);
    f32 m_WindSpeed = 1.0f;
};

} // namespace Weather
} // namespace Enjin

#endif // Dead code
