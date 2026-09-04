// A camera system has to move the ACTIVE CAMERA ENTITY, not just the
// Renderer::Camera.
//
// Every runtime rebuilds the render camera from the active CameraComponent
// entity's transform after gameplay has ticked -- Player/src/main.cpp,
// web_main.cpp and EditorLayer all do it. So a system that writes only the
// Renderer::Camera has its work thrown away in the same frame.
//
// CinematicSystem did exactly that: it set the Renderer::Camera, then wrote the
// CINEMATIC entity's own transform, which nothing renders from. Authored
// cutscenes ran their full timeline and fired their events while the view sat
// still, with no error anywhere. CameraDirector had already learned this and
// mirrored onto the camera entity; the two are now one function.
//
// These tests pass gameCamera = nullptr on purpose. The entity mirror is the
// half that reaches the screen, and it is the half that was missing, so testing
// it needs no renderer.
#include "EnjinTest.h"
#include "Enjin/Gameplay/CameraPose.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

namespace {

// An active camera entity, the thing every runtime rebuilds the view from.
ECS::Entity MakeActiveCamera(ECS::World& w) {
    const ECS::Entity e = w.CreateEntity();
    ECS::TransformComponent t;
    t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    w.AddComponent<ECS::TransformComponent>(e, t);
    ECS::CameraComponent c;
    c.isActive = true;
    c.fieldOfView = 60.0f;
    w.AddComponent<ECS::CameraComponent>(e, c);
    return e;
}

} // namespace

ENJIN_TEST(CameraPose, MovesTheActiveCameraEntity) {
    // Arrange
    ECS::World w;
    const ECS::Entity cam = MakeActiveCamera(w);

    // Act
    ApplyCameraPose(&w, nullptr, Math::Vector3(10.0f, 5.0f, 20.0f),
                    Math::Vector3(0.0f, 0.0f, 0.0f), 75.0f);

    // Assert: the transform is what the runtimes read.
    const auto* t = w.GetComponent<ECS::TransformComponent>(cam);
    ENJIN_ASSERT_TRUE(t != nullptr);
    ENJIN_EXPECT_FLOAT_EQ(t->position.x, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(t->position.y, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(t->position.z, 20.0f);
}

ENJIN_TEST(CameraPose, WritesFieldOfViewOntoTheCameraComponent) {
    // Arrange
    ECS::World w;
    const ECS::Entity cam = MakeActiveCamera(w);

    // Act
    ApplyCameraPose(&w, nullptr, Math::Vector3(1.0f, 2.0f, 3.0f),
                    Math::Vector3(0.0f, 0.0f, 0.0f), 33.0f);

    // Assert
    const auto* c = w.GetComponent<ECS::CameraComponent>(cam);
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_EXPECT_FLOAT_EQ(c->fieldOfView, 33.0f);
}

ENJIN_TEST(CameraPose, PointsTheCameraDownItsLocalNegativeZ) {
    // Arrange: a camera at +Z looking back at the origin, so the view direction
    // is -Z and the camera's own rotation should be near identity. A camera
    // looks down local -Z while LookRotation puts its `forward` argument on
    // local +Z, and getting that negation wrong points the shot backwards.
    ECS::World w;
    const ECS::Entity cam = MakeActiveCamera(w);

    // Act
    ApplyCameraPose(&w, nullptr, Math::Vector3(0.0f, 0.0f, 10.0f),
                    Math::Vector3(0.0f, 0.0f, 0.0f), 60.0f);

    // Assert: rotating (0,0,-1) by the result must still face the look point.
    const auto* t = w.GetComponent<ECS::TransformComponent>(cam);
    ENJIN_ASSERT_TRUE(t != nullptr);
    const Math::Vector3 viewDir = t->rotation.GetForward();
    ENJIN_EXPECT_TRUE(viewDir.z < -0.99f);
    ENJIN_EXPECT_TRUE(Math::Abs(viewDir.x) < 0.01f);
    ENJIN_EXPECT_TRUE(Math::Abs(viewDir.y) < 0.01f);
}

ENJIN_TEST(CameraPose, ADegenerateLookPointLeavesRotationAlone) {
    // Arrange: look point equal to the position gives no direction to face.
    ECS::World w;
    const ECS::Entity cam = MakeActiveCamera(w);
    const Math::Quaternion before =
        w.GetComponent<ECS::TransformComponent>(cam)->rotation;

    // Act
    ApplyCameraPose(&w, nullptr, Math::Vector3(4.0f, 4.0f, 4.0f),
                    Math::Vector3(4.0f, 4.0f, 4.0f), 60.0f);

    // Assert: position still moves, rotation is untouched rather than NaN.
    const auto* t = w.GetComponent<ECS::TransformComponent>(cam);
    ENJIN_EXPECT_FLOAT_EQ(t->position.x, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(t->rotation.x, before.x);
    ENJIN_EXPECT_FLOAT_EQ(t->rotation.y, before.y);
    ENJIN_EXPECT_FLOAT_EQ(t->rotation.z, before.z);
    ENJIN_EXPECT_FLOAT_EQ(t->rotation.w, before.w);
}

ENJIN_TEST(CameraPose, ARunningCinematicMovesTheCameraEntity) {
    // Arrange: the end-to-end assertion. A two-waypoint cinematic, played, then
    // ticked. This is the test that fails against the old CinematicSystem --
    // it moved the cinematic entity and left the camera entity at the origin.
    ECS::World w;
    const ECS::Entity cam = MakeActiveCamera(w);

    const ECS::Entity shot = w.CreateEntity();
    ECS::TransformComponent st;
    w.AddComponent<ECS::TransformComponent>(shot, st);

    ECS::CinematicCameraComponent cine;
    ECS::CinematicCameraComponent::Waypoint a;
    a.position = Math::Vector3(0.0f, 10.0f, 0.0f);
    a.lookAt = Math::Vector3(0.0f, 0.0f, 0.0f);
    a.fov = 60.0f;
    a.duration = 1.0f;
    ECS::CinematicCameraComponent::Waypoint b;
    b.position = Math::Vector3(100.0f, 10.0f, 0.0f);
    b.lookAt = Math::Vector3(0.0f, 0.0f, 0.0f);
    b.fov = 60.0f;
    b.duration = 1.0f;
    cine.waypoints.push_back(a);
    cine.waypoints.push_back(b);
    w.AddComponent<ECS::CinematicCameraComponent>(shot, cine);

    // Act
    CinematicSystem sys;
    sys.SetEnabled(true);   // all three runtimes do this at play start
    sys.Play(&w, shot);
    sys.Update(&w, nullptr, 0.5f);

    // Assert: the camera entity has left the origin. Where exactly depends on
    // the easing curve, so this asserts the thing that was broken -- that the
    // shot reaches the camera at all -- not the interpolation.
    const auto* t = w.GetComponent<ECS::TransformComponent>(cam);
    ENJIN_ASSERT_TRUE(t != nullptr);
    ENJIN_EXPECT_TRUE(t->position.y > 1.0f);
}

ENJIN_TEST(CameraPose, NoActiveCameraIsNotACrash) {
    // Arrange: a world with no camera entity at all, as in a headless tick.
    ECS::World w;

    // Act / Assert: must simply do nothing.
    ApplyCameraPose(&w, nullptr, Math::Vector3(1.0f, 1.0f, 1.0f),
                    Math::Vector3(0.0f, 0.0f, 0.0f), 60.0f);
    ENJIN_EXPECT_TRUE(true);
}

ENJIN_TEST_MAIN()
