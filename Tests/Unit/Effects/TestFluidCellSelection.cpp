// Which fluid cells get drawn when there are more of them than budget.
//
// Regression tests for two bugs that shipped in BOTH backends at once, because
// the web copy was written from the Vulkan one:
//   - the shared instance budget was taken first-come, so every volume after
//     the first drew nothing;
//   - over budget the scan just stopped, and since the 3D scan is z-outermost
//     that removed a flat slab of the cloud rather than thinning it.
// Measured before the fix: a saturated 48^3 smoke volume wanted 40,118 cells
// and 16,384 were drawn (Tests/Integration/FluidBench.cpp).
#include "EnjinTest.h"
#include "Enjin/Effects/FluidCellSelection.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/FluidVolume.h"

#include <set>

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

// A grid whose interior is uniformly dense, so every interior cell wants to
// draw and the only thing under test is the selection.
FluidGridData FullGrid(u32 n, bool is3D, f32 density = 1.0f) {
    FluidGridData g;
    g.Allocate(n, is3D);
    if (is3D) {
        for (u32 k = 1; k <= n; ++k)
            for (u32 j = 1; j <= n; ++j)
                for (u32 i = 1; i <= n; ++i) g.density[g.IX3(i, j, k)] = density;
    } else {
        for (u32 j = 1; j <= n; ++j)
            for (u32 i = 1; i <= n; ++i) g.density[g.IX(i, j)] = density;
    }
    return g;
}

} // namespace

ENJIN_TEST(FluidCellSelection, test_selection_under_budget_takes_every_cell) {
    // Arrange: 8^3 = 512 interior cells, budget far above it.
    FluidGridData g = FullGrid(8, true);
    std::vector<FluidCellPick> out;

    // Act
    SelectFluidCells(g, 0.01f, 10000, out);

    // Assert
    ENJIN_ASSERT_EQ(out.size(), static_cast<usize>(512));
    ENJIN_EXPECT_FLOAT_NEAR(out[0].alphaScale, 1.0f, 0.0001f);
}

ENJIN_TEST(FluidCellSelection, test_selection_over_budget_stays_within_budget) {
    // Arrange
    FluidGridData g = FullGrid(16, true);   // 4096 cells
    std::vector<FluidCellPick> out;

    // Act
    SelectFluidCells(g, 0.01f, 500, out);

    // Assert
    ENJIN_ASSERT_TRUE(out.size() <= 500);
    ENJIN_ASSERT_TRUE(out.size() > 0);
}

ENJIN_TEST(FluidCellSelection, test_selection_over_budget_spans_the_full_depth) {
    // THE regression. Truncating a z-outermost scan keeps only the low-k slabs,
    // so the cloud loses its far half. An even thinning must still reach the
    // last slab.
    // Arrange
    const u32 n = 16;
    FluidGridData g = FullGrid(n, true);    // 4096 cells
    std::vector<FluidCellPick> out;

    // Act: a budget that fits one eighth of the cells.
    SelectFluidCells(g, 0.01f, 512, out);

    // Assert: cells come from the whole k range, not just the front.
    u32 minK = n + 1, maxK = 0;
    std::set<u32> slabs;
    for (const FluidCellPick& p : out) {
        minK = std::min(minK, p.k);
        maxK = std::max(maxK, p.k);
        slabs.insert(p.k);
    }
    ENJIN_ASSERT_EQ(minK, static_cast<u32>(1));
    ENJIN_ASSERT_EQ(maxK, n);
    // Every slab represented, not merely the first and last.
    ENJIN_ASSERT_EQ(slabs.size(), static_cast<usize>(n));
}

