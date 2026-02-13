#include "Enjin/Animation/Timeline.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Animation {

// --- Manual control ---

void TimelineSystem::Play(TimelineComponent& timeline) {
    timeline.isPlaying = true;
    timeline.isComplete = false;
    if (timeline.playOnAwake && timeline.currentTime == 0.0f) {
        // Reset all event fired flags when starting fresh
        for (auto& track : timeline.eventTracks) {
            for (auto& event : track.events) {
                event.fired = false;
            }
        }
    }
}

void TimelineSystem::Pause(TimelineComponent& timeline) {
    timeline.isPlaying = false;
}

void TimelineSystem::Stop(TimelineComponent& timeline) {
    timeline.isPlaying = false;
    timeline.currentTime = 0.0f;
    timeline.isComplete = false;
    timeline.direction = 1;

    // Reset all event fired flags
    for (auto& track : timeline.eventTracks) {
        for (auto& event : track.events) {
            event.fired = false;
        }
    }
}

void TimelineSystem::Seek(TimelineComponent& timeline, f32 time) {
    timeline.currentTime = std::clamp(time, 0.0f, timeline.duration);

    // Reset fired flags for events after the seek position
    for (auto& track : timeline.eventTracks) {
        for (auto& event : track.events) {
            event.fired = (event.time <= timeline.currentTime);
        }
    }
}

// --- Main update ---

void TimelineSystem::Update(ECS::World* world, f32 deltaTime) {
    if (!world) return;

    auto entities = world->GetEntitiesWithComponent<TimelineComponent>();
    for (auto entity : entities) {
        auto* tl = world->GetComponent<TimelineComponent>(entity);
        if (!tl || !tl->isPlaying) continue;

        // Handle play-on-awake first frame
        if (tl->playOnAwake && !tl->isPlaying) {
            tl->isPlaying = true;
        }

        f32 prevTime = tl->currentTime;

        // Advance time
        tl->currentTime += deltaTime * tl->playbackSpeed * static_cast<f32>(tl->direction);

        // Check bounds
        if (tl->currentTime >= tl->duration) {
            if (tl->pingPong) {
                tl->currentTime = tl->duration;
                tl->direction = -1;
                tl->onLoopNotify = true;
            } else if (tl->loop) {
                tl->currentTime = std::fmod(tl->currentTime, tl->duration);
                tl->onLoopNotify = true;

                // Reset event fired flags on loop
                for (auto& track : tl->eventTracks) {
                    for (auto& event : track.events) {
                        event.fired = false;
                    }
                }
            } else {
                tl->currentTime = tl->duration;
                tl->isPlaying = false;
                tl->isComplete = true;
                tl->onCompleteNotify = true;
            }
        } else if (tl->currentTime <= 0.0f) {
            if (tl->pingPong) {
                tl->currentTime = 0.0f;
                tl->direction = 1;
                if (tl->loop) {
                    tl->onLoopNotify = true;

                    // Reset event fired flags on loop
                    for (auto& track : tl->eventTracks) {
                        for (auto& event : track.events) {
                            event.fired = false;
                        }
                    }
                } else {
                    tl->isPlaying = false;
                    tl->isComplete = true;
                    tl->onCompleteNotify = true;
                }
            } else {
                tl->currentTime = 0.0f;
                tl->isPlaying = false;
                tl->isComplete = true;
                tl->onCompleteNotify = true;
            }
        }

        // Evaluate all tracks
        EvaluatePropertyTracks(world, *tl);
        EvaluateEventTracks(*tl);
        EvaluateAnimationTracks(world, *tl);
    }
}

// --- Property track evaluation ---

