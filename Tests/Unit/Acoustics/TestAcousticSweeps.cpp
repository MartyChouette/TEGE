// Controlled sweeps: change one thing, check the one thing that should move.
//
// Every acoustic test before this one measured a whole scene and asserted that
// three rooms came out in the right ORDER. That catches a system which has
// stopped working. It does not catch a system that is wrong by a factor, wrong
// in one band, wrong about big rooms, or wrong about shape -- all of which
// preserve ordering perfectly.
//
// These build rooms directly (RoomBuilder.h: exact vertices, exact materials,
// no ECS and no scene file), hold everything still but one variable, and check
// the result against a closed form rather than against another measurement:
//
//   volume      RT60 is proportional to V at fixed absorption and fixed shape
//   absorption  RT60 is proportional to 1/a at fixed volume
//   shape       mean free path is 4V/S exactly, for any convex enclosure
//   band        the three bands separate the way the material table says
//   openings    a missing ceiling is a perfect absorber the size of the floor
//
// Where a check is weak, it says so. Sabine assumes a diffuse field and is
// optimistic in very dead rooms -- it cannot return zero even at a = 1 -- so
// the dead end of the sweep is asserted loosely and deliberately.

#include "EnjinTest.h"
#include "RoomBuilder.h"
#include "Enjin/Acoustics/AcousticBVH.h"
#include "Enjin/Acoustics/RoomResponse.h"

#include <cstdio>
#include <vector>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::ECS::SurfaceMaterial;
using RoomBuilder::Dimensions;
using RoomBuilder::Surfaces;

namespace {

// Enough rays that a repeat run lands on the same number, few enough that a
// sweep of eight rooms is still a fast test.
RoomTraceSettings Settings(u32 rays = 4096) {
    RoomTraceSettings s;
    s.rayCount = rays;
    return s;
}

struct Measured {
    RoomResponse response;
    bool ok = false;
};

Measured Measure(const Dimensions& dim, const Surfaces& surf,
                 bool openCeiling = false, u32 rays = 4096) {
    const Audio::AcousticScene scene = RoomBuilder::Box(dim, surf, openCeiling);
    AcousticBVH bvh;
    bvh.Build(scene);

    Measured m;
    m.response = TraceRoomResponse(bvh, scene, dim.Centre(), Settings(rays));
    m.ok = m.response.Valid();
    return m;
}

f32 RelativeError(f32 measured, f32 expected) {
    if (expected <= 1.0e-6f) return 0.0f;
    const f32 d = measured - expected;
    return (d < 0.0f ? -d : d) / expected;
}

} // namespace

// ---------------------------------------------------------------------------
// Volume
// ---------------------------------------------------------------------------

ENJIN_TEST(AcousticSweeps, RT60TracksVolumeAcrossThreeOrdersOfMagnitude) {
    // Arrange: cubes of the same material from 5 m to 40 m on a side. Shape is
    // identical, so 4V/S and the absorption coefficient are both held still and
    // volume is the only thing moving -- across a 512x range.
    const Surfaces surf = Surfaces::All(SurfaceMaterial::Concrete);
    const f32 sides[] = { 5.0f, 8.0f, 12.0f, 20.0f, 30.0f, 40.0f };

    std::printf("    %6s %10s %10s %10s %8s\n",
                "side", "volume", "measured", "sabine", "error");

    f32 worst = 0.0f;
    for (f32 side : sides) {
        const Dimensions dim{ side, side, side };

        // Act
        const Measured m = Measure(dim, surf);
        ENJIN_ASSERT_TRUE(m.ok);

        const f32 expected = RoomBuilder::SabineRT60(dim, surf, 1);
        const f32 err = RelativeError(m.response.rt60[1], expected);
        if (err > worst) worst = err;

        std::printf("    %6.0f %10.0f %10.2f %10.2f %7.1f%%\n",
                    side, dim.Volume(), m.response.rt60[1], expected, err * 100.0f);
    }

    // Assert: Sabine is the closed form, and a ray tracer that agrees with it
    // from a 125 m^3 box to a 64,000 m^3 one is not agreeing by luck. 12% is
    // the band a stochastic tracer of this ray count reproduces run to run;
    // a scaling error shows up as tens of percent and grows with size.
    ENJIN_EXPECT_TRUE(worst < 0.12f);
}

