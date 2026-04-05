#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"

namespace Enjin {
namespace InputSystem { class MIDIInput; }
namespace Audio { class SimpleAudio; }
namespace Audio {

// Processes AudioReactiveComponent, AudioThresholdTriggerComponent,
// BeatClockComponent, BeatSyncComponent, and RTPCComponent each frame.
class ENJIN_API AudioReactiveSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetAudio(SimpleAudio* audio) { m_Audio = audio; }
    void SetMIDI(InputSystem::MIDIInput* midi) { m_MIDI = midi; }
    void Update(f32 deltaTime);

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
    void UpdateAmbientLayers(f32 deltaTime);
    void UpdateMusicZones(f32 deltaTime);

    void ApplyValueToTarget(ECS::Entity entity, ECS::AudioTargetProperty target, f32 value);

    ECS::World* m_World = nullptr;
    SimpleAudio* m_Audio = nullptr;
    InputSystem::MIDIInput* m_MIDI = nullptr;

    // Cached per-frame (avoids redundant lookups across subsystems)
    Math::Vector3 m_ListenerPos;
    const ECS::MaterialInteractionTableComponent* m_CachedMatTable = nullptr;
};

} // namespace Audio
} // namespace Enjin
