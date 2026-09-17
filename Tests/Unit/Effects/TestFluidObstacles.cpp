// The solver knowing about scene colliders.
//
// Until this existed, SetBoundary2D/SetBoundary3D touched only the six walls
// of the grid's own box, so smoke passed straight through every crate, wall
// and chimney placed inside a volume. That is tolerable for a plume in open
// air and useless for what the solver is actually for: baking a high-fidelity
// simulation against real level geometry and playing the recording back.
#include "EnjinTest.h"
#include "Enjin/Effects/FluidObstacles.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/FluidVolume.h"

#include <cmath>
#include <algorithm>

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

ParticleColliderShape Box(Math::Vector3 centre, Math::Vector3 halfExtents) {
    ParticleColliderShape s;
    s.posKind = Math::Vector4(centre.x, centre.y, centre.z, 0.0f);
    s.rot = Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    s.dims = Math::Vector4(halfExtents.x, halfExtents.y, halfExtents.z, 0.0f);
    return s;
}

} // namespace

// --- the point test ---------------------------------------------------------

ENJIN_TEST(FluidObstacles, test_point_inside_box_is_solid_and_outside_is_not) {
    // Arrange
    const ParticleColliderShape b = Box({0, 0, 0}, {1, 1, 1});

    // Act / Assert
    ENJIN_EXPECT_TRUE(PointInsideColliderShape(b, {0.0f, 0.0f, 0.0f}));
    ENJIN_EXPECT_TRUE(PointInsideColliderShape(b, {0.99f, -0.99f, 0.99f}));
    ENJIN_EXPECT_FALSE(PointInsideColliderShape(b, {1.01f, 0.0f, 0.0f}));
    ENJIN_EXPECT_FALSE(PointInsideColliderShape(b, {0.0f, 0.0f, -5.0f}));
}

ENJIN_TEST(FluidObstacles, test_a_rotated_box_is_tested_in_its_own_frame) {
    // A box turned 45 degrees about Y reaches sqrt(2) toward its corner and
    // only 1 toward its face. A test that ignored rotation calls both solid.
    // Arrange
    ParticleColliderShape b = Box({0, 0, 0}, {1, 1, 1});
    const Math::Quaternion q(Math::Vector3(0, 1, 0), Math::Radians(45.0f));
    b.rot = Math::Vector4(q.x, q.y, q.z, q.w);

    // Act / Assert
    ENJIN_EXPECT_TRUE(PointInsideColliderShape(b, {1.3f, 0.0f, 0.0f}));
    ENJIN_EXPECT_FALSE(PointInsideColliderShape(b, {1.0f, 0.0f, 1.0f}));
}

ENJIN_TEST(FluidObstacles, test_capsule_uses_the_cylinder_only_height_convention) {
    // Engine convention: height is the cylinder section, total is height + 2r.
    // Radius 1 with a cylinder half height of 2 reaches 3 along the axis.
    // Arrange
    ParticleColliderShape c;
    c.posKind = Math::Vector4(0.0f, 0.0f, 0.0f, 2.0f);
    c.rot = Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    c.dims = Math::Vector4(1.0f, 2.0f, 0.0f, 0.0f);

    // Act / Assert
    ENJIN_EXPECT_TRUE(PointInsideColliderShape(c, {0.0f, 2.9f, 0.0f}));
    ENJIN_EXPECT_FALSE(PointInsideColliderShape(c, {0.0f, 3.1f, 0.0f}));
    ENJIN_EXPECT_TRUE(PointInsideColliderShape(c, {0.9f, 0.0f, 0.0f}));
    ENJIN_EXPECT_FALSE(PointInsideColliderShape(c, {1.1f, 0.0f, 0.0f}));
}

// --- the voxeliser ----------------------------------------------------------

