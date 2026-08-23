#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

// Space-builder of the procgen suite (sibling of DungeonGenerator and Scatter).
// Where the dungeon paints a Tilemap and the scatter stamps prefabs, this bakes a
// heightfield into the entity's TerrainComponent: run fractional-Brownian-motion
// noise for the base shape, then optionally weather it with droplet (hydraulic)
// and talus (thermal) erosion so ridges carve valleys and slopes settle. The
// TerrainComponent already meshes + serializes itself, so this component only owns
// the parameters and drives one bake.
//
// Author it in the inspector, press Generate Now (or let generateOnStart run it at
// play start). Regenerating overwrites the heightmap, so it is idempotent.
struct TerrainGeneratorComponent {
    // Output grid + world sizing (mirrors TerrainComponent so the bake can size it).
    u32 gridWidth  = 128;
    u32 gridHeight = 128;
    f32 cellSize   = 1.0f;      // world units between samples
    f32 maxHeight  = 30.0f;     // normalized noise [0,1] is scaled to [0, maxHeight] world units

    // --- Fractional Brownian Motion (base shape) ---
    bool ridged      = false;   // false = rolling hills (standard fBm); true = sharp mountain ridges
    u32  octaves     = 6;       // detail layers (1-12)
    f32  lacunarity  = 2.0f;    // frequency step per octave (1.5-3.0)
    f32  gain        = 0.5f;    // amplitude falloff per octave / persistence (0.3-0.7)
    f32  frequency   = 1.0f;    // base noise frequency (bigger = more, smaller features)
    f32  ridgedPower = 2.0f;    // ridged only: sharpness exponent

    // --- Hydraulic erosion (rainfall droplets carve valleys) ---
    bool hydraulic          = false;
    u32  hydraulicDroplets  = 60000;  // number of water droplets (more = smoother, slower)

    // --- Thermal erosion (steep slopes slump to their talus angle) ---
    bool thermal            = false;
    u32  thermalIterations  = 40;
    f32  talusAngle         = 0.02f;  // max stable neighbour height diff (normalized units)

    // --- Auto-splat (paint the 4 splat layers by slope + height) ---
    // Layer 0 = base/grass (the remainder), layer 1 = rock (steep slopes),
    // layer 2 = snow (high altitude, avoids steeps), layer 3 = shore/sand (low).
    // Weights blend smoothly and feed the terrain's splatmap exactly like
    // hand-painted values, so assigned layer textures pick them up unchanged.
    bool autoSplat       = false;
    f32  rockSlopeDeg    = 35.0f;  // slope (degrees) where rock takes over
    f32  snowHeightFrac  = 0.70f;  // height fraction (0-1 of maxHeight) where snow starts
    f32  shoreHeightFrac = 0.12f;  // height fraction below which shore/sand shows
    f32  splatBlend      = 0.10f;  // transition softness (fraction of each threshold)

    u32  seed            = 0;    // 0 = pick a fresh random seed each generate
    bool generateOnStart = false; // rebake when play begins (off by default — authored terrain persists)

    u32 lastSeed = 0;            // the seed actually used (shown in the inspector)

    // Runtime: the inspector "Generate Now" button and play-start set this; the
    // TerrainGeneratorSystem consumes it. Not serialized.
    bool generateNow = false;
};

} // namespace ECS
} // namespace Enjin
