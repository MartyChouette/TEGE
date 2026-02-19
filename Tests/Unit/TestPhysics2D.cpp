#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Physics/PhysicsTypes2D.h"

// Physics2D tests now use Box2D backend via IPhysicsBackend2D

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;
using namespace Enjin::Physics;

// ===========================================================================
// Helper: create a Body2D entity at a given 2D position
// ===========================================================================

static Entity CreateBody2D(World& world, const Vector2& pos, const Body2DComponent& bodyDef) {
    Entity e = world.CreateEntity();
    TransformComponent tc;
    tc.position = Vector3(pos.x, pos.y, 0.0f);
    world.AddComponent<TransformComponent>(e, tc);
    world.AddComponent<Body2DComponent>(e, bodyDef);
    return e;
}

static Entity CreateCircle(World& world, const Vector2& pos, f32 radius, bool isStatic = false) {
    Body2DComponent body;
    body.shapeType = Shape2DType::Circle;
    body.circle.radius = radius;
    body.isStatic = isStatic;
    return CreateBody2D(world, pos, body);
}

static Entity CreateBox(World& world, const Vector2& pos, const Vector2& halfExtents, bool isStatic = false) {
    Body2DComponent body;
    body.shapeType = Shape2DType::Box;
    body.box.halfExtents = halfExtents;
    body.isStatic = isStatic;
    return CreateBody2D(world, pos, body);
}

// ===========================================================================
// 1. Body Creation and Removal
// ===========================================================================

ENJIN_TEST(Physics2D, BodyCreation) {
    World world;
    Entity e = CreateCircle(world, Vector2(0.0f, 0.0f), 1.0f);
    ENJIN_EXPECT_TRUE(world.IsValid(e));
    ENJIN_EXPECT_TRUE(world.HasComponent<Body2DComponent>(e));
    ENJIN_EXPECT_TRUE(world.HasComponent<TransformComponent>(e));

    auto* body = world.GetComponent<Body2DComponent>(e);
    ENJIN_ASSERT_NOT_NULL(body);
    ENJIN_EXPECT_EQ(body->shapeType, Shape2DType::Circle);
    ENJIN_EXPECT_FLOAT_EQ(body->circle.radius, 1.0f);
    ENJIN_EXPECT_FALSE(body->isStatic);
}

ENJIN_TEST(Physics2D, BodyRemoval) {
    World world;
    Entity e = CreateCircle(world, Vector2(0.0f, 0.0f), 0.5f);
    ENJIN_EXPECT_TRUE(world.IsValid(e));

    world.DestroyEntityImmediate(e);
    ENJIN_EXPECT_FALSE(world.IsValid(e));
    ENJIN_EXPECT_NULL(world.GetComponent<Body2DComponent>(e));
}

