#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

// Procedural dungeon/cave generator (first of the procgen component suite).
// Runs one of the grid-based algorithms from Enjin::Procedural and paints the
// result into this entity's TilemapComponent: carved cells become the floor
// tile, solid cells the wall tile (-1 = leave empty). All three algorithms
// share the same 0/1 grid output, so they live under one mode enum here; the
// sequential randomizers (bag/deck) are a separate component by design.
struct DungeonGeneratorComponent {
    enum class Algorithm : u8 {
        RandomWalk = 0,   // carve a winding cave with a drunkard's walk
        Cellular,         // cellular-automata organic caves
        BSPRooms          // binary-space-partition rooms + corridors
    };

    Algorithm algorithm = Algorithm::RandomWalk;
    u32 width  = 48;
    u32 height = 48;
    u32 seed   = 0;            // 0 = pick a random seed each generate

    // RandomWalk
    u32 walkSteps      = 1500;
    f32 walkTurnChance = 0.5f;

    // Cellular
    u32 fillPercent = 45;      // initial solid %
    u32 iterations  = 5;       // smoothing passes

    // BSP
    u32 minRoomSize   = 5;
    u32 maxRoomSize   = 12;
    u32 splitDepth    = 5;
    u32 corridorWidth = 2;

    // Output tile indices (into the tilemap's tileset)
    i32 floorTile = 0;
    i32 wallTile  = -1;        // -1 = empty (open dungeon over a background)
    bool fillCollision = true; // mark wall cells solid in the tilemap collision mask

    bool generateOnStart = true;  // regenerate when play begins
    u32  lastSeed = 0;            // the seed actually used (shown in the inspector)

    // Runtime: the editor "Generate" button and play-start set this; the
    // DungeonGeneratorSystem consumes it and paints the tilemap. Not serialized.
    bool generateNow = false;
};

} // namespace ECS
} // namespace Enjin
