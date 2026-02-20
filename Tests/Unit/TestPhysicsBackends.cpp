#include "EnjinTest.h"
#include "Enjin/Physics/PhysicsTypes.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Physics;

// ===========================================================================
// PhysicsBackendType Enum
// ===========================================================================

ENJIN_TEST(BackendType, Values) {
    ENJIN_EXPECT_EQ((int)PhysicsBackendType::Auto, 0);
    ENJIN_EXPECT_EQ((int)PhysicsBackendType::Jolt, 1);
    ENJIN_EXPECT_EQ((int)PhysicsBackendType::Box2D, 2);
}

// ===========================================================================
// Factory Availability
// ===========================================================================

ENJIN_TEST(Factory, JoltAvailable) {
    // Jolt should be compiled in (ENJIN_PHYSICS_JOLT=ON by default)
    ENJIN_EXPECT_TRUE(IsJoltAvailable());
}

ENJIN_TEST(Factory, Box2DAvailable) {
    // Box2D should be compiled in (ENJIN_PHYSICS_BOX2D=ON by default)
    ENJIN_EXPECT_TRUE(IsBox2DAvailable());
}

ENJIN_TEST(Factory, ResolveBackendName3D) {
    const char* name = ResolveBackendName(PhysicsBackendType::Auto, Scene::ProjectMode::Mode3D);
    ENJIN_EXPECT_NOT_NULL(name);
    // Auto for 3D should resolve to Jolt
    ENJIN_EXPECT_GT(strlen(name), (size_t)0);
}

ENJIN_TEST(Factory, ResolveBackendName2D) {
    const char* name = ResolveBackendName(PhysicsBackendType::Auto, Scene::ProjectMode::Mode2D);
    ENJIN_EXPECT_NOT_NULL(name);
    ENJIN_EXPECT_GT(strlen(name), (size_t)0);
}

ENJIN_TEST(Factory, ResolveExplicitNames) {
    const char* jolt = ResolveBackendName(PhysicsBackendType::Jolt, Scene::ProjectMode::Mode3D);
    const char* box2d = ResolveBackendName(PhysicsBackendType::Box2D, Scene::ProjectMode::Mode2D);
    ENJIN_EXPECT_NOT_NULL(jolt);
    ENJIN_EXPECT_NOT_NULL(box2d);
}

// ===========================================================================
// AABB
// ===========================================================================

