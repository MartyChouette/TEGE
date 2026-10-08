// Physics bodies for PARENTED entities must be placed in world space.
//
// A TransformComponent holds a LOCAL transform. JoltBackend used to create and
// sync bodies straight from transform->position, so a parented entity got its
// collider wherever its offset from the parent happened to land near the origin
// rather than where the object visibly is. The Playground door leaf sits at
// local (1.25, 0, 0) under a pivot at (7.25, 1.5, 16): its collider was ~8 units
// from its own doorway, so you walked through the door and hit nothing, then
// bumped into an invisible slab somewhere else.
//
// These tests pin the placement, and the round trip back into local space that a
// parented DYNAMIC body needs when the solver moves it.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include <algorithm>
#include <vector>
#include <cmath>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

// Parent at a real offset with a real rotation, so a local-vs-world mix-up
// cannot pass by coincidence.
struct Rig {
    Entity parent = INVALID_ENTITY;
    Entity child = INVALID_ENTITY;
};

Rig MakeRig(World& w, const Vector3& parentPos, const Quaternion& parentRot,
            const Vector3& childLocal) {
    Rig r;
    r.parent = w.CreateEntity();
    TransformComponent pt;
    pt.position = parentPos;
    pt.rotation = parentRot;
    w.AddComponent<TransformComponent>(r.parent, pt);

    r.child = w.CreateEntity();
    TransformComponent ct;
    ct.position = childLocal;
    w.AddComponent<TransformComponent>(r.child, ct);
    w.AddComponent<ParentComponent>(r.child, ParentComponent{r.parent});
    return r;
}

bool Near(f32 a, f32 b, f32 eps = 0.001f) { return std::fabs(a - b) < eps; }

} // namespace

ENJIN_TEST(ParentedBodies, WorldTransformIsNotTheLocalOne) {
    // Arrange: exactly the Playground door setup.
    World w;
    Rig rig = MakeRig(w, Vector3(7.25f, 1.5f, 16.0f), Quaternion(), Vector3(1.25f, 0.0f, 0.0f));

    // Act
    Vector3 pos; Quaternion rot;
    GetWorldTransform(&w, rig.child, pos, rot);

    // Assert: the door leaf is at 8.5, not at 1.25.
    ENJIN_EXPECT_TRUE(Near(pos.x, 8.5f));
    ENJIN_EXPECT_TRUE(Near(pos.y, 1.5f));
    ENJIN_EXPECT_TRUE(Near(pos.z, 16.0f));
}

ENJIN_TEST(ParentedBodies, RotatedParentCarriesTheChildAround) {
    // A 90 degree yaw on the parent must swing the child's offset with it,
    // which a plain position add would miss.
    World w;
    Rig rig = MakeRig(w, Vector3(0.0f, 0.0f, 0.0f),
                      Quaternion::FromEulerDegrees(Vector3(0.0f, 90.0f, 0.0f)),
                      Vector3(2.0f, 0.0f, 0.0f));

    Vector3 pos; Quaternion rot;
    GetWorldTransform(&w, rig.child, pos, rot);

    // +X rotated 90 degrees about +Y lands on -Z.
    ENJIN_EXPECT_TRUE(Near(pos.x, 0.0f, 0.01f));
    ENJIN_EXPECT_TRUE(Near(pos.z, -2.0f, 0.01f));
}

ENJIN_TEST(ParentedBodies, RootEntityWorldEqualsLocal) {
    // An unparented entity must not be disturbed by any of this.
    World w;
    Entity e = w.CreateEntity();
    TransformComponent t;
    t.position = Vector3(3.0f, 4.0f, 5.0f);
    w.AddComponent<TransformComponent>(e, t);

    Vector3 pos; Quaternion rot;
    GetWorldTransform(&w, e, pos, rot);

    ENJIN_EXPECT_TRUE(Near(pos.x, 3.0f));
    ENJIN_EXPECT_TRUE(Near(pos.y, 4.0f));
    ENJIN_EXPECT_TRUE(Near(pos.z, 5.0f));
}

