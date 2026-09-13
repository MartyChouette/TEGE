#pragma once

#include "Enjin/Acoustics/AcousticsSystem.h"

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Audio/AudioEngine.h"

#include <vector>

namespace Enjin {
namespace InputSystem { class MIDIInput; }
namespace Audio { class AudioEngine; }
namespace Audio {

// Processes AudioReactiveComponent, AudioThresholdTriggerComponent,
// BeatClockComponent, BeatSyncComponent, and RTPCComponent each frame.
class ENJIN_API AudioReactiveSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetAudio(AudioEngine* audio) { m_Audio = audio; }
    void SetMIDI(InputSystem::MIDIInput* midi) { m_MIDI = midi; }
    void Update(f32 deltaTime);

    // The room the listener is standing in, measured from its geometry.
    //
    // Lives here because this is already the thing that decides what the reverb
    // bus is doing each frame; a second system racing it for the same bus would
    // be two answers to one question.
    Acoustics::AcousticsSystem& Acoustics() { return m_Acoustics; }
    const Acoustics::AcousticsSystem& Acoustics() const { return m_Acoustics; }

    // Where this system decided the listener is, and whether it found one at
    // all. Readable because "the listener never moved" is invisible from the
    // outside otherwise -- it presents as an environment that never changes,
    // which reads as a broken reverb rather than a broken position.
    const Math::Vector3& ListenerPosition() const { return m_ListenerPos; }
    bool HasListener() const { return m_HasListener; }

private:
    void UpdateBeatClock(f32 deltaTime);
    void UpdateBeatSync(f32 deltaTime);
    void UpdateAudioReactive(f32 deltaTime);
    void UpdateThresholdTriggers(f32 deltaTime);
    void UpdateRTPC(f32 deltaTime);
    void UpdateConductor(f32 deltaTime);
    void UpdateSidechain(f32 deltaTime);
    void UpdateAudioCollisions(f32 deltaTime);
    void UpdateLipSync(f32 deltaTime);
    void UpdateMIDIBindings(f32 deltaTime);
    void UpdateOcclusion(f32 deltaTime);
    void UpdateReverbZones(f32 deltaTime);

    // Hand the nearest audible sources their own reflection patterns.
    //
    // One trace per call, round-robin, so the cost does not grow with the scene
    // -- it is the same budget whether three sounds are playing or three
    // hundred. Everything that does not hold a slot keeps the listener-centric
    // pattern off the shared bus.
    void UpdateSourceReflections(f32 deltaTime);

    void UpdateAmbientLayers(f32 deltaTime);
    void UpdateMusicZones(f32 deltaTime);

    void ApplyValueToTarget(ECS::Entity entity, ECS::AudioTargetProperty target, f32 value);

    ECS::World* m_World = nullptr;
    AudioEngine* m_Audio = nullptr;
    InputSystem::MIDIInput* m_MIDI = nullptr;

    // Where the listener is, answered once per frame by one authority.
    //
    // Returns false when the scene has neither an AudioListenerComponent nor an
    // active camera, which is the only case where there is genuinely no answer.
    // It used to default to the world origin in silence, and that is a wrong
    // answer that sounds like a working one: every room measurement, occlusion
    // test and reverb-zone check ran against (0,0,0) while the player walked
    // around, so the environment never changed no matter where they stood.
    bool ResolveListenerPosition(Math::Vector3& out) const;

    // Cached per-frame (avoids redundant lookups across subsystems)
    Math::Vector3 m_ListenerPos;
    bool m_HasListener = false;
    bool m_WarnedNoListener = false;
    const ECS::MaterialInteractionTableComponent* m_CachedMatTable = nullptr;

    Acoustics::AcousticsSystem m_Acoustics;

    // Round-robin cursor over the sources currently holding reflection slots,
    // and the handles they hold, so a source that drifts out of range can be
    // told to give its slot back.
    usize m_ReflectionCursor = 0;
    f32 m_SinceReflectionTrace = 0.0f;
    std::vector<SoundHandle> m_ReflectionOwners;
};

} // namespace Audio
} // namespace Enjin