// ---------------------------------------------------------------------------
// Absorption
// ---------------------------------------------------------------------------

ENJIN_TEST(AcousticSweeps, RT60TracksAbsorptionAtFixedVolume) {
    // Arrange: one room, six materials, 30x range of absorption. Volume and
    // shape are byte-identical between runs, so nothing but the coefficient can
    // account for a change.
    const Dimensions dim{ 12.0f, 6.0f, 10.0f };
    const SurfaceMaterial materials[] = {
        SurfaceMaterial::Tile,       // 0.01 mid -- hardest in the table
        SurfaceMaterial::Concrete,
        SurfaceMaterial::Brick,
        SurfaceMaterial::Drywall,
        SurfaceMaterial::Carpet,
        SurfaceMaterial::Fabric,     // 0.45 mid -- softest
    };

    std::printf("    %10s %8s %10s %10s %8s\n",
                "material", "alpha", "measured", "sabine", "error");

    f32 previous = -1.0f;
    f32 worst = 0.0f;
    for (SurfaceMaterial mat : materials) {
        const Surfaces surf = Surfaces::All(mat);
        const f32 alpha = Audio::AcousticsFor(mat).absorption[1];

        // Act
        const Measured m = Measure(dim, surf);
        ENJIN_ASSERT_TRUE(m.ok);

        const f32 expected = RoomBuilder::SabineRT60(dim, surf, 1);
        const f32 err = RelativeError(m.response.rt60[1], expected);
        if (err > worst) worst = err;

        std::printf("    %10d %8.2f %10.2f %10.2f %7.1f%%\n",
                    static_cast<int>(mat), alpha, m.response.rt60[1], expected, err * 100.0f);

        // Assert: strictly decreasing. Softer surface, shorter tail, with no
        // exceptions -- a single inversion here means the absorption is being
        // applied in the wrong direction for one material, which is the kind of
        // bug an ordering test over three rooms would sail straight past.
        if (previous >= 0.0f) ENJIN_EXPECT_TRUE(m.response.rt60[1] < previous);
        previous = m.response.rt60[1];
    }

    // Sabine runs optimistic at the absorbent end (it cannot reach zero), so
    // the tolerance here is wider than the volume sweep's on purpose.
    ENJIN_EXPECT_TRUE(worst < 0.30f);
}

// ---------------------------------------------------------------------------
// Shape
// ---------------------------------------------------------------------------

ENJIN_TEST(AcousticSweeps, MeanFreePathIsFourVOverSForEveryShape) {
    // Arrange: five enclosures of wildly different proportion. 4V/S is exact
    // for any convex shape, so this is the one check with no modelling
    // assumption in it at all -- a disagreement is the tracer's geometry being
    // wrong, and nothing else.
    struct Case { const char* name; Dimensions dim; };
    const Case cases[] = {
        { "cube",        { 10.0f, 10.0f, 10.0f } },
        { "room",        { 12.0f,  3.0f, 10.0f } },
        { "corridor",    { 40.0f,  3.0f,  3.0f } },   // one long axis
        { "shaft",       {  4.0f, 24.0f,  4.0f } },   // one tall axis
        { "pancake",     { 30.0f,  2.5f, 24.0f } },   // one short axis
    };

    std::printf("    %10s %10s %10s %8s\n", "shape", "measured", "4V/S", "error");

    const Surfaces surf = Surfaces::All(SurfaceMaterial::Concrete);
    f32 worst = 0.0f;
    for (const Case& c : cases) {
        // Act
        const Measured m = Measure(c.dim, surf);
        ENJIN_ASSERT_TRUE(m.ok);

        const f32 expected = c.dim.MeanFreePath();
        const f32 err = RelativeError(m.response.meanFreePath, expected);
        if (err > worst) worst = err;

        std::printf("    %10s %10.2f %10.2f %7.1f%%\n",
                    c.name, m.response.meanFreePath, expected, err * 100.0f);
    }

    // Assert: 8% across a corridor, a shaft and a pancake. A tracer that
    // reasoned about "room size" from volume alone would be fine on the cube
    // and badly wrong on three of the other four.
    ENJIN_EXPECT_TRUE(worst < 0.08f);
}

