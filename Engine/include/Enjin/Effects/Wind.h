#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Effects {

// Wind configuration parameters
struct WindParams {
    Math::Vector3 direction = Math::Vector3(1.0f, 0.0f, 0.0f);
    f32 strength = 1.0f;
    f32 gustStrength = 0.5f;
    f32 gustFrequency = 0.3f;   // Hz
    f32 turbulence = 0.2f;
};

// Standalone wind provider queried by both weather and vegetation systems
// Single authoritative source for wind data in the scene
class ENJIN_API WindSystem {
public:
    WindSystem() = default;
    ~WindSystem() = default;

    void Update(f32 deltaTime);

    // Configure global wind
    void SetGlobalWind(const WindParams& params) { m_GlobalParams = params; }
    const WindParams& GetGlobalParams() const { return m_GlobalParams; }

    // Zone overrides (set by WeatherZoneComponent or other systems)
    void SetZoneOverride(const Math::Vector3& dir, f32 strength);
    void ClearZoneOverride();
    bool HasZoneOverride() const { return m_HasZoneOverride; }

    // CPU query: returns wind vector at a world position (includes gusts + turbulence)
    Math::Vector3 GetWindAt(const Math::Vector3& pos) const;

    // Convenience: wind force with sensitivity multiplier
    Math::Vector3 GetWindForce(const Math::Vector3& pos, f32 windSensitivity) const;

    // GPU query: packed vec4 for shader upload (xyz = direction * strength, w = time)
    Math::Vector4 GetWindVector() const;

    f32 GetTime() const { return m_Time; }

    // Fire thermal convection feedback (registered each frame by ElementalSystem)
    void RegisterHeatSource(const Math::Vector3& pos, f32 intensity);
    void ClearHeatSources();

private:
    WindParams m_GlobalParams;

    // Zone override
    bool m_HasZoneOverride = false;
    Math::Vector3 m_ZoneDirection = Math::Vector3(1.0f, 0.0f, 0.0f);
    f32 m_ZoneStrength = 0.0f;

    // Heat sources (max 8, re-registered each frame by ElementalSystem)
    struct HeatSource {
        Math::Vector3 position;
        f32 intensity;
    };
    static constexpr u32 MAX_HEAT_SOURCES = 8;
    HeatSource m_HeatSources[MAX_HEAT_SOURCES];
    u32 m_HeatSourceCount = 0;

    f32 m_Time = 0.0f;
};

} // namespace Effects
} // namespace Enjin
