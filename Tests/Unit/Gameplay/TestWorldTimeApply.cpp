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
#include "Enjin/ECS/Systems/RenderSystem.h"

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

// The sky's sun and the sun light are one sun. World time turned the light and
// left the sky on the scene's saved direction, and with world time off nothing
// linked them either: the Playground drew its sun disc under the map while the
// light came from above.
ENJIN_TEST(WorldTimeApply, TheSkySunIsWhereTheLightComesFrom) {
    ECS::World w;
    const ECS::Entity sun = MakeSun(w);
    ECS::RenderSystem render(&w, nullptr);   // no GPU: only the sky state is exercised
    Renderer::SkyboxConfig saved;
    saved.sunDirection = Math::Vector3(0.35f, -0.7f, 0.4f);   // the Playground's, below the horizon

    // World time at noon: the sky sun is overhead, opposite the light's travel
    WorldTimeSystem time;
    time.SetTime(12.0f, 1, 6, 1);
    UpdateAndApplyWorldTime(&w, time, &render, nullptr, nullptr, 0.0f);
    const Math::Vector3 skyNoon = render.WeatherSky(saved).sunDirection;
    ENJIN_EXPECT_TRUE(skyNoon.y > 0.5f);

    // World time off: the sky follows the light, not the saved field
    w.GetComponent<ECS::TransformComponent>(sun)->rotation =
        Math::Quaternion(-0.34f, -0.1f, 0.0f, 0.93f).Normalized();
    SyncSkySunToSunLight(&w, &render);
    const Math::Vector3 sky = render.WeatherSky(saved).sunDirection;
    const Math::Vector3 travel = LightTravelDir(w, sun);
    ENJIN_EXPECT_TRUE(sky.y > 0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(sky.x, -travel.x, 1e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(sky.y, -travel.y, 1e-4f);

    // No sun light at all: the saved field stands
    ECS::World empty;
    SyncSkySunToSunLight(&empty, &render);
    ENJIN_EXPECT_FLOAT_NEAR(render.WeatherSky(saved).sunDirection.y, -0.7f, 1e-6f);
}

ENJIN_TEST_MAIN()
