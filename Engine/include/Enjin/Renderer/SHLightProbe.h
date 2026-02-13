#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace Enjin {

// Forward declarations
namespace ECS { class World; }

namespace Renderer {

// Spherical Harmonics probe — L2 (9 coefficients per RGB channel = 27 floats)
struct SHProbe {
    u32 id = 0;
    Math::Vector3 position;

    // 9 SH coefficients per channel (L0: 1, L1: 3, L2: 5)
    f32 coefficientsR[9] = {};
    f32 coefficientsG[9] = {};
    f32 coefficientsB[9] = {};

    bool baked = false;
};

// Grid of SH probes covering an axis-aligned bounding box
struct SHProbeGrid {
    Math::Vector3 boundsMin;
    Math::Vector3 boundsMax;
    u32 resolutionX = 4;
    u32 resolutionY = 2;
    u32 resolutionZ = 4;
    bool autoGenerate = false;
};

// SH lighting system — manages probes and provides irradiance queries
class SHLightingSystem {
public:
    SHLightingSystem() = default;
    ~SHLightingSystem() = default;

    // Probe management
    u32 AddProbe(const Math::Vector3& position);
    void RemoveProbe(u32 probeId);
    void Clear();

    // Bake a single probe (stub: samples ambient light at probe position)
    void BakeProbe(u32 probeId, ECS::World* world);
    void BakeAll(ECS::World* world);

    // Query irradiance at a world position (trilinear interpolation of nearest probes)
    Math::Vector3 GetIrradiance(const Math::Vector3& position) const;

    // Probe access
    const std::vector<SHProbe>& GetProbes() const { return m_Probes; }
    u32 GetProbeCount() const { return static_cast<u32>(m_Probes.size()); }

    // Grid configuration
    SHProbeGrid& GetGrid() { return m_Grid; }
    const SHProbeGrid& GetGrid() const { return m_Grid; }
    void GenerateGridProbes();

    // Serialization
    nlohmann::json Serialize() const;
    void Deserialize(const nlohmann::json& j);

private:
    std::vector<SHProbe> m_Probes;
    SHProbeGrid m_Grid;
    u32 m_NextId = 1;

    // Find nearest probes for interpolation
    const SHProbe* FindNearestProbe(const Math::Vector3& position) const;
};

} // namespace Renderer
} // namespace Enjin
