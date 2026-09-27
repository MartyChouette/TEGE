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
namespace ECS { class EntityEventBus; }
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
    bool started = false;   // runtime: Play was called on entering the range
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

    bool awakeStarted = false;  // runtime: Play On Awake has fired this play
};

// Timeline system — updates all timeline components
//
// Editor play and both players construct and tick one (SD-27); until then it
// existed only in tests, so every Flash-converted or SWF-imported timeline sat
// still. Event markers are sent on the bus as named events (sender = the
// timeline's entity, "data" string = the marker's data), which reach scripts
// and visual scripts through the runtime's bridge.
class ENJIN_API TimelineSystem {
public:
    void Update(ECS::World* world, f32 deltaTime);
    void SetEventBus(ECS::EntityEventBus* bus) { m_EventBus = bus; }

    // Manual control
    void Play(TimelineComponent& timeline);
    void Pause(TimelineComponent& timeline);
    void Stop(TimelineComponent& timeline);
    void Seek(TimelineComponent& timeline, f32 time);

private:
    void EvaluatePropertyTracks(ECS::World* world, TimelineComponent& tl);
    void EvaluateEventTracks(ECS::Entity entity, TimelineComponent& tl);
    void EvaluateAnimationTracks(ECS::World* world, TimelineComponent& tl);

    f32 ApplyEasing(f32 t, TimelineEasing easing) const;
    f32 LerpValue(f32 a, f32 b, f32 t) const;
    Math::Vector3 LerpVector(const Math::Vector3& a, const Math::Vector3& b, f32 t) const;

    ECS::EntityEventBus* m_EventBus = nullptr;
};

} // namespace Animation
} // namespace Enjin
