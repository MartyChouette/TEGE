// Which GI a runtime actually uses.
//
// The reason this is a tested decision rather than an if-statement somewhere is
// that its failure mode is silence: a scene with no global illumination does not
// look broken, it looks flat, and flat gets blamed on the art. Every row here is
// a case someone could otherwise ship without noticing.
#include "EnjinTest.h"
#include "Enjin/Renderer/GIFallback.h"

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

ENJIN_TEST_MAIN()
