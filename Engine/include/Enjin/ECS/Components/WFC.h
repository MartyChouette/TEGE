#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// Space-builder of the procgen suite (sibling of DungeonGenerator/Scatter/Terrain).
// Wave Function Collapse fills a grid so that every neighbour pairing is legal.
// Unlike the dungeon algorithms it is CONSTRAINT-based: you describe which tiles may
// touch, and WFC finds an arrangement that satisfies all of them (Carcassonne / pipe
// mazes / autotiled dungeons / 3D module assembly). It can also FAIL — if the rules
// are too tight there is no legal arrangement, so the component retries with fresh
// seeds and reports whether the last run fully collapsed.
//
// Two modes share one authoring model:
//   Tiles2D    - solves a flat W×H grid and paints the entity's TilemapComponent
//                (each tile = a tileset index).
//   Modules3D  - solves a W×H×D volume and instantiates a prefab per cell into the
//                world (each tile = a prefab "module"), parented to this entity.
//
// Adjacency is authored with EDGE SOCKETS, not raw neighbour lists: each tile carries
// a label on each edge, and two tiles may sit next to each other only when their
// touching edges carry the same label. 2D uses 4 edges (N/E/S/W); 3D adds Up/Down.
// A tile with an empty prefab in 3D leaves that cell as air, which is how you author
// open space. Author the tiles + sockets in the inspector; the WFCSystem compiles
// them into the low-level adjacency the solver needs.
struct WFCComponent {
    enum class Mode : u8 {
        Tiles2D = 0,   // W×H grid -> TilemapComponent (tileIndex per cell)
        Modules3D      // W×H×D volume -> prefab instance per cell
    };

    struct Tile {
        i32 tileIndex = 0;        // Tiles2D: index into the Tilemap's tileset (-1 = empty cell)
        std::string prefabPath;   // Modules3D: .enjprefab placed in the cell (empty = air / no module)
        // Edge socket labels: 0=North, 1=East, 2=South, 3=West, 4=Up, 5=Down. Two tiles
        // may be adjacent when the labels on their touching edges are equal. 2D ignores
        // Up/Down (indices 4-5).
        std::string edges[6];
        bool solid = false;       // Tiles2D: mark this tile's cells solid in the collision mask
    };

    Mode mode   = Mode::Tiles2D;
    u32  width  = 20;
    u32  height = 20;
    u32  depth  = 4;              // Modules3D only: number of layers along world Y
    f32  cellSize = 1.0f;         // Modules3D only: world-unit spacing between module cells

    u32 seed   = 0;               // 0 = pick a random seed each generate
    u32 maxBacktracks = 1000;     // 2D solver's internal contradiction-recovery budget

    bool retryOnFail = true;      // on contradiction, reseed and try again
    u32  maxRetries  = 8;         // how many fresh seeds to try before giving up

    std::vector<Tile> tiles;      // the tile set + adjacency sockets
    bool fillCollision = true;    // Tiles2D: write the per-tile `solid` flag into the tilemap collision mask

    bool generateOnStart = false; // regenerate when play begins (off by default — authored layouts persist)

    u32  lastSeed    = 0;         // the seed the last successful/final run used (shown in inspector)
    bool lastSuccess = false;     // did the last generate fully collapse with no contradiction?
    u32  lastCount   = 0;         // Modules3D: how many prefab modules the last run placed

    // Runtime: the inspector "Generate Now" button and play-start set this; the
    // WFCSystem consumes it. Not serialized.
    bool generateNow = false;
};

} // namespace ECS
} // namespace Enjin