ENJIN_TEST(FluidCellSelection, test_selection_over_budget_scales_alpha_by_the_stride) {
    // Dropping cells without compensating would read as the smoke fading.
    // Arrange
    FluidGridData g = FullGrid(16, true);   // 4096 cells
    std::vector<FluidCellPick> out;

    // Act: budget of 1024 means roughly one cell in four.
    SelectFluidCells(g, 0.01f, 1024, out);

    // Assert
    ENJIN_ASSERT_TRUE(out.size() > 0);
    ENJIN_EXPECT_FLOAT_NEAR(out[0].alphaScale, 4.0f, 0.0001f);
}

ENJIN_TEST(FluidCellSelection, test_selection_skips_cells_below_the_threshold) {
    // Arrange: everything at 0.005, threshold 0.01.
    FluidGridData g = FullGrid(8, true, 0.005f);
    std::vector<FluidCellPick> out;

    // Act
    SelectFluidCells(g, 0.01f, 10000, out);

    // Assert
    ENJIN_ASSERT_EQ(out.size(), static_cast<usize>(0));
}

ENJIN_TEST(FluidCellSelection, test_selection_appends_without_clearing_the_caller_cache) {
    // The renderers share one cache across volumes, so the budget is on what
    // this call adds. A budget applied to the vector's total size would starve
    // every volume after the first -- which is the bug this replaced.
    // Arrange
    FluidGridData g = FullGrid(8, true);     // 512 cells each
    std::vector<FluidCellPick> out;

    // Act
    SelectFluidCells(g, 0.01f, 512, out);
    const usize afterFirst = out.size();
    SelectFluidCells(g, 0.01f, 512, out);

    // Assert: the second volume got its own 512, not zero.
    ENJIN_ASSERT_EQ(afterFirst, static_cast<usize>(512));
    ENJIN_ASSERT_EQ(out.size(), static_cast<usize>(1024));
}

ENJIN_TEST(FluidCellSelection, test_selection_handles_2d_grids) {
    // Arrange
    FluidGridData g = FullGrid(32, false);   // 1024 cells
    std::vector<FluidCellPick> out;

    // Act
    SelectFluidCells(g, 0.01f, 256, out);

    // Assert
    ENJIN_ASSERT_TRUE(out.size() <= 256);
    for (const FluidCellPick& p : out) ENJIN_ASSERT_EQ(p.k, static_cast<u32>(0));
}

ENJIN_TEST(FluidCellSelection, test_selection_empty_grid_selects_nothing) {
    // Arrange
    FluidGridData g;    // N == 0, never allocated
    std::vector<FluidCellPick> out;

    // Act
    SelectFluidCells(g, 0.01f, 1000, out);

    // Assert
    ENJIN_ASSERT_EQ(out.size(), static_cast<usize>(0));
}

// The grid allocated for a volume must not outlive the volume. FluidSimulation
// has an OnEntityRemoved that frees it and has never had a caller, so a deleted
// chimney leaked about 5 MB for the life of the process and a streaming level
// leaked one per cycle.
ENJIN_TEST(FluidSimulation, test_grid_is_released_when_its_volume_is_destroyed) {
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.gridSize = 16;
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);

    FluidSimulation sim;
    sim.Update(1.0f / 60.0f, &world);
    ENJIN_EXPECT_TRUE(sim.GetGridData(e) != nullptr);

    // Act
    world.DestroyEntity(e);
    world.Update(0.0f);              // flushes the deferred destroy
    sim.Update(1.0f / 60.0f, &world);

    // Assert
    ENJIN_EXPECT_NULL(sim.GetGridData(e));
}

ENJIN_TEST(FluidSimulation, test_grid_is_released_when_the_component_is_removed) {
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.gridSize = 16;
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);

    FluidSimulation sim;
    sim.Update(1.0f / 60.0f, &world);
    ENJIN_EXPECT_TRUE(sim.GetGridData(e) != nullptr);

    // Act: the entity survives, the volume does not.
    world.RemoveComponent<ECS::FluidVolumeComponent>(e);
    sim.Update(1.0f / 60.0f, &world);

    // Assert
    ENJIN_EXPECT_NULL(sim.GetGridData(e));
}

ENJIN_TEST_MAIN()
