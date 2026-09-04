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

ENJIN_TEST_MAIN()
