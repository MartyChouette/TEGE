#include "Enjin/Audio/AudioReactiveSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin::Audio {

void AudioReactiveSystem::Update(f32 deltaTime) {
    if (!m_World || !m_Audio) return;

    UpdateBeatClock(deltaTime);
    UpdateBeatSync(deltaTime);
    UpdateAudioReactive(deltaTime);
    UpdateThresholdTriggers(deltaTime);
    UpdateRTPC(deltaTime);
}

// ============================================================================
// Beat Clock — global BPM timing
// ============================================================================

void AudioReactiveSystem::UpdateBeatClock(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::BeatClockComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* clock = m_World->GetComponent<ECS::BeatClockComponent>(entity);
        if (!clock || !clock->playing) continue;

        f64 beatsPerSecond = static_cast<f64>(clock->bpm) / 60.0;
        f64 prevAccum = clock->beatAccumulator;
        clock->beatAccumulator += beatsPerSecond * static_cast<f64>(deltaTime);

        u32 prevBeat = static_cast<u32>(prevAccum);
        u32 newBeat = static_cast<u32>(clock->beatAccumulator);

        clock->beatThisFrame = (newBeat != prevBeat);
        if (clock->beatThisFrame) {
            clock->totalBeats++;
            clock->currentBeat = clock->totalBeats % clock->beatsPerBar;
            clock->currentBar = clock->totalBeats / clock->beatsPerBar;
            clock->downbeatThisFrame = (clock->currentBeat == 0);
        } else {
            clock->downbeatThisFrame = false;
        }

        // Wrap accumulator to prevent float precision loss over long sessions
        if (clock->beatAccumulator > 1000000.0) {
            clock->beatAccumulator = std::fmod(clock->beatAccumulator, static_cast<f64>(clock->beatsPerBar));
        }
    }
}

// ============================================================================
// Beat Sync — pulse entity properties to the beat
// ============================================================================

void AudioReactiveSystem::UpdateBeatSync(f32 deltaTime) {
    // Find the active beat clock
    ECS::BeatClockComponent* clock = nullptr;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::BeatClockComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        clock = m_World->GetComponent<ECS::BeatClockComponent>(entity);
        if (clock && clock->playing) break;
        clock = nullptr;
    }
    if (!clock) return;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::BeatSyncComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* sync = m_World->GetComponent<ECS::BeatSyncComponent>(entity);
        if (!sync || !sync->enabled) continue;

        bool shouldPulse = false;
        switch (sync->mode) {
            case ECS::BeatSyncComponent::SyncMode::EveryBeat:
                shouldPulse = clock->beatThisFrame;
                break;
            case ECS::BeatSyncComponent::SyncMode::EveryDownbeat:
                shouldPulse = clock->downbeatThisFrame;
                break;
            case ECS::BeatSyncComponent::SyncMode::EveryNBeats:
                shouldPulse = clock->beatThisFrame && (clock->totalBeats % sync->beatDivisor == 0);
                break;
            case ECS::BeatSyncComponent::SyncMode::Continuous: {
                // Smooth sine wave synced to beat phase
                f32 phase = clock->GetBeatPhase();
                sync->currentValue = sync->baseValue + (sync->pulseValue - sync->baseValue) *
                    (0.5f + 0.5f * std::sin(phase * 6.2831853f - 1.5707963f));
                ApplyValueToTarget(entity, sync->target, sync->currentValue);
                continue; // Skip pulse logic
            }
        }

        if (shouldPulse) {
            sync->currentValue = sync->pulseValue;
        }

        // Decay toward base
        sync->currentValue += (sync->baseValue - sync->currentValue) * Math::Min(sync->decaySpeed * deltaTime, 1.0f);
        ApplyValueToTarget(entity, sync->target, sync->currentValue);
    }
}

// ============================================================================
// Audio Reactive — bus VU drives entity properties
// ============================================================================

void AudioReactiveSystem::UpdateAudioReactive(f32 deltaTime) {
    const auto& mixer = m_Audio->GetMixer();

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::AudioReactiveComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* ar = m_World->GetComponent<ECS::AudioReactiveComponent>(entity);
        if (!ar || !ar->enabled) continue;

        // Read VU level from the specified bus
        f32 vuLevel = 0.0f;
        const AudioBus* bus = mixer.GetBus(ar->busName);
        if (bus) vuLevel = bus->vuLevel;

        // Apply threshold
        f32 signal = (vuLevel > ar->threshold) ? (vuLevel - ar->threshold) / (1.0f - ar->threshold) : 0.0f;
        signal *= ar->multiplier;
        signal = Math::Clamp(signal, 0.0f, 1.0f);
        if (ar->invert) signal = 1.0f - signal;

        // Map to output range
        f32 targetValue = ar->baseValue + (ar->maxValue - ar->baseValue) * signal;

        // Smooth
        ar->currentValue += (targetValue - ar->currentValue) * Math::Min(ar->smoothing * deltaTime, 1.0f);

        ApplyValueToTarget(entity, ar->target, ar->currentValue);
    }
}

