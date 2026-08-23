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
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
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

// Drop a dynamic box onto a static floor with the given collision filters and
// return the box's resting/final Y. Used to prove the engine applies filtering.
static f32 RunFilteredFall(u32 boxCat, u32 boxMask, u32 floorCat, u32 floorMask) {
    ECS::World world;

    ECS::Entity floor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(floor, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(50.0f, 1.0f, 50.0f);
        bc.categoryBits = floorCat;
        bc.collisionMask = floorMask;
        world.AddComponent<ECS::BoxColliderComponent>(floor, bc);
    }

    ECS::Entity box = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 10.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(box, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(1.0f, 1.0f, 1.0f);
        bc.categoryBits = boxCat;
        bc.collisionMask = boxMask;
        world.AddComponent<ECS::BoxColliderComponent>(box, bc);
        ECS::RigidbodyComponent rb;
        rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
        world.AddComponent<ECS::RigidbodyComponent>(box, rb);
    }

    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    backend->SetWorld(&world);
    backend->SetColliderEntities({floor, box});
    backend->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f));
    for (int i = 0; i < 180; ++i) backend->Update(1.0f / 60.0f);
    return world.GetComponent<ECS::TransformComponent>(box)->position.y;
}

ENJIN_TEST(PhysicsSim3D, CollisionFilteringSuppressesContact) {
    // Compatible filters (defaults on both) -> the box rests on the floor.
    f32 restY = RunFilteredFall(1, 0xFFFFFFFFu, 1, 0xFFFFFFFFu);
    ENJIN_EXPECT_FLOAT_NEAR(restY, 1.0f, 0.4f);

    // Mutually-exclusive groups -> bilateral mask fails, no contact, the box
    // passes straight through the floor (proves the engine applies filtering,
    // not just that the bitmask math is correct in the abstract).
    f32 throughY = RunFilteredFall(2, 2, 1, 1);
    ENJIN_EXPECT_TRUE(throughY < 0.0f);
}

ENJIN_TEST(PhysicsSim3D, ColliderSizeIsWorldSpaceIgnoringScale) {
    // Arrange: a static floor (top at y=0.5) and a dynamic box whose ENTITY is
    // scaled 2x but whose collider size is 1 unit. Collider extents are world
    // space, so scale must not change them.
    ECS::World world;

    ECS::Entity floor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(floor, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(50.0f, 1.0f, 50.0f);
        world.AddComponent<ECS::BoxColliderComponent>(floor, bc);
    }

    ECS::Entity box = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 10.0f, 0.0f);
        t.scale = Math::Vector3(2.0f, 2.0f, 2.0f);  // 2x entity scale
        world.AddComponent<ECS::TransformComponent>(box, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(1.0f, 1.0f, 1.0f);  // half extent 0.5 in WORLD units
        world.AddComponent<ECS::BoxColliderComponent>(box, bc);
        ECS::RigidbodyComponent rb;
        rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
        world.AddComponent<ECS::RigidbodyComponent>(box, rb);
    }

    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    backend->SetWorld(&world);
    backend->SetColliderEntities({floor, box});
    backend->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f));
    for (int i = 0; i < 180; ++i) backend->Update(1.0f / 60.0f);

    // Rests at floorTop(0.5) + colliderHalf(0.5) = 1.0. If scale wrongly applied,
    // it would rest at 0.5 + 1.0 = 1.5.
    auto* t = world.GetComponent<ECS::TransformComponent>(box);
    ENJIN_EXPECT_FLOAT_NEAR(t->position.y, 1.0f, 0.3f);
}

ENJIN_TEST(PhysicsSim3D, CharacterControllerGroundsOnFloor) {
    // Arrange: a static floor (top at y=0.5).
    ECS::World world;
    ECS::Entity floor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(floor, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(50.0f, 1.0f, 50.0f);
        world.AddComponent<ECS::BoxColliderComponent>(floor, bc);
    }

    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
    backend->SetWorld(&world);
    backend->SetColliderEntities({floor});
    backend->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f));
    backend->Update(1.0f / 60.0f);  // create the floor body

    // A capsule character starting above the floor.
    ECS::Entity ch = world.CreateEntity();
    ECS::TransformComponent ct;
    ct.position = Math::Vector3(0.0f, 5.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(ch, ct);
    backend->CreateCharacterController(ch, 0.3f, 0.9f, Math::Vector3(0.0f, 5.0f, 0.0f));

    // Act: drive it downward until it lands.
    Physics::IPhysicsBackend::CharacterState st;
    for (int i = 0; i < 240; ++i) {
        st = backend->UpdateCharacterController(ch, Math::Vector3(0.0f, -4.0f, 0.0f), 1.0f / 60.0f);
        backend->Update(1.0f / 60.0f);
    }

    // Assert: it grounded on the flat floor.
    ENJIN_EXPECT_EQ((int)st.groundState,
                    (int)Physics::IPhysicsBackend::CharacterGroundState::OnGround);
    ENJIN_EXPECT_FLOAT_NEAR(st.groundNormal.y, 1.0f, 0.15f);
}

