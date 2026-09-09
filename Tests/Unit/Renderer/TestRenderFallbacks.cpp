// Which GI a runtime actually uses.
//
// The reason this is a tested decision rather than an if-statement somewhere is
// that its failure mode is silence: a scene with no global illumination does not
// look broken, it looks flat, and flat gets blamed on the art. Every row here is
// a case someone could otherwise ship without noticing.
#include "EnjinTest.h"
#include "Enjin/Renderer/RenderFallbacks.h"

#include <cstring>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {
GICapabilities Caps(bool dynamic, bool baked) {
    GICapabilities c;
    c.canRunDynamic = dynamic;
    c.hasBakedLightmap = baked;
    return c;
}
} // namespace

ENJIN_TEST(GIFallback, DesktopRunsTheDynamicGIASceneAskedFor) {
    ENJIN_EXPECT_TRUE(ResolveGI(true, Caps(true, false)) == GISource::Dynamic);
    // Even with a bake present: dynamic responds to a scene that changes, which
    // is the only reason to pay for it at run time.
    ENJIN_EXPECT_TRUE(ResolveGI(true, Caps(true, true)) == GISource::Dynamic);
}

ENJIN_TEST(GIFallback, WebSubstitutesTheBakeForTheDynamicGIItCannotRun) {
    // The whole point. A browser cannot trace, so the scene gets the answer
    // somebody computed offline instead of getting nothing.
    ENJIN_EXPECT_TRUE(ResolveGI(true, Caps(false, true)) == GISource::Baked);
}

ENJIN_TEST(GIFallback, ABakeIsUsedEvenWhenTheSceneNeverAskedForDynamicGI) {
    // A lightmap in a project is there on purpose. Ignoring it because a
    // checkbox elsewhere is off would waste a bake somebody waited for.
    ENJIN_EXPECT_TRUE(ResolveGI(false, Caps(true, true)) == GISource::Baked);
    ENJIN_EXPECT_TRUE(ResolveGI(false, Caps(false, true)) == GISource::Baked);
}

ENJIN_TEST(GIFallback, NothingAskedForAndNothingBakedIsHonestlyNone) {
    ENJIN_EXPECT_TRUE(ResolveGI(false, Caps(true, false)) == GISource::None);
    ENJIN_EXPECT_TRUE(ResolveGI(false, Caps(false, false)) == GISource::None);
}

ENJIN_TEST(GIFallback, TheOneCaseThatLosesGIIsNamedOutLoud) {
    // Dynamic requested, backend cannot run it, no bake to substitute. This is
    // exactly what shipping a DDGI scene to web did before there was a fallback,
    // and it is the case a person most needs told.
    const char* gap = DescribeGIGap(true, Caps(false, false));
    ENJIN_EXPECT_TRUE(std::strlen(gap) > 0);
    // It has to say what to DO, not just that something is wrong.
    ENJIN_EXPECT_TRUE(std::strstr(gap, "Bake") != nullptr);
}

ENJIN_TEST(GIFallback, AWorkingSetupIsNotWarnedAbout) {
    // A warning that fires when everything is fine is a warning people learn to
    // ignore, which costs more than it saves.
    ENJIN_EXPECT_TRUE(std::strlen(DescribeGIGap(true, Caps(true, false))) == 0);
    ENJIN_EXPECT_TRUE(std::strlen(DescribeGIGap(true, Caps(false, true))) == 0);
    ENJIN_EXPECT_TRUE(std::strlen(DescribeGIGap(false, Caps(false, true))) == 0);
}

ENJIN_TEST(GIFallback, EverySourceHasAName) {
    ENJIN_EXPECT_TRUE(std::strlen(GISourceName(GISource::None)) > 0);
    ENJIN_EXPECT_TRUE(std::strlen(GISourceName(GISource::Dynamic)) > 0);
    ENJIN_EXPECT_TRUE(std::strlen(GISourceName(GISource::Baked)) > 0);
}

// --- Fog ---------------------------------------------------------------------

namespace {
VolumetricFogRequest Vol(bool on, f32 density) {
    VolumetricFogRequest r;
    r.enabled = on;
    r.density = density;
    r.color = Math::Vector3(0.8f, 0.85f, 1.0f);
    r.heightFalloff = 0.25f;
    return r;
}
} // namespace

ENJIN_TEST(RenderFallbacks, WebGetsAnalyticFogInPlaceOfVolumetric) {
    // A scene authored around thick atmosphere arriving perfectly clear changes
    // how it reads more than almost anything else that gets dropped.
    const auto sub = SubstituteVolumetricFog(Vol(true, 0.03f), /*canRun=*/false, 0.0f);

    ENJIN_EXPECT_TRUE(sub.apply);
    ENJIN_EXPECT_TRUE(sub.heightFalloff == 0.25f);
    ENJIN_EXPECT_TRUE(sub.color.z > sub.color.x);   // the authored tint survives

    // A RANGE has to come with it. The analytic fog is linear between start and
    // end, so density alone is inert -- the first version of this substitution
    // passed density through, left the scene's unused 20-unit start in place,
    // and fogged a 13-unit room by exactly nothing while logging success.
    ENJIN_EXPECT_TRUE(sub.start == 0.0f);
    ENJIN_EXPECT_TRUE(sub.end > 50.0f && sub.end < 90.0f);   // ln(10) / 0.03
    ENJIN_EXPECT_TRUE(sub.density == 1.0f);                  // full strength at the far end
}

