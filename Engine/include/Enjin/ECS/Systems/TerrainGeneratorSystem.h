#pragma once

#include "Enjin/ECS/Components/TerrainGenerator.h"
#include "Enjin/ECS/Components/Terrain.h"

namespace Enjin {
namespace ECS {

class World;

// Bridges the Enjin::Procedural FBM + erosion algorithms to a TerrainComponent.
// Bakes the generator's noise (plus optional erosion) into the terrain heightmap
// and flags it for a remesh. Returns the seed actually used so the inspector can
// show it / the user can reproduce a run.
class TerrainGeneratorSystem {
public:
    // Bake `terrain` from `gen`: FBM -> optional hydraulic/thermal erosion ->
    // renormalize -> scale to maxHeight. Resizes the terrain to gen's grid.
    // gen.lastSeed is set to the used seed.
    static u32 Generate(TerrainGeneratorComponent& gen, TerrainComponent& terrain);

    // Play-start pass: rebake every TerrainGeneratorComponent that has
    // generateOnStart set, creating a TerrainComponent if the entity lacks one.
    // Main-thread only (adr-0004).
    static void GenerateAll(World* world);
};

} // namespace ECS
} // namespace Enjin
