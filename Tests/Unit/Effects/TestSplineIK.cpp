// A spline IK chain that actually runs: tentacles, tails, vines.
//
// The component, the FABRIK solver and the tube mesh generator all existed and
// NO runtime ticked any of them, so adding the component to an entity did
// nothing. Worse than nothing once ticked, in fact: the system rewrites
// MeshComponent vertices every frame, and the GPU re-upload path is keyed to
// the components that own runtime-generated geometry. A chain that is not one
// of those reaches the GPU exactly once and then solves invisibly forever,
// which is the cruellest failure shape -- it renders correctly and never moves.
#include "EnjinTest.h"
#include "Enjin/Effects/SplineIKDeformer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/ProceduralMesh.h"

#include <cmath>

using namespace Enjin;

namespace {

ECS::Entity MakeChain(ECS::World& world, i32 joints = 8) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = Math::Vector3(0.0f, 10.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(e, xf);

    Effects::SplineIKComponent chain;
    chain.jointCount = joints;
    chain.totalLength = 5.0f;
    chain.usePhysics = true;
    chain.useGravity = true;
    world.AddComponent<Effects::SplineIKComponent>(e, chain);
    return e;
}

} // namespace

ENJIN_TEST(SplineIK, test_a_chain_builds_its_joints_and_a_mesh_on_the_first_tick) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeChain(world);
    Effects::SplineIKSystem system;

    // Act
    system.Update(&world, 1.0f / 60.0f);

    // Assert
    const auto* chain = world.GetComponent<Effects::SplineIKComponent>(e);
    ENJIN_ASSERT_TRUE(chain != nullptr);
    ENJIN_ASSERT_EQ(static_cast<i32>(chain->joints.size()), chain->jointCount);

    const auto* mesh = world.GetComponent<ECS::MeshComponent>(e);
    ENJIN_ASSERT_TRUE(mesh != nullptr);
    ENJIN_EXPECT_TRUE(!mesh->vertices.empty());
    ENJIN_EXPECT_TRUE(!mesh->indices.empty());
}

ENJIN_TEST(SplineIK, test_the_generated_mesh_is_marked_for_re_upload_every_tick) {
    // The bug this exists to stop: a chain that solves on the CPU and never
    // moves on screen. ProceduralMeshComponent is what the renderer watches for
    // runtime-generated geometry, and its Source enum has carried a SplineIK
    // value the whole time with nothing setting it.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeChain(world);
    Effects::SplineIKSystem system;

    // Act
    system.Update(&world, 1.0f / 60.0f);

    // Assert
    const auto* proc = world.GetComponent<ECS::ProceduralMeshComponent>(e);
    ENJIN_ASSERT_TRUE(proc != nullptr);
    ENJIN_EXPECT_TRUE(proc->meshDirty);
    ENJIN_EXPECT_TRUE(proc->source == ECS::ProceduralMeshComponent::Source::SplineIK);
}

ENJIN_TEST(SplineIK, test_a_hanging_chain_falls_under_gravity) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeChain(world);
    Effects::SplineIKSystem system;
    system.Update(&world, 1.0f / 60.0f);

    const auto* chain = world.GetComponent<Effects::SplineIKComponent>(e);
    ENJIN_ASSERT_TRUE(chain != nullptr);
    const f32 tipBefore = chain->joints.back().position.y;

    // Act
    for (int i = 0; i < 120; ++i) system.Update(&world, 1.0f / 60.0f);

    // Assert
    const f32 tipAfter = world.GetComponent<Effects::SplineIKComponent>(e)->joints.back().position.y;
    ENJIN_EXPECT_TRUE(tipAfter < tipBefore);
}

ENJIN_TEST(SplineIK, test_the_root_stays_pinned_to_the_entity) {
    // A chain whose root drifted would detach from whatever it hangs off, and
    // the entity's own transform is the only thing anchoring it.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeChain(world);
    Effects::SplineIKSystem system;

    // Act
    for (int i = 0; i < 60; ++i) system.Update(&world, 1.0f / 60.0f);
    world.GetComponent<ECS::TransformComponent>(e)->position = Math::Vector3(3.0f, 12.0f, -4.0f);
    for (int i = 0; i < 10; ++i) system.Update(&world, 1.0f / 60.0f);

    // Assert
    const auto* chain = world.GetComponent<Effects::SplineIKComponent>(e);
    ENJIN_ASSERT_TRUE(chain != nullptr);
    ENJIN_EXPECT_FLOAT_NEAR(chain->joints[0].position.x, 3.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(chain->joints[0].position.y, 12.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(chain->joints[0].position.z, -4.0f, 0.0001f);
}

ENJIN_TEST(SplineIK, test_changing_the_joint_count_rebuilds_the_chain) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeChain(world, 8);
    Effects::SplineIKSystem system;
    system.Update(&world, 1.0f / 60.0f);

    // Act
    auto* chain = world.GetComponent<Effects::SplineIKComponent>(e);
    chain->jointCount = 20;
    chain->initialized = false;
    system.Update(&world, 1.0f / 60.0f);

    // Assert
    ENJIN_EXPECT_EQ(static_cast<i32>(chain->joints.size()), 20);
}

ENJIN_TEST(SplineIK, test_a_joint_count_below_two_is_clamped_rather_than_crashing) {
    // A one-joint chain has no segment to build a tube around, and the mesh
    // generator indexes pairs.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeChain(world, 1);
    Effects::SplineIKSystem system;

    // Act
    system.Update(&world, 1.0f / 60.0f);

    // Assert
    const auto* chain = world.GetComponent<Effects::SplineIKComponent>(e);
    ENJIN_ASSERT_TRUE(chain != nullptr);
    ENJIN_EXPECT_TRUE(chain->jointCount >= 2);
    ENJIN_EXPECT_TRUE(chain->joints.size() >= 2);
}

ENJIN_TEST_MAIN()