void TimelineSystem::EvaluatePropertyTracks(ECS::World* world, TimelineComponent& tl) {
    for (auto& track : tl.propertyTracks) {
        if (!track.enabled || track.keyframes.empty()) continue;
        if (!world->IsValid(track.targetEntity)) continue;

        // Sort keyframes by time (should already be sorted, but ensure)
        // We avoid sorting each frame for performance; assume sorted on creation

        // Find the two surrounding keyframes
        const PropertyKeyframe* kfBefore = nullptr;
        const PropertyKeyframe* kfAfter = nullptr;

        for (usize i = 0; i < track.keyframes.size(); ++i) {
            if (track.keyframes[i].time <= tl.currentTime) {
                kfBefore = &track.keyframes[i];
            }
            if (track.keyframes[i].time >= tl.currentTime && !kfAfter) {
                kfAfter = &track.keyframes[i];
            }
        }

        // If only one side found, clamp to that keyframe
        if (!kfBefore && kfAfter) kfBefore = kfAfter;
        if (!kfAfter && kfBefore) kfAfter = kfBefore;
        if (!kfBefore || !kfAfter) continue;

        // Calculate interpolation factor
        f32 t = 0.0f;
        f32 segmentDuration = kfAfter->time - kfBefore->time;
        if (segmentDuration > 0.0f) {
            t = (tl.currentTime - kfBefore->time) / segmentDuration;
            t = std::clamp(t, 0.0f, 1.0f);
            t = ApplyEasing(t, kfAfter->easing);
        }

        // Apply interpolated value to target property
        const std::string& prop = track.targetProperty;

        // --- Position ---
        if (prop == "position") {
            auto* transform = world->GetComponent<ECS::TransformComponent>(track.targetEntity);
            if (transform) {
                if (auto* a = std::get_if<Math::Vector3>(&kfBefore->value)) {
                    if (auto* b = std::get_if<Math::Vector3>(&kfAfter->value)) {
                        transform->position = LerpVector(*a, *b, t);
                    }
                }
            }
        }
        // --- Position components ---
        else if (prop == "position.x" || prop == "position.y" || prop == "position.z") {
            auto* transform = world->GetComponent<ECS::TransformComponent>(track.targetEntity);
            if (transform) {
                if (auto* a = std::get_if<f32>(&kfBefore->value)) {
                    if (auto* b = std::get_if<f32>(&kfAfter->value)) {
                        f32 val = LerpValue(*a, *b, t);
                        if (prop == "position.x") transform->position.x = val;
                        else if (prop == "position.y") transform->position.y = val;
                        else if (prop == "position.z") transform->position.z = val;
                    }
                }
            }
        }
        // --- Scale ---
        else if (prop == "scale") {
            auto* transform = world->GetComponent<ECS::TransformComponent>(track.targetEntity);
            if (transform) {
                if (auto* a = std::get_if<Math::Vector3>(&kfBefore->value)) {
                    if (auto* b = std::get_if<Math::Vector3>(&kfAfter->value)) {
                        transform->scale = LerpVector(*a, *b, t);
                    }
                }
            }
        }
        // --- Scale components ---
        else if (prop == "scale.x" || prop == "scale.y" || prop == "scale.z") {
            auto* transform = world->GetComponent<ECS::TransformComponent>(track.targetEntity);
            if (transform) {
                if (auto* a = std::get_if<f32>(&kfBefore->value)) {
                    if (auto* b = std::get_if<f32>(&kfAfter->value)) {
                        f32 val = LerpValue(*a, *b, t);
                        if (prop == "scale.x") transform->scale.x = val;
                        else if (prop == "scale.y") transform->scale.y = val;
                        else if (prop == "scale.z") transform->scale.z = val;
                    }
                }
            }
        }
        // --- Rotation (Euler angles via quaternion components) ---
        else if (prop == "rotation.x" || prop == "rotation.y" || prop == "rotation.z") {
            auto* transform = world->GetComponent<ECS::TransformComponent>(track.targetEntity);
            if (transform) {
                if (auto* a = std::get_if<f32>(&kfBefore->value)) {
                    if (auto* b = std::get_if<f32>(&kfAfter->value)) {
                        f32 val = LerpValue(*a, *b, t);
                        // Build rotation from Euler angles; apply to the relevant axis
                        // For simplicity, modify the quaternion component directly
                        if (prop == "rotation.x") transform->rotation.x = val;
                        else if (prop == "rotation.y") transform->rotation.y = val;
                        else if (prop == "rotation.z") transform->rotation.z = val;
                    }
                }
            }
        }
        // --- Material opacity ---
        else if (prop == "material.opacity") {
            auto* mat = world->GetComponent<ECS::MaterialComponent>(track.targetEntity);
            if (mat) {
                if (auto* a = std::get_if<f32>(&kfBefore->value)) {
                    if (auto* b = std::get_if<f32>(&kfAfter->value)) {
                        mat->opacity = LerpValue(*a, *b, t);
                    }
                }
            }
        }
        // --- Material base color ---
        else if (prop == "material.baseColor") {
            auto* mat = world->GetComponent<ECS::MaterialComponent>(track.targetEntity);
            if (mat) {
                if (auto* a = std::get_if<Math::Vector3>(&kfBefore->value)) {
                    if (auto* b = std::get_if<Math::Vector3>(&kfAfter->value)) {
                        mat->baseColor = LerpVector(*a, *b, t);
                    }
                }
            }
        }
        // --- Material emissive color ---
        else if (prop == "material.emissiveColor") {
            auto* mat = world->GetComponent<ECS::MaterialComponent>(track.targetEntity);
            if (mat) {
                if (auto* a = std::get_if<Math::Vector3>(&kfBefore->value)) {
                    if (auto* b = std::get_if<Math::Vector3>(&kfAfter->value)) {
                        mat->emissiveColor = LerpVector(*a, *b, t);
                    }
                }
            }
        }
        // --- Material emissive strength ---
        else if (prop == "material.emissiveStrength") {
            auto* mat = world->GetComponent<ECS::MaterialComponent>(track.targetEntity);
            if (mat) {
                if (auto* a = std::get_if<f32>(&kfBefore->value)) {
                    if (auto* b = std::get_if<f32>(&kfAfter->value)) {
                        mat->emissiveStrength = LerpValue(*a, *b, t);
                    }
                }
            }
        }
        // --- Material metallic ---
        else if (prop == "material.metallic") {
            auto* mat = world->GetComponent<ECS::MaterialComponent>(track.targetEntity);
            if (mat) {
                if (auto* a = std::get_if<f32>(&kfBefore->value)) {
                    if (auto* b = std::get_if<f32>(&kfAfter->value)) {
                        mat->metallic = LerpValue(*a, *b, t);
                    }
                }
            }
        }
        // --- Material roughness ---
        else if (prop == "material.roughness") {
            auto* mat = world->GetComponent<ECS::MaterialComponent>(track.targetEntity);
            if (mat) {
                if (auto* a = std::get_if<f32>(&kfBefore->value)) {
                    if (auto* b = std::get_if<f32>(&kfAfter->value)) {
                        mat->roughness = LerpValue(*a, *b, t);
                    }
                }
            }
        }
        // Unknown property
        else {
            ENJIN_LOG_WARN(Animation, "Unknown timeline property: %s", prop.c_str());
        }
    }
}

