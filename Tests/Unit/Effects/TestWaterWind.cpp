// Wind drives the water, and things float to the surface that is on screen.
//
// Two gaps, both on every platform rather than only on web:
//
// Water had no wind at all. waveDirection and waveHeight were authored
// constants, so a storm rolled in and the sea kept its calm-day swell pointing
// wherever the author had aimed it -- while the grass and the cloth beside it
// were already bending to the same WindSystem.
//
// And buoyancy floated things at a FLAT plane, the water entity's Y, while the
// surface visibly waved. A boat sat at the mean level and the swell passed
// straight through it. The more visible the waves, the more obviously wrong.
#include "EnjinTest.h"
#include "Enjin/Effects/Water.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

Water3D MakeWater(f32 windInfluence) {
    Water3DSettings s;
    s.waveHeight = 0.4f;
    s.waveSpeed = 1.0f;
    s.waveFrequency = 0.5f;
    s.waveDirection = Math::Vector2(1.0f, 0.0f);
    s.windInfluence = windInfluence;
    Water3D w;
    w.Initialize(s);
    return w;
}

// Peak-to-trough of the surface over a patch, which is what "how rough is it"
// actually means. A single sample can coincide with a zero crossing.
f32 SurfaceRange(const Water3D& w) {
    f32 lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 40; ++i) {
        const f32 x = static_cast<f32>(i) * 0.7f;
        const f32 h = w.GetWaveHeight(x, x * 0.3f);
        if (h < lo) lo = h;
        if (h > hi) hi = h;
    }
    return hi - lo;
}

} // namespace

ENJIN_TEST(WaterWind, WithNoInfluenceWindChangesNothing) {
    // Arrange: every scene authored before this existed. windInfluence
    // defaults to 0, and those waves must be bit-for-bit what they were.
    Water3D calm = MakeWater(0.0f);
    Water3D blown = MakeWater(0.0f);
    blown.SetWind(Math::Vector3(0.0f, 0.0f, 1.0f), 5.0f);

    // Act
    calm.Update(0.016f);
    blown.Update(0.016f);

    // Assert: identical surface, and the authored heading untouched.
    ENJIN_EXPECT_TRUE(std::fabs(calm.GetWaveHeight(3.0f, 2.0f) -
                                blown.GetWaveHeight(3.0f, 2.0f)) < 1e-6f);
    ENJIN_EXPECT_TRUE(std::fabs(blown.GetEffectiveSettings().waveDirection.x - 1.0f) < 1e-6f);
    ENJIN_EXPECT_TRUE(std::fabs(blown.GetEffectiveSettings().waveDirection.y - 0.0f) < 1e-6f);
}

ENJIN_TEST(WaterWind, TheWavesTurnTowardTheWind) {
    // Arrange: the authored heading is +X; the wind comes from +Z. Fully
    // wind-driven water should end up heading +Z.
    Water3D w = MakeWater(1.0f);
    w.SetWind(Math::Vector3(0.0f, 0.0f, 1.0f), 1.0f);

    // Act
    w.Update(0.016f);

    // Assert
    const Water3DSettings& eff = w.GetEffectiveSettings();
    ENJIN_EXPECT_TRUE(std::fabs(eff.waveDirection.x) < 0.01f);
    ENJIN_EXPECT_TRUE(std::fabs(eff.waveDirection.y - 1.0f) < 0.01f);
}

ENJIN_TEST(WaterWind, PartialInfluenceLandsBetweenTheTwoHeadings) {
    // Arrange: half-driven water keeps some of the swell the author asked for,
    // rather than snapping to the wind.
    Water3D w = MakeWater(0.5f);
    w.SetWind(Math::Vector3(0.0f, 0.0f, 1.0f), 1.0f);

    // Act
    w.Update(0.016f);

    // Assert: strictly between the authored (1,0) and the wind (0,1).
    const Water3DSettings& eff = w.GetEffectiveSettings();
    ENJIN_EXPECT_TRUE(eff.waveDirection.x > 0.01f && eff.waveDirection.x < 0.99f);
    ENJIN_EXPECT_TRUE(eff.waveDirection.y > 0.01f && eff.waveDirection.y < 0.99f);
}

