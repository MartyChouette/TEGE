#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Physics/PhysicsTypes2D.h"

// Physics2D tests: ECS component tests run directly, simulation tests
// require a physics backend (Box2D via IPhysicsBackend2D).
// Legacy SimplePhysics tests removed — backend was retired.

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

// ===========================================================================
// 2. Polygon Shape Creation with Vertex Count Limits
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
// 3. Joint Creation (Distance, Revolute)
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
// 4. Component defaults and collision filtering
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
