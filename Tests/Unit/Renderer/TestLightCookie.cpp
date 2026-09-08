// Light cookie (gobo) pattern generation.
//
// The generator is pure so it can be tested with no GPU, which is most of why
// the patterns can be trusted: a cookie that is silently all-black reads in the
// viewport as "the light is broken", and a cookie that is all-white reads as
// "the feature does nothing". Both are indistinguishable from a wiring bug, so
// each pattern is checked for actually having structure.
#include "EnjinTest.h"
#include "Enjin/Renderer/LightCookie.h"

#include <algorithm>
#include <cstdio>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {

struct Stats {
    u8 lo = 255;
    u8 hi = 0;
    f64 mean = 0.0;
};

Stats Measure(const std::vector<u8>& px) {
    Stats s;
    f64 sum = 0.0;
    for (u8 v : px) {
        s.lo = std::min(s.lo, v);
        s.hi = std::max(s.hi, v);
        sum += v;
    }
    s.mean = px.empty() ? 0.0 : sum / static_cast<f64>(px.size());
    return s;
}

// Average brightness of one row, used to prove blinds actually band.
f64 RowMean(const std::vector<u8>& px, u32 res, u32 row) {
    f64 sum = 0.0;
    for (u32 x = 0; x < res; ++x) sum += px[static_cast<usize>(row) * res + x];
    return sum / static_cast<f64>(res);
}

f64 ColMean(const std::vector<u8>& px, u32 res, u32 col) {
    f64 sum = 0.0;
    for (u32 y = 0; y < res; ++y) sum += px[static_cast<usize>(y) * res + col];
    return sum / static_cast<f64>(res);
}

} // namespace

ENJIN_TEST(LightCookie, GeneratesTheRequestedSize) {
    // Arrange
    CookieParams p;
    p.resolution = 64;

    // Act
    std::vector<u8> px;
    GenerateCookie(p, px);

    // Assert
    ENJIN_ASSERT_EQ(px.size(), static_cast<usize>(64 * 64));
}

ENJIN_TEST(LightCookie, EveryProceduralPatternHasActualStructure) {
    // A pattern that comes out uniform is indistinguishable from a broken light
    // once it is projected, so every one of them must span a real range.
    for (u32 i = 0; i < kCookiePatternCount; ++i) {
        const CookiePattern pattern = static_cast<CookiePattern>(i);
        if (pattern == CookiePattern::Custom) continue;   // nothing procedural

        CookieParams p;
        p.pattern = pattern;
        p.resolution = 128;

        std::vector<u8> px;
        GenerateCookie(p, px);
        const Stats s = Measure(px);

        // Both a dark and a bright region must exist.
        ENJIN_EXPECT_TRUE(s.lo < 64);
        ENJIN_EXPECT_TRUE(s.hi > 191);
        // And it must not be almost entirely one or the other. The band is
        // deliberately tight: a cookie averaging under 8% reads as a light that
        // is simply off, and one over 94% reads as no cookie at all. Both look
        // like a wiring bug rather than a pattern.
        ENJIN_EXPECT_TRUE(s.mean > 20.0);
        ENJIN_EXPECT_TRUE(s.mean < 240.0);
    }
}

ENJIN_TEST(LightCookie, BlindsBandHorizontallyAndBarsBandVertically) {
    // The two are the same code path with the axes swapped, which is exactly the
    // kind of thing that ships transposed.
    CookieParams blinds;
    blinds.pattern = CookiePattern::Blinds;
    blinds.resolution = 128;
    blinds.rows = 4.0f;
    blinds.softness = 0.0f;
    std::vector<u8> px;
    GenerateCookie(blinds, px);

    // Rows differ from each other; columns do not.
    f64 rowLo = 1e9, rowHi = -1e9, colLo = 1e9, colHi = -1e9;
    for (u32 i = 0; i < 128; ++i) {
        rowLo = std::min(rowLo, RowMean(px, 128, i));
        rowHi = std::max(rowHi, RowMean(px, 128, i));
        colLo = std::min(colLo, ColMean(px, 128, i));
        colHi = std::max(colHi, ColMean(px, 128, i));
    }
    ENJIN_EXPECT_TRUE(rowHi - rowLo > 200.0);   // strong banding down the image
    ENJIN_EXPECT_TRUE(colHi - colLo < 5.0);     // uniform across it

    CookieParams bars = blinds;
    bars.pattern = CookiePattern::Bars;
    bars.columns = 4.0f;
    GenerateCookie(bars, px);

    rowLo = 1e9; rowHi = -1e9; colLo = 1e9; colHi = -1e9;
    for (u32 i = 0; i < 128; ++i) {
        rowLo = std::min(rowLo, RowMean(px, 128, i));
        rowHi = std::max(rowHi, RowMean(px, 128, i));
        colLo = std::min(colLo, ColMean(px, 128, i));
        colHi = std::max(colHi, ColMean(px, 128, i));
    }
    ENJIN_EXPECT_TRUE(colHi - colLo > 200.0);
    ENJIN_EXPECT_TRUE(rowHi - rowLo < 5.0);
}

ENJIN_TEST(LightCookie, GenerationIsDeterministic) {
    // A cookie is regenerated from its params rather than shipped as an image,
    // so the same params must always produce the same bytes.
    CookieParams p;
    p.pattern = CookiePattern::LeafDapple;
    p.resolution = 64;
    p.seed = 42;

    std::vector<u8> a, b;
    GenerateCookie(p, a);
    GenerateCookie(p, b);

    ENJIN_ASSERT_EQ(a.size(), b.size());
    ENJIN_EXPECT_TRUE(a == b);
}