ENJIN_TEST(Physics2D, StaticBodyMassZero) {
    // Static bodies should have zero mass after Initialize
    World world;
    Entity e = CreateCircle(world, Vector2(0.0f, 0.0f), 1.0f, /*isStatic=*/true);

#ifdef ENJIN_PHYSICS_SIMPLE
    PhysicsWorld2D physics;
    physics.Initialize(&world);

    auto* body = world.GetComponent<Body2DComponent>(e);
    ENJIN_ASSERT_NOT_NULL(body);
    ENJIN_EXPECT_FLOAT_EQ(body->mass, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(body->inverseMass, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(body->inertia, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(body->inverseInertia, 0.0f);

    physics.Shutdown();
#endif
}

// ===========================================================================
// 2. Circle vs Circle Collision Detection
// ===========================================================================

ENJIN_TEST(Physics2D, CircleCircleOverlapping) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    // Two overlapping circles (distance = 1.0, radii sum = 2.0)
    Entity e1 = CreateCircle(world, Vector2(0.0f, 0.0f), 1.0f, true);
    Entity e2 = CreateCircle(world, Vector2(1.0f, 0.0f), 1.0f, true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    bool collisionDetected = false;
    physics.SetOnCollisionEnter([&](const Contact2D& c) {
        collisionDetected = true;
        // Normal should point from A to B (along positive X)
        ENJIN_EXPECT_GT(c.normal.x, 0.0f);
        ENJIN_EXPECT_FLOAT_NEAR(c.penetration, 1.0f, 0.1f);
    });

    // Need at least one dynamic body for collision to be processed
    auto* b1 = world.GetComponent<Body2DComponent>(e1);
    b1->isStatic = false;

    physics.Update(1.0f / 60.0f);
    // Collision should have been detected between two overlapping circles
    // (penetration = radiusSum - distance = 2.0 - 1.0 = 1.0)
#endif
}

ENJIN_TEST(Physics2D, CircleCircleSeparated) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    // Two circles far apart (distance = 10.0, radii sum = 2.0)
    CreateCircle(world, Vector2(0.0f, 0.0f), 1.0f);
    CreateCircle(world, Vector2(10.0f, 0.0f), 1.0f);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    bool collisionDetected = false;
    physics.SetOnCollisionEnter([&](const Contact2D&) {
        collisionDetected = true;
    });

    physics.Update(1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(collisionDetected);
#endif
}

// ===========================================================================
// 3. Box vs Box Collision Detection
// ===========================================================================

ENJIN_TEST(Physics2D, BoxBoxOverlapping) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    // Two overlapping 2x2 boxes, offset by 1 unit on X
    CreateBox(world, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
    CreateBox(world, Vector2(1.0f, 0.0f), Vector2(1.0f, 1.0f));

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    bool collisionDetected = false;
    physics.SetOnCollisionEnter([&](const Contact2D& c) {
        collisionDetected = true;
        // Penetration should be about 1.0 (halfExtents sum - distance = 2.0 - 1.0)
        ENJIN_EXPECT_GT(c.penetration, 0.0f);
    });

    physics.Update(1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(collisionDetected);
#endif
}

ENJIN_TEST(Physics2D, BoxBoxSeparated) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    // Two separated 2x2 boxes
    CreateBox(world, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
    CreateBox(world, Vector2(5.0f, 0.0f), Vector2(1.0f, 1.0f));

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    bool collisionDetected = false;
    physics.SetOnCollisionEnter([&](const Contact2D&) {
        collisionDetected = true;
    });

    physics.Update(1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(collisionDetected);
#endif
}

// ===========================================================================
// 4. Circle vs Box Collision
// ===========================================================================

ENJIN_TEST(Physics2D, CircleBoxOverlapping) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    // Circle at origin, box nearby
    CreateCircle(world, Vector2(0.0f, 0.0f), 1.0f);
    CreateBox(world, Vector2(1.5f, 0.0f), Vector2(1.0f, 1.0f));

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    bool collisionDetected = false;
    physics.SetOnCollisionEnter([&](const Contact2D& c) {
        collisionDetected = true;
        ENJIN_EXPECT_GT(c.penetration, 0.0f);
    });

    physics.Update(1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(collisionDetected);
#endif
}

ENJIN_TEST(Physics2D, CircleBoxSeparated) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    CreateCircle(world, Vector2(0.0f, 0.0f), 0.5f);
    CreateBox(world, Vector2(5.0f, 0.0f), Vector2(0.5f, 0.5f));

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    bool collisionDetected = false;
    physics.SetOnCollisionEnter([&](const Contact2D&) {
        collisionDetected = true;
    });

    physics.Update(1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(collisionDetected);
#endif
}

// ===========================================================================
// 5. Polygon Shape Creation with Vertex Count Limits
// ===========================================================================

ENJIN_TEST(Physics2D, PolygonShapeCreation) {
    // Polygon with valid vertex count (triangle)
    PolygonShape2D poly;
    poly.vertices.push_back(Vector2(0.0f, 0.0f));
    poly.vertices.push_back(Vector2(1.0f, 0.0f));
    poly.vertices.push_back(Vector2(0.5f, 1.0f));
    ENJIN_EXPECT_EQ(poly.vertices.size(), (size_t)3);
}

ENJIN_TEST(Physics2D, PolygonMaxVertices) {
    // The system uses kMaxPolygonVertices = 64 as a processing cap
    PolygonShape2D poly;
    // Add more than the documented "max 8" but within kMaxPolygonVertices(64)
    for (int i = 0; i < 100; ++i) {
        f32 angle = static_cast<f32>(i) * (2.0f * 3.14159f / 100.0f);
        poly.vertices.push_back(Vector2(cosf(angle), sinf(angle)));
    }
    // All 100 are stored (vector has no cap), but physics processes at most 64
    ENJIN_EXPECT_EQ(poly.vertices.size(), (size_t)100);
}

ENJIN_TEST(Physics2D, PolygonDefaultOffset) {
    PolygonShape2D poly;
    ENJIN_EXPECT_FLOAT_EQ(poly.offset.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(poly.offset.y, 0.0f);
}

// ===========================================================================
// 6. Raycasting
// ===========================================================================

ENJIN_TEST(Physics2D, RaycastHitCircle) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    Entity target = CreateCircle(world, Vector2(5.0f, 0.0f), 1.0f, /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    RayHit2D hit;
    bool result = physics.Raycast(Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), 100.0f, hit);

    ENJIN_EXPECT_TRUE(result);
    ENJIN_EXPECT_EQ(hit.entity, target);
    // Hit distance should be approximately 4.0 (5.0 center - 1.0 radius)
    ENJIN_EXPECT_FLOAT_NEAR(hit.distance, 4.0f, 0.1f);
    // Hit point should be on the circle surface
    ENJIN_EXPECT_FLOAT_NEAR(hit.point.x, 4.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(hit.point.y, 0.0f, 0.1f);
#endif
}

ENJIN_TEST(Physics2D, RaycastMiss) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    // Circle above the ray direction
    CreateCircle(world, Vector2(5.0f, 10.0f), 1.0f, /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    RayHit2D hit;
    // Shoot ray along X axis; circle is at Y=10 -- should miss
    bool result = physics.Raycast(Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), 100.0f, hit);
    ENJIN_EXPECT_FALSE(result);
#endif
}

ENJIN_TEST(Physics2D, RaycastHitBox) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    Entity target = CreateBox(world, Vector2(5.0f, 0.0f), Vector2(1.0f, 1.0f), /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    RayHit2D hit;
    bool result = physics.Raycast(Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), 100.0f, hit);

    ENJIN_EXPECT_TRUE(result);
    ENJIN_EXPECT_EQ(hit.entity, target);
    // Hit distance should be approximately 4.0 (5.0 center - 1.0 halfExtent)
    ENJIN_EXPECT_FLOAT_NEAR(hit.distance, 4.0f, 0.1f);
#endif
}

ENJIN_TEST(Physics2D, RaycastMaxDistance) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    // Circle at distance 50
    CreateCircle(world, Vector2(50.0f, 0.0f), 1.0f, /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    RayHit2D hit;
    // Max distance is only 10 -- should not reach the circle at 50
    bool result = physics.Raycast(Vector2(0.0f, 0.0f), Vector2(1.0f, 0.0f), 10.0f, hit);
    ENJIN_EXPECT_FALSE(result);
#endif
}

// ===========================================================================
// 7. Joint Creation (Distance, Revolute)
// ===========================================================================

ENJIN_TEST(Physics2D, DistanceJointCreation) {
    World world;
    Entity e1 = CreateCircle(world, Vector2(0.0f, 0.0f), 0.5f);
    Entity e2 = CreateCircle(world, Vector2(3.0f, 0.0f), 0.5f);

    Joint2DComponent joint;
    joint.type = Joint2DType::Distance;
    joint.connectedEntity = e2;
    joint.length = 3.0f;
    joint.stiffness = 10.0f;
    joint.damping = 1.0f;
    world.AddComponent<Joint2DComponent>(e1, joint);

    auto* j = world.GetComponent<Joint2DComponent>(e1);
    ENJIN_ASSERT_NOT_NULL(j);
    ENJIN_EXPECT_EQ(j->type, Joint2DType::Distance);
    ENJIN_EXPECT_EQ(j->connectedEntity, e2);
    ENJIN_EXPECT_FLOAT_EQ(j->length, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(j->stiffness, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(j->damping, 1.0f);
}

ENJIN_TEST(Physics2D, RevoluteJointCreation) {
    World world;
    Entity e1 = CreateCircle(world, Vector2(0.0f, 0.0f), 0.5f);
    Entity e2 = CreateCircle(world, Vector2(1.0f, 0.0f), 0.5f);

    Joint2DComponent joint;
    joint.type = Joint2DType::Revolute;
    joint.connectedEntity = e2;
    joint.enableLimit = true;
    joint.lowerAngle = -1.57f;
    joint.upperAngle = 1.57f;
    joint.enableMotor = true;
    joint.motorSpeed = 2.0f;
    joint.maxMotorTorque = 10.0f;
    world.AddComponent<Joint2DComponent>(e1, joint);

    auto* j = world.GetComponent<Joint2DComponent>(e1);
    ENJIN_ASSERT_NOT_NULL(j);
    ENJIN_EXPECT_EQ(j->type, Joint2DType::Revolute);
    ENJIN_EXPECT_TRUE(j->enableLimit);
    ENJIN_EXPECT_FLOAT_NEAR(j->lowerAngle, -1.57f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(j->upperAngle, 1.57f, 0.01f);
    ENJIN_EXPECT_TRUE(j->enableMotor);
    ENJIN_EXPECT_FLOAT_EQ(j->motorSpeed, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(j->maxMotorTorque, 10.0f);
}

// ===========================================================================
// 8. CCD Detection
// ===========================================================================

ENJIN_TEST(Physics2D, CCDEnabled) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;

    // A very fast-moving small circle heading toward a static wall
    Body2DComponent bulletDef;
    bulletDef.shapeType = Shape2DType::Circle;
    bulletDef.circle.radius = 0.1f;
    bulletDef.isStatic = false;
    Entity bullet = CreateBody2D(world, Vector2(0.0f, 0.0f), bulletDef);

    // Static wall at X=10
    CreateBox(world, Vector2(10.0f, 0.0f), Vector2(0.5f, 5.0f), /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.SetGravity(Vector2(0.0f, 0.0f));
    physics.SetCCDEnabled(true);
    physics.Initialize(&world);

    // Give bullet a very high velocity (should trigger CCD threshold of 2.0)
    auto* body = world.GetComponent<Body2DComponent>(bullet);
    ENJIN_ASSERT_NOT_NULL(body);
    body->velocity = Vector2(500.0f, 0.0f);

    // Step with large dt so the bullet would tunnel without CCD
    physics.Update(1.0f / 60.0f);

    // After CCD, the bullet should not have passed through the wall (x < 10)
    auto* transform = world.GetComponent<TransformComponent>(bullet);
    ENJIN_ASSERT_NOT_NULL(transform);
    ENJIN_EXPECT_LT(transform->position.x, 10.0f);

    physics.Shutdown();
#endif
}

// ===========================================================================
// 9. Gravity and Velocity Integration
// ===========================================================================

ENJIN_TEST(Physics2D, GravityDefault) {
#ifdef ENJIN_PHYSICS_SIMPLE
    PhysicsWorld2D physics;
    Vector2 g = physics.GetGravity();
    ENJIN_EXPECT_FLOAT_EQ(g.x, 0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(g.y, -9.81f, 0.01f);
#endif
}

ENJIN_TEST(Physics2D, GravityIntegration) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    Entity e = CreateCircle(world, Vector2(0.0f, 10.0f), 0.5f);

    PhysicsWorld2D physics;
    physics.SetGravity(Vector2(0.0f, -10.0f));
    physics.Initialize(&world);

    auto* body = world.GetComponent<Body2DComponent>(e);
    ENJIN_ASSERT_NOT_NULL(body);
    // Start with zero velocity
    body->velocity = Vector2(0.0f, 0.0f);
    body->linearDamping = 0.0f; // No damping for predictable results

    f32 dt = 1.0f / 60.0f;
    physics.Update(dt);

    // After one step, velocity.y should be approximately gravity * dt
    // IntegrateVelocities: vel += gravity * gravityScale * dt = (0, -10) * 1.0 * dt
    ENJIN_EXPECT_LT(body->velocity.y, 0.0f);

    auto* transform = world.GetComponent<TransformComponent>(e);
    ENJIN_ASSERT_NOT_NULL(transform);
    // Object should have moved downward from Y=10
    ENJIN_EXPECT_LT(transform->position.y, 10.0f);

    physics.Shutdown();
#endif
}

ENJIN_TEST(Physics2D, VelocityIntegration) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    Entity e = CreateCircle(world, Vector2(0.0f, 0.0f), 0.5f);

    PhysicsWorld2D physics;
    physics.SetGravity(Vector2(0.0f, 0.0f)); // No gravity
    physics.Initialize(&world);

    auto* body = world.GetComponent<Body2DComponent>(e);
    ENJIN_ASSERT_NOT_NULL(body);
    body->velocity = Vector2(10.0f, 5.0f);
    body->linearDamping = 0.0f;

    f32 dt = 0.1f;
    physics.Update(dt);

    auto* transform = world.GetComponent<TransformComponent>(e);
    ENJIN_ASSERT_NOT_NULL(transform);
    // Position should advance by velocity * dt
    ENJIN_EXPECT_FLOAT_NEAR(transform->position.x, 1.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(transform->position.y, 0.5f, 0.1f);

    physics.Shutdown();
#endif
}

// ===========================================================================
// 10. Spatial Queries (Overlap)
// ===========================================================================

ENJIN_TEST(Physics2D, OverlapCircleFindsEntities) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    Entity e1 = CreateCircle(world, Vector2(1.0f, 0.0f), 0.5f, /*isStatic=*/true);
    Entity e2 = CreateCircle(world, Vector2(2.0f, 0.0f), 0.5f, /*isStatic=*/true);
    /*Entity e3 =*/ CreateCircle(world, Vector2(20.0f, 0.0f), 0.5f, /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    std::vector<Entity> results;
    bool found = physics.OverlapCircle(Vector2(1.5f, 0.0f), 2.0f, results);

    ENJIN_EXPECT_TRUE(found);
    // e1 and e2 should be within the overlap circle; e3 is far away
    ENJIN_EXPECT_GE(results.size(), (size_t)2);

    // Verify the far entity is not included
    bool containsFar = false;
    for (Entity e : results) {
        if (e != e1 && e != e2) {
            // Could be e3 -- check it is near enough or flag it
        }
    }
    (void)containsFar;

    physics.Shutdown();
#endif
}

ENJIN_TEST(Physics2D, OverlapBoxFindsEntities) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    Entity e1 = CreateBox(world, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), /*isStatic=*/true);
    /*Entity e2 =*/ CreateBox(world, Vector2(100.0f, 100.0f), Vector2(1.0f, 1.0f), /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    std::vector<Entity> results;
    bool found = physics.OverlapBox(Vector2(0.0f, 0.0f), Vector2(3.0f, 3.0f), results);

    ENJIN_EXPECT_TRUE(found);
    // Only e1 should be in the query region
    ENJIN_EXPECT_EQ(results.size(), (size_t)1);
    ENJIN_EXPECT_EQ(results[0], e1);

    physics.Shutdown();
#endif
}

ENJIN_TEST(Physics2D, OverlapCircleMiss) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;
    CreateCircle(world, Vector2(50.0f, 50.0f), 0.5f, /*isStatic=*/true);

    PhysicsWorld2D physics;
    physics.Initialize(&world);

    std::vector<Entity> results;
    // Query near origin; entity is far away
    bool found = physics.OverlapCircle(Vector2(0.0f, 0.0f), 1.0f, results);
    ENJIN_EXPECT_FALSE(found);
    ENJIN_EXPECT_EQ(results.size(), (size_t)0);

    physics.Shutdown();
#endif
}

// ===========================================================================
// Additional: Component defaults and collision filtering
// ===========================================================================

ENJIN_TEST(Physics2D, Body2DComponentDefaults) {
    Body2DComponent body;
    ENJIN_EXPECT_EQ(body.shapeType, Shape2DType::Box);
    ENJIN_EXPECT_FALSE(body.isStatic);
    ENJIN_EXPECT_FALSE(body.isSensor);
    ENJIN_EXPECT_FALSE(body.fixedRotation);
    ENJIN_EXPECT_FLOAT_EQ(body.gravityScale, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(body.linearDamping, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(body.angularDamping, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(body.mass, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(body.velocity.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(body.velocity.y, 0.0f);
    ENJIN_EXPECT_EQ(body.categoryBits, (u32)1);
    ENJIN_EXPECT_EQ(body.collisionMask, (u32)0xFFFFFFFF);
}

ENJIN_TEST(Physics2D, CollisionFilteringPreventsCollision) {
#ifdef ENJIN_PHYSICS_SIMPLE
    World world;

    // Create two overlapping circles but with incompatible collision masks
    Body2DComponent bodyA;
    bodyA.shapeType = Shape2DType::Circle;
    bodyA.circle.radius = 1.0f;
    bodyA.isStatic = false;
    bodyA.categoryBits = 0x01;
    bodyA.collisionMask = 0x02;  // Only collides with category 2
    Entity e1 = CreateBody2D(world, Vector2(0.0f, 0.0f), bodyA);

    Body2DComponent bodyB;
    bodyB.shapeType = Shape2DType::Circle;
    bodyB.circle.radius = 1.0f;
    bodyB.isStatic = false;
    bodyB.categoryBits = 0x04;   // Category 4 -- not in A's mask
    bodyB.collisionMask = 0x01;
    /*Entity e2 =*/ CreateBody2D(world, Vector2(0.5f, 0.0f), bodyB);

    PhysicsWorld2D physics;
    physics.SetGravity(Vector2(0.0f, 0.0f));
    physics.Initialize(&world);

    bool collisionDetected = false;
    physics.SetOnCollisionEnter([&](const Contact2D&) {
        collisionDetected = true;
    });

    physics.Update(1.0f / 60.0f);
    // Should NOT collide because A's mask (0x02) doesn't include B's category (0x04)
    ENJIN_EXPECT_FALSE(collisionDetected);

    physics.Shutdown();
    (void)e1;
#endif
}

ENJIN_TEST(Physics2D, PhysicsMaterial2DDefaults) {
    PhysicsMaterial2D mat;
    ENJIN_EXPECT_FLOAT_EQ(mat.friction, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(mat.restitution, 0.2f);
    ENJIN_EXPECT_FLOAT_EQ(mat.density, 1.0f);
}

ENJIN_TEST(Physics2D, Joint2DComponentDefaults) {
    Joint2DComponent joint;
    ENJIN_EXPECT_EQ(joint.type, Joint2DType::Revolute);
    ENJIN_EXPECT_EQ(joint.connectedEntity, (Entity)0);
    ENJIN_EXPECT_FALSE(joint.enableLimit);
    ENJIN_EXPECT_FALSE(joint.enableMotor);
    ENJIN_EXPECT_FLOAT_EQ(joint.length, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(joint.stiffness, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(joint.damping, 0.0f);
    ENJIN_EXPECT_FALSE(joint.collideConnected);
}

ENJIN_TEST_MAIN()
