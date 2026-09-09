// The three-basis blend at the heart of radiosity normal mapping.
//
// Every property here is one the SHADER also has to satisfy, because the bake
// integrates light against this basis and the shader blends by it. If the two
// disagree the picture does not break, it leans: surfaces pick up a directional
// bias nobody authored, which reads as a bad bake rather than as a mismatch.
// That is the failure this file exists to make impossible to ship.
#include "EnjinTest.h"
#include "Enjin/Renderer/RadiosityNormalMap.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {
bool Near(f32 a, f32 b, f32 eps = 1e-4f) { return std::fabs(a - b) <= eps; }
f32 Dot(const Math::Vector3& a, const Math::Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
f32 Len(const Math::Vector3& v) { return std::sqrt(Dot(v, v)); }
} // namespace

// --- The basis itself -------------------------------------------------------

ENJIN_TEST(RadiosityNormalMap, TheBasisVectorsAreUnitLength) {
    const Math::Vector3* b = RNMBasis();
    for (u32 i = 0; i < kRNMBasisCount; ++i) {
        ENJIN_EXPECT_TRUE(Near(Len(b[i]), 1.0f));
    }
}

ENJIN_TEST(RadiosityNormalMap, TheBasisIsEvenlySpreadAroundTheSurfaceNormal) {
    // Evenly spread is what makes three samples enough: no direction in the
    // hemisphere is far from all of them. A basis that drifted would leave a
    // band of normals lit by almost nothing.
    const Math::Vector3* b = RNMBasis();

    // Every vector leans the same amount away from tangent-space Z...
    const f32 z0 = b[0].z;
    for (u32 i = 0; i < kRNMBasisCount; ++i) ENJIN_EXPECT_TRUE(Near(b[i].z, z0));

    // ...and any two are separated by the same angle, which for three vectors
    // on a cone means 120 degrees apart around it.
    const f32 d01 = Dot(b[0], b[1]);
    const f32 d12 = Dot(b[1], b[2]);
    const f32 d02 = Dot(b[0], b[2]);
    ENJIN_EXPECT_TRUE(Near(d01, d12));
    ENJIN_EXPECT_TRUE(Near(d12, d02));
}

ENJIN_TEST(RadiosityNormalMap, TheBasisSumsToTheSurfaceNormal) {
    // The three directions average out to straight up. This is why an unbumped
    // surface reproduces its lightmap exactly rather than leaning somewhere.
    const Math::Vector3* b = RNMBasis();
    const Math::Vector3 sum(b[0].x + b[1].x + b[2].x,
                            b[0].y + b[1].y + b[2].y,
                            b[0].z + b[1].z + b[2].z);
    ENJIN_EXPECT_TRUE(Near(sum.x, 0.0f));
    ENJIN_EXPECT_TRUE(Near(sum.y, 0.0f));
    ENJIN_EXPECT_TRUE(sum.z > 1.0f);   // straight up, and nowhere else
}

// --- Weights ----------------------------------------------------------------

ENJIN_TEST(RadiosityNormalMap, AFlatNormalTakesAnEqualShareOfAllThree) {
    const Math::Vector3 w = RNMWeights(Math::Vector3(0.0f, 0.0f, 1.0f));
    ENJIN_EXPECT_TRUE(Near(w.x, 1.0f / 3.0f));
    ENJIN_EXPECT_TRUE(Near(w.y, 1.0f / 3.0f));
    ENJIN_EXPECT_TRUE(Near(w.z, 1.0f / 3.0f));
}

ENJIN_TEST(RadiosityNormalMap, WeightsAlwaysSumToOne) {
    // This is what keeps a bumpy surface as bright as the flat lightmap says.
    // Without it total light changes with the bump angle, and flat regions come
    // out darker than the value baked for them -- a whole-scene dimming that
    // looks like the bake being wrong.
    for (int i = 0; i <= 20; ++i) {
        for (int j = 0; j <= 20; ++j) {
            const f32 x = -1.0f + 0.1f * static_cast<f32>(i);
            const f32 y = -1.0f + 0.1f * static_cast<f32>(j);
            const f32 zz = 1.0f - x * x - y * y;
            if (zz <= 0.0f) continue;                 // outside the hemisphere
            const Math::Vector3 w = RNMWeights(Math::Vector3(x, y, std::sqrt(zz)));
            ENJIN_EXPECT_TRUE(Near(w.x + w.y + w.z, 1.0f));
        }
    }
}

