#include "Enjin/Audio/AudioReactiveSystem.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/MorphTarget.h"
#include "Enjin/Input/MIDIInput.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin::Audio {

void AudioReactiveSystem::Update(f32 deltaTime) {
    if (!m_World || !m_Audio) return;

    // Cache listener position once (used by occlusion, reverb, ambient, music, collisions)
    m_ListenerPos = Math::Vector3(0.0f);
    for (auto e : m_World->GetEntitiesWithComponent<ECS::AudioListenerComponent>()) {
        if (!m_World->IsValid(e)) continue;
        auto* lt = m_World->GetComponent<ECS::TransformComponent>(e);
        if (lt) { m_ListenerPos = lt->position; break; }
    }

    // Cache material interaction table (used by collision audio)
    m_CachedMatTable = nullptr;
    for (auto e : m_World->GetEntitiesWithComponent<ECS::MaterialInteractionTableComponent>()) {
        if (!m_World->IsValid(e)) continue;
        m_CachedMatTable = m_World->GetComponent<ECS::MaterialInteractionTableComponent>(e);
        if (m_CachedMatTable) break;
    }

    UpdateBeatClock(deltaTime);
    UpdateBeatSync(deltaTime);
    UpdateAudioReactive(deltaTime);
    UpdateThresholdTriggers(deltaTime);
    UpdateRTPC(deltaTime);
    UpdateConductor(deltaTime);
    UpdateSidechain(deltaTime);
    UpdateAudioCollisions(deltaTime);
    UpdateOcclusion(deltaTime);
    UpdateReverbZones(deltaTime);
    UpdateAmbientLayers(deltaTime);
    UpdateMusicZones(deltaTime);
    UpdateLipSync(deltaTime);
    UpdateMIDIBindings(deltaTime);
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

            // Accessibility: fire visual indicator on downbeat
            if (clock->downbeatThisFrame) {
                auto cb = m_Audio->GetOnSoundPlayed();
                if (cb) cb("[Beat: downbeat]");
            }
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
                    // Accessibility: visual indicator for state change
                    static const char* stateLabels[] = {"[Music: Explore]", "[Music: Combat]", "[Music: Stealth]", "[Music: Cutscene]"};
                    auto cb = m_Audio->GetOnSoundPlayed();
                    if (cb) cb(stateLabels[static_cast<int>(detected)]);
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
// Audio Collision — TOTK-style physics audio
// - Material×material interaction lookup
// - Velocity-driven volume + clip selection (soft/hard)
// - Hollowness affects resonance pitch
// - Mass affects base pitch and volume
// - Continuous contact sounds (scrape/roll) with velocity-driven pitch
// - Distance-based detail culling
// ============================================================================

void AudioReactiveSystem::UpdateAudioCollisions(f32 deltaTime) {
    // Use cached material interaction table (looked up once in Update)
    const ECS::MaterialInteractionTableComponent* matTable = m_CachedMatTable;

    // Get listener position for distance culling
    const Math::Vector3& listenerPos = m_ListenerPos;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::AudioCollisionComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* ac = m_World->GetComponent<ECS::AudioCollisionComponent>(entity);
        if (!ac || !ac->enabled) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Distance culling (TOTK optimization)
        f32 distToListener = (transform->position - listenerPos).Length();
        if (distToListener > ac->cullDistance) {
            // Stop any continuous sounds
            if (ac->scrapeSoundHandle) { m_Audio->Stop(ac->scrapeSoundHandle); ac->scrapeSoundHandle = 0; }
            if (ac->rollSoundHandle) { m_Audio->Stop(ac->rollSoundHandle); ac->rollSoundHandle = 0; }
            continue;
        }
        bool useSimplifiedSounds = (distToListener > ac->detailDistance);

        // Cooldown for impact sounds
        if (ac->cooldownTimer > 0.0f) ac->cooldownTimer -= deltaTime;

        // Detect impact velocity from controllers
        f32 impactVelocity = 0.0f;
        f32 horizontalVelocity = 0.0f;
        bool isGrounded = false;

        if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity)) {
            if (ctrl->isGrounded && ctrl->velocity.y < -ac->softThreshold) {
                impactVelocity = std::fabs(ctrl->velocity.y);
            }
            horizontalVelocity = std::fabs(ctrl->velocity.x);
            isGrounded = ctrl->isGrounded;
        } else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity)) {
            if (ctrl3->isGrounded && ctrl3->velocity.y < -ac->softThreshold) {
                impactVelocity = std::fabs(ctrl3->velocity.y);
            }
            horizontalVelocity = Math::Vector2(ctrl3->velocity.x, ctrl3->velocity.z).Length();
            isGrounded = ctrl3->isGrounded;
        }

        // --- Impact sounds ---
        if (impactVelocity > ac->softThreshold && ac->cooldownTimer <= 0.0f) {
            ac->cooldownTimer = ac->cooldown;

            // Select clip: check material interaction table first, fall back to entity clips
            std::string clip;
            f32 pitchOffset = 0.0f;
            f32 volumeMult = 1.0f;

            if (matTable && !useSimplifiedSounds) {
                // Try to find the ground/contact material (default to Stone for ground)
                ECS::SurfaceMaterial otherMat = ECS::SurfaceMaterial::Stone;
                const auto* interaction = matTable->Find(ac->material, otherMat);
                if (interaction) {
                    clip = (impactVelocity > ac->hardThreshold) ? interaction->hardClip : interaction->softClip;
                    pitchOffset = interaction->pitchOffset;
                    volumeMult = interaction->volumeMultiplier;
                }
            }

            // Fall back to entity's own clips
            if (clip.empty()) {
                clip = (impactVelocity > ac->hardThreshold) ? ac->impactHardClip : ac->impactSoftClip;
            }

            if (!clip.empty()) {
                // Volume from velocity + mass
                f32 maxVel = ac->hardThreshold * 2.0f;
                f32 vol = Math::Clamp(impactVelocity / maxVel, 0.2f, 1.0f) * ac->volumeScale * volumeMult;
                vol *= Math::Clamp(ac->mass * 0.5f, 0.3f, 2.0f); // Heavier = louder

                // Pitch from mass, hollowness, random variation
                f32 massPitch = 1.0f / Math::Clamp(ac->mass * 0.3f + 0.7f, 0.5f, 2.0f); // Heavier = lower pitch
                f32 hollowPitch = 1.0f + ac->hollowness * 0.5f; // Hollow = higher resonance
                f32 randomPitch = 1.0f + ((static_cast<f32>(rand()) / RAND_MAX) * 2.0f - 1.0f) * ac->pitchVariance;
                f32 pitch = massPitch * hollowPitch * randomPitch + pitchOffset;

                Audio::AudioClipHandle clipHandle = m_Audio->LoadClip(clip);
                if (clipHandle != Audio::INVALID_AUDIO_CLIP) {
                    Audio::SoundHandle sh = m_Audio->Play3D(clipHandle, transform->position, vol, 1.0f, ac->cullDistance);
                    if (sh && pitch != 1.0f) m_Audio->SetPitch(sh, pitch);
                }
            }
        }

        // --- Continuous contact sounds (scrape/roll) ---
        bool shouldScrape = isGrounded && horizontalVelocity > ac->scrapeMinVelocity && !ac->scrapeClip.empty();

        if (shouldScrape && !useSimplifiedSounds) {
            ac->inContact = true;
            ac->contactVelocity = horizontalVelocity;

            // Start scrape sound if not already playing
            if (!ac->scrapeSoundHandle || !m_Audio->IsPlaying(ac->scrapeSoundHandle)) {
                Audio::AudioClipHandle clipHandle = m_Audio->LoadClip(ac->scrapeClip);
                if (clipHandle != Audio::INVALID_AUDIO_CLIP) {
                    ac->scrapeSoundHandle = m_Audio->Play3D(clipHandle, transform->position,
                        0.5f * ac->volumeScale, 1.0f, ac->cullDistance, Audio::AudioChannel::SFX);
                }
            }

            // Update scrape pitch + volume based on velocity
            if (ac->scrapeSoundHandle && m_Audio->IsPlaying(ac->scrapeSoundHandle)) {
                f32 normVel = Math::Clamp(horizontalVelocity / 5.0f, 0.0f, 1.0f);
                f32 scrapePitch = 0.8f + normVel * ac->scrapePitchScale;
                f32 scrapeVol = Math::Clamp(normVel * 0.8f, 0.1f, 1.0f) * ac->volumeScale;
                m_Audio->SetPitch(ac->scrapeSoundHandle, scrapePitch);
                m_Audio->SetVolume(ac->scrapeSoundHandle, scrapeVol);
                m_Audio->SetPosition(ac->scrapeSoundHandle, transform->position);
            }
        } else {
            // Stop continuous sounds when not in contact
            if (ac->scrapeSoundHandle && m_Audio->IsPlaying(ac->scrapeSoundHandle)) {
                m_Audio->Stop(ac->scrapeSoundHandle);
            }
            ac->scrapeSoundHandle = 0;
            ac->inContact = false;
        }
    }
}

