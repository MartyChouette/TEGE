#include "EnjinTest.h"
#include "Enjin/Animation/Timeline.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Animation;
using namespace Enjin::Math;

// NOTE: the Forward/Loop/PingPong/Easing tests below drive the real
// TimelineSystem::Update against a World instead of re-implementing the
// advancement/easing math in the test body. Earlier versions asserted their own
// arithmetic (e.g. `tl.currentTime += dt*speed*dir`), which passed regardless of
// what the engine actually did.

// Build a one-entity world carrying `tlc`, return world + the live component so a
// test can Play it and step TimelineSystem::Update.
namespace {
struct Rig {
    ECS::World world;
    ECS::Entity entity = 0;
    TimelineComponent* tl = nullptr;
    TimelineSystem sys;
};

void MakeRig(Rig& r, const TimelineComponent& tlc, bool withTransform = false) {
    r.entity = r.world.CreateEntity();
    if (withTransform) r.world.AddComponent<ECS::TransformComponent>(r.entity);
    r.world.AddComponent<TimelineComponent>(r.entity, tlc);
    r.tl = r.world.GetComponent<TimelineComponent>(r.entity);
}

// Drive a single float "position.x" track from 0 to endVal over `duration` with
// the given easing, evaluate at `atTime` through the engine, return the result.
f32 EvalEasedTrack(TimelineEasing easing, f32 atTime, f32 duration, f32 endVal) {
    TimelineComponent tlc;
    tlc.duration = duration;
    PropertyTrack track;
    track.targetProperty = "position.x";
    PropertyKeyframe k0; k0.time = 0.0f;       k0.value = 0.0f;
    PropertyKeyframe k1; k1.time = duration;    k1.value = endVal; k1.easing = easing;
    track.keyframes.push_back(k0);
    track.keyframes.push_back(k1);
    tlc.propertyTracks.push_back(track);

    Rig r;
    MakeRig(r, tlc, /*withTransform*/ true);
    r.tl->propertyTracks[0].targetEntity = r.entity;  // point the track at our entity
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, atTime);
    return r.world.GetComponent<ECS::TransformComponent>(r.entity)->position.x;
}
} // namespace

// ===========================================================================
// TimelineComponent Defaults
// ===========================================================================

ENJIN_TEST(Defaults, InitialState) {
    TimelineComponent tl;
    ENJIN_EXPECT_FLOAT_EQ(tl.duration, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(tl.playbackSpeed, 1.0f);
    ENJIN_EXPECT_FALSE(tl.isPlaying);
    ENJIN_EXPECT_FALSE(tl.loop);
    ENJIN_EXPECT_FALSE(tl.pingPong);
    ENJIN_EXPECT_FALSE(tl.isComplete);
    ENJIN_EXPECT_EQ(tl.direction, 1);
}

ENJIN_TEST(Defaults, EmptyTracks) {
    TimelineComponent tl;
    ENJIN_EXPECT_EQ(tl.propertyTracks.size(), (size_t)0);
    ENJIN_EXPECT_EQ(tl.eventTracks.size(), (size_t)0);
    ENJIN_EXPECT_EQ(tl.animationTracks.size(), (size_t)0);
}

// ===========================================================================
// Manual Control (Play/Pause/Stop/Seek)
// ===========================================================================

ENJIN_TEST(Control, PlaySetsIsPlaying) {
    TimelineComponent tl;
    tl.duration = 5.0f;
    TimelineSystem sys;
    sys.Play(tl);
    ENJIN_EXPECT_TRUE(tl.isPlaying);
    ENJIN_EXPECT_FALSE(tl.isComplete);
}

ENJIN_TEST(Control, PauseStopsPlayback) {
    TimelineComponent tl;
    tl.duration = 5.0f;
    TimelineSystem sys;
    sys.Play(tl);
    tl.currentTime = 2.0f;
    sys.Pause(tl);
    ENJIN_EXPECT_FALSE(tl.isPlaying);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 2.0f); // preserved
}

ENJIN_TEST(Control, StopResetsTime) {
    TimelineComponent tl;
    tl.duration = 5.0f;
    TimelineSystem sys;
    sys.Play(tl);
    tl.currentTime = 3.0f;
    sys.Stop(tl);
    ENJIN_EXPECT_FALSE(tl.isPlaying);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 0.0f);
    ENJIN_EXPECT_EQ(tl.direction, 1);
    ENJIN_EXPECT_FALSE(tl.isComplete);
}