// --- Event track evaluation ---

void TimelineSystem::EvaluateEventTracks(TimelineComponent& tl) {
    for (auto& track : tl.eventTracks) {
        if (!track.enabled) continue;

        for (auto& event : track.events) {
            // In PingPong reverse playback, skip firing events to prevent
            // duplicate triggers when time re-crosses event timestamps
            if (tl.pingPong && tl.direction < 0) continue;

            if (!event.fired && event.time <= tl.currentTime) {
                event.fired = true;
                ENJIN_LOG_INFO(Animation, "Timeline event fired: %s (t=%.3f, data=%s)",
                    event.eventName.c_str(), event.time, event.eventData.c_str());
            }
        }
    }
}

// --- Animation track evaluation ---

void TimelineSystem::EvaluateAnimationTracks(ECS::World* world, TimelineComponent& tl) {
    for (auto& track : tl.animationTracks) {
        if (!track.enabled) continue;
        if (!world->IsValid(track.targetEntity)) continue;

        // Check if current time has entered this animation's time range
        f32 endTime = track.startTime + track.duration;
        if (tl.currentTime >= track.startTime && tl.currentTime <= endTime) {
            auto* animator = world->GetComponent<ECS::AnimatorComponent>(track.targetEntity);
            if (animator) {
                // Play the animation directly via the skeletal animator
                animator->animator.Play(track.animationName);
            }
        }
    }
}

// --- Easing functions ---

f32 TimelineSystem::ApplyEasing(f32 t, TimelineEasing easing) const {
    switch (easing) {
        case TimelineEasing::Linear:
            return t;

        case TimelineEasing::EaseIn:
            return t * t;

        case TimelineEasing::EaseOut:
            return 1.0f - (1.0f - t) * (1.0f - t);

        case TimelineEasing::EaseInOut: {
            if (t < 0.5f) {
                return 2.0f * t * t;
            } else {
                f32 inv = 1.0f - t;
                return 1.0f - 2.0f * inv * inv;
            }
        }

        case TimelineEasing::Step:
            return t < 1.0f ? 0.0f : 1.0f;

        default:
            return t;
    }
}

// --- Interpolation helpers ---

f32 TimelineSystem::LerpValue(f32 a, f32 b, f32 t) const {
    return a + (b - a) * t;
}

Math::Vector3 TimelineSystem::LerpVector(const Math::Vector3& a, const Math::Vector3& b, f32 t) const {
    return Math::Vector3(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    );
}

} // namespace Animation
} // namespace Enjin