// ============================================================================
// Occlusion — raycast-based muffling of sounds behind geometry
// Adjusts volume + pitch to simulate low-pass filtering
// ============================================================================

void AudioReactiveSystem::UpdateOcclusion(f32 deltaTime) {
    // Get listener position
    const Math::Vector3& listenerPos = m_ListenerPos;

    // Cache collider entities once, not per audio source (was O(n*m), now O(n+m))
    const auto& colliderEntities = m_World->GetEntitiesWithComponent<ECS::BoxColliderComponent>();

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::AudioOcclusionComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* occ = m_World->GetComponent<ECS::AudioOcclusionComponent>(entity);
        if (!occ || !occ->enabled) continue;

        auto* audioSrc = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
        if (!audioSrc || audioSrc->soundHandle == 0) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Throttle raycast checks
        occ->updateTimer += deltaTime;
        if (occ->updateTimer < 1.0f / occ->updateRate) continue;
        occ->updateTimer = 0.0f;

        // Simple occlusion: check if there are solid entities between listener and source
        // For now, use distance-based heuristic + check for obstacles with colliders
        // (Full raycast would require physics backend access — we approximate)
        Math::Vector3 toSource = transform->position - listenerPos;
        f32 distance = toSource.Length();

        // Check for entities with colliders between listener and source (simple AABB test)
        f32 occlusionScore = 0.0f;
        if (distance > 1.0f) {
            Math::Vector3 dir = toSource * (1.0f / distance);
            Math::Vector3 midpoint = listenerPos + dir * (distance * 0.5f);

            // Count entities with box colliders near the midpoint
            for (auto obstacle : colliderEntities) {
                if (obstacle == entity || !m_World->IsValid(obstacle)) continue;
                auto* ot = m_World->GetComponent<ECS::TransformComponent>(obstacle);
                auto* col = m_World->GetComponent<ECS::BoxColliderComponent>(obstacle);
                if (!ot || !col) continue;

                // Simple: is the obstacle between listener and source?
                Math::Vector3 toObs = ot->position - listenerPos;
                f32 dot = toObs.x * dir.x + toObs.y * dir.y + toObs.z * dir.z;
                if (dot > 0.0f && dot < distance) {
                    // Check if obstacle is close to the line
                    Math::Vector3 projected = listenerPos + dir * dot;
                    f32 perpDist = (ot->position - projected).Length();
                    f32 colSize = Math::Max(col->size.x, Math::Max(col->size.y, col->size.z));
                    if (perpDist < colSize) {
                        occlusionScore += 0.5f;
                    }
                }
            }
        }

        occ->occlusionAmount = Math::Clamp(occlusionScore, 0.0f, 1.0f);

        // Apply occlusion: reduce volume and simulate low-pass by slightly lowering pitch
        if (occ->occlusionAmount > 0.0f) {
            f32 volReduction = 1.0f - (occ->occlusionAmount * occ->volumeReduction);
            m_Audio->SetVolume(audioSrc->soundHandle, audioSrc->volume * volReduction);
            // Simulate muffling: slightly lower pitch (crude but effective without per-sample LPF)
            f32 mufflePitch = 1.0f - (occ->occlusionAmount * 0.15f);
            m_Audio->SetPitch(audioSrc->soundHandle, audioSrc->pitch * mufflePitch);
        } else {
            m_Audio->SetVolume(audioSrc->soundHandle, audioSrc->volume);
            m_Audio->SetPitch(audioSrc->soundHandle, audioSrc->pitch);
        }
    }
}

