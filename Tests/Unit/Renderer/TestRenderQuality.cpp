// A project quality tier is a CEILING on render cost, never a look.
//
// ADR-0006: a player who picks "Medium" to make the game run must not have that
// undone by whatever a scene saved. The inverse matters just as much, and is the
// easier thing to get wrong: Ultra must not force an intentionally cheap or
// stylized scene to become expensive. So every knob here clamps downward only,
// and nothing artistic is touched at all.
//
// ApplyTo is pure, which is why these tests need no renderer and why desktop and
// web share one implementation instead of two that drift.
#include "EnjinTest.h"
#include "Enjin/Renderer/RenderQualitySettings.h"

#include <nlohmann/json.hpp>

#include <cmath>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {

bool Near(f32 a, f32 b, f32 eps = 0.001f) { return std::fabs(a - b) < eps; }

// An expensive scene, so every ceiling has something to bite on.
SceneRenderSettings ExpensiveScene() {
    SceneRenderSettings s;
    s.rtEnabled = true;
    s.rtMode = 1;                       // path tracing
    s.rtPathTracerTargetSPP = 4096;
    s.rtGIBounces = 8;
    s.rtDenoiserIterations = 8;
    s.restirInitialCandidates = 32;
    s.restirSpatialReuse = true;
    s.restirSpatialNeighbors = 16;
    s.surfelCacheEnabled = true;
    s.surfelCacheMaxSurfels = 262144;
    s.radianceCacheEnabled = true;
    s.adaptiveRayMinPerPixel = 4;
    s.adaptiveRayMaxPerPixel = 16;
    s.ddgiRaysPerProbe = 256;
    s.ddgiAmortizationRate = 1;
    return s;
}

} // namespace

ENJIN_TEST(RenderQuality, DisabledByDefaultSoExistingProjectsAreUntouched) {
    RenderQualitySettings q;
    ENJIN_EXPECT_FALSE(q.enabled);

    SceneRenderSettings s = ExpensiveScene();
    q.ApplyTo(s, QualityTier::Low);   // the cheapest tier, and still a no-op

    ENJIN_EXPECT_TRUE(s.rtEnabled);
    ENJIN_EXPECT_EQ(s.rtPathTracerTargetSPP, 4096u);
    ENJIN_EXPECT_EQ(s.rtGIBounces, 8u);
}

ENJIN_TEST(RenderQuality, LowTurnsRayTracingOffEntirely) {
    RenderQualitySettings q;
    q.enabled = true;
    SceneRenderSettings s = ExpensiveScene();
    q.ApplyTo(s, QualityTier::Low);

    // A cheap RT pass is still an RT pass, so the master gate is what Low uses.
    ENJIN_EXPECT_FALSE(s.rtEnabled);
    ENJIN_EXPECT_FALSE(s.surfelCacheEnabled);
    ENJIN_EXPECT_FALSE(s.radianceCacheEnabled);
    ENJIN_EXPECT_FALSE(s.restirSpatialReuse);
    ENJIN_EXPECT_EQ(s.rtGIBounces, 0u);
}

ENJIN_TEST(RenderQuality, MediumFallsBackToHybridInsteadOfRefusingToRender) {
    RenderQualitySettings q;
    q.enabled = true;
    SceneRenderSettings s = ExpensiveScene();
    q.ApplyTo(s, QualityTier::Medium);

    // Path tracing is disallowed, but the scene still has to draw.
    ENJIN_EXPECT_TRUE(s.rtEnabled);
    ENJIN_EXPECT_EQ(s.rtMode, 0u);
    ENJIN_EXPECT_EQ(s.rtGIBounces, 1u);
    ENJIN_EXPECT_EQ(s.rtDenoiserIterations, 3u);
    ENJIN_EXPECT_EQ(s.restirInitialCandidates, 8u);
}

ENJIN_TEST(RenderQuality, UltraNeverMakesACheapSceneExpensive) {
    RenderQualitySettings q;
    q.enabled = true;

    // A scene deliberately authored cheap: one bounce, few samples, no surfels.
    SceneRenderSettings s;
    s.rtEnabled = true;
    s.rtGIBounces = 1;
    s.rtPathTracerTargetSPP = 16;
    s.restirInitialCandidates = 2;
    s.surfelCacheEnabled = false;
    s.ddgiRaysPerProbe = 32;

    q.ApplyTo(s, QualityTier::Ultra);

    // The authored value is the artistic intent. A tier exists to go cheaper.
    ENJIN_EXPECT_EQ(s.rtGIBounces, 1u);
    ENJIN_EXPECT_EQ(s.rtPathTracerTargetSPP, 16u);
    ENJIN_EXPECT_EQ(s.restirInitialCandidates, 2u);
    ENJIN_EXPECT_FALSE(s.surfelCacheEnabled);
    ENJIN_EXPECT_EQ(s.ddgiRaysPerProbe, 32u);
}

