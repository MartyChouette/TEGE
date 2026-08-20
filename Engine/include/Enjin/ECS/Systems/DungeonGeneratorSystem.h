#pragma once

#include "Enjin/ECS/Components/DungeonGenerator.h"
#include "Enjin/ECS/Components/Gameplay.h"   // TilemapComponent

namespace Enjin {
namespace ECS {

class World;

// Bridges the Enjin::Procedural grid algorithms to a TilemapComponent. Runs the
// dungeon component's chosen algorithm and paints floor/wall tiles. Returns the
// seed actually used (so the inspector can show it / the user can reproduce it).
class DungeonGeneratorSystem {
public:
    // Paint `tilemap` from `gen` using its algorithm + params. Resizes the
    // tilemap to gen.width x gen.height. gen.lastSeed is set to the used seed.
    static u32 Generate(DungeonGeneratorComponent& gen, TilemapComponent& tilemap);

    // Play-start pass: regenerate every DungeonGeneratorComponent that has
    // generateOnStart set and a TilemapComponent to paint into. Main-thread only
    // (reads/writes components) — call once when play begins.
    static void GenerateAll(World* world);
};

} // namespace ECS
} // namespace Enjin
