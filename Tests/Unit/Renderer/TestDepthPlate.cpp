// Depth plates: turning a distance into the exact value the depth buffer holds.
//
// Every failure mode of a pre-rendered background is quiet. A mapping that is
// slightly wrong does not error -- the character's feet sink into the floor, or
// they stand a few inches inside a wall, and the cause looks like art. So the
// maths is pinned here, against the engine's OWN projection matrix rather than
// against a formula copied from a book.
#include "EnjinTest.h"
#include "Enjin/Renderer/DepthPlate.h"
#include "Enjin/Math/Matrix.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {
bool Near(f32 a, f32 b, f32 eps = 1e-4f) { return std::fabs(a - b) <= eps; }
} // namespace

// --- Packing ----------------------------------------------------------------

ENJIN_TEST(DepthPlate, PackingSurvivesARoundTrip) {
    // Arrange / Act / Assert: several depths across the range, because the ends
    // are where an off-by-one in the packing hides.
    const f32 samples[] = { 0.0f, 0.001f, 0.25f, 0.5f, 0.75f, 0.999f, 1.0f };
    for (f32 v : samples) {
        const u32 packed = PackPlateDepth24(v);
        const u8 r = static_cast<u8>((packed >> 16) & 0xFF);
        const u8 g = static_cast<u8>((packed >> 8) & 0xFF);
        const u8 b = static_cast<u8>(packed & 0xFF);
        ENJIN_EXPECT_TRUE(Near(UnpackPlateDepth24(r, g, b), v, 1e-6f));
    }
}

ENJIN_TEST(DepthPlate, TwentyFourBitsResolveFarMoreThanEightWould) {
    // The reason the plate is not a grayscale image: at 8 bits a 100-unit room
    // has 40cm depth steps and a character visibly pops between them.
    const u32 a = PackPlateDepth24(0.5f);
    const u32 b = PackPlateDepth24(0.5f + 1.0f / 70000.0f);
    ENJIN_EXPECT_TRUE(a != b);
    // The same two distances are the same byte in an 8-bit plate.
    ENJIN_EXPECT_EQ(static_cast<u32>((a >> 16) & 0xFF), static_cast<u32>((b >> 16) & 0xFF));
}

ENJIN_TEST(DepthPlate, OutOfRangeDepthsClampInsteadOfWrapping) {
    // A wrap would put the furthest wall at the camera's nose, which is the
    // worst possible failure: it looks like geometry, not like bad data.
    ENJIN_EXPECT_EQ(PackPlateDepth24(-5.0f), 0u);
    ENJIN_EXPECT_EQ(PackPlateDepth24(5.0f), 0xFFFFFFu);
}

// --- Distance normalisation --------------------------------------------------

ENJIN_TEST(DepthPlate, DistanceNormalisesAcrossThePlatesOwnRange) {
    ENJIN_EXPECT_TRUE(Near(NormalizePlateDistance(2.0f, 2.0f, 10.0f), 0.0f));
    ENJIN_EXPECT_TRUE(Near(NormalizePlateDistance(10.0f, 2.0f, 10.0f), 1.0f));
    ENJIN_EXPECT_TRUE(Near(NormalizePlateDistance(6.0f, 2.0f, 10.0f), 0.5f));

    // And back, so a bake and a load agree.
    ENJIN_EXPECT_TRUE(Near(DenormalizePlateDistance(0.5f, 2.0f, 10.0f), 6.0f));
}

ENJIN_TEST(DepthPlate, ADegenerateRangeDoesNotDivideByZero) {
    // Authored data: near == far is a plate nobody finished setting up.
    ENJIN_EXPECT_TRUE(Near(NormalizePlateDistance(5.0f, 4.0f, 4.0f), 0.0f));
    ENJIN_EXPECT_TRUE(Near(DenormalizePlateDistance(0.5f, 4.0f, 4.0f), 4.0f));
}

// --- The projection mapping --------------------------------------------------

ENJIN_TEST(DepthPlate, ProjectedDepthMatchesTheEnginesOwnPerspectiveMatrix) {
    // This engine's Perspective is the OpenGL form: the near plane lands at -1,
    // not at 0, and Vulkan clips everything below zero. Anything that assumed
    // the Vulkan convention would be wrong by half the depth range, so the
    // convention itself is asserted here rather than trusted.
    const f32 n = 0.1f, f = 100.0f;
    const Math::Matrix4 proj = Math::Matrix4::Perspective(Math::Radians(60.0f), 16.0f / 9.0f, n, f);

    ENJIN_EXPECT_TRUE(Near(ProjectViewDistanceToDepth(proj, n), -1.0f, 1e-3f));
    ENJIN_EXPECT_TRUE(Near(ProjectViewDistanceToDepth(proj, f), 1.0f, 1e-3f));

    // Monotonic: further away is always a larger depth value, which is what
    // makes the depth test order the plate against live geometry at all.
    f32 prev = -2.0f;
    for (f32 d = n; d <= f; d += 1.7f) {
        const f32 z = ProjectViewDistanceToDepth(proj, d);
        ENJIN_EXPECT_TRUE(z > prev);
        prev = z;
    }
}

