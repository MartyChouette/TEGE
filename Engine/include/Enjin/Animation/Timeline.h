#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include <string>
#include <vector>
#include <functional>
#include <variant>

namespace Enjin {
namespace Animation {

// Easing functions for timeline keyframes
enum class TimelineEasing : u8 {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Step
};

// A keyframe on a property track
struct PropertyKeyframe {
    f32 time = 0.0f;
    std::variant<f32, Math::Vector3, bool, std::string> value;
    TimelineEasing easing = TimelineEasing::Linear;
};

// A track that animates a specific property over time
struct PropertyTrack {
    std::string targetProperty;  // e.g., "position", "rotation.y", "material.opacity"
    ECS::Entity targetEntity = 0;
    std::vector<PropertyKeyframe> keyframes;
    bool enabled = true;
};

// An event fired at a specific time
struct TimelineEvent {
    f32 time = 0.0f;
    std::string eventName;
    std::string eventData;  // JSON-encoded data
    bool fired = false;
};

// Event track containing timed events
struct EventTrack {
    std::string name;
    std::vector<TimelineEvent> events;
    bool enabled = true;
};

// Animation track — plays a skeletal animation at a given time
struct AnimationTrack {
    f32 startTime = 0.0f;
    f32 duration = 0.0f;
    std::string animationName;
    ECS::Entity targetEntity = 0;
    f32 blendWeight = 1.0f;
    bool enabled = true;
};

// Timeline component — attach to an entity to create a sequence
struct TimelineComponent {
    std::vector<PropertyTrack> propertyTracks;
    std::vector<EventTrack> eventTracks;
    std::vector<AnimationTrack> animationTracks;

    f32 duration = 0.0f;       // Total timeline duration
    f32 currentTime = 0.0f;
    f32 playbackSpeed = 1.0f;
    bool isPlaying = false;
    bool loop = false;
    bool pingPong = false;
    bool playOnAwake = false;
    bool isComplete = false;
    i32 direction = 1;          // 1 = forward, -1 = backward (for ping-pong)

    // Callbacks
    bool onCompleteNotify = false;
    bool onLoopNotify = false;
};

// Timeline system — updates all timeline components
class ENJIN_API TimelineSystem {
public:
    void Update(ECS::World* world, f32 deltaTime);

    // Manual control
    void Play(TimelineComponent& timeline);
    void Pause(TimelineComponent& timeline);
    void Stop(TimelineComponent& timeline);
    void Seek(TimelineComponent& timeline, f32 time);

private:
    void EvaluatePropertyTracks(ECS::World* world, TimelineComponent& tl);
    void EvaluateEventTracks(TimelineComponent& tl);
    void EvaluateAnimationTracks(ECS::World* world, TimelineComponent& tl);

    f32 ApplyEasing(f32 t, TimelineEasing easing) const;
    f32 LerpValue(f32 a, f32 b, f32 t) const;
    Math::Vector3 LerpVector(const Math::Vector3& a, const Math::Vector3& b, f32 t) const;
};

} // namespace Animation
} // namespace Enjin
