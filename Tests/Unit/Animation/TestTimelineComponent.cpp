#include "EnjinTest.h"
#include "Enjin/Animation/Timeline.h"

using namespace Enjin;
using namespace Enjin::Animation;

// ===========================================================================
// TimelineEasing Enum
// ===========================================================================

ENJIN_TEST(TimelineEasingEnum, Values) {
    ENJIN_EXPECT_EQ((int)TimelineEasing::Linear, 0);
    ENJIN_EXPECT_EQ((int)TimelineEasing::EaseIn, 1);
    ENJIN_EXPECT_EQ((int)TimelineEasing::EaseOut, 2);
    ENJIN_EXPECT_EQ((int)TimelineEasing::EaseInOut, 3);
    ENJIN_EXPECT_EQ((int)TimelineEasing::Step, 4);
}

// ===========================================================================
// PropertyKeyframe Defaults
// ===========================================================================

ENJIN_TEST(PropertyKeyframe, TimeDefault) {
    PropertyKeyframe kf;
    ENJIN_EXPECT_FLOAT_EQ(kf.time, 0.0f);
}

ENJIN_TEST(PropertyKeyframe, EasingDefault) {
    PropertyKeyframe kf;
    ENJIN_EXPECT_EQ((int)kf.easing, (int)TimelineEasing::Linear);
}

// ===========================================================================
// PropertyTrack Defaults
// ===========================================================================

ENJIN_TEST(PropertyTrack, Defaults) {
    PropertyTrack track;
    ENJIN_EXPECT_TRUE(track.targetProperty.empty());
    ENJIN_EXPECT_EQ(track.targetEntity, (ECS::Entity)0);
    ENJIN_EXPECT_EQ(track.keyframes.size(), (size_t)0);
    ENJIN_EXPECT_TRUE(track.enabled);
}

// ===========================================================================
// TimelineEvent Defaults
// ===========================================================================

ENJIN_TEST(TimelineEvent, Defaults) {
    TimelineEvent evt;
    ENJIN_EXPECT_FLOAT_EQ(evt.time, 0.0f);
    ENJIN_EXPECT_TRUE(evt.eventName.empty());
    ENJIN_EXPECT_TRUE(evt.eventData.empty());
    ENJIN_EXPECT_FALSE(evt.fired);
}

// ===========================================================================
// EventTrack Defaults
// ===========================================================================

ENJIN_TEST(EventTrack, Defaults) {
    EventTrack track;
    ENJIN_EXPECT_TRUE(track.name.empty());
    ENJIN_EXPECT_EQ(track.events.size(), (size_t)0);
    ENJIN_EXPECT_TRUE(track.enabled);
}

// ===========================================================================
// AnimationTrack Defaults
// ===========================================================================

ENJIN_TEST(AnimationTrack, Defaults) {
    AnimationTrack track;
    ENJIN_EXPECT_FLOAT_EQ(track.startTime, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(track.duration, 0.0f);
    ENJIN_EXPECT_TRUE(track.animationName.empty());
    ENJIN_EXPECT_EQ(track.targetEntity, (ECS::Entity)0);
    ENJIN_EXPECT_FLOAT_EQ(track.blendWeight, 1.0f);
    ENJIN_EXPECT_TRUE(track.enabled);
}

// ===========================================================================
// TimelineComponent Defaults
// ===========================================================================

ENJIN_TEST(TimelineComponent, EmptyTracks) {
    TimelineComponent tl;
    ENJIN_EXPECT_EQ(tl.propertyTracks.size(), (size_t)0);
    ENJIN_EXPECT_EQ(tl.eventTracks.size(), (size_t)0);
    ENJIN_EXPECT_EQ(tl.animationTracks.size(), (size_t)0);
}

ENJIN_TEST(TimelineComponent, PlaybackDefaults) {
    TimelineComponent tl;
    ENJIN_EXPECT_FLOAT_EQ(tl.duration, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(tl.playbackSpeed, 1.0f);
    ENJIN_EXPECT_FALSE(tl.isPlaying);
    ENJIN_EXPECT_FALSE(tl.loop);
    ENJIN_EXPECT_FALSE(tl.pingPong);
    ENJIN_EXPECT_FALSE(tl.playOnAwake);
    ENJIN_EXPECT_FALSE(tl.isComplete);
}

ENJIN_TEST(TimelineComponent, DirectionDefault) {
    TimelineComponent tl;
    ENJIN_EXPECT_EQ(tl.direction, 1);
}

ENJIN_TEST(TimelineComponent, NotifyDefaults) {
    TimelineComponent tl;
    ENJIN_EXPECT_FALSE(tl.onCompleteNotify);
    ENJIN_EXPECT_FALSE(tl.onLoopNotify);
}

// ===========================================================================
// TimelineSystem Control
// ===========================================================================

ENJIN_TEST(TimelineSystem, PlaySetsPlaying) {
    TimelineSystem sys;
    TimelineComponent tl;
    ENJIN_EXPECT_FALSE(tl.isPlaying);
    sys.Play(tl);
    ENJIN_EXPECT_TRUE(tl.isPlaying);
}

ENJIN_TEST(TimelineSystem, PauseClearsPlaying) {
    TimelineSystem sys;
    TimelineComponent tl;
    sys.Play(tl);
    sys.Pause(tl);
    ENJIN_EXPECT_FALSE(tl.isPlaying);
}

ENJIN_TEST(TimelineSystem, StopResetsTime) {
    TimelineSystem sys;
    TimelineComponent tl;
    tl.currentTime = 5.0f;
    sys.Play(tl);
    sys.Stop(tl);
    ENJIN_EXPECT_FALSE(tl.isPlaying);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 0.0f);
}

ENJIN_TEST(TimelineSystem, SeekSetsTime) {
    TimelineSystem sys;
    TimelineComponent tl;
    tl.duration = 10.0f;
    sys.Seek(tl, 7.5f);
    ENJIN_EXPECT_FLOAT_EQ(tl.currentTime, 7.5f);
}

ENJIN_TEST_MAIN()