ENJIN_TEST(RenderFallbacks, ThickerFogReachesFullStrengthSooner) {
    // The range is derived from density, so the two have to move together --
    // otherwise 'denser' would change nothing that anybody could see.
    const auto thin = SubstituteVolumetricFog(Vol(true, 0.01f), false, 0.0f);
    const auto thick = SubstituteVolumetricFog(Vol(true, 0.10f), false, 0.0f);
    ENJIN_EXPECT_TRUE(thick.end < thin.end);
}

ENJIN_TEST(RenderFallbacks, DesktopKeepsItsVolumetricFogUntouched) {
    const auto sub = SubstituteVolumetricFog(Vol(true, 0.03f), /*canRun=*/true, 0.0f);
    ENJIN_EXPECT_FALSE(sub.apply);
}

ENJIN_TEST(RenderFallbacks, AuthoredFogIsNeverDoubledUp) {
    // The scene already has analytic fog somebody tuned by eye on this backend.
    // Substituting on top of it would stack two fogs and darken the scene for a
    // reason nobody could find.
    const auto sub = SubstituteVolumetricFog(Vol(true, 0.03f), /*canRun=*/false,
                                             /*existingFogDensity=*/0.02f);
    ENJIN_EXPECT_FALSE(sub.apply);
}

ENJIN_TEST(RenderFallbacks, NothingToStandInForIsNotSubstituted) {
    // Volumetric fog switched on with zero density is off in all but name.
    ENJIN_EXPECT_FALSE(SubstituteVolumetricFog(Vol(true, 0.0f), false, 0.0f).apply);
    ENJIN_EXPECT_FALSE(SubstituteVolumetricFog(Vol(false, 0.03f), false, 0.0f).apply);
}

// --- Reflections -------------------------------------------------------------

namespace {
ReflectionCapabilities RCaps(bool trace, bool sky) {
    ReflectionCapabilities c;
    c.canTrace = trace;
    c.hasConfiguredSky = sky;
    return c;
}
f32 Luma(const Math::Vector3& c) { return (c.x + c.y + c.z) / 3.0f; }
} // namespace

ENJIN_TEST(RenderFallbacks, DesktopTracesReflectionsWhenAsked) {
    ENJIN_EXPECT_TRUE(ResolveReflections(true, RCaps(true, false)) == ReflectionSource::Traced);
}

ENJIN_TEST(RenderFallbacks, WebSubstitutesAnEnvironmentReflectionRatherThanAScreenSpaceOne) {
    // Deliberately not SSR. A screen-space reflection can only show what is
    // already on screen, so it admits the world stops at the frame edge --
    // which is the single thing this family of techniques exists to avoid.
    ENJIN_EXPECT_TRUE(ResolveReflections(true, RCaps(false, false)) == ReflectionSource::Environment);
    ENJIN_EXPECT_TRUE(ResolveReflections(true, RCaps(false, true)) == ReflectionSource::Environment);
}

ENJIN_TEST(RenderFallbacks, ASceneThatWantedNeitherStaysFlat) {
    // A scene with no sky that never asked for reflections chose that look.
    // Adding an environment term would change art nobody asked to change.
    ENJIN_EXPECT_TRUE(ResolveReflections(false, RCaps(false, false)) == ReflectionSource::None);
    // But a configured sky is itself the request.
    ENJIN_EXPECT_TRUE(ResolveReflections(false, RCaps(false, true)) == ReflectionSource::Environment);
}

ENJIN_TEST(RenderFallbacks, TheDerivedDomeIsBrighterAboveThanBelow) {
    // What makes a reflection read as a direction rather than a flat tint.
    const auto dome = DeriveEnvironmentDome(Math::Vector3(0.3f, 0.32f, 0.4f));
    ENJIN_EXPECT_TRUE(Luma(dome.top) > Luma(dome.horizon));
    ENJIN_EXPECT_TRUE(Luma(dome.horizon) > Luma(dome.bottom));
}

ENJIN_TEST(RenderFallbacks, ABlackAmbientDerivesABlackDome) {
    // The rule that a fallback is EMPTY rather than a plausible guess. Inventing
    // a blue sky here would light a night interior like an afternoon, using a
    // colour nobody in the scene ever chose.
    const auto dome = DeriveEnvironmentDome(Math::Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(Luma(dome.top) == 0.0f);
    ENJIN_EXPECT_TRUE(Luma(dome.horizon) == 0.0f);
    ENJIN_EXPECT_TRUE(Luma(dome.bottom) == 0.0f);
}

ENJIN_TEST(RenderFallbacks, TheDomeIsTheScenesOwnColourAndNotSomeOtherHue) {
    // Scaled, never re-tinted: a warm ambient must not come back blue.
    const auto dome = DeriveEnvironmentDome(Math::Vector3(0.5f, 0.25f, 0.1f));
    ENJIN_EXPECT_TRUE(dome.top.x > dome.top.y && dome.top.y > dome.top.z);
    ENJIN_EXPECT_TRUE(dome.bottom.x > dome.bottom.y && dome.bottom.y > dome.bottom.z);
}

ENJIN_TEST_MAIN()