// ============================================================================
// Threshold Triggers — fire events when bus exceeds level
// ============================================================================

void AudioReactiveSystem::UpdateThresholdTriggers(f32 deltaTime) {
    const auto& mixer = m_Audio->GetMixer();

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::AudioThresholdTriggerComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* trig = m_World->GetComponent<ECS::AudioThresholdTriggerComponent>(entity);
        if (!trig || !trig->enabled) continue;

        // Cooldown
        if (trig->cooldownTimer > 0.0f) trig->cooldownTimer -= deltaTime;

        // Active effect
        if (trig->effectTimer > 0.0f) {
            trig->effectTimer -= deltaTime;
            f32 t = trig->effectTimer / trig->effectDuration;

            switch (trig->action) {
                case ECS::AudioThresholdTriggerComponent::Action::FlickerLights: {
                    auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
                    if (light) {
                        // Random flicker
                        f32 flicker = 0.5f + 0.5f * std::sin(trig->effectTimer * 40.0f);
                        light->intensity *= (0.3f + 0.7f * flicker) * trig->effectIntensity;
                    }
                    break;
                }
                case ECS::AudioThresholdTriggerComponent::Action::CameraShake: {
                    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (transform) {
                        f32 shake = t * trig->effectIntensity * 0.1f;
                        transform->position.x += (std::sin(trig->effectTimer * 50.0f) * shake);
                        transform->position.y += (std::cos(trig->effectTimer * 47.0f) * shake);
                    }
                    break;
                }
                default: break;
            }

            if (trig->effectTimer <= 0.0f) trig->triggered = false;
            continue;
        }

        // Check threshold
        const AudioBus* bus = mixer.GetBus(trig->busName);
        f32 vuLevel = bus ? bus->vuLevel : 0.0f;

        if (vuLevel > trig->threshold && trig->cooldownTimer <= 0.0f && !trig->triggered) {
            trig->triggered = true;
            trig->effectTimer = trig->effectDuration;
            trig->cooldownTimer = trig->cooldown;
        }
    }
}

// ============================================================================
// RTPC — game parameters drive audio properties
// ============================================================================

void AudioReactiveSystem::UpdateRTPC(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::RTPCComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* rtpc = m_World->GetComponent<ECS::RTPCComponent>(entity);
        if (!rtpc || !rtpc->enabled) continue;

        auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
        if (!audio) continue;

        for (const auto& mapping : rtpc->mappings) {
            f32 paramValue = rtpc->GetParameter(mapping.parameterName);

            // Normalize to 0-1 range
            f32 range = mapping.paramMax - mapping.paramMin;
            if (range < 0.0001f) continue;
            f32 normalized = Math::Clamp((paramValue - mapping.paramMin) / range, 0.0f, 1.0f);

            // Apply curve
            if (mapping.curve < 0.0f) {
                normalized = std::pow(normalized, 1.0f / (1.0f - mapping.curve)); // Ease in
            } else if (mapping.curve > 0.0f) {
                normalized = std::pow(normalized, 1.0f + mapping.curve); // Ease out
            }

            // Map to output range
            f32 output = mapping.outputMin + (mapping.outputMax - mapping.outputMin) * normalized;

            // Apply to audio target
            switch (mapping.audioTarget) {
                case ECS::RTPCComponent::Mapping::AudioTarget::Volume:
                    audio->volume = output;
                    break;
                case ECS::RTPCComponent::Mapping::AudioTarget::Pitch:
                    audio->pitch = output;
                    break;
                default: break;
            }
        }
    }
}

// ============================================================================
// Apply value to target property on an entity
// ============================================================================

void AudioReactiveSystem::ApplyValueToTarget(ECS::Entity entity, ECS::AudioTargetProperty target, f32 value) {
    switch (target) {
        case ECS::AudioTargetProperty::LightIntensity: {
            auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
            if (light) light->intensity = value;
            break;
        }
        case ECS::AudioTargetProperty::LightColor: {
            auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
            if (light) {
                f32 factor = Math::Clamp(value, 0.0f, 5.0f);
                light->color = Math::Vector3(factor, factor, factor);
            }
            break;
        }
        case ECS::AudioTargetProperty::EmissiveStrength: {
            auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
            if (mat) mat->emissiveStrength = value;
            break;
        }
        case ECS::AudioTargetProperty::Scale: {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (transform) transform->scale = Math::Vector3(value, value, value);
            break;
        }
        case ECS::AudioTargetProperty::ParticleRate: {
            auto* pe = m_World->GetComponent<ECS::ParticleEmitterComponent>(entity);
            if (pe) pe->emissionRate = value;
            break;
        }
        default: break;
    }
}

} // namespace Enjin::Audio
