#include "EnjinTest.h"
#include "Enjin/Gameplay/CinematicSystem.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

// ===========================================================================
// CinematicCameraComponent::Waypoint Defaults
// ===========================================================================

ENJIN_TEST(Waypoint, Defaults) {
    ECS::CinematicCameraComponent::Waypoint wp;
    ENJIN_EXPECT_FLOAT_EQ(wp.fov, 60.0f);
    ENJIN_EXPECT_FLOAT_EQ(wp.duration, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(wp.holdTime, 0.0f);
    ENJIN_EXPECT_EQ((int)wp.easing,
                    (int)ECS::CinematicCameraComponent::Waypoint::Easing::EaseInOut);
}

ENJIN_TEST(Waypoint, EasingValues) {
    using Easing = ECS::CinematicCameraComponent::Waypoint::Easing;
    ENJIN_EXPECT_EQ((int)Easing::Linear, 0);
    ENJIN_EXPECT_EQ((int)Easing::EaseIn, 1);
    ENJIN_EXPECT_EQ((int)Easing::EaseOut, 2);
    ENJIN_EXPECT_EQ((int)Easing::EaseInOut, 3);
    ENJIN_EXPECT_EQ((int)Easing::SmashCut, 4);
}

ENJIN_TEST(Waypoint, CustomValues) {
    ECS::CinematicCameraComponent::Waypoint wp;
    wp.position = Math::Vector3(10, 5, 20);
    wp.lookAt = Math::Vector3(0, 0, 0);
    wp.fov = 90.0f;
    wp.duration = 5.0f;
    wp.holdTime = 1.0f;
    ENJIN_EXPECT_FLOAT_EQ(wp.fov, 90.0f);
    ENJIN_EXPECT_FLOAT_EQ(wp.duration, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(wp.holdTime, 1.0f);
}

// ===========================================================================
// CinematicCameraComponent Defaults
// ===========================================================================

ENJIN_TEST(Component, Defaults) {
    ECS::CinematicCameraComponent comp;
    ENJIN_EXPECT_FALSE(comp.isPlaying);
    ENJIN_EXPECT_FALSE(comp.loop);
    ENJIN_EXPECT_FALSE(comp.autoPlay);
    ENJIN_EXPECT_TRUE(comp.hideHUD);
    ENJIN_EXPECT_TRUE(comp.disableInput);
    ENJIN_EXPECT_FLOAT_EQ(comp.currentTime, 0.0f);
    ENJIN_EXPECT_EQ(comp.currentSegment, 0);
    ENJIN_EXPECT_FLOAT_EQ(comp.segmentProgress, 0.0f);
    ENJIN_EXPECT_FALSE(comp.isComplete);
}

ENJIN_TEST(Component, EmptyWaypoints) {
    ECS::CinematicCameraComponent comp;
    ENJIN_EXPECT_EQ(comp.waypoints.size(), (size_t)0);
}

ENJIN_TEST(Component, AddWaypoints) {
    ECS::CinematicCameraComponent comp;
    ECS::CinematicCameraComponent::Waypoint wp1, wp2;
    wp1.position = Math::Vector3(0, 0, 0);
    wp1.lookAt = Math::Vector3(0, 0, 10);
    wp2.position = Math::Vector3(10, 5, 0);
    wp2.lookAt = Math::Vector3(0, 0, 0);
    comp.waypoints.push_back(wp1);
    comp.waypoints.push_back(wp2);
    ENJIN_EXPECT_EQ(comp.waypoints.size(), (size_t)2);
}

// ===========================================================================
// CinematicSystem Enable/Disable
// ===========================================================================

ENJIN_TEST(System, DefaultDisabled) {
    CinematicSystem sys;
    ENJIN_EXPECT_FALSE(sys.IsEnabled());
}

ENJIN_TEST(System, EnableDisable) {
    CinematicSystem sys;
    sys.SetEnabled(true);
    ENJIN_EXPECT_TRUE(sys.IsEnabled());
    sys.SetEnabled(false);
    ENJIN_EXPECT_FALSE(sys.IsEnabled());
}

ENJIN_TEST_MAIN()