ENJIN_TEST(PhysicsSim2D, SensorFiresOnOverlap) {
    // Arrange: a static sensor zone at the origin and a dynamic body above it.
    ECS::World world;

    ECS::Entity zone = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(zone, t);
        Physics::Body2DComponent b;
        b.isStatic = true;
        b.isSensor = true;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(3.0f, 3.0f);
        world.AddComponent<Physics::Body2DComponent>(zone, b);
    }

    ECS::Entity visitor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 8.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(visitor, t);
        Physics::Body2DComponent b;
        b.isStatic = false;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(0.5f, 0.5f);
        world.AddComponent<Physics::Body2DComponent>(visitor, b);
    }

    auto backend = Physics::CreatePhysicsBackend2D(Physics::PhysicsBackendType::Auto);
    backend->Initialize(&world);
    backend->SetGravity(Math::Vector2(0.0f, -9.81f));

    bool entered = false;
    backend->SetOnSensorEnter([&](const Physics::Contact2D&) { entered = true; });

    // Act: let the visitor fall through the sensor zone.
    for (int i = 0; i < 180; ++i) backend->Update(1.0f / 60.0f);

    // Assert: the sensor enter event fired.
    ENJIN_EXPECT_TRUE(entered);
}

ENJIN_TEST(PhysicsSim2D, RaycastSkipsSensors) {
    // Arrange: a sensor zone at x=5 and a solid wall at x=10, ray fired along +x.
    // Box2DBackend::Raycast filters out sensor bodies BY DESIGN (documented
    // engine rule) — the ray must report the wall behind the sensor, not the
    // sensor itself.
    ECS::World world;

    ECS::Entity sensor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(5.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(sensor, t);
        Physics::Body2DComponent b;
        b.isStatic = true;
        b.isSensor = true;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(1.0f, 3.0f);
        world.AddComponent<Physics::Body2DComponent>(sensor, b);
    }

    ECS::Entity wall = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(10.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(wall, t);
        Physics::Body2DComponent b;
        b.isStatic = true;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(0.5f, 3.0f);
        world.AddComponent<Physics::Body2DComponent>(wall, b);
    }

    auto backend = Physics::CreatePhysicsBackend2D(Physics::PhysicsBackendType::Auto);
    ENJIN_ASSERT_NOT_NULL(backend.get());
    backend->Initialize(&world);
    backend->Update(1.0f / 60.0f);  // create the bodies

    // Act
    Physics::RayHit2D hit;
    bool hitAnything = backend->Raycast(Math::Vector2(0.0f, 0.0f), Math::Vector2(1.0f, 0.0f), 20.0f, hit);

    // Assert: the wall (near face x=9.5), not the sensor (near face x=4).
    ENJIN_ASSERT_TRUE(hitAnything);
    ENJIN_EXPECT_EQ(hit.entity, wall);
    ENJIN_EXPECT_FLOAT_NEAR(hit.distance, 9.5f, 0.3f);
}

ENJIN_TEST(PhysicsSim2D, DistanceJointMaintainsSeparation) {
    // Arrange: a static anchor at (0,10) and a dynamic weight below it joined by
    // a RIGID distance joint (stiffness 0) of length 2. Under gravity the weight
    // must hang at the joint length instead of falling away.
    ECS::World world;

    ECS::Entity anchor = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 10.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(anchor, t);
        Physics::Body2DComponent b;
        b.isStatic = true;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(0.25f, 0.25f);
        world.AddComponent<Physics::Body2DComponent>(anchor, b);
    }

    ECS::Entity weight = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 7.0f, 0.0f);   // starts 3 below: joint must pull to 2
        world.AddComponent<ECS::TransformComponent>(weight, t);
        Physics::Body2DComponent b;
        b.isStatic = false;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(0.25f, 0.25f);
        world.AddComponent<Physics::Body2DComponent>(weight, b);
        Physics::Joint2DComponent j;
        j.type = Physics::Joint2DType::Distance;
        j.connectedEntity = anchor;
        j.length = 2.0f;
        j.stiffness = 0.0f;   // rigid, no spring
        world.AddComponent<Physics::Joint2DComponent>(weight, j);
    }

    auto backend = Physics::CreatePhysicsBackend2D(Physics::PhysicsBackendType::Auto);
    ENJIN_ASSERT_NOT_NULL(backend.get());
    backend->Initialize(&world);
    backend->SetGravity(Math::Vector2(0.0f, -9.81f));

    // Act: settle for 3 seconds.
    for (int i = 0; i < 180; ++i) backend->Update(1.0f / 60.0f);

    // Assert: separation equals the joint length; the weight hangs, not falls.
    auto* ta = world.GetComponent<ECS::TransformComponent>(anchor);
    auto* tw = world.GetComponent<ECS::TransformComponent>(weight);
    ENJIN_ASSERT_NOT_NULL(ta);
    ENJIN_ASSERT_NOT_NULL(tw);
    f32 dx = tw->position.x - ta->position.x;
    f32 dy = tw->position.y - ta->position.y;
    f32 dist = std::sqrt(dx * dx + dy * dy);
    ENJIN_EXPECT_FLOAT_NEAR(dist, 2.0f, 0.3f);
    ENJIN_EXPECT_TRUE(tw->position.y > 5.0f);   // did not fall away
}