ENJIN_TEST(Control, SeekClampsToRange) {
    TimelineComponent tl;
    tl.duration = 10.0f;
    TimelineSystem sys;

    sys.Seek(tl, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 5.0f);

    sys.Seek(tl, 15.0f);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 10.0f); // clamped to duration

    sys.Seek(tl, -5.0f);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 0.0f); // clamped to 0
}

// ===========================================================================
// Forward Playback (drives TimelineSystem::Update)
// ===========================================================================

ENJIN_TEST(Forward, TimeAdvances) {
    TimelineComponent tlc;
    tlc.duration = 5.0f;
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(r.tl->currentTime, 1.0f, 0.001f);
    ENJIN_EXPECT_TRUE(r.tl->isPlaying);
}

ENJIN_TEST(Forward, CompletesAtDuration) {
    TimelineComponent tlc;
    tlc.duration = 2.0f;
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 2.5f);  // step past the end
    ENJIN_EXPECT_TRUE(r.tl->isComplete);
    ENJIN_EXPECT_FALSE(r.tl->isPlaying);
    ENJIN_EXPECT_FLOAT_NEAR(r.tl->currentTime, 2.0f, 0.001f);
}

ENJIN_TEST(Forward, PlaybackSpeedScalesTime) {
    TimelineComponent tlc;
    tlc.duration = 10.0f;
    tlc.playbackSpeed = 2.0f;
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(r.tl->currentTime, 2.0f, 0.001f);  // 2x speed
}

// ===========================================================================
// Loop Mode (drives TimelineSystem::Update)
// ===========================================================================

ENJIN_TEST(Loop, TimeWraps) {
    TimelineComponent tlc;
    tlc.duration = 3.0f;
    tlc.loop = true;
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 3.5f);  // past end -> wraps to 0.5
    ENJIN_EXPECT_FLOAT_NEAR(r.tl->currentTime, 0.5f, 0.01f);
    ENJIN_EXPECT_TRUE(r.tl->isPlaying);
}

ENJIN_TEST(Loop, DoesNotComplete) {
    TimelineComponent tlc;
    tlc.duration = 2.0f;
    tlc.loop = true;
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 5.0f);  // way past end
    ENJIN_EXPECT_FALSE(r.tl->isComplete);
    ENJIN_EXPECT_TRUE(r.tl->isPlaying);
}

// ===========================================================================
// Ping-Pong Mode (drives TimelineSystem::Update)
// ===========================================================================

ENJIN_TEST(PingPong, DirectionFlipsAtEnd) {
    TimelineComponent tlc;
    tlc.duration = 2.0f;
    tlc.pingPong = true;
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 2.5f);  // hits the end, bounces
    ENJIN_EXPECT_EQ(r.tl->direction, -1);
    ENJIN_EXPECT_FLOAT_NEAR(r.tl->currentTime, 2.0f, 0.001f);
}

ENJIN_TEST(PingPong, DirectionFlipsAtStart) {
    TimelineComponent tlc;
    tlc.duration = 2.0f;
    tlc.pingPong = true;
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.tl->currentTime = 0.3f;
    r.tl->direction = -1;          // already moving backward
    r.sys.Update(&r.world, 1.0f);  // crosses 0, bounces forward
    ENJIN_EXPECT_EQ(r.tl->direction, 1);
    ENJIN_EXPECT_FLOAT_NEAR(r.tl->currentTime, 0.0f, 0.001f);
}

// ===========================================================================
// Event Tracks
// ===========================================================================

ENJIN_TEST(Events, DefaultNotFired) {
    TimelineEvent event;
    ENJIN_EXPECT_FALSE(event.fired);
    ENJIN_EXPECT_FLOAT_EQ(event.time, 0.0f);
}

ENJIN_TEST(Events, EventFiresAtTime) {
    // Drive the system past an event and confirm the engine marks it fired.
    TimelineComponent tlc;
    tlc.duration = 5.0f;
    EventTrack track;
    track.events.push_back({ 1.0f, "explode", "", false });
    tlc.eventTracks.push_back(track);
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 1.5f);  // crosses t=1.0
    ENJIN_EXPECT_TRUE(r.tl->eventTracks[0].events[0].fired);
}

ENJIN_TEST(Events, EventStaysFiredAcrossUpdates) {
    // After firing, a further step must not un-fire or error (idempotent).
    TimelineComponent tlc;
    tlc.duration = 5.0f;
    EventTrack track;
    track.events.push_back({ 1.0f, "explode", "", false });
    tlc.eventTracks.push_back(track);
    Rig r;
    MakeRig(r, tlc);
    r.sys.Play(*r.tl);
    r.sys.Update(&r.world, 1.5f);
    r.sys.Update(&r.world, 0.5f);
    ENJIN_EXPECT_TRUE(r.tl->eventTracks[0].events[0].fired);
}