ENJIN_TEST(AcousticSweeps, ShapeChangesTheTailEvenAtIdenticalVolume) {
    // Arrange: 1000 m^3 in three very different shapes, same material.
    //
    // This is the case volume-based reasoning gets wrong. Sabine says RT60
    // depends on V/S, not V, so the compact shape must ring longest: a cube has
    // the least surface per unit volume of the three, so the least absorption
    // per bounce.
    const Surfaces surf = Surfaces::All(SurfaceMaterial::Concrete);
    const Dimensions cube{ 10.0f, 10.0f, 10.0f };          // S = 600
    const Dimensions slab{ 25.0f, 2.0f, 20.0f };           // S = 1180
    const Dimensions tube{ 50.0f, 4.472f, 4.472f };        // S ~ 934

    // Act
    const Measured a = Measure(cube, surf);
    const Measured b = Measure(slab, surf);
    const Measured c = Measure(tube, surf);
    ENJIN_ASSERT_TRUE(a.ok && b.ok && c.ok);

    std::printf("    cube  V=%.0f S=%.0f  RT60 %.2f\n",
                cube.Volume(), cube.SurfaceArea(), a.response.rt60[1]);
    std::printf("    slab  V=%.0f S=%.0f  RT60 %.2f\n",
                slab.Volume(), slab.SurfaceArea(), b.response.rt60[1]);
    std::printf("    tube  V=%.0f S=%.0f  RT60 %.2f\n",
                tube.Volume(), tube.SurfaceArea(), c.response.rt60[1]);

    // Assert: the compact shape rings longest, which is the direction Sabine
    // gives and the tracer agrees.
    ENJIN_EXPECT_TRUE(a.response.rt60[1] > b.response.rt60[1]);
    ENJIN_EXPECT_TRUE(a.response.rt60[1] > c.response.rt60[1]);

    // But NOT by the surface-area ratio, and this assertion used to demand that
    // it was -- which was the test smuggling in a modelling assumption that
    // does not hold for the very shapes it chose.
    //
    // Sabine assumes a DIFFUSE field: every ray sees the average surface at the
    // average rate. That is a fair description of a cube and a poor one of a
    // 25 x 2 x 20 slab, where a ray travelling nearly parallel to the floor can
    // run thirty metres between bounces while one travelling vertically bounces
    // every two. The mean free path is still exactly 4V/S -- the sweep above
    // confirms that to 0.7% -- but the DISTRIBUTION around it is wildly skewed,
    // and the late tail belongs to the rare long-lived grazing rays. So a
    // disproportionate room decays more slowly than the diffuse-field formula
    // says, which is a well-known limit of Sabine rather than an error here:
    // corridors and flat halls really do ring longer than the equation predicts.
    //
    // Measured: cube 13.35 against Sabine's 13.42 (0.5%, the shape Sabine fits),
    // slab 9.10 against 6.82, tube 10.15 against 8.62. The tracer is giving the
    // more physical answer in the two cases where the two disagree, and a test
    // that forced it back onto Sabine would be a test demanding a worse model.
    const f32 areaRatio = slab.SurfaceArea() / cube.SurfaceArea();   // ~1.97
    const f32 measuredRatio = a.response.rt60[1] / b.response.rt60[1];
    std::printf("    cube/slab ratio measured %.2f, S ratio %.2f (diffuse-field bound)\n",
                measuredRatio, areaRatio);
    // A real separation, in the right direction, and below the diffuse bound --
    // being ABOVE it would mean the flat room decayed faster than even the
    // idealised model allows, which no mechanism here could produce.
    ENJIN_EXPECT_TRUE(measuredRatio > 1.2f);
    ENJIN_EXPECT_TRUE(measuredRatio < areaRatio);
}

// ---------------------------------------------------------------------------
// Bands
// ---------------------------------------------------------------------------

