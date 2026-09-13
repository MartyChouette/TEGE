// The listener has to be where the player is.
//
// There were two answers to "where is the listener" in this engine and they
// disagreed. miniaudio's spatializer is driven from the active camera, so
// panning and distance followed the player correctly. Everything inside
// AudioReactiveSystem -- room measurement, occlusion, reverb zones, ambient
// layers, music zones -- read an AudioListenerComponent and, finding none,
// used the world origin without saying anything.
//
// Nothing requires that component and nothing warned it was missing, so the
// normal case was a listener nailed to (0,0,0) while the player walked around.
// The symptom is not "the reverb is broken": it is that every room sounds
// identical, because the room being measured is always the one at the origin.
// A demo built to prove three rooms sound different shipped sounding the same
// in all three, and the acoustics were correct the whole time.
//
// These tests assert the position, not the sound, because the position is the
// thing that was wrong.

#include "EnjinTest.h"
#include "Enjin/Audio/AudioReactiveSystem.h"
#include "Enjin/Audio/AudioEngine.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using Enjin::Math::Vector3;

namespace {

// An AudioEngine that was never Initialize()d is safe to drive: every entry
// point checks its impl and returns. That keeps this test off the sound card,
// which is what makes it run in CI at all.
struct Harness {
    ECS::World world;
    Audio::AudioEngine audio;
    Audio::AudioReactiveSystem system;

    Harness() {
        system.SetWorld(&world);
        system.SetAudio(&audio);
    }

    ECS::Entity AddCamera(const Vector3& at, i32 priority = 0, bool active = true) {
        ECS::Entity e = world.CreateEntity();
        ECS::TransformComponent t;
        t.position = at;
        world.AddComponent<ECS::TransformComponent>(e, t);
        ECS::CameraComponent c;
        c.isActive = active;
        c.priority = priority;
        world.AddComponent<ECS::CameraComponent>(e, c);
        return e;
    }

    ECS::Entity AddListener(const Vector3& at) {
        ECS::Entity e = world.CreateEntity();
        ECS::TransformComponent t;
        t.position = at;
        world.AddComponent<ECS::TransformComponent>(e, t);
        world.AddComponent<ECS::AudioListenerComponent>(e, ECS::AudioListenerComponent{});
        return e;
    }
};

} // namespace

ENJIN_TEST(ListenerPosition, ASceneWithNoListenerComponentFollowsTheActiveCamera) {
    // Arrange: the ordinary scene. A camera, and nobody has ever heard of
    // AudioListenerComponent -- which is every scene the editor creates.
    Harness h;
    h.AddCamera(Vector3(-21.0f, 1.6f, 0.0f));

    // Act
    h.system.Update(0.016f);

    // Assert
    ENJIN_EXPECT_TRUE(h.system.HasListener());
    ENJIN_EXPECT_FLOAT_NEAR(h.system.ListenerPosition().x, -21.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(h.system.ListenerPosition().y, 1.6f, 0.001f);
}

ENJIN_TEST(ListenerPosition, TheListenerMovesWhenTheCameraMoves) {
    // Arrange
    Harness h;
    ECS::Entity cam = h.AddCamera(Vector3(-21.0f, 1.6f, 0.0f));
    h.system.Update(0.016f);
    const Vector3 before = h.system.ListenerPosition();

    // Act: walk from the kitchen to the basement.
    h.world.GetComponent<ECS::TransformComponent>(cam)->position = Vector3(21.0f, 1.6f, 0.0f);
    h.system.Update(0.016f);
    const Vector3 after = h.system.ListenerPosition();

    // Assert: this is the whole bug. It used to be (0,0,0) both times, so no
    // amount of correct acoustics downstream could make the rooms differ.
    ENJIN_EXPECT_FLOAT_NEAR(before.x, -21.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(after.x, 21.0f, 0.001f);
}

ENJIN_TEST(ListenerPosition, AnExplicitListenerComponentBeatsTheCamera) {
    // Arrange: placing the component is a deliberate act -- a listener on a head
    // bone rather than on a third-person camera hanging four metres behind it --
    // so it has to win.
    Harness h;
    h.AddCamera(Vector3(0.0f, 2.0f, 8.0f));
    h.AddListener(Vector3(0.0f, 1.7f, 0.0f));

    // Act
    h.system.Update(0.016f);

    // Assert
    ENJIN_EXPECT_TRUE(h.system.HasListener());
    ENJIN_EXPECT_FLOAT_NEAR(h.system.ListenerPosition().z, 0.0f, 0.001f);
}

ENJIN_TEST(ListenerPosition, TheHighestPriorityActiveCameraWins) {
    // Arrange
    Harness h;
    h.AddCamera(Vector3(-21.0f, 1.6f, 0.0f), /*priority*/ 0);
    h.AddCamera(Vector3(21.0f, 1.6f, 0.0f),  /*priority*/ 10);

    // Act
    h.system.Update(0.016f);

    // Assert: the same camera miniaudio's spatializer picks. Two subsystems
    // disagreeing about which camera you are looking through would be the same
    // class of bug one layer down.
    ENJIN_EXPECT_FLOAT_NEAR(h.system.ListenerPosition().x, 21.0f, 0.001f);
}

ENJIN_TEST(ListenerPosition, AnInactiveCameraIsNotAListener) {
    // Arrange
    Harness h;
    h.AddCamera(Vector3(-21.0f, 1.6f, 0.0f), 0, /*active*/ false);

    // Act
    h.system.Update(0.016f);

    // Assert: nothing to listen with, and the system says so rather than
    // quietly answering "the origin".
    ENJIN_EXPECT_TRUE(!h.system.HasListener());
}

ENJIN_TEST(ListenerPosition, NoListenerMeansNoRoomMeasurementRatherThanTheOriginsRoom) {
    // Arrange: an empty scene with no camera at all.
    Harness h;

    // Act
    h.system.Update(0.016f);

    // Assert: measuring here would be inventing a place to stand. The old code
    // measured the room around (0,0,0) and fed it to the reverb bus as though
    // it were where the player was.
    ENJIN_EXPECT_TRUE(!h.system.HasListener());
    ENJIN_EXPECT_TRUE(!h.system.Acoustics().HasMeasurement());
}

ENJIN_TEST_MAIN()