ENJIN_TEST(GameplayHazards, HazardOverlapDamagesPlayerWithoutSensorEvents) {
    // Box2D v3 doesn't fire sensor events between kinematic sensors and
    // kinematic visitors — CheckHazardOverlaps is the engine's documented
    // per-frame AABB workaround. Prove the workaround itself: overlap damages,
    // damageOnce doesn't repeat, and a distant player is untouched.
    ECS::World world;

    ECS::Entity player = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(player, t);
        world.AddComponent<ECS::Platformer2DController>(player);
        ECS::HealthComponent hp;
        hp.maxHealth = 100.0f;
        hp.currentHealth = 100.0f;
        world.AddComponent<ECS::HealthComponent>(player, hp);
    }

    ECS::Entity hazard = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);   // overlapping the player
        world.AddComponent<ECS::TransformComponent>(hazard, t);
        ECS::DamageComponent dmg;
        dmg.damage = 25.0f;
        dmg.damageOnce = true;
        world.AddComponent<ECS::DamageComponent>(hazard, dmg);
        Physics::Body2DComponent b;
        b.isKinematic = true;    // the documented hazard-sensor configuration
        b.isSensor = true;
        b.gravityScale = 0.0f;
        b.shapeType = Physics::Shape2DType::Box;
        b.box.halfExtents = Math::Vector2(1.0f, 1.0f);
        world.AddComponent<Physics::Body2DComponent>(hazard, b);
    }

    // Act: one hazard pass while overlapping.
    std::vector<ECS::Entity> deferred;
    Gameplay::GameplayLoop::CheckHazardOverlaps(&world, 1.0f / 60.0f, deferred);

    // Assert: damage applied exactly once.
    auto* hp = world.GetComponent<ECS::HealthComponent>(player);
    ENJIN_ASSERT_NOT_NULL(hp);
    ENJIN_EXPECT_FLOAT_NEAR(hp->currentHealth, 75.0f, 0.01f);

    // Act again: damageOnce must not re-apply.
    Gameplay::GameplayLoop::CheckHazardOverlaps(&world, 1.0f / 60.0f, deferred);
    ENJIN_EXPECT_FLOAT_NEAR(hp->currentHealth, 75.0f, 0.01f);

    // A player far away takes no damage.
    ECS::Entity farPlayer = world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(100.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(farPlayer, t);
        world.AddComponent<ECS::Platformer2DController>(farPlayer);
        ECS::HealthComponent fhp;
        fhp.maxHealth = 100.0f;
        fhp.currentHealth = 100.0f;
        world.AddComponent<ECS::HealthComponent>(farPlayer, fhp);
    }
    Gameplay::GameplayLoop::CheckHazardOverlaps(&world, 1.0f / 60.0f, deferred);
    auto* fhp = world.GetComponent<ECS::HealthComponent>(farPlayer);
    ENJIN_ASSERT_NOT_NULL(fhp);
    ENJIN_EXPECT_FLOAT_NEAR(fhp->currentHealth, 100.0f, 0.01f);
}

// ===========================================================================
// Factory severity — Auto in a 2D project declines the 3D backend on purpose
// (regression: this used to log "No 3D physics backend available" as an ERROR
// at every 2D project play start)
// ===========================================================================

namespace {
    int g_PhysicsErrorCount = 0;
    void CountPhysicsErrors(LogLevel level, LogCategory category, const char*) {
        if (level == LogLevel::Error && category == LogCategory::Physics)
            ++g_PhysicsErrorCount;
    }
    // The logger silently drops everything (callback included) until
    // Initialize is called; idempotent, so safe from every test.
    void ArmPhysicsErrorCounter() {
        Logger::Get().Initialize("test_physics_factory.log");
        g_PhysicsErrorCount = 0;
        Logger::Get().SetLogCallback(&CountPhysicsErrors);
    }
}

ENJIN_TEST(PhysicsFactory, AutoIn2DProjectDeclines3DBackendWithoutError) {
    // Arrange
    ArmPhysicsErrorCounter();

    // Act
    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto,
                                                 Scene::ProjectMode::Mode2D);

    // Assert (callback restored first so a failing expect can't leak it)
    Logger::Get().SetLogCallback(nullptr);
    ENJIN_EXPECT_TRUE(backend == nullptr);
    ENJIN_EXPECT_EQ(g_PhysicsErrorCount, 0);
}

ENJIN_TEST(PhysicsFactory, ExplicitBox2DFor3DPhysicsStillErrors) {
    // Arrange: an explicit, unfulfillable request must keep its ERROR.
    // This is also the positive control proving the counter actually fires.
    ArmPhysicsErrorCounter();

    // Act
    auto backend = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Box2D,
                                                 Scene::ProjectMode::Mode3D);

    // Assert
    Logger::Get().SetLogCallback(nullptr);
    ENJIN_EXPECT_TRUE(backend == nullptr);
    ENJIN_EXPECT_TRUE(g_PhysicsErrorCount >= 1);
}

ENJIN_TEST_MAIN()
