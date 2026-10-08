// A thing parented under another thing is where its parent put it.
//
// `transform->position` is an offset from the parent. Until 2026-10-07 nearly
// every system that places something in the world read that field directly:
// lights, audio sources, trigger / gravity / weather / camera zones, spawn
// points, vegetation volumes, 2D bodies and several cameras all stayed at their
// local offset, near the origin, while the parent moved. Meshes and 3D bodies
// followed, so the model went one way and everything attached to it stayed.
//
// ECS::WorldPosition / WorldRotation are the fix. These tests pin the helpers
// and then one consumer of each kind that can run without a GPU.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/CameraTrigger.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/CameraMath.h"
#include "Enjin/ECS/CameraZones.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include "Enjin/Renderer/Camera.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

bool Near(f32 a, f32 b, f32 eps = 0.001f) { return std::fabs(a - b) < eps; }

Entity MakeAt(World& w, const Vector3& pos, Entity parent = INVALID_ENTITY) {
    const Entity e = w.CreateEntity();
    TransformComponent t;
    t.position = pos;
    w.AddComponent<TransformComponent>(e, t);
    if (parent != INVALID_ENTITY) w.AddComponent<ParentComponent>(e, ParentComponent{parent});
    return e;
}

} // namespace

ENJIN_TEST(WorldPlacement, ARootEntityReturnsItsOwnFieldsExactly) {
    // Arrange: values that would not survive a matrix round trip unchanged
    World w;
    const Entity e = MakeAt(w, Vector3(0.1f, 123.456f, -7.336276f));
    auto* t = w.GetComponent<TransformComponent>(e);
    t->rotation = Quaternion::FromEuler(Vector3(0.3f, 1.1f, -0.7f));

    // Act
    const Vector3 p = WorldPosition(&w, e, *t);
    const Quaternion r = WorldRotation(&w, e, *t);

    // Assert: bit for bit, so an unparented scene renders exactly as before
    ENJIN_EXPECT_TRUE(p.x == t->position.x && p.y == t->position.y && p.z == t->position.z);
    ENJIN_EXPECT_TRUE(r.x == t->rotation.x && r.y == t->rotation.y &&
                      r.z == t->rotation.z && r.w == t->rotation.w);
}

ENJIN_TEST(WorldPlacement, AChildIsWhereItsParentPutIt) {
    // Arrange: the reported scene. Child at local 0,0,0, parent moved and scaled.
    World w;
    const Entity parent = MakeAt(w, Vector3(0.0f, 0.0f, -7.34f));
    w.GetComponent<TransformComponent>(parent)->scale = Vector3(2.0f, 2.0f, 2.0f);
    const Entity child = MakeAt(w, Vector3(1.0f, 0.0f, 0.0f), parent);

    // Act
    const Vector3 p = WorldPosition(&w, child);

    // Assert: the parent's scale reaches the child's offset
    ENJIN_EXPECT_TRUE(Near(p.x, 2.0f));
    ENJIN_EXPECT_TRUE(Near(p.y, 0.0f));
    ENJIN_EXPECT_TRUE(Near(p.z, -7.34f));
}

// The cached world matrix is only reset once a frame, inside RenderSystem. A
// gameplay system asking earlier in the frame must not get last frame's parent.
ENJIN_TEST(WorldPlacement, AMovedParentIsSeenWithoutAnyCacheReset) {
    // Arrange: warm the cache where the parent used to be
    World w;
    const Entity parent = MakeAt(w, Vector3(5.0f, 0.0f, 0.0f));
    const Entity child = MakeAt(w, Vector3(1.0f, 0.0f, 0.0f), parent);
    (void)ComputeWorldMatrix(&w, child);

    // Act: move the parent and touch no dirty flag
    w.GetComponent<TransformComponent>(parent)->position = Vector3(50.0f, 0.0f, 0.0f);
    const Vector3 p = WorldPosition(&w, child);

    // Assert
    ENJIN_EXPECT_TRUE(Near(p.x, 51.0f));
}

