#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {
namespace ECS {

// Space-builder of the procgen suite (sibling of the DungeonGenerator). Where the
// dungeon paints a grid, the Scatter stamps COPIES of a prefab across a region:
// foliage, rocks, debris, crowd props, spawn points. It samples a set of points on
// a plane using one of three distributions, then instantiates the prefab at each,
// parented to this entity so the whole batch moves and cleans up as a unit.
//
// Distribution picks the "feel" of the placement:
//   Uniform      - pure random. Clumpy, cheap, fine for sparse debris.
//   Poisson       - blue-noise, every point at least `minSpacing` apart. The
//                   natural-looking one: even coverage with no visible grid.
//   JitteredGrid  - a grid with each point nudged inside its cell. Regular but
//                   organic, good for orchards / tiled props.
//
// Author it in the inspector, press Generate Now (or let generateOnStart run it at
// play start). Regenerating first destroys the previous batch, so it is idempotent.
struct ScatterComponent {
    enum class Distribution : u8 {
        Uniform = 0,   // random points, may clump
        Poisson,       // blue-noise, min spacing between points (natural)
        JitteredGrid,  // grid cells with a random offset (regular but not rigid)
        Voronoi        // random points relaxed toward their Voronoi cell centroids
                       // (Lloyd's algorithm): organically even, softer than Poisson
    };

    // Which plane the points spread across, relative to this entity.
    //   XZ = the ground plane (3D scenes). Points spread in X and Z, height along Y.
    //   XY = the screen plane (2D scenes). Points spread in X and Y, depth along Z.
    enum class Plane : u8 {
        XZ = 0,
        XY
    };

    Distribution distribution = Distribution::Poisson;
    Plane        plane        = Plane::XZ;

    std::string prefabPath;          // .prefab instantiated at each point (empty = nothing to place)

    f32 regionWidth  = 20.0f;        // full extent of the region along the first axis (world units)
    f32 regionHeight = 20.0f;        // full extent along the second axis (Z for XZ, Y for XY)
    u32 targetCount  = 40;           // desired instances (Uniform / JitteredGrid / Voronoi; Poisson fits as many as spacing allows)
    f32 minSpacing   = 2.0f;         // Poisson: minimum distance between points; also the grid cell hint for JitteredGrid
    u32 relaxIterations = 3;         // Voronoi: Lloyd relaxation passes (0 = plain random, more = more even)

    // Per-instance randomization so the batch does not look cloned.
    f32  scaleMin     = 1.0f;        // uniform scale multiplier range applied to each instance
    f32  scaleMax     = 1.0f;
    bool randomYaw    = true;        // random rotation around the plane normal
    f32  heightJitter = 0.0f;        // random offset along the plane normal (adds thickness/relief)

    // Terrain conform (XZ plane only): drop each instance onto the surface of the
    // TerrainComponent on THIS entity (put the Scatter on the terrain entity — both
    // are local to the same transform, so the frames line up). Points outside the
    // terrain's extent, or on slopes steeper than maxSlopeDeg, are culled.
    // heightJitter still applies on top of the sampled height.
    bool conformToTerrain = false;
    f32  maxSlopeDeg      = 90.0f;   // 90 = keep everything; ~30 keeps trees off cliffs

    u32  seed            = 0;        // 0 = pick a fresh random seed each generate
    bool generateOnStart = false;    // regenerate when play begins (off by default — authored scatter usually persists)

    u32 lastSeed  = 0;               // the seed actually used (shown in the inspector)
    u32 lastCount = 0;               // how many instances the last generate actually placed

    // Runtime: the inspector "Generate Now" button and play-start set this; the
    // ScatterSystem consumes it. Not serialized.
    bool generateNow = false;
};

// Marker placed on every entity the ScatterSystem spawns. Serialized (presence
// only) so a saved scatter batch stays regenerable: a regenerate destroys every
// marked child of the source entity before spawning the fresh batch, which is what
// keeps Generate Now idempotent across save/load instead of accumulating copies.
struct ScatterInstanceComponent {
    // no fields — presence is the whole signal (the parent link identifies the source)
};

} // namespace ECS
} // namespace Enjin