ENJIN_TEST(AABB, FromCenterSize) {
    AABB box = AABB::FromCenterSize(Math::Vector3(5, 5, 5), Math::Vector3(2, 2, 2));
    ENJIN_EXPECT_FLOAT_EQ(box.min.x, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(box.max.x, 6.0f);
    Math::Vector3 center = box.GetCenter();
    ENJIN_EXPECT_FLOAT_EQ(center.x, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(center.y, 5.0f);
}

ENJIN_TEST(AABB, ContainsPoint) {
    AABB box(Math::Vector3(0, 0, 0), Math::Vector3(10, 10, 10));
    ENJIN_EXPECT_TRUE(box.Contains(Math::Vector3(5, 5, 5)));
    ENJIN_EXPECT_FALSE(box.Contains(Math::Vector3(11, 5, 5)));
}

ENJIN_TEST(AABB, Intersects) {
    AABB a(Math::Vector3(0, 0, 0), Math::Vector3(5, 5, 5));
    AABB b(Math::Vector3(3, 3, 3), Math::Vector3(8, 8, 8));
    AABB c(Math::Vector3(6, 6, 6), Math::Vector3(10, 10, 10));
    ENJIN_EXPECT_TRUE(a.Intersects(b));
    ENJIN_EXPECT_FALSE(a.Intersects(c));
}

ENJIN_TEST(AABB, SizeAndHalfSize) {
    AABB box(Math::Vector3(0, 0, 0), Math::Vector3(10, 6, 4));
    Math::Vector3 size = box.GetSize();
    ENJIN_EXPECT_FLOAT_EQ(size.x, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(size.y, 6.0f);
    Math::Vector3 half = box.GetHalfSize();
    ENJIN_EXPECT_FLOAT_EQ(half.x, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(half.z, 2.0f);
}

// ===========================================================================
// Ray
// ===========================================================================

ENJIN_TEST(Ray, GetPoint) {
    Ray ray;
    ray.origin = Math::Vector3(0, 0, 0);
    ray.direction = Math::Vector3(1, 0, 0);
    Math::Vector3 p = ray.GetPoint(5.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.x, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.y, 0.0f);
}

// ===========================================================================
// RaycastHit Defaults
// ===========================================================================

ENJIN_TEST(RaycastHit, Defaults) {
    RaycastHit hit;
    ENJIN_EXPECT_FALSE(hit.hit);
    ENJIN_EXPECT_FLOAT_EQ(hit.distance, 0.0f);
    ENJIN_EXPECT_EQ(hit.entity, (ECS::Entity)0);
}

// ===========================================================================
// CollisionResult & CollisionEvent Defaults
// ===========================================================================

ENJIN_TEST(Collision, ResultDefaults) {
    CollisionResult r;
    ENJIN_EXPECT_FALSE(r.hit);
    ENJIN_EXPECT_FLOAT_EQ(r.penetration, 0.0f);
}

ENJIN_TEST(Collision, EventDefaults) {
    CollisionEvent e;
    ENJIN_EXPECT_EQ(e.entityA, (ECS::Entity)0);
    ENJIN_EXPECT_EQ(e.entityB, (ECS::Entity)0);
    ENJIN_EXPECT_EQ((int)e.type, (int)CollisionEvent::Type::Enter);
    ENJIN_EXPECT_FALSE(e.isTrigger);
}

// ===========================================================================
// ColliderInfo Defaults
// ===========================================================================

ENJIN_TEST(ColliderInfo, Defaults) {
    ColliderInfo info;
    ENJIN_EXPECT_EQ(info.categoryBits, 1u);
    ENJIN_EXPECT_EQ(info.collisionMask, 0xFFFFFFFF);
    ENJIN_EXPECT_FALSE(info.isTrigger);
    ENJIN_EXPECT_FALSE(info.hasCollider);
}

// ===========================================================================
// 2D Physics Types
// ===========================================================================

ENJIN_TEST(Shape2D, TypeEnum) {
    ENJIN_EXPECT_EQ((int)Shape2DType::Circle, 0);
    ENJIN_EXPECT_EQ((int)Shape2DType::Box, 1);
    ENJIN_EXPECT_EQ((int)Shape2DType::Polygon, 2);
    ENJIN_EXPECT_EQ((int)Shape2DType::Capsule, 3);
}

ENJIN_TEST(Shape2D, CircleDefaults) {
    CircleShape2D c;
    ENJIN_EXPECT_FLOAT_EQ(c.radius, 0.5f);
}

ENJIN_TEST(Shape2D, BoxDefaults) {
    BoxShape2D b;
    ENJIN_EXPECT_FLOAT_EQ(b.halfExtents.x, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(b.halfExtents.y, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(b.rotation, 0.0f);
}

ENJIN_TEST(Shape2D, CapsuleDefaults) {
    CapsuleShape2D c;
    ENJIN_EXPECT_FLOAT_EQ(c.radius, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(c.height, 1.0f);
}

ENJIN_TEST(Material2D, Defaults) {
    PhysicsMaterial2D m;
    ENJIN_EXPECT_FLOAT_NEAR(m.friction, 0.3f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(m.restitution, 0.2f, 0.01f);
    ENJIN_EXPECT_FLOAT_EQ(m.density, 1.0f);
}

ENJIN_TEST(Body2D, Defaults) {
    Body2DComponent b;
    ENJIN_EXPECT_EQ((int)b.shapeType, (int)Shape2DType::Box);
    ENJIN_EXPECT_FALSE(b.isStatic);
    ENJIN_EXPECT_FALSE(b.isSensor);
    ENJIN_EXPECT_FALSE(b.fixedRotation);
    ENJIN_EXPECT_FLOAT_EQ(b.gravityScale, 1.0f);
    ENJIN_EXPECT_EQ(b.categoryBits, 1u);
    ENJIN_EXPECT_EQ(b.collisionMask, 0xFFFFFFFF);
}

ENJIN_TEST(Joint2D, TypeEnum) {
    ENJIN_EXPECT_EQ((int)Joint2DType::Revolute, 0);
    ENJIN_EXPECT_EQ((int)Joint2DType::Prismatic, 1);
    ENJIN_EXPECT_EQ((int)Joint2DType::Distance, 2);
    ENJIN_EXPECT_EQ((int)Joint2DType::Rope, 3);
    ENJIN_EXPECT_EQ((int)Joint2DType::Weld, 4);
}

ENJIN_TEST(Joint2D, Defaults) {
    Joint2DComponent j;
    ENJIN_EXPECT_EQ((int)j.type, (int)Joint2DType::Revolute);
    ENJIN_EXPECT_EQ(j.connectedEntity, (ECS::Entity)0);
    ENJIN_EXPECT_FALSE(j.enableLimit);
    ENJIN_EXPECT_FALSE(j.enableMotor);
    ENJIN_EXPECT_FLOAT_EQ(j.length, 1.0f);
    ENJIN_EXPECT_FALSE(j.collideConnected);
}

ENJIN_TEST(Contact2D, Defaults) {
    Contact2D c;
    ENJIN_EXPECT_FLOAT_EQ(c.penetration, 0.0f);
}

ENJIN_TEST_MAIN()
