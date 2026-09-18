#pragma once
// Which fluid cells get drawn, in ONE place.
//
// It was in two -- FluidRenderer (Vulkan) and RenderSystem's web sprite path --
// and both copies had the same two bugs, because the second was written from
// the first and the comment above it said so:
//
//   1. The instance budget was taken first-come. One saturated volume filled it
//      and every later volume in the scene drew nothing at all, which reads as
//      smoke that was never placed rather than smoke that was dropped.
//   2. Over budget, the scan simply stopped. The 3D scan is z-outermost, so
//      stopping cut a FLAT PLANE through the cloud: the far side ceased to
//      exist. Measured (Tests/Integration/FluidBench.cpp): one 48^3 smoke
//      volume wants 40,118 cells at steady state and 16,384 were drawn, so 59%
//      of the smoke was missing and all of it from one side.
//
// The backends legitimately differ in how they build an instance. They must not
// differ in which cells they build one FOR, which is what this header is.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Effects/DrawBudget.h"

#include <vector>

namespace Enjin {
namespace Effects {

struct FluidCellPick {
    u32 i = 0;              // grid coords, 1..N
    u32 j = 0;
    u32 k = 0;              // always 0 in 2D
    f32 density = 0.0f;     // raw density, already at or above the threshold
    f32 alphaScale = 1.0f;  // stride compensation -- multiply density by this

    // Offset from the cell CENTRE in cell widths, and a multiplier on the
    // billboard's size. Both exist to break up the lattice: one quad per cell,
    // at one size, on a regular grid draws the grid, which is why fluid reads
    // as voxels however fine the simulation underneath it is.
    //
    // Deterministic per cell, hashed from i/j/k alone and never from time, so
    // a still frame stays still -- jitter that reseeds per frame is a boil,
    // and a boil is worse than a lattice.
    f32 offsetX = 0.0f;
    f32 offsetY = 0.0f;
    f32 offsetZ = 0.0f;
    f32 sizeScale = 1.0f;
};

// How far a cell's billboard may wander from its centre, in cell widths, and
// how much of its size follows its density.
//
// Zero for both is the old look exactly, quads pinned to lattice points all the
// same size, so this can be turned off rather than tuned away. Per VOLUME
// rather than per process: a chimney and a lava pool want different amounts of
// it, and it belongs in the scene with everything else about how a volume
// looks. FluidVolumeComponent carries the authored values.
struct ENJIN_API FluidLookSettings {
    f32 jitter = 0.0f;        // cell widths, +/- half this on each axis
    f32 sizeVariance = 0.0f;  // fraction of the billboard size that follows density
};

// Cells of `grid` worth drawing, at most `budget` of them, appended to `out`.
//
// When more cells want to draw than fit, every stride-th one is taken rather
// than the first `budget`: the cloud keeps its SHAPE and loses density, and
// alphaScale puts the density back, so thinning does not read as fading. Half
// the billboards at twice the alpha integrates to about the same cloud.
//
// `look` defaults to OFF so a caller that does not care about the look cannot
// accidentally get one; the renderers pass the volume's authored values.
ENJIN_API void SelectFluidCells(const FluidGridData& grid, f32 densityThreshold,
                                usize budget, std::vector<FluidCellPick>& out,
                                const FluidLookSettings& look = FluidLookSettings{});

} // namespace Effects
} // namespace Enjin