// ============================================================================
// Reverb Zones — apply reverb when listener is inside a zone
// Uses volume-based wet/dry mixing on the master bus
// ============================================================================

void AudioReactiveSystem::UpdateReverbZones(f32 deltaTime) {
    // Get listener position
    const Math::Vector3& listenerPos = m_ListenerPos;

    // Find the highest-priority reverb zone influencing the listener. Non-global
    // zones fade in across blendRadius outside their shape so walking into a
    // cave swells the echo instead of snapping it.
    ECS::ReverbZoneComponent* activeZone = nullptr;
    f32 activeBlend = 0.0f;
    i32 bestPriority = -999999;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::ReverbZoneComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* rz = m_World->GetComponent<ECS::ReverbZoneComponent>(entity);
        if (!rz || !rz->isActive) continue;

        f32 blend = 0.0f;
        if (rz->isGlobal) {
            blend = 1.0f;
        } else {
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (!transform) continue;
            Math::Vector3 local = listenerPos - transform->position;
            f32 outside = 0.0f;   // distance beyond the shape surface
            if (rz->shape == ECS::ReverbZoneComponent::Shape::Sphere) {
                f32 r = rz->halfExtents.x;
                f32 d = local.Length();
                outside = d - r;
            } else {
                Math::Vector3 q(std::fabs(local.x) - rz->halfExtents.x,
                                std::fabs(local.y) - rz->halfExtents.y,
                                std::fabs(local.z) - rz->halfExtents.z);
                f32 qx = q.x > 0.0f ? q.x : 0.0f;
                f32 qy = q.y > 0.0f ? q.y : 0.0f;
                f32 qz = q.z > 0.0f ? q.z : 0.0f;
                outside = std::sqrt(qx * qx + qy * qy + qz * qz);
            }
            if (outside <= 0.0f) blend = 1.0f;
            else if (rz->blendRadius > 0.001f && outside < rz->blendRadius)
                blend = 1.0f - outside / rz->blendRadius;
        }
        if (blend > 0.0f && (rz->priority > bestPriority ||
                             (rz->priority == bestPriority && blend > activeBlend))) {
            bestPriority = rz->priority;
            activeZone = rz;
            activeBlend = blend;
        }
    }

    // Resolve preset -> parameters (Custom keeps the authored fields).
    f32 wet = 0.0f, room = 0.5f, damp = 0.5f, decayT = 1.5f, pre = 0.02f;
    if (activeZone) {
        using P = ECS::ReverbZoneComponent::Preset;
        room = activeZone->roomSize; damp = activeZone->damping;
        wet = activeZone->wetDryMix; decayT = activeZone->decayTime; pre = activeZone->preDelay;
        switch (activeZone->preset) {
            case P::SmallRoom:  room = 0.35f; damp = 0.5f;  wet = 0.22f; decayT = 0.8f; pre = 0.008f; break;
            case P::LargeRoom:  room = 0.6f;  damp = 0.45f; wet = 0.3f;  decayT = 1.5f; pre = 0.015f; break;
            case P::Hall:       room = 0.8f;  damp = 0.4f;  wet = 0.35f; decayT = 2.5f; pre = 0.025f; break;
            case P::Cathedral:  room = 1.0f;  damp = 0.3f;  wet = 0.45f; decayT = 5.0f; pre = 0.04f;  break;
            case P::Cave:       room = 0.9f;  damp = 0.2f;  wet = 0.5f;  decayT = 3.5f; pre = 0.03f;  break;
            case P::Outdoors:   room = 0.2f;  damp = 0.8f;  wet = 0.08f; decayT = 0.4f; pre = 0.005f; break;
            case P::Bathroom:   room = 0.3f;  damp = 0.1f;  wet = 0.5f;  decayT = 1.2f; pre = 0.005f; break;
            case P::UnderWater: room = 0.8f;  damp = 0.95f; wet = 0.7f;  decayT = 3.0f; pre = 0.02f;  break;
            case P::Custom: default: break;
        }
        wet *= activeBlend;
    }

    // Feed the real Freeverb bus (SimpleAudio smooths on the audio thread).
    m_Audio->SetEnvironmentReverb(wet, room, damp, decayT, pre);
    (void)deltaTime;
}

