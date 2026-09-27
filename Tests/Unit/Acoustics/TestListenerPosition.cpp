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
#include "Enjin/Audio/LoFi.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/ArtStyle.h"
#include <cmath>
#include <vector>

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

// ===========================================================================
// Audio Snapshot Trigger: nothing read it, and nothing pushed a snapshot (SD-27)
// ===========================================================================

namespace {
ECS::Entity AddSnapshotTrigger(ECS::World& w, const Vector3& at, const char* name) {
    ECS::Entity e = w.CreateEntity();
    ECS::TransformComponent t;
    t.position = at;
    w.AddComponent<ECS::TransformComponent>(e, t);
    ECS::AudioSnapshotTriggerComponent st;
    st.snapshotName = name;
    st.halfExtents = Vector3(2.0f);
    w.AddComponent<ECS::AudioSnapshotTriggerComponent>(e, st);
    return e;
}
}

ENJIN_TEST(SnapshotTrigger, ListenerInsidePushesAndLeavingPops) {
    Harness h;
    ECS::Entity cam = h.AddCamera(Vector3(20.0f, 0.0f, 0.0f));
    ECS::Entity trig = AddSnapshotTrigger(h.world, Vector3(0.0f), "Dialogue");
    auto& mixer = h.audio.GetMixer();

    h.system.Update(0.016f);
    ENJIN_EXPECT_FALSE(mixer.IsSnapshotActive("Dialogue"));

    h.world.GetComponent<ECS::TransformComponent>(cam)->position = Vector3(1.0f, 0.5f, -1.0f);
    h.system.Update(0.016f);
    ENJIN_EXPECT_TRUE(mixer.IsSnapshotActive("Dialogue"));
    ENJIN_EXPECT_TRUE(h.world.GetComponent<ECS::AudioSnapshotTriggerComponent>(trig)->listenerInside);
    mixer.Update(0.016f);
    ENJIN_EXPECT_FLOAT_NEAR(mixer.GetBus("Music")->targetVolume, 0.3f, 1e-4f);   // ducked

    h.world.GetComponent<ECS::TransformComponent>(cam)->position = Vector3(20.0f, 0.0f, 0.0f);
    h.system.Update(0.016f);
    ENJIN_EXPECT_FALSE(mixer.IsSnapshotActive("Dialogue"));
    ENJIN_EXPECT_FLOAT_NEAR(mixer.GetBus("Music")->targetVolume, 1.0f, 1e-4f);
}

ENJIN_TEST(SnapshotTrigger, SwitchingItOffOrDestroyingItPops) {
    Harness h;
    h.AddCamera(Vector3(0.0f));
    ECS::Entity trig = AddSnapshotTrigger(h.world, Vector3(0.0f), "Combat");
    h.system.Update(0.016f);
    ENJIN_EXPECT_TRUE(h.audio.GetMixer().IsSnapshotActive("Combat"));

    h.world.GetComponent<ECS::AudioSnapshotTriggerComponent>(trig)->isActive = false;
    h.system.Update(0.016f);
    ENJIN_EXPECT_FALSE(h.audio.GetMixer().IsSnapshotActive("Combat"));

    h.world.GetComponent<ECS::AudioSnapshotTriggerComponent>(trig)->isActive = true;
    h.system.Update(0.016f);
    ENJIN_EXPECT_TRUE(h.audio.GetMixer().IsSnapshotActive("Combat"));
    h.world.RemoveComponent<ECS::AudioSnapshotTriggerComponent>(trig);
    h.system.Update(0.016f);
    ENJIN_EXPECT_FALSE(h.audio.GetMixer().IsSnapshotActive("Combat"));
}

ENJIN_TEST(SnapshotTrigger, ReleaseOnStopPopsWhatTriggersPushed) {
    // Editor Stop: the mixer outlives play
    Harness h;
    h.AddCamera(Vector3(0.0f));
    AddSnapshotTrigger(h.world, Vector3(0.0f), "Cutscene");
    h.system.Update(0.016f);
    ENJIN_EXPECT_TRUE(h.audio.GetMixer().IsSnapshotActive("Cutscene"));
    h.system.ReleaseSnapshotTriggers();
    ENJIN_EXPECT_FALSE(h.audio.GetMixer().IsSnapshotActive("Cutscene"));
}

ENJIN_TEST(SnapshotTrigger, AnUnknownNamePushesNothing) {
    Harness h;
    h.AddCamera(Vector3(0.0f));
    AddSnapshotTrigger(h.world, Vector3(0.0f), "Underwater");
    h.system.Update(0.016f);
    ENJIN_EXPECT_FALSE(h.audio.GetMixer().IsSnapshotActive("Underwater"));
}

// ===========================================================================
// Audio Fidelity: the lo-fi master effect (SD-27)
// ===========================================================================

namespace {
std::vector<f32> Ramp(u64 frames) {
    std::vector<f32> v(frames * 2);
    for (u64 i = 0; i < frames; ++i) {
        v[i * 2] = static_cast<f32>(i) / static_cast<f32>(frames) - 0.5f;
        v[i * 2 + 1] = -v[i * 2];
    }
    return v;
}
}

