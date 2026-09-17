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

#include <vector>

namespace Enjin {
namespace Effects {

struct FluidCellPick {
    u32 i = 0;              // grid coords, 1..N
    u32 j = 0;
    u32 k = 0;              // always 0 in 2D
    f32 density = 0.0f;     // raw density, already at or above the threshold
    f32 alphaScale = 1.0f;  // stride compensation -- multiply density by this
};

// An equal slice of a shared budget, never first-come. A zero volume count
// gets the whole budget so a caller need not special-case an empty scene.
ENJIN_API usize FluidBudgetShare(usize budget, usize volumeCount);

// Cells of `grid` worth drawing, at most `budget` of them, appended to `out`.
//
// When more cells want to draw than fit, every stride-th one is taken rather
// than the first `budget`: the cloud keeps its SHAPE and loses density, and
// alphaScale puts the density back, so thinning does not read as fading. Half
// the billboards at twice the alpha integrates to about the same cloud.
ENJIN_API void SelectFluidCells(const FluidGridData& grid, f32 densityThreshold,
                                usize budget, std::vector<FluidCellPick>& out);

} // namespace Effects
} // namespace Enjin
