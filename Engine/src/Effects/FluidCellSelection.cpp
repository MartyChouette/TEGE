#include "Enjin/Effects/FluidCellSelection.h"
#include "Enjin/Effects/DrawBudget.h"

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

// A cell's own random numbers. Integer hash rather than a noise function: it
// costs three multiplies, needs no table, and gives the same answer on both
// backends and on a machine that reloads the scene tomorrow.
u32 HashCell(u32 i, u32 j, u32 k, u32 salt) {
    u32 h = i * 73856093u ^ j * 19349663u ^ k * 83492791u ^ salt * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

// [-0.5, 0.5]
f32 SignedUnit(u32 hash) {
    return static_cast<f32>(hash & 0xFFFFFFu) / static_cast<f32>(0xFFFFFF) - 0.5f;
}

} // namespace

void SelectFluidCells(const FluidGridData& grid, f32 densityThreshold,
                      usize budget, std::vector<FluidCellPick>& out,
                      const FluidLookSettings& look) {
    if (grid.N == 0 || budget == 0) return;

    // How many cells want to draw. Knowing the total BEFORE emitting is the
    // whole point: it is what lets an over-budget volume thin itself evenly
    // instead of running out partway through the scan.
    usize wanted = 0;
    ForEachCell(grid, [&](u32 i, u32 j, u32 k) {
        if (DensityAt(grid, i, j, k) >= densityThreshold) ++wanted;
    });
    if (wanted == 0) return;

    const usize stride = DrawStride(wanted, budget);
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

        FluidCellPick pick{i, j, k, d, alphaScale};

        // Wander, but never out of the cell: the offset is half the jitter at
        // most, so a quad stays inside the volume it was sampled from and the
        // cloud's silhouette is the simulation's, not the jitter's.
        if (look.jitter > 0.0f) {
            pick.offsetX = SignedUnit(HashCell(i, j, k, 0u)) * look.jitter;
            pick.offsetY = SignedUnit(HashCell(i, j, k, 1u)) * look.jitter;
            if (grid.is3D) pick.offsetZ = SignedUnit(HashCell(i, j, k, 2u)) * look.jitter;
        }

        // Size follows density, so a cell at the faint edge of a plume draws a
        // small puff and one in the core draws a full quad. That is what stops
        // the edge of a cloud being a row of identical squares at low alpha --
        // the alpha already fades, and it is the SHAPE that gives the grid away.
        if (look.sizeVariance > 0.0f && densityThreshold > 0.0f) {
            // Against the threshold rather than the field's peak: the peak is a
            // per-frame number, and scaling by it would make every billboard in
            // the scene breathe whenever the brightest cell changed.
            const f32 t = std::clamp(d / (densityThreshold * 4.0f), 0.0f, 1.0f);
            pick.sizeScale = 1.0f - look.sizeVariance * (1.0f - t);
        }

        out.push_back(pick);
    });
}

} // namespace Effects
} // namespace Enjin