// Amortization is "update one probe in N", so cheaper is a BIGGER number. It is
// the one field here that is a floor, and getting it backwards would make Low
// the most expensive tier for DDGI.
ENJIN_TEST(RenderQuality, DdgiAmortizationIsAFloorNotACeiling) {
    RenderQualitySettings q;
    q.enabled = true;

    SceneRenderSettings s = ExpensiveScene();
    s.ddgiAmortizationRate = 1;          // most expensive: every probe, every frame
    q.ApplyTo(s, QualityTier::Low);
    ENJIN_EXPECT_EQ(s.ddgiAmortizationRate, 16u);

    // A scene already cheaper than the floor keeps its own value.
    SceneRenderSettings cheap;
    cheap.ddgiAmortizationRate = 64;
    q.ApplyTo(cheap, QualityTier::Low);
    ENJIN_EXPECT_EQ(cheap.ddgiAmortizationRate, 64u);
}

// Low caps the max ray count at 1. If the min is not pulled down with it the
// budget range ends up inverted (min 4, max 1).
ENJIN_TEST(RenderQuality, AdaptiveRayMinNeverExceedsTheClampedMax) {
    RenderQualitySettings q;
    q.enabled = true;
    SceneRenderSettings s = ExpensiveScene();
    q.ApplyTo(s, QualityTier::Low);

    ENJIN_EXPECT_EQ(s.adaptiveRayMaxPerPixel, 1u);
    ENJIN_EXPECT_TRUE(s.adaptiveRayMinPerPixel <= s.adaptiveRayMaxPerPixel);
}

// The whole promise of "clamps cost, not look".
ENJIN_TEST(RenderQuality, ArtisticValuesAreNeverTouchedByATier) {
    RenderQualitySettings q;
    q.enabled = true;

    SceneRenderSettings s = ExpensiveScene();
    s.rtGIIntensity = 2.5f;
    s.rtShadowStrength = 0.75f;
    s.rtReflectionStrength = 0.9f;
    s.volumetricFogDensity = 0.08f;
    s.volumetricFogColor = Math::Vector3(0.2f, 0.4f, 0.6f);
    s.fogDensity = 0.03f;

    q.ApplyTo(s, QualityTier::Low);   // the most aggressive tier there is

    ENJIN_EXPECT_TRUE(Near(s.rtGIIntensity, 2.5f));
    ENJIN_EXPECT_TRUE(Near(s.rtShadowStrength, 0.75f));
    ENJIN_EXPECT_TRUE(Near(s.rtReflectionStrength, 0.9f));
    ENJIN_EXPECT_TRUE(Near(s.volumetricFogDensity, 0.08f));
    ENJIN_EXPECT_TRUE(Near(s.volumetricFogColor.z, 0.6f));
    ENJIN_EXPECT_TRUE(Near(s.fogDensity, 0.03f));
}

ENJIN_TEST(RenderQuality, TierSettingsSurviveASave) {
    RenderQualitySettings q;
    q.enabled = true;
    q.defaultTier = QualityTier::Medium;
    q.playerCanChange = false;
    q.CapsFor(QualityTier::High).maxGIBounces = 3;
    q.CapsFor(QualityTier::High).allowPathTracing = false;
    q.CapsFor(QualityTier::Custom).maxSurfelCount = 4096;

    const auto out = DeserializeRenderQuality(SerializeRenderQuality(q));

    ENJIN_EXPECT_TRUE(out.enabled);
    ENJIN_EXPECT_FALSE(out.playerCanChange);
    ENJIN_EXPECT_TRUE(out.defaultTier == QualityTier::Medium);
    ENJIN_EXPECT_EQ(out.CapsFor(QualityTier::High).maxGIBounces, 3u);
    ENJIN_EXPECT_FALSE(out.CapsFor(QualityTier::High).allowPathTracing);
    ENJIN_EXPECT_EQ(out.CapsFor(QualityTier::Custom).maxSurfelCount, 4096u);
}

// A project file written before tiers existed, or one that authored only a
// single tier, must still come back with usable presets for the others.
ENJIN_TEST(RenderQuality, AProjectWithoutTierKeysKeepsTheBuiltInPresets) {
    const auto out = DeserializeRenderQuality(nlohmann::json::object());

    ENJIN_EXPECT_FALSE(out.enabled);
    ENJIN_EXPECT_TRUE(out.defaultTier == QualityTier::High);
    ENJIN_EXPECT_FALSE(out.CapsFor(QualityTier::Low).allowRayTracing);
    ENJIN_EXPECT_EQ(out.CapsFor(QualityTier::Medium).maxGIBounces, 1u);
    ENJIN_EXPECT_EQ(out.CapsFor(QualityTier::High).maxDenoiserIterations, 5u);
}

ENJIN_TEST(RenderQuality, TierNamesRoundTripAndUnknownNamesFallBack) {
    ENJIN_EXPECT_TRUE(QualityTierFromName("Low", QualityTier::High) == QualityTier::Low);
    ENJIN_EXPECT_TRUE(QualityTierFromName("Ultra", QualityTier::High) == QualityTier::Ultra);
    // A hand-edited project file is user text, so a typo must not throw.
    ENJIN_EXPECT_TRUE(QualityTierFromName("Potato", QualityTier::Medium) == QualityTier::Medium);
}

ENJIN_TEST_MAIN()