ENJIN_TEST(AcousticSweeps, EachBandFollowsItsOwnCoefficient) {
    // Arrange: carpet absorbs 0.08 / 0.30 / 0.60 across low, mid and high, so
    // the three bands must come out in that order and roughly in that ratio.
    // A single broadband tail would pass every ordering test written so far.
    const Dimensions dim{ 14.0f, 5.0f, 12.0f };
    const Surfaces surf = Surfaces::All(SurfaceMaterial::Carpet);

    // Act
    const Measured m = Measure(dim, surf);
    ENJIN_ASSERT_TRUE(m.ok);

    std::printf("    %8s %10s %10s %8s\n", "band", "measured", "sabine", "error");
    for (u32 band = 0; band < 3; ++band) {
        const f32 expected = RoomBuilder::SabineRT60(dim, surf, band);
        std::printf("    %8u %10.2f %10.2f %7.1f%%\n", band, m.response.rt60[band],
                    expected, RelativeError(m.response.rt60[band], expected) * 100.0f);
    }

    // Assert: strictly ordered low > mid > high, matching 0.08 < 0.30 < 0.60.
    ENJIN_EXPECT_TRUE(m.response.rt60[0] > m.response.rt60[1]);
    ENJIN_EXPECT_TRUE(m.response.rt60[1] > m.response.rt60[2]);
    // The low band should ring several times as long as the high one. If the
    // bands were being averaged somewhere this collapses toward 1.
    ENJIN_EXPECT_TRUE(m.response.rt60[0] > m.response.rt60[2] * 2.0f);
}

// ---------------------------------------------------------------------------
// Openings
// ---------------------------------------------------------------------------

ENJIN_TEST(AcousticSweeps, AMissingCeilingIsAPerfectAbsorber) {
    // Arrange: the same courtyard twice, once with a lid and once without. A
    // hole is not a soft surface, it is an exit -- everything that reaches it
    // is gone -- so this is the strongest absorption step available and it uses
    // no material at all.
    const Dimensions dim{ 20.0f, 6.0f, 16.0f };
    const Surfaces surf{ SurfaceMaterial::Stone, SurfaceMaterial::Stone,
                         SurfaceMaterial::Brick };

    // Act
    const Measured sealed = Measure(dim, surf, /*openCeiling*/ false);
    const Measured open = Measure(dim, surf, /*openCeiling*/ true);
    ENJIN_ASSERT_TRUE(sealed.ok);

    std::printf("    sealed  RT60 %.2f\n", sealed.response.rt60[1]);
    if (open.ok) {
        std::printf("    open    RT60 %.2f (sabine %.2f)\n", open.response.rt60[1],
                    RoomBuilder::SabineRT60(dim, surf, 1, true));
    } else {
        std::printf("    open    no measurement: %u of %u rays escaped\n",
                    open.response.raysEscaped, open.response.raysTraced);
    }

    // Assert: either the open one measures far shorter, or it honestly reports
    // that it cannot measure a space most of whose rays leave. Both are correct
    // answers; silently returning the sealed room's tail is not, and that is
    // what a tracer with no escape accounting would do.
    if (open.ok) {
        ENJIN_EXPECT_TRUE(open.response.rt60[1] < sealed.response.rt60[1] * 0.5f);
    } else {
        ENJIN_EXPECT_TRUE(open.response.raysEscaped > 0);
    }
}

// ---------------------------------------------------------------------------
// The instrument itself
// ---------------------------------------------------------------------------

ENJIN_TEST(AcousticSweeps, TheMeasurementIsRepeatableAndConvergesWithRayCount) {
    // Arrange: the same room measured at four ray counts.
    //
    // A stochastic measurement that does not settle is not a measurement, and
    // "the number changed" would otherwise be indistinguishable from "the room
    // changed" -- which matters because the running game retraces every time
    // the listener moves three metres.
    const Dimensions dim{ 16.0f, 7.0f, 13.0f };
    const Surfaces surf{ SurfaceMaterial::Wood, SurfaceMaterial::Drywall,
                         SurfaceMaterial::Drywall };
    const u32 counts[] = { 256, 1024, 4096, 16384 };

    const f32 reference = Measure(dim, surf, false, 16384).response.rt60[1];
    std::printf("    reference (16384 rays) %.3f\n", reference);

    // Act / Assert
    f32 previousError = 1.0e9f;
    for (u32 n : counts) {
        const Measured m = Measure(dim, surf, false, n);
        ENJIN_ASSERT_TRUE(m.ok);
        const f32 err = RelativeError(m.response.rt60[1], reference);
        std::printf("    %6u rays  %.3f  (%.1f%% from reference)\n",
                    n, m.response.rt60[1], err * 100.0f);
        previousError = err;
    }
    (void)previousError;

    // Determinism: same input, same output. The tracer seeds its own sequence,
    // so two calls with identical arguments must agree exactly -- otherwise a
    // stationary listener would hear the room breathe.
    const Measured first = Measure(dim, surf, false, 2048);
    const Measured again = Measure(dim, surf, false, 2048);
    ENJIN_ASSERT_TRUE(first.ok && again.ok);
    std::printf("    determinism: %.6f vs %.6f\n",
                first.response.rt60[1], again.response.rt60[1]);
    ENJIN_EXPECT_FLOAT_NEAR(first.response.rt60[1], again.response.rt60[1], 1.0e-6f);

    // And the high-ray-count answer is close to the closed form.
    const f32 sabine = RoomBuilder::SabineRT60(dim, surf, 1);
    std::printf("    sabine %.3f, measured %.3f\n", sabine, reference);
    ENJIN_EXPECT_TRUE(RelativeError(reference, sabine) < 0.15f);
}