ENJIN_TEST(DepthPlate, TheSolvedMappingReproducesTheMatrixEverywhereBetween) {
    // Two points fit the form exactly by construction; the test is whether it
    // holds in between, because that is where a character actually stands.
    const Math::Matrix4 proj = Math::Matrix4::Perspective(Math::Radians(50.0f), 1.6f, 0.1f, 200.0f);
    const f32 plateNear = 1.0f, plateFar = 60.0f;
    const PlateDepthMapping m = SolvePlateDepthMapping(proj, plateNear, plateFar);

    ENJIN_EXPECT_TRUE(m.inverse);   // perspective is a + b/distance
    for (f32 d = plateNear; d <= plateFar; d += 0.9f) {
        ENJIN_EXPECT_TRUE(Near(m.Evaluate(d), ProjectViewDistanceToDepth(proj, d), 1e-4f));
    }
}

ENJIN_TEST(DepthPlate, AnOrthographicShotFitsTheLinearFormInstead) {
    // Fixed-camera scenes are often orthographic, and depth is linear there.
    // Forcing the perspective form on one would bend the whole plate.
    const Math::Matrix4 proj = Math::Matrix4::Orthographic(-10.0f, 10.0f, -6.0f, 6.0f, 0.1f, 80.0f);
    const PlateDepthMapping m = SolvePlateDepthMapping(proj, 1.0f, 50.0f);

    ENJIN_EXPECT_FALSE(m.inverse);
    for (f32 d = 1.0f; d <= 50.0f; d += 0.7f) {
        ENJIN_EXPECT_TRUE(Near(m.Evaluate(d), ProjectViewDistanceToDepth(proj, d), 1e-4f));
    }
}

ENJIN_TEST(DepthPlate, InvertingTheMappingReturnsTheDistanceItCameFrom) {
    // The bake needs this direction: the depth buffer hands back projected
    // depth and the plate has to store a linear distance. A mismatch between
    // the two directions would only show up as a plate that occludes wrongly
    // once it is loaded back, which is a slow thing to debug.
    const Math::Matrix4 proj = Math::Matrix4::Perspective(Math::Radians(70.0f), 1.0f, 0.05f, 500.0f);
    const PlateDepthMapping m = SolvePlateDepthMapping(proj, 0.5f, 120.0f);

    for (f32 d = 0.5f; d <= 120.0f; d += 3.3f) {
        const f32 depth = m.Evaluate(d);
        ENJIN_EXPECT_TRUE(Near(InvertPlateDepth(m, depth), d, 1e-2f));
    }
}

ENJIN_TEST(DepthPlate, AClearedDepthBufferInvertsToNothingRatherThanInfinity) {
    // Background pixels of a bake sit at the far plane, so this is the ordinary
    // case, not an edge case: it must not produce inf and poison the plate.
    const Math::Matrix4 proj = Math::Matrix4::Perspective(Math::Radians(60.0f), 1.0f, 0.1f, 100.0f);
    const PlateDepthMapping m = SolvePlateDepthMapping(proj, 1.0f, 50.0f);

    const f32 atHorizon = InvertPlateDepth(m, m.a);
    ENJIN_EXPECT_TRUE(atHorizon == 0.0f);
    ENJIN_EXPECT_FALSE(std::isinf(atHorizon));
    ENJIN_EXPECT_FALSE(std::isnan(atHorizon));
}

ENJIN_TEST(DepthPlate, AnUnbuildableRangePutsTheWholePlateAtTheBack) {
    // near >= far is unfinished authoring. Everything lands on the far plane, so
    // the plate draws behind live geometry: visibly wrong, which is the right
    // failure. Silently picking a plausible range would hide the mistake.
    const Math::Matrix4 proj = Math::Matrix4::Perspective(Math::Radians(60.0f), 1.0f, 0.1f, 100.0f);
    const PlateDepthMapping m = SolvePlateDepthMapping(proj, 10.0f, 10.0f);

    ENJIN_EXPECT_TRUE(Near(m.Evaluate(1.0f), 1.0f));
    ENJIN_EXPECT_TRUE(Near(m.Evaluate(50.0f), 1.0f));
}

ENJIN_TEST(DepthPlate, TheWholeChainHoldsFromDistanceToBytesAndBack) {
    // End to end, the way the runtime actually uses it: a distance is packed
    // into image bytes at bake time, read back at load time, and turned into a
    // depth value. A quarter-unit error here is a character inside a wall.
    const Math::Matrix4 proj = Math::Matrix4::Perspective(Math::Radians(55.0f), 1.777f, 0.1f, 300.0f);
    const f32 plateNear = 2.0f, plateFar = 40.0f;
    const PlateDepthMapping m = SolvePlateDepthMapping(proj, plateNear, plateFar);

    for (f32 d = plateNear; d <= plateFar; d += 1.1f) {
        const u32 packed = PackPlateDepth24(NormalizePlateDistance(d, plateNear, plateFar));
        const f32 back = DenormalizePlateDistance(
            UnpackPlateDepth24(static_cast<u8>((packed >> 16) & 0xFF),
                               static_cast<u8>((packed >> 8) & 0xFF),
                               static_cast<u8>(packed & 0xFF)),
            plateNear, plateFar);

        // Sub-millimetre over a 40-unit room. Eight-bit packing would be 15cm.
        ENJIN_EXPECT_TRUE(Near(back, d, 1e-3f));
        ENJIN_EXPECT_TRUE(Near(m.Evaluate(back), ProjectViewDistanceToDepth(proj, d), 1e-4f));
    }
}

ENJIN_TEST_MAIN()
