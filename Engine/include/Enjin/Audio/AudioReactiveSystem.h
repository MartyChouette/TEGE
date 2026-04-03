#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"

namespace Enjin {
namespace Audio { class SimpleAudio; }
namespace Audio {

// Processes AudioReactiveComponent, AudioThresholdTriggerComponent,
// BeatClockComponent, BeatSyncComponent, and RTPCComponent each frame.
class ENJIN_API AudioReactiveSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetAudio(SimpleAudio* audio) { m_Audio = audio; }
    void Update(f32 deltaTime);

private:
    void UpdateBeatClock(f32 deltaTime);
    void UpdateBeatSync(f32 deltaTime);
    void UpdateAudioReactive(f32 deltaTime);
    void UpdateThresholdTriggers(f32 deltaTime);
    void UpdateRTPC(f32 deltaTime);

    void ApplyValueToTarget(ECS::Entity entity, ECS::AudioTargetProperty target, f32 value);

    ECS::World* m_World = nullptr;
    SimpleAudio* m_Audio = nullptr;
};

} // namespace Audio
} // namespace Enjin