ENJIN_TEST(WorldPlacement, AChildFacesTheWayItsParentTurnsIt) {
    // Arrange: parent yawed a quarter turn, child with no rotation of its own
    World w;
    const Entity parent = MakeAt(w, Vector3(0.0f, 0.0f, 0.0f));
    const Quaternion quarterYaw = Quaternion::FromEuler(Vector3(0.0f, 1.5707963f, 0.0f));
    w.GetComponent<TransformComponent>(parent)->rotation = quarterYaw;
    const Entity child = MakeAt(w, Vector3(0.0f, 0.0f, 0.0f), parent);

    // Act
    const Vector3 forward = WorldRotation(&w, child).GetForward();
    const Vector3 expected = quarterYaw.GetForward();

    // Assert: this is what aims a parented spot light or directional light
    ENJIN_EXPECT_TRUE(Near(forward.x, expected.x));
    ENJIN_EXPECT_TRUE(Near(forward.y, expected.y));
    ENJIN_EXPECT_TRUE(Near(forward.z, expected.z));
    ENJIN_EXPECT_FALSE(Near(forward.z, -1.0f));   // not the child's own, unrotated forward
}

ENJIN_TEST(WorldPlacement, ACameraUnderARigRendersFromTheRig) {
    // Arrange
    World w;
    const Entity rig = MakeAt(w, Vector3(10.0f, 2.0f, 30.0f));
    const Entity cam = MakeAt(w, Vector3(0.0f, 1.0f, 0.0f), rig);
    w.AddComponent<CameraComponent>(cam, CameraComponent{});

    // Act
    Renderer::Camera out;
    ENJIN_ASSERT_TRUE(BuildCameraFromEntity(&w, cam, 16.0f / 9.0f, out));

    // Assert
    const Vector3 p = out.GetPosition();
    ENJIN_EXPECT_TRUE(Near(p.x, 10.0f));
    ENJIN_EXPECT_TRUE(Near(p.y, 3.0f));
    ENJIN_EXPECT_TRUE(Near(p.z, 30.0f));
}

