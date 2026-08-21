#pragma once

#include "Enjin/ECS/Components/WFC.h"
#include "Enjin/ECS/Components/Gameplay.h"   // TilemapComponent

namespace Enjin {
namespace ECS {

class World;

// Bridges the Enjin::Procedural WaveFunctionCollapse solver to a TilemapComponent.
// Compiles the component's edge-socket rules into per-tile adjacency, runs the
// solver (reseeding on contradiction up to maxRetries), and paints the collapsed
// grid into the tilemap. Sets gen.lastSeed / gen.lastSuccess.
class WFCSystem {
public:
    // Solve `gen` and paint `tilemap`. Returns true if the grid fully collapsed with
    // no contradiction; false if every retry hit a contradiction (the tilemap is
    // still painted with the best partial result so the user can see what happened).
    static bool Generate(WFCComponent& gen, TilemapComponent& tilemap);

    // Play-start pass: solve every WFCComponent with generateOnStart set, creating a
    // TilemapComponent if the entity lacks one. Main-thread only (adr-0004).
    static void GenerateAll(World* world);
};

} // namespace ECS
} // namespace Enjin