// ============================================================================
// Ambient Sound Layers — fade in/out looping sounds based on listener zone
// ============================================================================

void AudioReactiveSystem::UpdateAmbientLayers(f32 deltaTime) {
    const Math::Vector3& listenerPos = m_ListenerPos;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::AmbientSoundLayerComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* asl = m_World->GetComponent<ECS::AmbientSoundLayerComponent>(entity);
        if (!asl || !asl->isActive) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Check if listener is inside the zone
        Math::Vector3 local = listenerPos - transform->position;
        bool inside = std::fabs(local.x) <= asl->halfExtents.x &&
                      std::fabs(local.y) <= asl->halfExtents.y &&
                      std::fabs(local.z) <= asl->halfExtents.z;

        // Calculate blend weight based on distance from zone edge
        f32 blendWeight = 0.0f;
        if (inside) {
            blendWeight = 1.0f;
        } else if (asl->blendRadius > 0.0f) {
            // Distance from zone surface
            f32 dx = Math::Max(0.0f, std::fabs(local.x) - asl->halfExtents.x);
            f32 dy = Math::Max(0.0f, std::fabs(local.y) - asl->halfExtents.y);
            f32 dz = Math::Max(0.0f, std::fabs(local.z) - asl->halfExtents.z);
            f32 dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < asl->blendRadius) {
                blendWeight = 1.0f - (dist / asl->blendRadius);
            }
        }

        // Ensure handle vector matches layer count
        asl->layerHandles.resize(asl->layers.size(), 0);

        for (usize i = 0; i < asl->layers.size(); ++i) {
            auto& layer = asl->layers[i];
            f32 targetVol = blendWeight * layer.volume;

            if (targetVol > 0.01f) {
                // Start sound if not playing
                if (!asl->layerHandles[i] || !m_Audio->IsPlaying(asl->layerHandles[i])) {
                    if (!layer.clipPath.empty()) {
                        Audio::AudioClipHandle clip = m_Audio->LoadClip(layer.clipPath);
                        if (clip != Audio::INVALID_AUDIO_CLIP) {
                            asl->layerHandles[i] = m_Audio->Play(clip, targetVol, layer.pitch, layer.loop,
                                Audio::AudioChannel::SFX);
                        }
                    }
                } else {
                    // Update volume
                    m_Audio->SetVolume(asl->layerHandles[i], targetVol);
                }
            } else {
                // Stop sound if playing
                if (asl->layerHandles[i] && m_Audio->IsPlaying(asl->layerHandles[i])) {
                    m_Audio->Stop(asl->layerHandles[i]);
                    asl->layerHandles[i] = 0;
                }
            }
        }
    }
}

