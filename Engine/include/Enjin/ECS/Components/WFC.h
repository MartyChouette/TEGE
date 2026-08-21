#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// Space-builder of the procgen suite (sibling of DungeonGenerator/Scatter/Terrain).
// Wave Function Collapse fills a grid so that every neighbour pairing is legal,
// then paints the result into the entity's TilemapComponent. Unlike the dungeon
// algorithms it is CONSTRAINT-based: you describe which tiles may touch, and WFC
// finds an arrangement that satisfies all of them (think Carcassonne / pipe mazes /
// autotiled dungeons). It can also FAIL — if the rules are too tight there is no
// legal arrangement, so the component retries with fresh seeds and reports whether
// the last run fully collapsed.
//
// Adjacency is authored with EDGE SOCKETS, not raw neighbour lists: each tile has a
// label on its North/East/South/West edge, and two tiles may sit next to each other
// only when their touching edges carry the same label. A road tile with edges
// "road,grass,road,grass" tiles horizontally with other roads and vertically with
// grass. Author the tiles + sockets in the inspector; the WFCSystem compiles them
// into the low-level adjacency the solver needs.
struct WFCComponent {
    struct Tile {
        i32 tileIndex = 0;        // index into the Tilemap's tileset (-1 = empty cell)
        // Edge socket labels, indexed 0=North, 1=East, 2=South, 3=West. Two tiles may
        // be adjacent when the labels on their touching edges are equal.
        std::string edges[4];
        bool solid = false;       // mark this tile's cells solid in the collision mask
    };

    u32 width  = 20;
    u32 height = 20;
    u32 seed   = 0;               // 0 = pick a random seed each generate
    u32 maxBacktracks = 1000;     // solver's internal contradiction-recovery budget

    bool retryOnFail = true;      // on contradiction, reseed and try again
    u32  maxRetries  = 8;         // how many fresh seeds to try before giving up

    std::vector<Tile> tiles;      // the tile set + adjacency sockets
    bool fillCollision = true;    // write the per-tile `solid` flag into the tilemap collision mask

    bool generateOnStart = false; // regenerate when play begins (off by default — authored layouts persist)

    u32  lastSeed    = 0;         // the seed the last successful/final run used (shown in inspector)
    bool lastSuccess = false;     // did the last generate fully collapse with no contradiction?

    // Runtime: the inspector "Generate Now" button and play-start set this; the
    // WFCSystem consumes it. Not serialized.
    bool generateNow = false;
};

} // namespace ECS
} // namespace Enjin
