#pragma once

#include "Enjin/ECS/Components/WFC.h"
#include "Enjin/ECS/Components/Gameplay.h"   // TilemapComponent
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

class World;

// Bridges the WaveFunctionCollapse solver to the world. In Tiles2D mode it paints a
// TilemapComponent; in Modules3D mode it instantiates a prefab per solved cell. Both
// compile the component's edge-socket rules into per-tile adjacency, run the solver
// (reseeding on contradiction up to maxRetries), and set gen.lastSeed / lastSuccess.
class WFCSystem {
public:
    // Tiles2D data path: solve `gen` and paint `tilemap`. Returns true on a full,
    // contradiction-free collapse; false otherwise (the tilemap still holds the best
    // partial result). Ignores `gen.mode` — always 2D. Kept world-free for testing.
    static bool Generate(WFCComponent& gen, TilemapComponent& tilemap);

    // Full path: dispatches on gen.mode. Tiles2D creates/paints the entity's
    // TilemapComponent; Modules3D clears the previous module batch and instantiates a
    // prefab per solved cell, parented to `entity`. Main-thread only (adr-0004).
    static bool Generate(World* world, Entity entity, WFCComponent& gen);

    // Play-start pass: solve every WFCComponent with generateOnStart set. Main-thread only.
    static void GenerateAll(World* world);
};

} // namespace ECS
} // namespace Enjin
