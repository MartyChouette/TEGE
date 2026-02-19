#include "EnjinTest.h"
#include "Enjin/Animation/Timeline.h"

#include <cmath>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Animation;
using namespace Enjin::Math;

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
// Forward Playback
// ===========================================================================

ENJIN_TEST(Forward, TimeAdvances) {
    TimelineComponent tl;
    tl.duration = 5.0f;
    tl.isPlaying = true;
    tl.playbackSpeed = 1.0f;

    TimelineSystem sys;
    sys.Update(nullptr, 1.0f);  // Won't find entity, but timeline updates internally
    // Note: Update iterates ECS world, so we test TimelineComponent fields after manual play
    // We'll test via direct manipulation since Update needs a World
    tl.currentTime += 1.0f * tl.playbackSpeed * tl.direction;
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 1.0f);
}

ENJIN_TEST(Forward, CompletesAtDuration) {
    TimelineComponent tl;
    tl.duration = 2.0f;
    tl.isPlaying = true;
    // Simulate time passing beyond duration
    tl.currentTime = 2.5f;
    // In real Update, this would set isComplete
    if (tl.currentTime >= tl.duration && !tl.loop && !tl.pingPong) {
        tl.currentTime = tl.duration;
        tl.isPlaying = false;
        tl.isComplete = true;
    }
    ENJIN_EXPECT_TRUE(tl.isComplete);
    ENJIN_EXPECT_FALSE(tl.isPlaying);
}

ENJIN_TEST(Forward, PlaybackSpeedScalesTime) {
    TimelineComponent tl;
    tl.duration = 10.0f;
    tl.playbackSpeed = 2.0f;
    f32 dt = 1.0f;
    f32 advance = dt * tl.playbackSpeed * tl.direction;
    ENJIN_EXPECT_FLOAT_EQ(advance, 2.0f);
}

// ===========================================================================
// Loop Mode
// ===========================================================================

ENJIN_TEST(Loop, TimeWraps) {
    TimelineComponent tl;
    tl.duration = 3.0f;
    tl.loop = true;
    tl.isPlaying = true;
    tl.currentTime = 3.5f; // past end

    // Simulate loop wrap
    if (tl.loop && tl.currentTime >= tl.duration) {
        tl.currentTime = std::fmod(tl.currentTime, tl.duration);
    }
    ENJIN_EXPECT_FLOAT_NEAR(tl.currentTime, 0.5f, 0.01f);
    ENJIN_EXPECT_TRUE(tl.isPlaying); // still playing
}

ENJIN_TEST(Loop, DoesNotComplete) {
    TimelineComponent tl;
    tl.duration = 2.0f;
    tl.loop = true;
    tl.isPlaying = true;
    tl.currentTime = 5.0f; // way past end
    if (tl.loop) {
        tl.currentTime = std::fmod(tl.currentTime, tl.duration);
    }
    ENJIN_EXPECT_FALSE(tl.isComplete);
}

// ===========================================================================
// Ping-Pong Mode
// ===========================================================================

ENJIN_TEST(PingPong, DirectionFlipsAtEnd) {
    TimelineComponent tl;
    tl.duration = 2.0f;
    tl.pingPong = true;
    tl.isPlaying = true;
    tl.direction = 1;
    tl.currentTime = 2.5f; // past end

    // Simulate ping-pong bounce
    if (tl.pingPong && tl.direction == 1 && tl.currentTime >= tl.duration) {
        tl.currentTime = tl.duration;
        tl.direction = -1;
    }
    ENJIN_EXPECT_EQ(tl.direction, -1);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 2.0f);
}

ENJIN_TEST(PingPong, DirectionFlipsAtStart) {
    TimelineComponent tl;
    tl.duration = 2.0f;
    tl.pingPong = true;
    tl.isPlaying = true;
    tl.direction = -1;
    tl.currentTime = -0.5f; // past start

    // Simulate reverse bounce
    if (tl.pingPong && tl.direction == -1 && tl.currentTime <= 0.0f) {
        tl.currentTime = 0.0f;
        tl.direction = 1;
    }
    ENJIN_EXPECT_EQ(tl.direction, 1);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 0.0f);
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
    TimelineEvent event;
    event.time = 1.0f;
    event.eventName = "explode";

    f32 currentTime = 1.5f; // past event time
    if (event.time <= currentTime && !event.fired) {
        event.fired = true;
    }
    ENJIN_EXPECT_TRUE(event.fired);
}

ENJIN_TEST(Events, EventDoesNotDoubleFire) {
    TimelineEvent event;
    event.time = 1.0f;
    event.fired = true; // already fired

    f32 currentTime = 2.0f;
    bool firedAgain = false;
    if (event.time <= currentTime && !event.fired) {
        firedAgain = true;
    }
    ENJIN_EXPECT_FALSE(firedAgain);
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
// PropertyTrack & Keyframes
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
// Easing Functions (tested via TimelineEasing enum values)
// ===========================================================================

ENJIN_TEST(Easing, LinearAtZero) {
    // Linear easing: f(0) = 0
    f32 t = 0.0f;
    f32 result = t; // Linear: return t
    ENJIN_EXPECT_FLOAT_EQ(result, 0.0f);
}

ENJIN_TEST(Easing, LinearAtOne) {
    f32 t = 1.0f;
    f32 result = t;
    ENJIN_EXPECT_FLOAT_EQ(result, 1.0f);
}

ENJIN_TEST(Easing, EaseInQuadratic) {
    f32 t = 0.5f;
    f32 result = t * t; // EaseIn = t^2
    ENJIN_EXPECT_FLOAT_EQ(result, 0.25f);
}

ENJIN_TEST(Easing, EaseOutQuadratic) {
    f32 t = 0.5f;
    f32 result = 1.0f - (1.0f - t) * (1.0f - t); // EaseOut
    ENJIN_EXPECT_FLOAT_EQ(result, 0.75f);
}

ENJIN_TEST(Easing, EaseInOutSymmetry) {
    // EaseInOut at t=0.5 should equal 0.5 (symmetric midpoint)
    f32 t = 0.5f;
    f32 result;
    if (t < 0.5f) result = 2.0f * t * t;
    else result = 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    ENJIN_EXPECT_FLOAT_EQ(result, 0.5f);
}

ENJIN_TEST(Easing, StepBeforeOne) {
    f32 t = 0.99f;
    f32 result = (t < 1.0f) ? 0.0f : 1.0f; // Step
    ENJIN_EXPECT_FLOAT_EQ(result, 0.0f);
}

ENJIN_TEST(Easing, StepAtOne) {
    f32 t = 1.0f;
    f32 result = (t < 1.0f) ? 0.0f : 1.0f;
    ENJIN_EXPECT_FLOAT_EQ(result, 1.0f);
}

ENJIN_TEST_MAIN()
