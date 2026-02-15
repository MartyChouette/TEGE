#include "EnjinTest.h"
#include "Enjin/Physics/PhysicsTypes.h"

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::Physics;

// ===========================================================================
// AABB
// ===========================================================================

ENJIN_TEST(AABB, DefaultConstruction) {
    AABB box;
    ENJIN_EXPECT_VEC3_EQ(box.min, 0.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_VEC3_EQ(box.max, 0.0f, 0.0f, 0.0f);
}

ENJIN_TEST(AABB, ExplicitConstruction) {
    AABB box(Vector3(-1.0f, -2.0f, -3.0f), Vector3(1.0f, 2.0f, 3.0f));
    ENJIN_EXPECT_VEC3_EQ(box.min, -1.0f, -2.0f, -3.0f);
    ENJIN_EXPECT_VEC3_EQ(box.max, 1.0f, 2.0f, 3.0f);
}

ENJIN_TEST(AABB, FromCenterSize) {
    AABB box = AABB::FromCenterSize(Vector3(0.0f, 0.0f, 0.0f), Vector3(4.0f, 6.0f, 8.0f));
    ENJIN_EXPECT_VEC3_EQ(box.min, -2.0f, -3.0f, -4.0f);
    ENJIN_EXPECT_VEC3_EQ(box.max, 2.0f, 3.0f, 4.0f);
}

ENJIN_TEST(AABB, GetCenterAndSize) {
    AABB box(Vector3(-2.0f, -3.0f, -4.0f), Vector3(2.0f, 3.0f, 4.0f));
    Vector3 center = box.GetCenter();
    Vector3 size = box.GetSize();
    Vector3 half = box.GetHalfSize();
    ENJIN_EXPECT_VEC3_EQ(center, 0.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_VEC3_EQ(size, 4.0f, 6.0f, 8.0f);
    ENJIN_EXPECT_VEC3_EQ(half, 2.0f, 3.0f, 4.0f);
}

ENJIN_TEST(AABB, ContainsCenter) {
    AABB box(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
    ENJIN_EXPECT_TRUE(box.Contains(Vector3(0.0f, 0.0f, 0.0f)));
}

ENJIN_TEST(AABB, ContainsCorner) {
    AABB box(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
    ENJIN_EXPECT_TRUE(box.Contains(Vector3(1.0f, 1.0f, 1.0f)));
}

ENJIN_TEST(AABB, DoesNotContainOutside) {
    AABB box(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
    ENJIN_EXPECT_FALSE(box.Contains(Vector3(2.0f, 0.0f, 0.0f)));
}

ENJIN_TEST(AABB, IntersectsOverlap) {
    AABB a(Vector3(-1.0f, -1.0f, -1.0f), Vector3(1.0f, 1.0f, 1.0f));
    AABB b(Vector3(0.0f, 0.0f, 0.0f), Vector3(2.0f, 2.0f, 2.0f));
    ENJIN_EXPECT_TRUE(a.Intersects(b));
    ENJIN_EXPECT_TRUE(b.Intersects(a));
}

ENJIN_TEST(AABB, DoesNotIntersectSeparate) {
    AABB a(Vector3(-1.0f, -1.0f, -1.0f), Vector3(0.0f, 0.0f, 0.0f));
    AABB b(Vector3(1.0f, 1.0f, 1.0f), Vector3(2.0f, 2.0f, 2.0f));
    ENJIN_EXPECT_FALSE(a.Intersects(b));
}

// ===========================================================================
// Ray
// ===========================================================================

ENJIN_TEST(Ray, GetPoint) {
    Ray ray;
    ray.origin = Vector3(0.0f, 0.0f, 0.0f);
    ray.direction = Vector3(0.0f, 0.0f, 1.0f);
    Vector3 p = ray.GetPoint(5.0f);
    ENJIN_EXPECT_VEC3_EQ(p, 0.0f, 0.0f, 5.0f);
}

// ===========================================================================
// ColliderInfo Defaults
// ===========================================================================

ENJIN_TEST(ColliderInfo, DefaultValues) {
    ColliderInfo info;
    ENJIN_EXPECT_EQ(info.categoryBits, (u32)1);
    ENJIN_EXPECT_EQ(info.collisionMask, (u32)0xFFFFFFFF);
    ENJIN_EXPECT_FALSE(info.isTrigger);
    ENJIN_EXPECT_FALSE(info.hasCollider);
}

// ===========================================================================
// CollisionResult Defaults
// ===========================================================================

ENJIN_TEST(CollisionResult, DefaultValues) {
    CollisionResult result;
    ENJIN_EXPECT_FALSE(result.hit);
    ENJIN_EXPECT_FLOAT_EQ(result.penetration, 0.0f);
}

ENJIN_TEST_MAIN()
