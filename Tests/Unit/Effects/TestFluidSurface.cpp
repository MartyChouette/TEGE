// Meshing a liquid surface out of a fluid density field.
//
// A liquid's defining visual feature is that it has a SURFACE, and a cloud of
// per-cell billboards has none -- which is both why fluid volumes read as
// voxels and why FluidType::Water never looked like water. The engine already
// owns a mesher for scalar fields (Geometry::BuildSurfaceNet, written for SDF
// caves), and a density field is a scalar field, so this is a bridge rather
// than a new algorithm.
#include "EnjinTest.h"
#include "Enjin/Effects/FluidSurface.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/FluidVolume.h"

#include <algorithm>
#include <cmath>

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

// A grid whose lower `fillRows` interior rows are full liquid and the rest air.
FluidGridData PooledGrid(u32 n, u32 fillRows, f32 density = 1.0f) {
    FluidGridData g;
    g.Allocate(n, true);
    for (u32 k = 1; k <= n; ++k)
        for (u32 j = 1; j <= fillRows; ++j)
            for (u32 i = 1; i <= n; ++i)
                g.density[g.IX3(i, j, k)] = density;
    return g;
}

} // namespace

ENJIN_TEST(FluidSurface, test_an_empty_volume_meshes_to_nothing) {
    // Arrange: allocated but never filled, the normal state before anything
    // has been injected.
    FluidGridData g;
    g.Allocate(8, true);

    // Act
    const Geometry::SurfaceMesh mesh =
        BuildFluidSurface(g, {0, 0, 0}, {4, 4, 4}, 0.5f);

    // Assert: a skip, not a failure.
    ENJIN_EXPECT_TRUE(mesh.Empty());
}

ENJIN_TEST(FluidSurface, test_a_2d_grid_has_no_surface_to_mesh) {
    // Arrange
    FluidGridData g;
    g.Allocate(8, false);
    for (u32 j = 1; j <= 4; ++j)
        for (u32 i = 1; i <= 8; ++i) g.density[g.IX(i, j)] = 1.0f;

    // Act
    const Geometry::SurfaceMesh mesh =
        BuildFluidSurface(g, {0, 0, 0}, {4, 4, 4}, 0.5f);

    // Assert
    ENJIN_EXPECT_TRUE(mesh.Empty());
}

ENJIN_TEST(FluidSurface, test_pooled_liquid_produces_a_surface) {
    // Arrange: the bottom half of a 16 grid is liquid.
    const FluidGridData g = PooledGrid(16, 8);

    // Act
    const Geometry::SurfaceMesh mesh =
        BuildFluidSurface(g, {0, 0, 0}, {8, 8, 8}, 0.5f);

    // Assert
    ENJIN_EXPECT_FALSE(mesh.Empty());
    ENJIN_EXPECT_TRUE(mesh.TriangleCount() > 0);
    ENJIN_EXPECT_EQ(mesh.positions.size(), mesh.normals.size());
}

ENJIN_TEST(FluidSurface, test_the_surface_sits_at_the_liquid_level_not_the_volume_centre) {
    // THE test. A pool filling the bottom quarter must mesh a surface near the
    // quarter height, not at the middle and not at the floor. This is what
    // catches an origin or half-cell mistake in the world mapping, which would
    // otherwise draw the water plane sunk into the floor or floating above it.
    //
    // Volume spans Y -8..8 over a 16 grid, so a cell is 1 unit. Four filled
    // rows are cells j=1..4, whose centres are at Y -7.5..-4.5, so the surface
    // between the last liquid row and the first air row sits near Y -4.
    // Arrange
    const FluidGridData g = PooledGrid(16, 4);

    // Act
    const Geometry::SurfaceMesh mesh =
        BuildFluidSurface(g, {0, 0, 0}, {8, 8, 8}, 0.5f);

    // Assert
    ENJIN_ASSERT_FALSE(mesh.Empty());

    // The TOP face of the pool: the highest vertices should cluster near -4.
    f32 highest = -1000.0f;
    for (const Math::Vector3& p : mesh.positions) highest = std::max(highest, p.y);
    ENJIN_EXPECT_TRUE(highest > -5.5f);   // not stuck down at the floor
    ENJIN_EXPECT_TRUE(highest < -2.5f);   // and not up at the volume centre
}

ENJIN_TEST(FluidSurface, test_the_surface_follows_the_volume_transform) {
    // Moving the volume must move the water with it. A surface built in grid
    // space and never placed would sit at the origin whatever the entity does,
    // which reads as the water refusing to be moved.
    // Arrange
    const FluidGridData g = PooledGrid(16, 8);

    // Act
    const Geometry::SurfaceMesh atOrigin =
        BuildFluidSurface(g, {0, 0, 0}, {8, 8, 8}, 0.5f);
    const Geometry::SurfaceMesh moved =
        BuildFluidSurface(g, {100, 0, 0}, {8, 8, 8}, 0.5f);

    // Assert: same mesh, shifted 100 along X.
    ENJIN_ASSERT_FALSE(atOrigin.Empty());
    ENJIN_ASSERT_EQ(moved.positions.size(), atOrigin.positions.size());
    for (usize n = 0; n < atOrigin.positions.size(); ++n) {
        ENJIN_EXPECT_FLOAT_NEAR(moved.positions[n].x - atOrigin.positions[n].x, 100.0f, 0.001f);
        ENJIN_EXPECT_FLOAT_NEAR(moved.positions[n].y, atOrigin.positions[n].y, 0.001f);
    }
}

