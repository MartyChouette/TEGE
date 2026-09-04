// The sun must never light the world from underneath.
//
// WorldTimeSystem::GetSunDirection returns the direction light TRAVELS, so
// above the horizon its y is negative. Below the horizon it flips positive --
// and a directional light pointing upward lights the underside of every object
// and the floor itself, which reads as light bleeding out from under things.
//
// Night is lit by a moon, and a moon is in the sky. UpdateAndApplyWorldTime
// mirrors the direction back below the horizon; colour and intensity are what
// make it read as moonlight.
//
// This is exercised through the shared apply function on purpose: that is the
// one place all three runtimes get their sun from, so this is the level the
// invariant has to hold at.
#include "EnjinTest.h"
#include "Enjin/Effects/WorldTimeApply.h"
#include "Enjin/Effects/WorldTime.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

// A world with one directional light, the thing the sun drives.
ECS::Entity MakeSun(ECS::World& w) {
    const ECS::Entity e = w.CreateEntity();
    w.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::LightComponent lc;
    lc.type = ECS::LightType::Directional;
    lc.intensity = 1.0f;
    w.AddComponent<ECS::LightComponent>(e, lc);
    return e;
}

// The direction the light travels, taken from the transform the same way the
// renderer takes it.
Math::Vector3 LightTravelDir(ECS::World& w, ECS::Entity e) {
    return w.GetComponent<ECS::TransformComponent>(e)->rotation.GetForward();
}

} // namespace

ENJIN_TEST(WorldTimeApply, MiddayLightComesFromAbove) {
    // Arrange
    ECS::World w;
    const ECS::Entity sun = MakeSun(w);
    WorldTimeSystem time;
    time.SetTime(12.0f, 1, 6, 1);

    // Act
    UpdateAndApplyWorldTime(&w, time, nullptr, nullptr, nullptr, 0.0f);

    // Assert: travelling downward.
    ENJIN_EXPECT_TRUE(LightTravelDir(w, sun).y < 0.0f);
}

ENJIN_TEST(WorldTimeApply, MidnightLightStillComesFromAbove) {
    // Arrange: the hour that produced the bug. The sun is well below the
    // horizon, so the raw direction points up.
    ECS::World w;
    const ECS::Entity sun = MakeSun(w);
    WorldTimeSystem time;
    time.SetTime(0.0f, 1, 6, 1);

    // Act
    UpdateAndApplyWorldTime(&w, time, nullptr, nullptr, nullptr, 0.0f);

    // Assert
    ENJIN_EXPECT_TRUE(LightTravelDir(w, sun).y < 0.0f);
}

ENJIN_TEST(WorldTimeApply, NoHourOfTheDayLightsTheWorldFromBelow) {
    // Arrange / Act / Assert: sweep a whole day at ten-minute steps. Sunrise
    // and sunset are where a fixed sun vector crosses zero, so a spot check at
    // noon and midnight would miss exactly the hours that break.
    for (int step = 0; step < 144; ++step) {
        ECS::World w;
        const ECS::Entity sun = MakeSun(w);
        WorldTimeSystem time;
        time.SetTime(static_cast<f32>(step) * (24.0f / 144.0f), 1, 6, 1);

        UpdateAndApplyWorldTime(&w, time, nullptr, nullptr, nullptr, 0.0f);

        ENJIN_EXPECT_TRUE(LightTravelDir(w, sun).y < 0.0f);
    }
}

ENJIN_TEST(WorldTimeApply, NightIsDimmerAndCoolerThanNoon) {
    // Arrange: what makes the mirrored direction read as moonlight rather than
    // a second sun.
    ECS::World w;
    const ECS::Entity sun = MakeSun(w);
    WorldTimeSystem time;

    // Act
    time.SetTime(12.0f, 1, 6, 1);
    UpdateAndApplyWorldTime(&w, time, nullptr, nullptr, nullptr, 0.0f);
    const f32 dayIntensity = w.GetComponent<ECS::LightComponent>(sun)->intensity;
    const Math::Vector3 dayColor = w.GetComponent<ECS::LightComponent>(sun)->color;

    time.SetTime(0.0f, 1, 6, 1);
    UpdateAndApplyWorldTime(&w, time, nullptr, nullptr, nullptr, 0.0f);
    const f32 nightIntensity = w.GetComponent<ECS::LightComponent>(sun)->intensity;
    const Math::Vector3 nightColor = w.GetComponent<ECS::LightComponent>(sun)->color;

    // Assert: dimmer, and blue-shifted rather than warm.
    ENJIN_EXPECT_TRUE(nightIntensity < dayIntensity);
    ENJIN_EXPECT_TRUE(nightColor.z > nightColor.x);
    ENJIN_EXPECT_TRUE(dayColor.x >= dayColor.z);
}

ENJIN_TEST(WorldTimeApply, AWorldWithNoDirectionalLightIsNotACrash) {
    // Arrange: plenty of scenes have no sun at all.
    ECS::World w;
    const ECS::Entity e = w.CreateEntity();
    w.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});

    // Act / Assert
    WorldTimeSystem time;
    UpdateAndApplyWorldTime(&w, time, nullptr, nullptr, nullptr, 0.016f);
    ENJIN_EXPECT_TRUE(w.IsValid(e));
}

ENJIN_TEST_MAIN()
