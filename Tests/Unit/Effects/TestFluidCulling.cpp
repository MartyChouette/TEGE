// Not solving volumes nobody can see.
//
// A 48^3 volume is about 56 ms a frame at the shipped defaults and a grid pays
// for its whole box whether or not there is smoke in it, so a village with
// twenty chimneys paid for twenty of them with two on screen. The frame budget
// already stopped that from scaling; this stops it being spent at all on work
// that cannot be seen.
#include "EnjinTest.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/FluidVolume.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

ECS::Entity MakeVolume(ECS::World& world, const Math::Vector3& position) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = position;
    world.AddComponent<ECS::TransformComponent>(e, xf);
    ECS::FluidVolumeComponent v;
    v.dimension = ECS::FluidDimension::Mode3D;
    v.fluidType = ECS::FluidType::Smoke;
    v.gridSize = 8;
    v.buoyancy = 1.0f;
    v.halfExtents = Math::Vector3(2.0f, 2.0f, 2.0f);
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);
    return e;
}

// A frustum that admits only what is near the origin, built by hand so the
// test does not depend on a camera or a projection convention. Inward-facing
// planes, (nx, ny, nz, d), admitting the box [-10,10] on every axis.
void BoxFrustum(Math::Vector4 out[6], f32 extent) {
    out[0] = Math::Vector4( 1,  0,  0, extent);
    out[1] = Math::Vector4(-1,  0,  0, extent);
    out[2] = Math::Vector4( 0,  1,  0, extent);
    out[3] = Math::Vector4( 0, -1,  0, extent);
    out[4] = Math::Vector4( 0,  0,  1, extent);
    out[5] = Math::Vector4( 0,  0, -1, extent);
}

} // namespace

ENJIN_TEST(FluidCulling, test_nothing_is_culled_until_a_runtime_sets_a_viewer) {
    // The default has to be "solve everything". A viewer that is stale or
    // wrong freezes smoke in front of the player, which is far worse than
    // paying for a volume that is off screen -- so the feature stays off until
    // something opts in.
    ECS::World world;
    MakeVolume(world, Math::Vector3(1000.0f, 0.0f, 0.0f));

    FluidSimulation sim;
    ENJIN_EXPECT_FALSE(sim.HasViewer());
    sim.Update(1.0f / 60.0f, &world);
    ENJIN_EXPECT_EQ(sim.GetCulledVolumeCount(), 0u);
}

ENJIN_TEST(FluidCulling, test_a_volume_outside_the_frustum_is_not_solved) {
    ECS::World world;
    ECS::Entity offScreen = MakeVolume(world, Math::Vector3(100.0f, 0.0f, 0.0f));

    FluidViewer viewer;
    viewer.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    BoxFrustum(viewer.frustumPlanes, 10.0f);
    viewer.hasFrustum = true;

    FluidSimulation sim;
    sim.SetViewer(viewer);
    sim.Update(1.0f / 60.0f, &world);

    ENJIN_EXPECT_EQ(sim.GetCulledVolumeCount(), 1u);
    // Culled before a grid was ever allocated: the saving is the whole step,
    // not just the solve.
    ENJIN_EXPECT_TRUE(sim.GetGridData(offScreen) == nullptr);
}

ENJIN_TEST(FluidCulling, test_a_volume_inside_the_frustum_is_solved) {
    ECS::World world;
    ECS::Entity onScreen = MakeVolume(world, Math::Vector3(1.0f, 0.0f, 0.0f));

    FluidViewer viewer;
    BoxFrustum(viewer.frustumPlanes, 10.0f);
    viewer.hasFrustum = true;

    FluidSimulation sim;
    sim.SetViewer(viewer);
    sim.Update(1.0f / 60.0f, &world);

    ENJIN_EXPECT_EQ(sim.GetCulledVolumeCount(), 0u);
    ENJIN_EXPECT_TRUE(sim.GetGridData(onScreen) != nullptr);
}

ENJIN_TEST(FluidCulling, test_a_volume_straddling_a_plane_is_kept) {
    // The bounding sphere is the volume's own box, so a volume half out of
    // view still solves. Culling on the centre alone would pop the near edge
    // of every plume that walks off screen.
    ECS::World world;
    MakeVolume(world, Math::Vector3(11.0f, 0.0f, 0.0f));   // centre outside, radius 3.46

    FluidViewer viewer;
    BoxFrustum(viewer.frustumPlanes, 10.0f);
    viewer.hasFrustum = true;

    FluidSimulation sim;
    sim.SetViewer(viewer);
    sim.Update(1.0f / 60.0f, &world);

    ENJIN_EXPECT_EQ(sim.GetCulledVolumeCount(), 0u);
}

ENJIN_TEST(FluidCulling, test_distance_culling_is_off_unless_a_distance_is_given) {
    // 0 means no limit rather than "cull everything": what counts as far
    // depends on the scene's scale, and a guessed default would freeze a
    // chimney in a cramped level.
    ECS::World world;
    MakeVolume(world, Math::Vector3(10000.0f, 0.0f, 0.0f));

    FluidViewer viewer;   // no frustum, no distance
    FluidSimulation sim;
    sim.SetViewer(viewer);
    sim.Update(1.0f / 60.0f, &world);

    ENJIN_EXPECT_EQ(sim.GetCulledVolumeCount(), 0u);
}

ENJIN_TEST(FluidCulling, test_a_volume_past_the_cull_distance_is_not_solved) {
    ECS::World world;
    MakeVolume(world, Math::Vector3(200.0f, 0.0f, 0.0f));

    FluidViewer viewer;
    viewer.cullDistance = 50.0f;
    FluidSimulation sim;
    sim.SetViewer(viewer);
    sim.Update(1.0f / 60.0f, &world);

    ENJIN_EXPECT_EQ(sim.GetCulledVolumeCount(), 1u);
}

ENJIN_TEST(FluidCulling, test_a_culled_volume_resumes_where_it_stopped_not_fast_forwarded) {
    // The compromise this buys is a FROZEN volume, not a reset one -- and the
    // time it missed is dropped rather than owed. Banking it would fast-forward
    // a minute of simulation the instant the player turned around, which is a
    // visible lurch and costs exactly what culling was meant to save.
    ECS::World world;
    ECS::Entity e = MakeVolume(world, Math::Vector3(0.0f, 0.0f, 0.0f));

    FluidSimulation sim;
    for (int i = 0; i < 30; ++i) sim.Update(1.0f / 60.0f, &world);

    const FluidGridData* g = sim.GetGridData(e);
    ENJIN_ASSERT_TRUE(g != nullptr);
    f32 before = 0.0f;
    for (f32 d : g->density) before += d;
    ENJIN_ASSERT_TRUE(before > 0.0f);

    // Out of view for a long time.
    FluidViewer viewer;
    BoxFrustum(viewer.frustumPlanes, 1.0f);
    viewer.hasFrustum = true;
    viewer.position = Math::Vector3(500.0f, 0.0f, 0.0f);
    // Place the volume far outside that box.
    world.GetComponent<ECS::TransformComponent>(e)->position = Math::Vector3(500.0f, 0, 0);
    sim.SetViewer(viewer);
    for (int i = 0; i < 600; ++i) sim.Update(1.0f / 60.0f, &world);

    f32 frozen = 0.0f;
    for (f32 d : sim.GetGridData(e)->density) frozen += d;
    ENJIN_EXPECT_FLOAT_NEAR(frozen, before, 0.0001f);   // untouched, not cleared
}

ENJIN_TEST_MAIN()