ENJIN_TEST(LightCookie, TheSeedActuallyChangesTheOrganicPatterns) {
    CookieParams p;
    p.pattern = CookiePattern::LeafDapple;
    p.resolution = 64;

    p.seed = 1;
    std::vector<u8> a;
    GenerateCookie(p, a);

    p.seed = 2;
    std::vector<u8> b;
    GenerateCookie(p, b);

    ENJIN_EXPECT_FALSE(a == b);
}

ENJIN_TEST(LightCookie, InvertFlipsTheImage) {
    CookieParams p;
    p.pattern = CookiePattern::WindowPanes;
    p.resolution = 64;
    p.softness = 0.0f;

    std::vector<u8> normal, inverted;
    GenerateCookie(p, normal);
    p.invert = true;
    GenerateCookie(p, inverted);

    ENJIN_ASSERT_EQ(normal.size(), inverted.size());
    bool allComplementary = true;
    for (usize i = 0; i < normal.size(); ++i) {
        if (static_cast<int>(normal[i]) + static_cast<int>(inverted[i]) != 255) {
            allComplementary = false;
            break;
        }
    }
    ENJIN_EXPECT_TRUE(allComplementary);
}

ENJIN_TEST(LightCookie, AZeroSizedOrAbsurdRequestIsClampedNotAllocated) {
    // These are the values a hand-edited scene file produces.
    CookieParams p;
    p.resolution = 0;
    p.columns = -5.0f;
    p.rows = 100000.0f;
    p.barWidth = 9.0f;
    p.softness = -3.0f;

    ClampCookieParams(p);

    ENJIN_EXPECT_TRUE(p.resolution >= kCookieResolutionMin);
    ENJIN_EXPECT_TRUE(p.resolution <= kCookieResolutionMax);
    ENJIN_EXPECT_TRUE(p.columns > 0.0f);
    ENJIN_EXPECT_TRUE(p.rows <= 32.0f);
    // A bar wider than half a cell meets its neighbour and the cookie goes
    // solid black, which reads as a broken light rather than a choice.
    ENJIN_EXPECT_TRUE(p.barWidth < 0.5f);
    ENJIN_EXPECT_TRUE(p.softness >= 0.0f);
}

ENJIN_TEST(LightCookie, ClampingIsIdempotent) {
    // The editor compares a clamped request against the current params to decide
    // whether to regenerate; a clamp that moved a value twice would regenerate
    // every frame.
    CookieParams p;
    p.resolution = 5000;
    p.rotation = -450.0f;
    p.barWidth = 3.0f;
    ClampCookieParams(p);

    CookieParams q = p;
    ClampCookieParams(q);

    ENJIN_EXPECT_EQ(q.resolution, p.resolution);
    ENJIN_EXPECT_TRUE(q.rotation == p.rotation);
    ENJIN_EXPECT_TRUE(q.barWidth == p.barWidth);
}

ENJIN_TEST(LightCookie, RotationChangesTheImageButKeepsItLegal) {
    CookieParams p;
    p.pattern = CookiePattern::Blinds;
    p.resolution = 64;
    p.softness = 0.0f;

    std::vector<u8> straight, tilted;
    GenerateCookie(p, straight);
    p.rotation = 30.0f;
    GenerateCookie(p, tilted);

    ENJIN_EXPECT_FALSE(straight == tilted);
    const Stats s = Measure(tilted);
    ENJIN_EXPECT_TRUE(s.lo < 64);
    ENJIN_EXPECT_TRUE(s.hi > 191);
}

ENJIN_TEST(LightCookie, ACustomCookieIsFlatRatherThanBlack) {
    // Custom points at an image that the generator does not load. A flat white
    // cookie is a no-op projection; a black one would silently switch the light
    // off and look like a bug in the light, not a missing texture.
    CookieParams p;
    p.pattern = CookiePattern::Custom;
    p.resolution = 32;

    std::vector<u8> px;
    GenerateCookie(p, px);
    const Stats s = Measure(px);

    ENJIN_EXPECT_EQ(s.lo, 255);
    ENJIN_EXPECT_EQ(s.hi, 255);
}

// Regression: Math::ValueNoise2D returns [-1,1], and the FBM helper originally
// summed it as though it were [0,1]. That centred the whole noise field on black,
// so Leaf Dapple averaged 6% brightness and Water Caustics 3%: both looked like a
// light that had failed rather than a pattern. Any future change to the remap
// shows up here.
ENJIN_TEST(LightCookie, OrganicPatternsAreCentredNotCrushedToBlack) {
    for (CookiePattern pattern : { CookiePattern::LeafDapple, CookiePattern::Caustics }) {
        CookieParams p;
        p.pattern = pattern;
        p.resolution = 128;

        std::vector<u8> px;
        GenerateCookie(p, px);

        f64 sum = 0.0;
        for (u8 v : px) sum += v;
        const f64 mean = sum / static_cast<f64>(px.size());

        // Summed in the wrong range these land near 10; remapped correctly they
        // land in the middle of the histogram.
        ENJIN_EXPECT_TRUE(mean > 50.0);
        ENJIN_EXPECT_TRUE(mean < 205.0);
    }
}

ENJIN_TEST_MAIN()