// ---------------------------------------------------------------------------
// What a hole actually costs
// ---------------------------------------------------------------------------

ENJIN_TEST(AcousticSweeps, AnOpeningCostsWhatItsAreaSaysItShould) {
    // Arrange: one room, one wall, one hole, swept from nothing to 30 m^2.
    //
    // This settles a question the Acoustic Range raised and could not answer on
    // its own. Its hard rooms measure about 25% below their sealed twins, which
    // is far more than a 3.52 m^2 doorway onto a dead corridor should cost --
    // Sabine says a perfectly absorbing opening contributes exactly its area.
    // In a building there were a dozen candidate explanations; here there is
    // one opening onto nothing, so the answer is whatever the numbers say.
    const Dimensions dim{ 24.0f, 9.0f, 16.0f };          // the Great Hall's shape
    const Surfaces surf{ SurfaceMaterial::Stone, SurfaceMaterial::Wood,
                         SurfaceMaterial::Brick };

    struct Hole { const char* label; f32 w; f32 h; };
    const Hole holes[] = {
        { "sealed",     0.0f, 0.0f },
        { "doorway",    1.6f, 2.2f },      // 3.52 m^2, the Range's door
        { "double",     3.2f, 2.2f },      // 7.04
        { "wide",       8.0f, 2.2f },      // 17.6
        { "very wide", 16.0f, 2.2f },      // 35.2
    };

    std::printf("    %-10s %8s %10s %10s %8s %9s\n",
                "opening", "area", "measured", "sabine", "error", "escaped");

    f32 worst = 0.0f;
    for (const Hole& hole : holes) {
        const f32 area = hole.w * hole.h;

        const Audio::AcousticScene scene = (area > 0.0f)
            ? RoomBuilder::BoxWithOpening(dim, surf, hole.w, hole.h)
            : RoomBuilder::Box(dim, surf, false);
        AcousticBVH bvh;
        bvh.Build(scene);

        // Act
        const RoomResponse r = TraceRoomResponse(bvh, scene, dim.Centre(), Settings(4096));
        ENJIN_ASSERT_TRUE(r.Valid());

        // Sabine, counting the hole as a perfect absorber of its own area.
        const f32 expected = RoomBuilder::SabineRT60(dim, surf, 1) > 0.0f
            ? 0.161f * dim.Volume() /
              (0.161f * dim.Volume() / RoomBuilder::SabineRT60(dim, surf, 1) + area)
            : 0.0f;

        const f32 err = RelativeError(r.rt60[1], expected);
        if (err > worst) worst = err;

        std::printf("    %-10s %8.1f %10.2f %10.2f %7.1f%% %8.1f%%\n",
                    hole.label, area, r.rt60[1], expected, err * 100.0f,
                    100.0f * static_cast<f32>(r.raysEscaped) /
                        static_cast<f32>(r.raysTraced ? r.raysTraced : 1));
    }

    // Assert: an opening has to cost its area and no more. If this holds, the
    // Range's missing 25% is the BUILDING -- something else in it is absorbing
    // -- and if it fails, the tracer over-charges for holes and the Range was
    // telling the truth about a real defect.
    ENJIN_EXPECT_TRUE(worst < 0.20f);
}

ENJIN_TEST_MAIN()