ENJIN_TEST(ParentedBodies, WorldToLocalRoundTrips) {
    // The write-back path: a solver result expressed in world space must come
    // back as the same local transform we started from, or a parented dynamic
    // body jumps by the parent's offset every frame.
    World w;
    Rig rig = MakeRig(w, Vector3(7.25f, 1.5f, 16.0f),
                      Quaternion::FromEulerDegrees(Vector3(0.0f, 35.0f, 0.0f)),
                      Vector3(1.25f, 0.5f, -0.75f));

    Vector3 worldPos; Quaternion worldRot;
    GetWorldTransform(&w, rig.child, worldPos, worldRot);

    Vector3 localPos; Quaternion localRot;
    WorldToLocalTransform(&w, rig.child, worldPos, worldRot, localPos, localRot);

    ENJIN_EXPECT_TRUE(Near(localPos.x, 1.25f, 0.01f));
    ENJIN_EXPECT_TRUE(Near(localPos.y, 0.5f, 0.01f));
    ENJIN_EXPECT_TRUE(Near(localPos.z, -0.75f, 0.01f));
}

ENJIN_TEST(ParentedBodies, WorldToLocalOnARootIsIdentity) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e, TransformComponent{});

    Vector3 localPos; Quaternion localRot;
    WorldToLocalTransform(&w, e, Vector3(9.0f, -2.0f, 4.0f), Quaternion(), localPos, localRot);

    ENJIN_EXPECT_TRUE(Near(localPos.x, 9.0f));
    ENJIN_EXPECT_TRUE(Near(localPos.y, -2.0f));
    ENJIN_EXPECT_TRUE(Near(localPos.z, 4.0f));
}

// The editor drew collider wireframes at `transform->position + center`. On a
// child that is the offset from its parent, so the box stayed near the origin
// while the model (and the real body) went wherever the parent was moved.
// Reported on a mesh at local 0,0,0 under a parent at z = -7.34, scale 2.
ENJIN_TEST(ParentedBodies, ColliderCenterFollowsTheParent) {
    World w;
    Rig rig = MakeRig(w, Vector3(0.0f, 0.0f, -7.34f), Quaternion(), Vector3(0.0f, 0.0f, 0.0f));
    w.GetComponent<TransformComponent>(rig.parent)->scale = Vector3(2.0f, 2.0f, 2.0f);
    w.GetComponent<TransformComponent>(rig.child)->scale = Vector3(0.25f, 0.25f, 0.25f);

    const Vector3 localCenter(-0.15f, 2.36f, -0.06f);
    const Vector3 c = ColliderWorldCenter(&w, rig.child, localCenter);

    // The parent's position is in; the offset is NOT scaled (collider sizes and
    // offsets are world units, which is how the body is built).
    ENJIN_EXPECT_TRUE(Near(c.x, -0.15f));
    ENJIN_EXPECT_TRUE(Near(c.y, 2.36f));
    ENJIN_EXPECT_TRUE(Near(c.z, -7.40f));
}

// The offset turns with the entity's WORLD rotation, which on a child is the
// parent's rotation too.
ENJIN_TEST(ParentedBodies, ColliderCenterTurnsWithTheParent) {
    World w;
    const Quaternion quarterYaw = Quaternion::FromEuler(Vector3(0.0f, 1.5707963f, 0.0f));
    Rig rig = MakeRig(w, Vector3(10.0f, 0.0f, 0.0f), quarterYaw, Vector3(0.0f, 0.0f, 0.0f));

    Quaternion rot;
    const Vector3 c = ColliderWorldCenter(&w, rig.child, Vector3(1.0f, 0.0f, 0.0f), &rot);
    const Vector3 expected = Vector3(10.0f, 0.0f, 0.0f) + quarterYaw.Rotate(Vector3(1.0f, 0.0f, 0.0f));

    ENJIN_EXPECT_TRUE(Near(c.x, expected.x));
    ENJIN_EXPECT_TRUE(Near(c.y, expected.y));
    ENJIN_EXPECT_TRUE(Near(c.z, expected.z));
    // Not where the old sum put it
    ENJIN_EXPECT_FALSE(Near(c.x, 1.0f));
}

// The spatial queries scripts call (Physics_OverlapBox, the overlap-sphere
// pair, the visual script nodes) read `transform->position`, which on a child
// is its offset from the parent. A collider under a moved parent was found at
// the origin and missed where it actually was.
namespace {

bool Has(const std::vector<Entity>& list, Entity e) {
    return std::find(list.begin(), list.end(), e) != list.end();
}

// A 1-unit box collider on a child at local 0,0,0, under a parent at x = 20.
Rig MakeParentedBox(World& w) {
    Rig rig = MakeRig(w, Vector3(20.0f, 0.0f, 0.0f), Quaternion(), Vector3(0.0f, 0.0f, 0.0f));
    BoxColliderComponent box;
    box.size = Vector3(1.0f, 1.0f, 1.0f);
    w.AddComponent<BoxColliderComponent>(rig.child, box);
    return rig;
}

} // namespace

