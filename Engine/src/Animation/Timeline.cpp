#include "Enjin/Animation/Timeline.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Animation {

// --- Manual control ---

namespace {
// Marker fired flags and animation-track starts are per pass through the
// timeline. Stop, a loop and a fresh Play all clear both.
void ResetLatches(TimelineComponent& tl) {
    for (auto& track : tl.eventTracks) {
        for (auto& event : track.events) event.fired = false;
    }
    for (auto& track : tl.animationTracks) track.started = false;
}
}

void TimelineSystem::Play(TimelineComponent& timeline) {
    // Starting from the top is a fresh pass. This used to reset only when Play
    // On Awake was also set, so a script replaying a finished timeline skipped
    // every marker it had already fired.
    if (timeline.currentTime == 0.0f || timeline.isComplete) {
        if (timeline.isComplete && timeline.direction > 0) timeline.currentTime = 0.0f;
        ResetLatches(timeline);
    }
    timeline.isPlaying = true;
    timeline.isComplete = false;
}

void TimelineSystem::Pause(TimelineComponent& timeline) {
    timeline.isPlaying = false;
}

void TimelineSystem::Stop(TimelineComponent& timeline) {
    timeline.isPlaying = false;
    timeline.currentTime = 0.0f;
    timeline.isComplete = false;
    timeline.direction = 1;

    ResetLatches(timeline);
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

    // A copy: a marker listener may add or remove timelines
    auto entities = world->GetEntitiesWithComponent<TimelineComponent>();
    for (auto entity : entities) {
        auto* tl = world->GetComponent<TimelineComponent>(entity);
        if (!tl) continue;

        // Play On Awake starts it on its first tick in play. This check sat
        // after the !isPlaying skip, so it could never fire, and isPlaying is
        // not saved: a saved timeline had no way to start without a script.
        if (tl->playOnAwake && !tl->awakeStarted) {
            tl->awakeStarted = true;
            Play(*tl);
        }
        if (!tl->isPlaying) continue;

        // Nothing to play through. fmod by zero below would make the time NaN.
        if (tl->duration <= 0.0f) {
            tl->currentTime = 0.0f;
            tl->isPlaying = false;
            tl->isComplete = true;
            tl->onCompleteNotify = true;
            EvaluatePropertyTracks(world, *tl);
            continue;
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

                ResetLatches(*tl);
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

                    ResetLatches(*tl);
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

        // Evaluate all tracks. Markers go last: sending one can run a script
        // that adds a timeline and moves the storage tl points into.
        EvaluatePropertyTracks(world, *tl);
        EvaluateAnimationTracks(world, *tl);
        EvaluateEventTracks(entity, *tl);
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
        if (prop.rfind("position", 0) == 0 || prop.rfind("scale", 0) == 0 ||
            prop.rfind("rotation", 0) == 0) {
            if (auto* tf = world->GetComponent<ECS::TransformComponent>(track.targetEntity)) {
                tf->worldMatrixDirty = true;
            }
        }

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
                        // Degrees about one axis, the other two kept. This wrote
                        // the number straight into the quaternion's x/y/z, so a
                        // 45-degree key produced a denormalised quaternion and
                        // a mangled mesh rather than a rotation.
                        f32 val = LerpValue(*a, *b, t);
                        Math::Vector3 euler = transform->rotation.ToEulerDegrees();
                        if (prop == "rotation.x") euler.x = val;
                        else if (prop == "rotation.y") euler.y = val;
                        else if (prop == "rotation.z") euler.z = val;
                        transform->rotation = Math::Quaternion::FromEulerDegrees(euler);
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
        // --- Visibility (steps: holds the last key at or before now) ---
        else if (prop == "visible") {
            auto* transform = world->GetComponent<ECS::TransformComponent>(track.targetEntity);
            const PropertyKeyframe* held = (kfBefore->time <= tl.currentTime) ? kfBefore : kfAfter;
            if (transform) {
                if (auto* v = std::get_if<bool>(&held->value)) transform->visible = *v;
            }
        }
        // Unknown property
        else {
            ENJIN_LOG_WARN(Animation, "Unknown timeline property: %s", prop.c_str());
        }
    }
}

// --- Event track evaluation ---

void TimelineSystem::EvaluateEventTracks(ECS::Entity entity, TimelineComponent& tl) {
    // Collected first and sent after: a listener may touch this component.
    // Markers used to be logged and nothing else.
    std::vector<ECS::EntityEvent> due;
    for (auto& track : tl.eventTracks) {
        if (!track.enabled) continue;

        for (auto& event : track.events) {
            // In PingPong reverse playback, skip firing events to prevent
            // duplicate triggers when time re-crosses event timestamps
            if (tl.pingPong && tl.direction < 0) continue;

            if (!event.fired && event.time <= tl.currentTime) {
                event.fired = true;
                if (event.eventName.empty()) continue;
                ECS::EntityEvent ev;
                ev.name = event.eventName;
                ev.sender = entity;
                ev.strings["data"] = event.eventData;
                ev.floats["time"] = event.time;
                due.push_back(std::move(ev));
            }
        }
    }
    if (!m_EventBus) return;
    for (const auto& ev : due) m_EventBus->Send(ev.name, ev);
}

// --- Animation track evaluation ---

void TimelineSystem::EvaluateAnimationTracks(ECS::World* world, TimelineComponent& tl) {
    for (auto& track : tl.animationTracks) {
        if (!track.enabled) continue;
        if (!world->IsValid(track.targetEntity)) continue;

        // Start the clip once on entering its range. Play rewinds the clip to
        // its first frame, and this called it every frame inside the range, so
        // a timeline-driven animation stood frozen on frame one.
        f32 endTime = track.startTime + track.duration;
        const bool inRange = tl.currentTime >= track.startTime && tl.currentTime <= endTime;
        if (inRange && !track.started) {
            auto* animator = world->GetComponent<ECS::AnimatorComponent>(track.targetEntity);
            if (animator) {
                animator->animator.Play(track.animationName);
                track.started = true;
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
