#include "Enjin/Effects/FluidCellSelection.h"

#include <algorithm>

namespace Enjin {
namespace Effects {

namespace {

// One scan order for both modes, so the counting pass and the emitting pass
// cannot drift apart. 2D passes k = 0 and the index ignores it.
template <typename Fn>
void ForEachCell(const FluidGridData& grid, Fn&& fn) {
    const u32 N = grid.N;
    if (!grid.is3D) {
        for (u32 j = 1; j <= N; ++j)
            for (u32 i = 1; i <= N; ++i) fn(i, j, 0u);
    } else {
        for (u32 k = 1; k <= N; ++k)
            for (u32 j = 1; j <= N; ++j)
                for (u32 i = 1; i <= N; ++i) fn(i, j, k);
    }
}

f32 DensityAt(const FluidGridData& grid, u32 i, u32 j, u32 k) {
    return grid.is3D ? grid.density[grid.IX3(i, j, k)] : grid.density[grid.IX(i, j)];
}

} // namespace

usize FluidBudgetShare(usize budget, usize volumeCount) {
    if (volumeCount == 0) return budget;
    return std::max<usize>(1, budget / volumeCount);
}

void SelectFluidCells(const FluidGridData& grid, f32 densityThreshold,
                      usize budget, std::vector<FluidCellPick>& out) {
    if (grid.N == 0 || budget == 0) return;

    // How many cells want to draw. Knowing the total BEFORE emitting is the
    // whole point: it is what lets an over-budget volume thin itself evenly
    // instead of running out partway through the scan.
    usize wanted = 0;
    ForEachCell(grid, [&](u32 i, u32 j, u32 k) {
        if (DensityAt(grid, i, j, k) >= densityThreshold) ++wanted;
    });
    if (wanted == 0) return;

    const usize stride = (wanted > budget) ? (wanted + budget - 1) / budget : usize(1);
    const f32 alphaScale = static_cast<f32>(stride);

    // `out` is the caller's shared cache across volumes, so the budget is on
    // what THIS call appends, not on the vector's total size.
    const usize startSize = out.size();
    usize seen = 0;
    ForEachCell(grid, [&](u32 i, u32 j, u32 k) {
        if (out.size() - startSize >= budget) return;
        const f32 d = DensityAt(grid, i, j, k);
        if (d < densityThreshold) return;
        if ((seen++ % stride) != 0) return;
        out.push_back(FluidCellPick{i, j, k, d, alphaScale});
    });
}

} // namespace Effects
} // namespace Enjin
