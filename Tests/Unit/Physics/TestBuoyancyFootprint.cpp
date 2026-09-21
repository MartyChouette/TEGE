// Buoyancy stops at the shoreline, not at the bounding box.
//
// BoundaryPolygonComponent lets a lake be dragged into any shape, and until now
// only the renderer read it. JoltBackend::ApplyBuoyancy built its zone from
// halfExtents alone, so a concave lake held things up across the whole rectangle
// it was seeded from -- props bobbing in mid-air over the dry corner, with the
// water visibly somewhere else.
//
// The unit test beside this one (TestWaterFootprint) covers the containment rule
// itself. This one is here because that rule has to survive the trip into the
// physics step, where the zone is CACHED and the outline had to be carried along
// with it.
#include "EnjinTest.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/BoundaryPolygon.h"

#include <memory>

using namespace Enjin;

namespace {

// An L-shaped lake, surface at Y = 0, ten deep. The missing quadrant is +X/+Z,
// so (5, 5) sits inside the bounding box and outside the water.
ECS::Entity MakeLShapedLake(ECS::World& world) {
    ECS::Entity e = world.CreateEntity();

    ECS::TransformComponent xf;
    xf.position = Math::Vector3(0.0f, 0.0f, 0.0f);   // position IS the surface
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::WaterVolumeComponent wv;
    wv.halfExtents = Math::Vector3(10.0f, 5.0f, 10.0f);
    wv.enableBuoyancy = true;
    wv.buoyancyStrength = 1.6f;
    wv.buoyancyDrag = 2.5f;
    world.AddComponent<ECS::WaterVolumeComponent>(e, wv);

    ECS::BoundaryPolygonComponent bp;
    bp.points = {
        {-10.0f, -10.0f},
        { 10.0f, -10.0f},
        { 10.0f,   0.0f},
        {  0.0f,   0.0f},
        {  0.0f,  10.0f},
        {-10.0f,  10.0f},
    };
    world.AddComponent<ECS::BoundaryPolygonComponent>(e, bp);
    return e;
}

// A dynamic box dropped below the surface with nothing under it. Whether it
// rises or falls is entirely down to whether the water claims its XZ.
ECS::Entity MakeSubmergedBox(ECS::World& world, f32 x, f32 z) {
    ECS::Entity e = world.CreateEntity();

    ECS::TransformComponent xf;
    xf.position = Math::Vector3(x, -2.0f, z);
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::RigidbodyComponent rb;
    rb.mass = 1.0f;
    rb.useGravity = true;
    rb.drag = 0.0f;
    world.AddComponent<ECS::RigidbodyComponent>(e, rb);

    ECS::BoxColliderComponent col;
    col.size = Math::Vector3(1.0f, 1.0f, 1.0f);
    world.AddComponent<ECS::BoxColliderComponent>(e, col);
    return e;
}

f32 HeightOf(ECS::World& world, ECS::Entity e) {
    return world.GetComponent<ECS::TransformComponent>(e)->position.y;
}

} // namespace

ENJIN_TEST(BuoyancyFootprint, FloatsInsideTheOutlineAndSinksInTheBite) {
    // Arrange: two identical boxes, same depth, both inside the lake's bounding
    // box. One is in the L, the other in the quadrant the L does not cover.
    ECS::World world;
    MakeLShapedLake(world);
    const ECS::Entity inWater = MakeSubmergedBox(world, -5.0f, -5.0f);
    const ECS::Entity inBite  = MakeSubmergedBox(world,  5.0f,  5.0f);

    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) return;                 // no Jolt in this build
    physics->SetWorld(&world);

    // Act
    for (int i = 0; i < 180; ++i) physics->Update(1.0f / 60.0f);

    const f32 yWater = HeightOf(world, inWater);
    const f32 yBite  = HeightOf(world, inBite);

    // Assert: buoyancy is net upward at full submersion, so the one in the water
    // climbs toward the surface while the one over dry ground just falls.
    ENJIN_EXPECT_GT(yWater, -2.0f);
    ENJIN_EXPECT_LT(yBite, -2.0f);
    ENJIN_EXPECT_GT(yWater, yBite);
}

ENJIN_TEST(BuoyancyFootprint, NoOutlineStillFloatsTheWholeBox) {
    // Arrange: the same corner, in a lake with no outline at all. Every scene
    // authored before the outline existed is this one, and it has to be
    // untouched by the change.
    ECS::World world;
    ECS::Entity lake = MakeLShapedLake(world);
    world.RemoveComponent<ECS::BoundaryPolygonComponent>(lake);
    const ECS::Entity inCorner = MakeSubmergedBox(world, 5.0f, 5.0f);

    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) return;
    physics->SetWorld(&world);

    // Act
    for (int i = 0; i < 180; ++i) physics->Update(1.0f / 60.0f);

    // Assert
    ENJIN_EXPECT_GT(HeightOf(world, inCorner), -2.0f);
}

ENJIN_TEST_MAIN()
