// Behavioral physics tests: actually create a backend, step the simulation, and
// assert physical outcomes. The existing physics suites only check struct
// defaults, factory metadata, and bitmask math against a test-local copy of the
// filter; none step a world. These do.

#include "EnjinTest.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <memory>

using namespace Enjin;

// ===========================================================================
// Jolt 3D — a dynamic box falls under gravity and rests on a static floor
// ===========================================================================

ENJIN_TEST(PhysicsSim3D, DynamicBoxFallsAndRestsOnFloor) {
    // Arrange: a static floor (no rigidbody) at y=0, a dynamic box at y=10.
    ECS::World world;

    ECS::Entity floor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(floor, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(50.0f, 1.0f, 50.0f);   // full extents -> top at y=0.5
        world.AddComponent<ECS::BoxColliderComponent>(floor, bc);
    }

    ECS::Entity box = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 10.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(box, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(1.0f, 1.0f, 1.0f);      // half extent 0.5
        world.AddComponent<ECS::BoxColliderComponent>(box, bc);
        ECS::RigidbodyComponent rb;
        rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
        rb.mass = 1.0f;
        world.AddComponent<ECS::RigidbodyComponent>(box, rb);
    }

    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    ENJIN_ASSERT_NOT_NULL(backend.get());
    backend->SetWorld(&world);
    backend->SetColliderEntities({floor, box});
    backend->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f));

    // Act: step 3 seconds at 60 Hz.
    for (int i = 0; i < 180; ++i) backend->Update(1.0f / 60.0f);

    // Assert: it fell from y=10 and settled on the floor top (~1.0), no tunneling.
    auto* t = world.GetComponent<ECS::TransformComponent>(box);
    ENJIN_ASSERT_NOT_NULL(t);
    ENJIN_EXPECT_TRUE(t->position.y < 9.0f);              // fell
    ENJIN_EXPECT_TRUE(t->position.y > 0.4f);              // did not tunnel through
    ENJIN_EXPECT_FLOAT_NEAR(t->position.y, 1.0f, 0.4f);   // rests on the surface
}

ENJIN_TEST(PhysicsSim3D, GravityRoundTrips) {
    // Arrange / Act
    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    ENJIN_ASSERT_NOT_NULL(backend.get());
    backend->SetGravity(Math::Vector3(0.0f, -20.0f, 0.0f));
    // Assert
    Math::Vector3 g = backend->GetGravity();
    ENJIN_EXPECT_FLOAT_NEAR(g.y, -20.0f, 0.01f);
    ENJIN_EXPECT_STR_EQ(backend->GetName(), backend->GetName());  // name is non-null
    ENJIN_EXPECT_NOT_NULL(backend->GetName());
}

ENJIN_TEST(PhysicsSim3D, RaycastHitsFloor) {
    // Arrange: a single static floor.
    ECS::World world;
    ECS::Entity floor = world.CreateEntity();
    ECS::TransformComponent t;
    t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(floor, t);
    ECS::BoxColliderComponent bc;
    bc.size = Math::Vector3(50.0f, 1.0f, 50.0f);  // top at y=0.5
    world.AddComponent<ECS::BoxColliderComponent>(floor, bc);

    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    ENJIN_ASSERT_NOT_NULL(backend.get());
    backend->SetWorld(&world);
    backend->SetColliderEntities({floor});
    backend->Update(1.0f / 60.0f);  // create the body

    // Act: cast straight down from y=5 at the floor.
    Physics::Ray ray;
    ray.origin = Math::Vector3(0.0f, 5.0f, 0.0f);
    ray.direction = Math::Vector3(0.0f, -1.0f, 0.0f);
    Physics::RaycastHit hit = backend->Raycast(ray, 10.0f);

    // Assert: hits the floor at ~4.5 units (5.0 - 0.5 top).
    ENJIN_EXPECT_TRUE(hit.hit);
    ENJIN_EXPECT_EQ(hit.entity, floor);
    ENJIN_EXPECT_FLOAT_NEAR(hit.distance, 4.5f, 0.3f);

    // And a ray pointing away from the floor misses.
    Physics::Ray up;
    up.origin = Math::Vector3(0.0f, 5.0f, 0.0f);
    up.direction = Math::Vector3(0.0f, 1.0f, 0.0f);
    Physics::RaycastHit miss = backend->Raycast(up, 10.0f);
    ENJIN_EXPECT_FALSE(miss.hit);
}

// ===========================================================================
// Box2D 2D — a dynamic body falls under gravity and rests on a static floor
// ===========================================================================

ENJIN_TEST(PhysicsSim2D, DynamicBodyFallsAndRestsOnFloor) {
    // Arrange: a static floor at y=0, a dynamic box at y=10.
    ECS::World world;

    ECS::Entity floor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(floor, t);
        Physics::Body2DComponent b;
        b.isStatic = true;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(25.0f, 0.5f);  // top at y=0.5
        world.AddComponent<Physics::Body2DComponent>(floor, b);
    }

    ECS::Entity ball = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 10.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(ball, t);
        Physics::Body2DComponent b;
        b.isStatic = false;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(0.5f, 0.5f);
        b.gravityScale = 1.0f;
        world.AddComponent<Physics::Body2DComponent>(ball, b);
    }

    auto backend = Physics::CreatePhysicsBackend2D(Physics::PhysicsBackendType::Auto);
    ENJIN_ASSERT_NOT_NULL(backend.get());
    backend->Initialize(&world);
    backend->SetGravity(Math::Vector2(0.0f, -9.81f));

    // Act: step 3 seconds at 60 Hz.
    for (int i = 0; i < 180; ++i) backend->Update(1.0f / 60.0f);

    // Assert: fell from y=10 and settled on the floor top (~1.0), no tunneling.
    auto* t = world.GetComponent<ECS::TransformComponent>(ball);
    ENJIN_ASSERT_NOT_NULL(t);
    ENJIN_EXPECT_TRUE(t->position.y < 9.0f);             // fell
    ENJIN_EXPECT_TRUE(t->position.y > 0.4f);             // did not tunnel through
    ENJIN_EXPECT_FLOAT_NEAR(t->position.y, 1.0f, 0.5f);  // rests on the surface
}

ENJIN_TEST_MAIN()