// ============================================================================
// Music Zones — crossfade music when listener enters/exits zones
// ============================================================================

void AudioReactiveSystem::UpdateMusicZones(f32 deltaTime) {
    const Math::Vector3& listenerPos = m_ListenerPos;

    // Find the highest-priority music zone the listener is inside
    ECS::MusicZoneComponent* activeZone = nullptr;
    i32 bestPriority = -999999;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::MusicZoneComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* mz = m_World->GetComponent<ECS::MusicZoneComponent>(entity);
        if (!mz || !mz->isActive) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        Math::Vector3 local = listenerPos - transform->position;
        bool inside = std::fabs(local.x) <= mz->halfExtents.x &&
                      std::fabs(local.y) <= mz->halfExtents.y &&
                      std::fabs(local.z) <= mz->halfExtents.z;

        if (inside && mz->priority > bestPriority) {
            bestPriority = mz->priority;
            activeZone = mz;
        }
    }

    // Crossfade to the active zone's track
    auto& crossfader = m_Audio->GetCrossfader();
    if (activeZone && !activeZone->trackPath.empty()) {
        if (crossfader.GetCurrentTrack() != activeZone->trackPath) {
            crossfader.Play(activeZone->trackPath, activeZone->fadeInTime, activeZone->fadeOutTime);
        }
    } else {
        // No active music zone — fade out
        if (crossfader.IsPlaying()) {
            crossfader.Stop(2.0f);
        }
    }
}