ENJIN_TEST(Events, StopResetsEventFlags) {
    TimelineComponent tl;
    tl.duration = 5.0f;
    EventTrack track;
    track.events.push_back({ 1.0f, "test", "", true });
    track.events.push_back({ 2.0f, "test2", "", true });
    tl.eventTracks.push_back(track);

    TimelineSystem sys;
    sys.Stop(tl);

    for (auto& et : tl.eventTracks) {
        for (auto& ev : et.events) {
            ENJIN_EXPECT_FALSE(ev.fired);
        }
    }
}

// ===========================================================================
// PropertyTrack & Keyframes (storage)
// ===========================================================================

ENJIN_TEST(Keyframes, DefaultEasingIsLinear) {
    PropertyKeyframe kf;
    ENJIN_EXPECT_EQ((int)kf.easing, (int)TimelineEasing::Linear);
}

ENJIN_TEST(Keyframes, PropertyTrackDefaults) {
    PropertyTrack track;
    ENJIN_EXPECT_TRUE(track.enabled);
    ENJIN_EXPECT_EQ(track.keyframes.size(), (size_t)0);
    ENJIN_EXPECT_EQ(track.targetEntity, (u64)0);
}

ENJIN_TEST(Keyframes, FloatKeyframeStorage) {
    PropertyKeyframe kf;
    kf.time = 1.0f;
    kf.value = 42.0f;
    ENJIN_EXPECT_FLOAT_EQ(kf.time, 1.0f);
    ENJIN_EXPECT_TRUE(std::holds_alternative<f32>(kf.value));
    ENJIN_EXPECT_FLOAT_EQ(std::get<f32>(kf.value), 42.0f);
}

ENJIN_TEST(Keyframes, Vector3KeyframeStorage) {
    PropertyKeyframe kf;
    kf.value = Vector3(1, 2, 3);
    ENJIN_EXPECT_TRUE(std::holds_alternative<Vector3>(kf.value));
    Vector3 v = std::get<Vector3>(kf.value);
    ENJIN_EXPECT_FLOAT_EQ(v.x, 1.0f);
}

ENJIN_TEST(Keyframes, BoolKeyframeStorage) {
    PropertyKeyframe kf;
    kf.value = true;
    ENJIN_EXPECT_TRUE(std::holds_alternative<bool>(kf.value));
    ENJIN_EXPECT_TRUE(std::get<bool>(kf.value));
}

ENJIN_TEST(Keyframes, StringKeyframeStorage) {
    PropertyKeyframe kf;
    kf.value = std::string("hello");
    ENJIN_EXPECT_TRUE(std::holds_alternative<std::string>(kf.value));
    ENJIN_EXPECT_STR_EQ(std::get<std::string>(kf.value).c_str(), "hello");
}

// ===========================================================================
// AnimationTrack Defaults
// ===========================================================================

ENJIN_TEST(AnimTrack, Defaults) {
    AnimationTrack at;
    ENJIN_EXPECT_FLOAT_EQ(at.startTime, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(at.duration, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(at.blendWeight, 1.0f);
    ENJIN_EXPECT_TRUE(at.enabled);
}

// ===========================================================================
// Easing (drives the engine's easing through a property track)
// ===========================================================================

ENJIN_TEST(Easing, LinearMidpointIsHalfway) {
    // Linear from 0->10 over 2s, sampled at t=1, should be ~5.
    ENJIN_EXPECT_FLOAT_NEAR(EvalEasedTrack(TimelineEasing::Linear, 1.0f, 2.0f, 10.0f), 5.0f, 0.2f);
}

ENJIN_TEST(Easing, EaseInIsBehindLinear) {
    // Slow start: at the midpoint the value is below the linear halfway point.
    ENJIN_EXPECT_TRUE(EvalEasedTrack(TimelineEasing::EaseIn, 1.0f, 2.0f, 10.0f) < 4.8f);
}

ENJIN_TEST(Easing, EaseOutIsAheadOfLinear) {
    // Fast start: at the midpoint the value is above the linear halfway point.
    ENJIN_EXPECT_TRUE(EvalEasedTrack(TimelineEasing::EaseOut, 1.0f, 2.0f, 10.0f) > 5.2f);
}

ENJIN_TEST(Easing, StepHoldsUntilEnd) {
    // Step easing holds the start value until the segment completes.
    ENJIN_EXPECT_TRUE(EvalEasedTrack(TimelineEasing::Step, 1.0f, 2.0f, 10.0f) < 1.0f);
}

ENJIN_TEST_MAIN()