ENJIN_TEST(FluidObstacles, test_mask_is_empty_when_the_scene_has_no_colliders) {
    // Arrange
    std::vector<u8> solid;

    // Act
    BuildFluidObstacleMask({}, {0, 0, 0}, {5, 5, 5}, 8, true, solid);

    // Assert: sized to index like the grid, and all fluid.
    ENJIN_EXPECT_EQ(solid.size(), static_cast<usize>(10 * 10 * 10));
    for (u8 s : solid) ENJIN_EXPECT_EQ(s, static_cast<u8>(0));
}

ENJIN_TEST(FluidObstacles, test_a_box_filling_the_volume_marks_every_interior_cell) {
    // Arrange
    std::vector<u8> solid;

    // Act
    BuildFluidObstacleMask({Box({0, 0, 0}, {50, 50, 50})}, {0, 0, 0}, {5, 5, 5}, 8, true, solid);

    // Assert: the interior is solid; the padding shell is left alone, because
    // the solver already treats it as a wall and marking it reflects twice.
    const usize stride = 10;
    for (u32 k = 1; k <= 8; ++k)
        for (u32 j = 1; j <= 8; ++j)
            for (u32 i = 1; i <= 8; ++i)
                ENJIN_EXPECT_EQ(solid[i + stride * (j + stride * k)], static_cast<u8>(1));
    ENJIN_EXPECT_EQ(solid[0], static_cast<u8>(0));
}

ENJIN_TEST(FluidObstacles, test_cell_centres_map_to_world_the_way_the_renderer_maps_them) {
    // A slab covering the upper half of the volume in Y must mark exactly the
    // upper half of the grid. If this mapping drifts from FluidRenderer's, the
    // smoke draws offset from the geometry it is flowing around, which reads
    // as a physics bug and is a coordinate bug.
    // Arrange: volume spans Y -4..4, slab covers Y 0..4.
    std::vector<u8> solid;

    // Act
    BuildFluidObstacleMask({Box({0, 2, 0}, {10, 2, 10})}, {0, 0, 0}, {4, 4, 4}, 8, true, solid);

    // Assert
    const usize stride = 10;
    for (u32 j = 1; j <= 4; ++j)
        ENJIN_EXPECT_EQ(solid[5 + stride * (j + stride * 5)], static_cast<u8>(0));
    for (u32 j = 5; j <= 8; ++j)
        ENJIN_EXPECT_EQ(solid[5 + stride * (j + stride * 5)], static_cast<u8>(1));
}

ENJIN_TEST(FluidObstacles, test_the_world_overload_reads_scene_colliders) {
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = Math::Vector3(0.0f, 2.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(e, xf);
    ECS::BoxColliderComponent bc;
    bc.size = Math::Vector3(20.0f, 4.0f, 20.0f);   // world space, per convention
    world.AddComponent<ECS::BoxColliderComponent>(e, bc);
    std::vector<u8> solid;

    // Act
    BuildFluidObstacleMask(&world, {0, 0, 0}, {4, 4, 4}, 8, true, solid);

    // Assert: same upper-half slab as above, gathered from the scene.
    const usize stride = 10;
    ENJIN_EXPECT_EQ(solid[5 + stride * (2 + stride * 5)], static_cast<u8>(0));
    ENJIN_EXPECT_EQ(solid[5 + stride * (7 + stride * 5)], static_cast<u8>(1));
}

// --- the solver actually respecting it --------------------------------------

ENJIN_TEST(FluidSimulationObstacles, test_smoke_does_not_cross_a_solid_wall) {
    // THE test. A slab across the middle, smoke injected below it and rising.
    // Without an obstacle mask the plume goes straight through.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.dimension = ECS::FluidDimension::Mode3D;
    v.fluidType = ECS::FluidType::Smoke;
    v.gridSize = 16;
    v.buoyancy = 2.0f;
    v.halfExtents = Math::Vector3(8.0f, 8.0f, 8.0f);
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);

    FluidSimulation sim;
    sim.Update(1.0f / 60.0f, &world);          // allocates the grid

    // A slab three cells thick across Y, above the source (injected at 15% of
    // the height) and below the ceiling.
    std::vector<u8> mask;
    BuildFluidObstacleMask({Box({0, 0, 0}, {20, 1.5f, 20})},
                           Math::Vector3(0, 0, 0), v.halfExtents, 16, true, mask);
    sim.SetObstacleMask(e, std::move(mask));

    // Act: long enough for an unobstructed plume to reach the ceiling.
    for (int i = 0; i < 400; ++i) sim.Update(1.0f / 60.0f, &world);

    // Assert
    const FluidGridData* g = sim.GetGridData(e);
    ENJIN_ASSERT_TRUE(g != nullptr);
    const u32 N = g->N;
    f32 below = 0.0f, above = 0.0f;
    for (u32 k = 1; k <= N; ++k)
        for (u32 j = 1; j <= N; ++j)
            for (u32 i = 1; i <= N; ++i) {
                const f32 d = g->density[g->IX3(i, j, k)];
                if (j <= 6) below += d;
                else if (j >= 11) above += d;
            }

    ENJIN_EXPECT_TRUE(below > 1.0f);              // the plume exists
    ENJIN_EXPECT_TRUE(above < below * 0.05f);     // and almost none got through
}