// ============================================================================
// LipSync — drive morph target weights from viseme state
// ============================================================================

void AudioReactiveSystem::UpdateLipSync(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::LipSyncComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* lip = m_World->GetComponent<ECS::LipSyncComponent>(entity);
        if (!lip) continue;

        auto* morph = m_World->GetComponent<ECS::MorphTargetComponent>(entity);
        if (!morph) continue;

        // Auto-map visemes to morph targets on first use
        if (lip->autoMapVisemes && !lip->autoMapped) {
            lip->autoMapped = true;
            // Try standard naming conventions
            static const struct { ECS::Viseme viseme; const char* names[4]; } autoMap[] = {
                {ECS::Viseme::AA,     {"viseme_aa", "AA", "jawOpen", "mouthOpen"}},
                {ECS::Viseme::EE,     {"viseme_ee", "EE", "mouthSmile", nullptr}},
                {ECS::Viseme::IH,     {"viseme_ih", "IH", "mouthNarrow", nullptr}},
                {ECS::Viseme::OH,     {"viseme_oh", "OH", "mouthFunnel", nullptr}},
                {ECS::Viseme::OU,     {"viseme_ou", "OU", "mouthPucker", nullptr}},
                {ECS::Viseme::PP,     {"viseme_pp", "PP", "mouthClose", nullptr}},
                {ECS::Viseme::FF,     {"viseme_ff", "FF", "mouthLowerDown", nullptr}},
                {ECS::Viseme::TH,     {"viseme_th", "TH", "tongueOut", nullptr}},
                {ECS::Viseme::DD,     {"viseme_dd", "DD", nullptr, nullptr}},
                {ECS::Viseme::KK,     {"viseme_kk", "KK", nullptr, nullptr}},
                {ECS::Viseme::CH,     {"viseme_ch", "CH", nullptr, nullptr}},
                {ECS::Viseme::SS,     {"viseme_ss", "SS", nullptr, nullptr}},
                {ECS::Viseme::NN,     {"viseme_nn", "NN", nullptr, nullptr}},
                {ECS::Viseme::RR,     {"viseme_rr", "RR", nullptr, nullptr}},
            };
            for (const auto& mapping : autoMap) {
                for (const char* name : mapping.names) {
                    if (!name) break;
                    i32 idx = morph->FindTarget(name);
                    if (idx >= 0) {
                        ECS::LipSyncComponent::VisemeMorphMapping m;
                        m.morphTargetName = name;
                        m.weight = 1.0f;
                        lip->visemeMorphMap[static_cast<usize>(mapping.viseme)].push_back(m);
                        break; // Found a match, don't check other names
                    }
                }
            }
        }

        // Apply current viseme to morph targets
        // First, decay all lip-sync-driven morph targets toward zero
        for (usize v = 0; v < static_cast<usize>(ECS::Viseme::Count); ++v) {
            for (const auto& mapping : lip->visemeMorphMap[v]) {
                i32 idx = morph->FindTarget(mapping.morphTargetName);
                if (idx >= 0) {
                    f32& w = morph->weights[idx];
                    w += (0.0f - w) * Math::Min(lip->blendSpeed * deltaTime, 1.0f);
                    morph->weightsDirty = true;
                }
            }
        }

        // Then set the active viseme's targets
        usize activeViseme = static_cast<usize>(lip->currentViseme);
        if (activeViseme < static_cast<usize>(ECS::Viseme::Count)) {
            for (const auto& mapping : lip->visemeMorphMap[activeViseme]) {
                i32 idx = morph->FindTarget(mapping.morphTargetName);
                if (idx >= 0) {
                    f32 targetW = mapping.weight * lip->currentWeight;
                    f32& w = morph->weights[idx];
                    w += (targetW - w) * Math::Min(lip->blendSpeed * deltaTime, 1.0f);
                    morph->weightsDirty = true;
                }
            }
        }

        // Auto amplitude fallback — use audio amplitude to drive jawOpen
        if (lip->autoFromAmplitude && lip->visemeMorphMap[static_cast<usize>(ECS::Viseme::AA)].empty()) {
            // Find a "jawOpen" or "mouthOpen" morph target
            i32 jawIdx = morph->FindTarget("jawOpen");
            if (jawIdx < 0) jawIdx = morph->FindTarget("mouthOpen");
            if (jawIdx < 0) jawIdx = morph->FindTarget("viseme_aa");
            if (jawIdx >= 0) {
                // Simple amplitude-driven mouth open/close
                f32 amplitude = lip->currentWeight; // Use viseme weight as proxy
                lip->amplitudeSmoothed += (amplitude - lip->amplitudeSmoothed) * Math::Min(lip->blendSpeed * deltaTime, 1.0f);
                morph->weights[jawIdx] = lip->amplitudeSmoothed;
                morph->weightsDirty = true;
            }
        }
    }

    // Also apply morph weights from skeletal animator (animation-driven morph targets)
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::MorphTargetComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* morph = m_World->GetComponent<ECS::MorphTargetComponent>(entity);
        if (!morph) continue;

        auto* animComp = m_World->GetComponent<ECS::AnimatorComponent>(entity);
        if (!animComp || !animComp->animator.HasMorphAnimation()) continue;

        const auto& animWeights = animComp->animator.GetMorphWeights();
        for (usize i = 0; i < std::min(morph->weights.size(), animWeights.size()); ++i) {
            if (morph->weights[i] != animWeights[i]) {
                morph->weights[i] = animWeights[i];
                morph->weightsDirty = true;
            }
        }
    }
}