ENJIN_TEST(LoFi, OffOrZeroIntensityLeavesTheMixAlone) {
    Audio::LoFiProcessor lofi;
    auto a = Ramp(256), b = a;
    lofi.Process(a.data(), 256, 48000);          // disabled by default
    ENJIN_EXPECT_TRUE(a == b);
    Audio::LoFiParams p;
    p.enabled = true;
    p.intensity = 0.0f;
    p.bitDepthReduction = 0.25f;
    lofi.SetParams(p);
    lofi.Process(a.data(), 256, 48000);
    ENJIN_EXPECT_TRUE(a == b);
}

ENJIN_TEST(LoFi, BitDepthQuantisesAndSampleRateHolds) {
    Audio::LoFiProcessor lofi;
    Audio::LoFiParams p;
    p.enabled = true;
    p.bitDepthReduction = 0.25f;                 // 4 bits: steps of 1/8
    p.sampleRateReduction = 0.25f;               // each sample held for four frames
    lofi.SetParams(p);
    auto a = Ramp(256);
    lofi.Process(a.data(), 256, 48000);
    for (u64 i = 0; i < 256; ++i) {
        const f32 steps = a[i * 2] * 8.0f;
        ENJIN_EXPECT_TRUE(std::abs(steps - std::round(steps)) < 1e-4f);
    }
    int changes = 0;
    for (u64 i = 1; i < 256; ++i) if (a[i * 2] != a[(i - 1) * 2]) ++changes;
    ENJIN_EXPECT_TRUE(changes <= 256 / 4 + 1);
}

ENJIN_TEST(LoFi, ZeroWidthIsMono) {
    Audio::LoFiProcessor lofi;
    Audio::LoFiParams p;
    p.enabled = true;
    p.stereoWidth = 0.0f;
    lofi.SetParams(p);
    auto a = Ramp(64);
    lofi.Process(a.data(), 64, 48000);
    for (u64 i = 0; i < 64; ++i) ENJIN_EXPECT_TRUE(std::abs(a[i * 2] - a[i * 2 + 1]) < 1e-6f);
}

ENJIN_TEST(LoFi, AutoMatchPicksThePresetFromTheCamerasArtStyle) {
    ECS::AudioFidelityMode m;
    ENJIN_EXPECT_TRUE(Audio::AudioReactiveSystem::FidelityForArtStyle(static_cast<u8>(ECS::ArtStyleType::Retro), m));
    ENJIN_EXPECT_TRUE(m == ECS::AudioFidelityMode::PSOne);
    ENJIN_EXPECT_TRUE(Audio::AudioReactiveSystem::FidelityForArtStyle(static_cast<u8>(ECS::ArtStyleType::Analog), m));
    ENJIN_EXPECT_TRUE(m == ECS::AudioFidelityMode::Cassette);
    ENJIN_EXPECT_FALSE(Audio::AudioReactiveSystem::FidelityForArtStyle(static_cast<u8>(ECS::ArtStyleType::HandPainted), m));

    Harness h;
    ECS::Entity cam = h.AddCamera(Vector3(0.0f));
    ECS::ArtStyleComponent art;
    art.style = ECS::ArtStyleType::PixelArt;
    h.world.AddComponent<ECS::ArtStyleComponent>(cam, art);
    ECS::Entity mgr = h.world.CreateEntity();
    h.world.AddComponent<ECS::AudioFidelityComponent>(mgr, ECS::AudioFidelityComponent{});
    h.system.Update(0.016f);
    ENJIN_EXPECT_TRUE(h.world.GetComponent<ECS::AudioFidelityComponent>(mgr)->mode == ECS::AudioFidelityMode::Retro8Bit);
}

// ===========================================================================
// Conductor stealth: the threshold was never read, Stealth never detected (SD-27)
// ===========================================================================

ENJIN_TEST(ConductorStealth, CreepingNearAnEnemyIsStealthRunningIsCombat) {
    Harness h;
    h.AddCamera(Vector3(0.0f));
    ECS::Entity player = h.world.CreateEntity();
    h.world.AddComponent<ECS::TransformComponent>(player);
    h.world.AddComponent<ECS::FirstPersonController>(player);
    ECS::Entity enemy = h.world.CreateEntity();
    h.world.AddComponent<ECS::TransformComponent>(enemy).position = Vector3(5.0f, 0.0f, 0.0f);
    h.world.AddComponent<ECS::HealthComponent>(enemy);
    h.world.AddComponent<ECS::DamageComponent>(enemy);
    ECS::Entity mgr = h.world.CreateEntity();
    h.world.AddComponent<ECS::TransformComponent>(mgr).position = Vector3(500.0f, 0.0f, 0.0f);   // far from the fight
    ECS::ConductorComponent c;
    c.stateChangeDelay = 0.0f;
    h.world.AddComponent<ECS::ConductorComponent>(mgr, c);

    // Standing still next to the enemy
    h.system.Update(0.1f);
    h.system.Update(0.1f);
    ENJIN_EXPECT_TRUE(h.world.GetComponent<ECS::ConductorComponent>(mgr)->currentState ==
                      ECS::ConductorComponent::GameplayState::Stealth);

    // Running: a unit every tenth of a second is 10 units a second
    for (int i = 0; i < 3; ++i) {
        h.world.GetComponent<ECS::TransformComponent>(player)->position.z += 1.0f;
        h.system.Update(0.1f);
    }
    ENJIN_EXPECT_TRUE(h.world.GetComponent<ECS::ConductorComponent>(mgr)->currentState ==
                      ECS::ConductorComponent::GameplayState::Combat);
}

ENJIN_TEST_MAIN()