ENJIN_TEST(FluidSimulationObstacles, test_a_mask_sized_for_another_resolution_is_refused) {
    // Half-applied obstacles look exactly like the solver ignoring geometry,
    // so a wrong-sized mask is rejected rather than clamped.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.dimension = ECS::FluidDimension::Mode3D;
    v.gridSize = 16;
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);

    FluidSimulation sim;
    sim.Update(1.0f / 60.0f, &world);

    // Act: a mask sized for a 32 grid handed to a 16 grid.
    sim.SetObstacleMask(e, std::vector<u8>(34 * 34 * 34, u8(1)));

    // Assert
    const FluidGridData* g = sim.GetGridData(e);
    ENJIN_ASSERT_TRUE(g != nullptr);
    ENJIN_EXPECT_FALSE(g->HasObstacles());
}

ENJIN_TEST(FluidSimulationObstacles, test_velocity_is_zero_inside_solid_cells) {
    // Direct probe of the no-flux condition, separate from what happens to the
    // density. Stopping smoke by DELETING whatever enters a wall would pass a
    // "does it cross" test while looking wrong: smoke should pile against the
    // obstacle and spill around it, not vanish into it.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.dimension = ECS::FluidDimension::Mode3D;
    v.fluidType = ECS::FluidType::Smoke;
    v.gridSize = 16;
    v.buoyancy = 2.0f;
    v.halfExtents = Math::Vector3(8.0f, 8.0f, 8.0f);
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);

    FluidSimulation sim;
    sim.Update(1.0f / 60.0f, &world);

    std::vector<u8> mask;
    BuildFluidObstacleMask({Box({0, 0, 0}, {20, 1.5f, 20})},
                           Math::Vector3(0, 0, 0), v.halfExtents, 16, true, mask);
    sim.SetObstacleMask(e, mask);

    // Act
    for (int i = 0; i < 120; ++i) sim.Update(1.0f / 60.0f, &world);

    // Assert
    const FluidGridData* g = sim.GetGridData(e);
    ENJIN_ASSERT_TRUE(g != nullptr);
    ENJIN_ASSERT_TRUE(g->HasObstacles());

    f32 worstInSolid = 0.0f;
    f32 mostInFluid = 0.0f;
    for (usize c = 0; c < g->density.size(); ++c) {
        const f32 speed = std::fabs(g->velocityX[c])
                        + std::fabs(g->velocityY[c])
                        + std::fabs(g->velocityZ[c]);
        if (g->IsSolid(c)) worstInSolid = std::max(worstInSolid, speed);
        else               mostInFluid  = std::max(mostInFluid, speed);
    }

    ENJIN_EXPECT_TRUE(mostInFluid > 0.01f);       // the sim is actually moving
    ENJIN_EXPECT_FLOAT_NEAR(worstInSolid, 0.0f, 1e-5f);
}

ENJIN_TEST_MAIN()
