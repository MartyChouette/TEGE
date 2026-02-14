#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/Components/Terrain.h"

namespace Enjin {
namespace Effects {

struct FluidTerrainConfig {
    f32 erosionRate = 0.01f;        // How fast fluid erodes terrain
    f32 depositionRate = 0.005f;    // How fast sediment deposits
    f32 hardness = 0.5f;            // Terrain resistance to erosion (0=soft, 1=hard)
    f32 heightScale = 1.0f;         // Scale fluid density -> terrain height change
    bool bidirectional = true;      // Terrain slope feeds back into fluid velocity
    bool accumulateMode = false;    // false=erosion mode, true=additive (lava building)
    f32 slopeVelocityScale = 5.0f;  // How strongly terrain slope drives fluid flow
    f32 maxErosionPerFrame = 0.5f;  // Clamp erosion per step to prevent instability
    bool enabled = false;           // Must be explicitly enabled
};

class ENJIN_API FluidTerrainCoupling {
public:
    void Update(f32 dt, ECS::World* world, FluidSimulation& fluidSim);

    void SetConfig(const FluidTerrainConfig& config) { m_Config = config; }
    const FluidTerrainConfig& GetConfig() const { return m_Config; }
    FluidTerrainConfig& GetConfig() { return m_Config; }

private:
    FluidTerrainConfig m_Config;

    // Sample terrain height with bilinear interpolation for sub-cell accuracy
    f32 SampleTerrainHeight(const ECS::TerrainComponent& terrain, f32 tx, f32 tz) const;

    // Compute terrain slope (gradient) at a given terrain-space position
    Math::Vector3 ComputeTerrainSlope(const ECS::TerrainComponent& terrain, f32 tx, f32 tz) const;
};

} // namespace Effects
} // namespace Enjin
