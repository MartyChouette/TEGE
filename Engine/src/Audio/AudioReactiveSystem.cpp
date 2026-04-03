#include "Enjin/Audio/AudioReactiveSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
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
    UpdateConductor(deltaTime);
    UpdateSidechain(deltaTime);
    UpdateAudioCollisions(deltaTime);
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

// ============================================================================
// Conductor — AI-driven dynamic music based on gameplay state
// ============================================================================

void AudioReactiveSystem::UpdateConductor(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::ConductorComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* cond = m_World->GetComponent<ECS::ConductorComponent>(entity);
        if (!cond || !cond->enabled) continue;

        // Auto-detect gameplay state from scene
        if (cond->autoDetect) {
            ECS::ConductorComponent::GameplayState detected = ECS::ConductorComponent::GameplayState::Explore;

            // Check for nearby enemies → Combat
            bool enemiesNearby = false;
            auto* listenerTransform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (listenerTransform) {
                for (auto e : m_World->GetEntitiesWithComponent<ECS::HealthComponent>()) {
                    if (e == entity || !m_World->IsValid(e)) continue;
                    auto* hp = m_World->GetComponent<ECS::HealthComponent>(e);
                    auto* et = m_World->GetComponent<ECS::TransformComponent>(e);
                    if (hp && !hp->isDead && et) {
                        f32 dist = (et->position - listenerTransform->position).Length();
                        if (dist < cond->combatRadius) {
                            // Only count as combat if the entity has a DamageComponent (it's hostile)
                            if (m_World->HasComponent<ECS::DamageComponent>(e)) {
                                enemiesNearby = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (enemiesNearby) detected = ECS::ConductorComponent::GameplayState::Combat;

            // State change with delay (prevents flicker)
            if (detected != cond->currentState) {
                cond->stateTimer += deltaTime;
                if (cond->stateTimer >= cond->stateChangeDelay) {
                    cond->previousState = cond->currentState;
                    cond->currentState = detected;
                    cond->stateTimer = 0.0f;
                    ENJIN_LOG_INFO(Audio, "Conductor: state changed to %d", static_cast<int>(detected));
                }
            } else {
                cond->stateTimer = 0.0f;
            }
        }

        // Fade stems based on current state
        for (auto& stem : cond->stems) {
            bool shouldPlay = false;
            switch (cond->currentState) {
                case ECS::ConductorComponent::GameplayState::Explore:
                    shouldPlay = stem.playDuringExplore; break;
                case ECS::ConductorComponent::GameplayState::Combat:
                    shouldPlay = stem.playDuringCombat; break;
                case ECS::ConductorComponent::GameplayState::Stealth:
                    shouldPlay = stem.playDuringStealth; break;
                case ECS::ConductorComponent::GameplayState::Cutscene:
                    shouldPlay = stem.playDuringCutscene; break;
            }

            stem.targetVolume = shouldPlay ? cond->masterVolume : 0.0f;

            // Smooth fade
            f32 diff = stem.targetVolume - stem.volume;
            f32 step = stem.fadeSpeed * deltaTime;
            if (std::fabs(diff) <= step) {
                stem.volume = stem.targetVolume;
            } else {
                stem.volume += (diff > 0.0f ? step : -step);
            }
        }
    }
}

// ============================================================================
// Sidechain Compression — duck one bus when another has signal
// ============================================================================

void AudioReactiveSystem::UpdateSidechain(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SidechainComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* sc = m_World->GetComponent<ECS::SidechainComponent>(entity);
        if (!sc || !sc->enabled) continue;

        const auto& mixer = m_Audio->GetMixer();
        const Audio::AudioBus* source = mixer.GetBus(sc->sourceBus);
        Audio::AudioBus* target = m_Audio->GetMixer().GetBus(sc->targetBus);
        if (!source || !target) continue;

        bool sourceActive = source->vuLevel > sc->threshold;

        if (sourceActive) {
            sc->ducking = true;
            sc->holdTimer = sc->holdTime;
            // Attack — duck toward ratio
            f32 targetGain = sc->ratio;
            sc->currentGain += (targetGain - sc->currentGain) * Math::Min(deltaTime / Math::Max(sc->attackTime, 0.001f), 1.0f);
        } else if (sc->ducking) {
            sc->holdTimer -= deltaTime;
            if (sc->holdTimer <= 0.0f) {
                // Release — return to 1.0
                sc->currentGain += (1.0f - sc->currentGain) * Math::Min(deltaTime / Math::Max(sc->releaseTime, 0.001f), 1.0f);
                if (sc->currentGain > 0.99f) {
                    sc->currentGain = 1.0f;
                    sc->ducking = false;
                }
            }
        }

        // Apply ducking to target bus volume
        target->targetVolume = sc->currentGain;
    }
}

// ============================================================================
// Audio Collision — physics impacts auto-generate sound
// Inspired by Zelda TOTK's physics audio system:
// - Every surface has a material type
// - Impact velocity determines volume and soft/hard clip selection
// - Pitch varies per hit for natural feel
// - Cooldown prevents sound spam from rapid collisions
// ============================================================================

void AudioReactiveSystem::UpdateAudioCollisions(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::AudioCollisionComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* ac = m_World->GetComponent<ECS::AudioCollisionComponent>(entity);
        if (!ac || !ac->enabled) continue;

        // Cooldown
        if (ac->cooldownTimer > 0.0f) {
            ac->cooldownTimer -= deltaTime;
            continue;
        }

        // Check for collision velocity from physics
        // The physics system sets velocity on controllers; for rigidbodies,
        // we'd read from the physics backend. For now, check controller velocity
        // as a proxy for impact detection.
        f32 impactVelocity = 0.0f;

        if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity)) {
            // Ground contact = impact if we just landed
            if (ctrl->isGrounded && ctrl->velocity.y < -ac->softThreshold) {
                impactVelocity = std::fabs(ctrl->velocity.y);
            }
        } else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity)) {
            if (ctrl3->isGrounded && ctrl3->velocity.y < -ac->softThreshold) {
                impactVelocity = std::fabs(ctrl3->velocity.y);
            }
        }

        if (impactVelocity > ac->softThreshold) {
            ac->cooldownTimer = ac->cooldown;

            // Select clip based on velocity
            const std::string& clip = (impactVelocity > ac->hardThreshold)
                ? ac->impactHardClip : ac->impactSoftClip;
            if (clip.empty()) continue;

            // Volume from velocity (normalized)
            f32 maxVel = ac->hardThreshold * 2.0f;
            f32 vol = Math::Clamp(impactVelocity / maxVel, 0.2f, 1.0f) * ac->volumeScale;

            // Random pitch variation
            f32 pitch = 1.0f + ((static_cast<f32>(rand()) / RAND_MAX) * 2.0f - 1.0f) * ac->pitchVariance;

            // Play at entity position
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (transform) {
                Audio::AudioClipHandle clipHandle = m_Audio->LoadClip(clip);
                if (clipHandle != Audio::INVALID_AUDIO_CLIP) {
                    m_Audio->Play3D(clipHandle, transform->position, vol, 1.0f, 50.0f);
                }
            }
        }
    }
}

} // namespace Enjin::Audio