ENJIN_TEST(WorldPlacement, ACameraZoneUnderAParentIsEnteredWhereItIs) {
    // Arrange: a 2-unit zone parented under a room at x = 40
    World w;
    const Entity room = MakeAt(w, Vector3(40.0f, 0.0f, 0.0f));
    const Entity zone = MakeAt(w, Vector3(0.0f, 0.0f, 0.0f), room);
    const Entity zoneCam = MakeAt(w, Vector3(0.0f, 5.0f, 0.0f));
    w.AddComponent<CameraComponent>(zoneCam, CameraComponent{});
    CameraTriggerComponent trig;
    trig.halfExtents = Vector3(2.0f, 2.0f, 2.0f);
    trig.targetCamera = zoneCam;
    w.AddComponent<CameraTriggerComponent>(zone, trig);
    const Entity player = MakeAt(w, Vector3(40.0f, 0.0f, 0.0f));

    // Act + Assert: in the room, the zone's camera wins
    ENJIN_EXPECT_TRUE(ResolveCameraZone(&w, player) == zoneCam);

    // Act + Assert: at the origin, where the zone's LOCAL position is, nothing
    w.GetComponent<TransformComponent>(player)->position = Vector3(0.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_TRUE(ResolveCameraZone(&w, player) == INVALID_ENTITY);
}

ENJIN_TEST(WorldPlacement, A2DBodyUnderAParentIsCreatedWhereItIs) {
    // Arrange: a static platform at local 0,0 under a parent at x = 20
    World w;
    const Entity parent = MakeAt(w, Vector3(20.0f, 0.0f, 0.0f));
    const Entity platform = MakeAt(w, Vector3(0.0f, 0.0f, 0.0f), parent);
    Physics::Body2DComponent b;
    b.isStatic = true;
    b.shapeType = Physics::Shape2DType::Box;
    b.box.halfExtents = Vector2(1.0f, 1.0f);
    w.AddComponent<Physics::Body2DComponent>(platform, b);

    auto backend = Physics::CreatePhysicsBackend2D(Physics::PhysicsBackendType::Auto);
    if (!backend) ENJIN_SKIP("no 2D physics backend in this build");
    backend->Initialize(&w);
    backend->Update(1.0f / 60.0f);

    // Act
    std::vector<Entity> atParent, atOrigin;
    backend->OverlapBox(Vector2(20.0f, 0.0f), Vector2(0.5f, 0.5f), atParent);
    backend->OverlapBox(Vector2(0.0f, 0.0f), Vector2(0.5f, 0.5f), atOrigin);

    // Assert
    ENJIN_EXPECT_TRUE(std::find(atParent.begin(), atParent.end(), platform) != atParent.end());
    ENJIN_EXPECT_TRUE(std::find(atOrigin.begin(), atOrigin.end(), platform) == atOrigin.end());
    backend->Shutdown();
}

// A dynamic body's result is written back into LOCAL space. Writing the body's
// world position straight into the transform moves the child by its parent's
// offset on every step.
ENJIN_TEST(WorldPlacement, AFalling2DBodyUnderAParentFallsStraightDown) {
    // Arrange
    World w;
    const Entity parent = MakeAt(w, Vector3(20.0f, 0.0f, 0.0f));
    const Entity ball = MakeAt(w, Vector3(0.0f, 10.0f, 0.0f), parent);
    Physics::Body2DComponent b;
    b.isStatic = false;
    b.shapeType = Physics::Shape2DType::Box;
    b.box.halfExtents = Vector2(0.5f, 0.5f);
    b.gravityScale = 1.0f;
    w.AddComponent<Physics::Body2DComponent>(ball, b);

    auto backend = Physics::CreatePhysicsBackend2D(Physics::PhysicsBackendType::Auto);
    if (!backend) ENJIN_SKIP("no 2D physics backend in this build");
    backend->Initialize(&w);
    backend->SetGravity(Vector2(0.0f, -9.81f));

    // Act: half a second
    for (int i = 0; i < 30; ++i) backend->Update(1.0f / 60.0f);

    // Assert: it fell, and it is still directly under where it started
    const auto* t = w.GetComponent<TransformComponent>(ball);
    const Vector3 world = WorldPosition(&w, ball);
    ENJIN_EXPECT_TRUE(world.y < 9.9f);
    ENJIN_EXPECT_TRUE(Near(world.x, 20.0f, 0.01f));
    ENJIN_EXPECT_TRUE(Near(t->position.x, 0.0f, 0.01f));   // local offset untouched
    backend->Shutdown();
}

// Trunk colliders are created at a world position and then parented under the
// volume. Left as they were, each one sat at the volume's position twice over.
ENJIN_TEST(WorldPlacement, TreeTrunkCollidersStayInsideTheirVolume) {
    // Arrange: a 10 x 10 volume centred at x = 30
    World w;
    const Entity volume = MakeAt(w, Vector3(30.0f, 0.0f, 0.0f));
    TreeVolumeComponent tree;
    tree.halfExtents = Vector3(5.0f, 0.0f, 5.0f);
    tree.density = 12;
    w.AddComponent<TreeVolumeComponent>(volume, tree);

    // Act
    Effects::TreeRenderer::GenerateColliders(&w, volume);

    // Assert: every trunk is inside the footprint, in the WORLD
    u32 trunks = 0;
    for (Entity e : w.GetEntitiesWithComponent<CapsuleColliderComponent>()) {
        const Vector3 p = WorldPosition(&w, e);
        ENJIN_EXPECT_TRUE(p.x >= 25.0f - 0.001f && p.x <= 35.0f + 0.001f);
        ENJIN_EXPECT_TRUE(p.z >= -5.0f - 0.001f && p.z <= 5.0f + 0.001f);
        ++trunks;
    }
    ENJIN_EXPECT_EQ(trunks, 12u);
}

ENJIN_TEST_MAIN()
