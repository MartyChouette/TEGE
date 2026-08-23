#pragma once

#include "Enjin/ECS/Components/Scatter.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

class World;

// Placement logic for ScatterComponent. Samples points with the chosen
// distribution, then instantiates the component's prefab at each point, parenting
// every instance to the source entity and tagging it with a ScatterInstanceComponent
// so the batch can be found and cleared on the next generate. Entity creation is
// main-thread only (adr-0004), so all of this runs from the editor button or the
// play-start pass — never a worker thread.
class ScatterSystem {
public:
    // Destroy the previous batch (marked children of `entity`) and place a fresh
    // one. Fills scatter.lastSeed / lastCount. No-op with a log if prefabPath is
    // empty or the prefab fails to load. Returns the number of instances placed.
    static u32 Generate(World* world, Entity entity, ScatterComponent& scatter);

    // Destroy every ScatterInstanceComponent child of `entity` (the previous batch).
    // Called by Generate; exposed for the inspector "Clear" button. Returns count removed.
    static u32 Clear(World* world, Entity entity);

    // Play-start pass: generate every ScatterComponent with generateOnStart set.
    // Main-thread only.
    static void GenerateAll(World* world);

    // Sample a terrain's surface at a local XZ point (terrain-local frame, origin
    // at its centre — the same frame scatter offsets use). Bilinear height, slope
    // from central differences. Returns false when the point lies outside the
    // terrain's extent. Public so the conform math is unit-testable.
    static bool SampleTerrainHeight(const TerrainComponent& terrain, f32 x, f32 z,
                                    f32& outHeight, f32& outSlopeDeg);
};

} // namespace ECS
} // namespace Enjin