ENJIN_TEST(WaterWind, StrongerWindMakesARougherSurface) {
    // Arrange: the point of the feature. A gale has to look different from a
    // calm day, not merely point elsewhere.
    Water3D calm = MakeWater(1.0f);
    Water3D gale = MakeWater(1.0f);
    calm.SetWind(Math::Vector3(1.0f, 0.0f, 0.0f), 0.0f);
    gale.SetWind(Math::Vector3(1.0f, 0.0f, 0.0f), 1.0f);

    // Act
    calm.Update(0.016f);
    gale.Update(0.016f);

    // Assert
    ENJIN_EXPECT_TRUE(gale.GetEffectiveSettings().waveHeight >
                      calm.GetEffectiveSettings().waveHeight);
    ENJIN_EXPECT_TRUE(SurfaceRange(gale) > SurfaceRange(calm));
}

ENJIN_TEST(WaterWind, AVerticalOrZeroWindLeavesTheHeadingAlone) {
    // Arrange: waveDirection is a 2D heading, so a wind with no horizontal
    // component has nothing to say about it. Normalising it anyway would
    // divide by zero and snap the sea somewhere arbitrary.
    Water3D up = MakeWater(1.0f);
    up.SetWind(Math::Vector3(0.0f, 5.0f, 0.0f), 1.0f);
    Water3D none = MakeWater(1.0f);
    none.SetWind(Math::Vector3(0.0f, 0.0f, 0.0f), 0.0f);

    // Act
    up.Update(0.016f);
    none.Update(0.016f);

    // Assert: authored heading intact, and finite.
    for (const Water3D* w : { &up, &none }) {
        const Water3DSettings& eff = w->GetEffectiveSettings();
        ENJIN_EXPECT_TRUE(std::fabs(eff.waveDirection.x - 1.0f) < 1e-4f);
        ENJIN_EXPECT_TRUE(eff.waveDirection.y == eff.waveDirection.y);   // not NaN
    }
}

ENJIN_TEST(WaterWind, TheSharedSamplerAgreesWithTheInstance) {
    // Arrange: physics samples the surface through the static sampler, using
    // the settings and clock the water published. If those two ever disagreed,
    // a boat would float to a surface that is not the one being drawn -- which
    // is the whole bug being fixed.
    Water3D w = MakeWater(1.0f);
    w.SetWind(Math::Vector3(0.0f, 0.0f, 1.0f), 0.8f);
    for (int i = 0; i < 10; ++i) w.Update(0.016f);

    // Act / Assert
    for (f32 x = -5.0f; x <= 5.0f; x += 2.5f) {
        const f32 viaInstance = w.GetWaveHeight(x, x * 0.5f);
        const f32 viaSampler = Water3D::SampleWaveHeight(
            w.GetEffectiveSettings(), w.GetWaveTime(), x, x * 0.5f);
        ENJIN_EXPECT_TRUE(std::fabs(viaInstance - viaSampler) < 1e-5f);
    }
}

ENJIN_TEST(WaterWind, TheSurfaceIsNotFlat) {
    // Arrange: the premise buoyancy now depends on. If the surface were flat,
    // sampling it per-body would be pointless and the old mean-level code
    // would have been right.
    Water3D w = MakeWater(0.0f);
    for (int i = 0; i < 5; ++i) w.Update(0.016f);

    // Act / Assert
    ENJIN_EXPECT_TRUE(SurfaceRange(w) > 0.01f);
}

ENJIN_TEST(WaterWind, GerstnerWaterAlsoSamplesConsistently) {
    // Arrange: the trochoidal path is a different branch, and buoyancy takes
    // it whenever a project turns Gerstner on.
    Water3DSettings s;
    s.gerstnerWaves = true;
    s.waveSteepness = 0.7f;
    s.waveHeight = 0.5f;
    s.windInfluence = 1.0f;
    Water3D w;
    w.Initialize(s);
    w.SetWind(Math::Vector3(1.0f, 0.0f, 0.0f), 1.0f);
    for (int i = 0; i < 5; ++i) w.Update(0.016f);

    // Act / Assert
    const f32 viaInstance = w.GetWaveHeight(2.0f, 1.0f);
    const f32 viaSampler = Water3D::SampleWaveHeight(
        w.GetEffectiveSettings(), w.GetWaveTime(), 2.0f, 1.0f);
    ENJIN_EXPECT_TRUE(std::fabs(viaInstance - viaSampler) < 1e-5f);
    ENJIN_EXPECT_TRUE(viaInstance == viaInstance);        // not NaN
}

ENJIN_TEST_MAIN()