// ============================================================================
// MIDI Bindings — map MIDI CC/notes to entity properties
// ============================================================================

void AudioReactiveSystem::UpdateMIDIBindings(f32 deltaTime) {
    if (!m_MIDI || !m_MIDI->IsDeviceOpen()) return;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::MIDIBindingComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* mb = m_World->GetComponent<ECS::MIDIBindingComponent>(entity);
        if (!mb || !mb->enabled) continue;

        for (auto& binding : mb->bindings) {
            // Read MIDI value (0-127)
            f32 midiNorm = 0.0f;
            switch (binding.source) {
                case ECS::MIDIBindingComponent::Binding::Source::CC: {
                    u8 val = m_MIDI->GetCCValue(binding.midiCC, binding.midiChannel);
                    midiNorm = static_cast<f32>(val) / 127.0f;
                    break;
                }
                case ECS::MIDIBindingComponent::Binding::Source::NoteVelocity: {
                    u8 vel = m_MIDI->GetNoteVelocity(binding.midiCC, binding.midiChannel);
                    midiNorm = static_cast<f32>(vel) / 127.0f;
                    break;
                }
                case ECS::MIDIBindingComponent::Binding::Source::PitchBend: {
                    // Pitch bend is 14-bit (0-16383), center = 8192
                    // Approximate from CC for now
                    midiNorm = 0.5f;
                    break;
                }
            }

            // Map to output range
            binding.rawValue = binding.outputMin + (binding.outputMax - binding.outputMin) * midiNorm;

            // Smooth
            binding.currentValue += (binding.rawValue - binding.currentValue) *
                Math::Min(binding.smoothing * deltaTime, 1.0f);

            // Apply to target
            if (binding.target == ECS::AudioTargetProperty::Custom) {
                // Custom target: try morph target name or bus volume
                auto* morph = m_World->GetComponent<ECS::MorphTargetComponent>(entity);
                if (morph && !binding.customTarget.empty()) {
                    morph->SetWeight(binding.customTarget, binding.currentValue);
                }
            } else {
                ApplyValueToTarget(entity, binding.target, binding.currentValue);
            }
        }
    }
}

} // namespace Enjin::Audio