ENJIN_TEST(ParentedBodies, OverlapBoxFindsAChildWhereItIs) {
    // Arrange
    World w;
    Rig rig = MakeParentedBox(w);
    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) ENJIN_SKIP("no Jolt in this build");
    physics->SetWorld(&w);
    physics->Update(1.0f / 60.0f);

    // Act
    const auto atParent = physics->OverlapBox(Vector3(20.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));
    const auto atOrigin = physics->OverlapBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));

    // Assert
    ENJIN_EXPECT_TRUE(Has(atParent, rig.child));
    ENJIN_EXPECT_FALSE(Has(atOrigin, rig.child));
}

ENJIN_TEST(ParentedBodies, OverlapSphereFindsAChildWhereItIs) {
    // Arrange
    World w;
    Rig rig = MakeParentedBox(w);
    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) ENJIN_SKIP("no Jolt in this build");
    physics->SetWorld(&w);
    physics->Update(1.0f / 60.0f);

    // Act
    const auto atParent = physics->GetCollidersInRadius(Vector3(20.0f, 0.0f, 0.0f), 1.0f);
    const auto atOrigin = physics->GetCollidersInRadius(Vector3(0.0f, 0.0f, 0.0f), 1.0f);

    // Assert
    ENJIN_EXPECT_TRUE(Has(atParent, rig.child));
    ENJIN_EXPECT_FALSE(Has(atOrigin, rig.child));
}

// An overlap is against the collider, not the entity's origin: a wide floor is
// found by a query that touches its edge, far from its centre.
ENJIN_TEST(ParentedBodies, OverlapQueriesTestTheColliderNotTheOrigin) {
    // Arrange: a 50-unit floor centred on the origin
    World w;
    const Entity floor = w.CreateEntity();
    w.AddComponent<TransformComponent>(floor, TransformComponent{});
    BoxColliderComponent box;
    box.size = Vector3(50.0f, 0.1f, 50.0f);
    w.AddComponent<BoxColliderComponent>(floor, box);
    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) ENJIN_SKIP("no Jolt in this build");
    physics->SetWorld(&w);
    physics->Update(1.0f / 60.0f);

    // Act: 20 units from the floor's centre, still over the floor
    const auto box20 = physics->OverlapBox(Vector3(20.0f, 0.0f, 20.0f), Vector3(0.5f, 0.5f, 0.5f));
    const auto sphere20 = physics->GetCollidersInRadius(Vector3(20.0f, 0.0f, 20.0f), 0.5f);
    // and well off its edge
    const auto box40 = physics->OverlapBox(Vector3(40.0f, 0.0f, 0.0f), Vector3(0.5f, 0.5f, 0.5f));
    const auto sphere40 = physics->GetCollidersInRadius(Vector3(40.0f, 0.0f, 0.0f), 0.5f);

    // Assert
    ENJIN_EXPECT_TRUE(Has(box20, floor));
    ENJIN_EXPECT_TRUE(Has(sphere20, floor));
    ENJIN_EXPECT_FALSE(Has(box40, floor));
    ENJIN_EXPECT_FALSE(Has(sphere40, floor));
}

// MoveAndSlide is stopped by a child's collider where the child is.
ENJIN_TEST(ParentedBodies, MoveAndSlideIsBlockedByAChildWhereItIs) {
    // Arrange
    World w;
    MakeParentedBox(w);
    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) ENJIN_SKIP("no Jolt in this build");
    physics->SetWorld(&w);
    physics->Update(1.0f / 60.0f);
    const Physics::AABB mover = Physics::AABB::FromCenterSize(Vector3(0.0f, 0.0f, 0.0f),
                                                              Vector3(1.0f, 1.0f, 1.0f));

    // Act: one step straight into the box at x = 20, and one through the origin
    const Vector3 intoBox = physics->MoveAndSlide(Vector3(18.5f, 0.0f, 0.0f),
                                                  Vector3(1.0f, 0.0f, 0.0f), mover, 1.0f, 0xFFFFFFFFu);
    const Vector3 pastOrigin = physics->MoveAndSlide(Vector3(-1.5f, 0.0f, 0.0f),
                                                     Vector3(1.0f, 0.0f, 0.0f), mover, 1.0f, 0xFFFFFFFFu);

    // Assert: pushed back out of the box (faces meet at x = 19.5, so the mover's
    // centre stops at 19.0); nothing is in the way at the origin
    ENJIN_EXPECT_TRUE(Near(intoBox.x, 19.0f, 0.05f));
    ENJIN_EXPECT_TRUE(Near(pastOrigin.x, -0.5f));
}

ENJIN_TEST_MAIN()