ENJIN_TEST(FluidSurface, test_a_higher_threshold_meshes_a_smaller_pool) {
    // The threshold is what "enough liquid to be liquid" means. Raising it must
    // shrink the surface, not move it arbitrarily.
    // Arrange: a half-density pool, so 0.25 includes it and 0.75 does not.
    const FluidGridData g = PooledGrid(16, 8, 0.5f);

    // Act
    const Geometry::SurfaceMesh included =
        BuildFluidSurface(g, {0, 0, 0}, {8, 8, 8}, 0.25f);
    const Geometry::SurfaceMesh excluded =
        BuildFluidSurface(g, {0, 0, 0}, {8, 8, 8}, 0.75f);

    // Assert
    ENJIN_EXPECT_FALSE(included.Empty());
    ENJIN_EXPECT_TRUE(excluded.Empty());
}

// --- liquid has to be able to FALL ------------------------------------------

ENJIN_TEST(FluidSimulationGravity, test_negative_buoyancy_sinks_relative_to_none) {
    // Both buoyancy loops were gated on `buoyancy > 0.0f`, so a negative value
    // was silently discarded and no fluid in the engine could fall. Water that
    // cannot fall cannot pool, and cannot be contained by anything.
    //
    // Measured DIFFERENTIALLY -- the same sim run with gravity and without --
    // rather than against the starting height. The first version of this test
    // compared the blob's centre of mass to where it started and passed even
    // with the old gate restored: a blob above centre in a bounded box drifts
    // downward from diffusion alone, because diffusion pulls the centre of
    // mass toward the middle of the box. That measured the box, not gravity.
    // Arrange
    auto runWithBuoyancy = [](f32 buoyancy) {
        ECS::World world;
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
        ECS::FluidVolumeComponent v;
        v.dimension = ECS::FluidDimension::Mode3D;
        v.gridSize = 16;
        v.halfExtents = Math::Vector3(8.0f, 8.0f, 8.0f);
        v.sourceDensity = 0.0f;      // no continuous source; just the one blob
        v.buoyancy = buoyancy;
        v.dissipation = 1.0f;        // so this measures motion, not decay
        world.AddComponent<ECS::FluidVolumeComponent>(e, v);

        FluidSimulation sim;
        sim.Update(1.0f / 60.0f, &world);
        sim.AddDensityAtWorldPos(e, Math::Vector3(8.0f, 8.0f, 8.0f), 40.0f, 2.0f);
        for (int i = 0; i < 120; ++i) sim.Update(1.0f / 60.0f, &world);

        const FluidGridData* g = sim.GetGridData(e);
        f64 mass = 0.0, weighted = 0.0;
        for (u32 k = 1; k <= g->N; ++k)
            for (u32 j = 1; j <= g->N; ++j)
                for (u32 i = 1; i <= g->N; ++i) {
                    const f32 d = g->density[g->IX3(i, j, k)];
                    mass += d;
                    weighted += static_cast<f64>(j) * d;
                }
        return mass > 0.0 ? weighted / mass : 0.0;
    };

    // Act
    const f64 still = runWithBuoyancy(0.0f);
    const f64 sinking = runWithBuoyancy(-3.0f);
    const f64 rising = runWithBuoyancy(3.0f);

    // Assert
    ENJIN_ASSERT_TRUE(still > 0.0);
    ENJIN_EXPECT_TRUE(sinking < still);    // negative falls
    ENJIN_EXPECT_TRUE(rising > still);     // positive still rises, unchanged
}

ENJIN_TEST(FluidSimulationGravity, test_the_liquid_presets_sink_and_the_gas_presets_rise) {
    // The presets are what a person gets from Entity > Effects > Fluid, so
    // "Water Volume" has to behave like water. Water and Lava carried
    // buoyancy 0 for as long as the solver discarded negative values, which
    // made them a colour and a viscosity on a gas solver.
    // Arrange / Act / Assert
    auto buoyancyOf = [](ECS::FluidType type) {
        ECS::FluidVolumeComponent v;
        v.fluidType = type;
        v.ApplyPreset();
        return v.buoyancy;
    };

    ENJIN_EXPECT_TRUE(buoyancyOf(ECS::FluidType::Water) < 0.0f);
    ENJIN_EXPECT_TRUE(buoyancyOf(ECS::FluidType::Lava) < 0.0f);
    // Lava oozes rather than pours, so it sinks more gently than water.
    ENJIN_EXPECT_TRUE(buoyancyOf(ECS::FluidType::Lava) > buoyancyOf(ECS::FluidType::Water));

    ENJIN_EXPECT_TRUE(buoyancyOf(ECS::FluidType::Smoke) > 0.0f);
    ENJIN_EXPECT_TRUE(buoyancyOf(ECS::FluidType::Steam) > 0.0f);
    ENJIN_EXPECT_TRUE(buoyancyOf(ECS::FluidType::Gas) > 0.0f);
}

ENJIN_TEST_MAIN()