ENJIN_TEST(RadiosityNormalMap, ANormalLeaningIntoABasisFavoursThatBasis) {
    const Math::Vector3* b = RNMBasis();
    // Lean the normal straight at basis 2 and it should win outright.
    const Math::Vector3 w = RNMWeights(b[2]);
    ENJIN_EXPECT_TRUE(w.z > w.x);
    ENJIN_EXPECT_TRUE(w.z > w.y);
    // And the two it turned away from contribute nothing at all, rather than a
    // little: the clamp is what stops light arriving from behind a bump.
    ENJIN_EXPECT_TRUE(Near(w.x, 0.0f));
    ENJIN_EXPECT_TRUE(Near(w.y, 0.0f));
}

ENJIN_TEST(RadiosityNormalMap, LeaningFurtherRaisesThatBasisMonotonically) {
    // Shading has to move smoothly as a normal turns. A weight that jumped
    // would show up as a hard seam across a smooth bump.
    const Math::Vector3* b = RNMBasis();
    f32 prev = -1.0f;
    for (int i = 0; i <= 10; ++i) {
        const f32 t = 0.1f * static_cast<f32>(i);
        const Math::Vector3 n(b[2].x * t, b[2].y * t, b[2].z * (1.0f - t) + t * b[2].z + (1.0f - t));
        const Math::Vector3 w = RNMWeights(n);
        ENJIN_EXPECT_TRUE(w.z >= prev - 1e-4f);
        prev = w.z;
    }
}

// --- Resolve ----------------------------------------------------------------

ENJIN_TEST(RadiosityNormalMap, ThreeIdenticalSamplesReproduceThatColourExactly) {
    // A texel lit evenly from every direction must come back unchanged whatever
    // the normal does. If this drifts, every uniformly lit surface in a scene
    // shifts colour when a normal map is applied.
    const Math::Vector3 lit(0.25f, 0.5f, 0.75f);
    const Math::Vector3 normals[4] = {
        Math::Vector3(0.0f, 0.0f, 1.0f),
        Math::Vector3(0.5f, 0.0f, 0.866f),
        Math::Vector3(-0.3f, 0.4f, 0.866f),
        RNMBasis()[1],
    };
    for (const auto& n : normals) {
        const Math::Vector3 out = RNMResolve(RNMWeights(n), lit, lit, lit);
        ENJIN_EXPECT_TRUE(Near(out.x, lit.x));
        ENJIN_EXPECT_TRUE(Near(out.y, lit.y));
        ENJIN_EXPECT_TRUE(Near(out.z, lit.z));
    }
}

ENJIN_TEST(RadiosityNormalMap, ANormalTurnedAtOneSampleReadsThatSample) {
    // The whole point of the technique in one assertion: bumps facing different
    // directions pick up different baked light.
    const Math::Vector3 red(1.0f, 0.0f, 0.0f);
    const Math::Vector3 green(0.0f, 1.0f, 0.0f);
    const Math::Vector3 blue(0.0f, 0.0f, 1.0f);

    const Math::Vector3 out = RNMResolve(RNMWeights(RNMBasis()[0]), red, green, blue);
    ENJIN_EXPECT_TRUE(out.x > 0.9f);    // all red, from basis 0
    ENJIN_EXPECT_TRUE(out.y < 0.1f);
    ENJIN_EXPECT_TRUE(out.z < 0.1f);
}

ENJIN_TEST(RadiosityNormalMap, ADegenerateNormalResolvesFlatRatherThanBlack) {
    // An unnormalized normal map, or a vertex with no tangent frame, would
    // otherwise produce a black texel -- indistinguishable from missing light,
    // and blamed on the bake.
    const Math::Vector3 lit(0.4f, 0.4f, 0.4f);
    const Math::Vector3 w = RNMWeights(Math::Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(Near(w.x + w.y + w.z, 1.0f));

    const Math::Vector3 out = RNMResolve(w, lit, lit, lit);
    ENJIN_EXPECT_TRUE(Near(out.x, lit.x));
}

ENJIN_TEST(RadiosityNormalMap, ANormalPointingIntoTheSurfaceDoesNotGoBlack) {
    // Straight down: every basis direction is turned away from. Authored data
    // being wrong, but it must not punch a black hole in the surface.
    const Math::Vector3 w = RNMWeights(Math::Vector3(0.0f, 0.0f, -1.0f));
    ENJIN_EXPECT_TRUE(Near(w.x + w.y + w.z, 1.0f));
}

ENJIN_TEST_MAIN()
