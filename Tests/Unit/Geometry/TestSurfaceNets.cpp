// Surface nets and the distance fields it meshes.
//
// The point of this file is that a cave's SHAPE is checkable without an editor,
// a GPU or a person standing inside it. Every claim the mesher makes -- the
// surface lands on the field, the mesh is closed, the normals point out, an
// overhang survives -- is asserted here rather than eyeballed in a viewport.

#include "EnjinTest.h"
#include "Enjin/Geometry/SurfaceNets.h"
#include "Enjin/Geometry/Sdf.h"

#include <cmath>
#include <map>

using namespace Enjin;
using namespace Enjin::Geometry;
using Enjin::Math::Vector3;

namespace {

ScalarGrid MakeGrid(u32 n, f32 voxel, const Vector3& origin) {
    ScalarGrid g;
    g.voxelSize = voxel;
    g.origin = origin;
    g.Resize(n, n, n, 1.0f);
    return g;
}

// Every undirected edge of the mesh, with how many triangles use it.
//
// A closed surface has every edge shared by exactly two triangles. That single
// number catches the whole family of mesher bugs at once: a missing quad, a
// duplicated one, a surface that does not seal at the volume boundary.
std::map<std::pair<u32, u32>, u32> EdgeUseCounts(const SurfaceMesh& m) {
    std::map<std::pair<u32, u32>, u32> counts;
    for (usize i = 0; i + 2 < m.indices.size(); i += 3) {
        const u32 t[3] = { m.indices[i], m.indices[i + 1], m.indices[i + 2] };
        for (u32 e = 0; e < 3; ++e) {
            u32 a = t[e], b = t[(e + 1) % 3];
            if (a > b) std::swap(a, b);
            ++counts[{a, b}];
        }
    }
    return counts;
}

} // namespace

// --------------------------------------------------------------------------
// The field
// --------------------------------------------------------------------------

ENJIN_TEST(Sdf, ASphereFieldIsTheDistanceToItsSurface) {
    const Vector3 c(0.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(SdfSphere(Vector3(0, 0, 0), c, 2.0f), -2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(SdfSphere(Vector3(2, 0, 0), c, 2.0f), 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(SdfSphere(Vector3(5, 0, 0), c, 2.0f), 3.0f, 0.001f);
}

ENJIN_TEST(Sdf, ACapsuleIsASweptSphereAndNotAPipe) {
    const Vector3 a(0, 0, 0), b(10, 0, 0);
    // On the axis, anywhere along it.
    ENJIN_EXPECT_FLOAT_NEAR(SdfCapsule(Vector3(5, 0, 0), a, b, 2.0f), -2.0f, 0.001f);
    // Off the side.
    ENJIN_EXPECT_FLOAT_NEAR(SdfCapsule(Vector3(5, 3, 0), a, b, 2.0f), 1.0f, 0.001f);
    // Past the end it is ROUND, not flat: distance is to the end point. A prism
    // would report a flat cap here, which is what made the old tunnel a pipe.
    ENJIN_EXPECT_FLOAT_NEAR(SdfCapsule(Vector3(14, 0, 0), a, b, 2.0f), 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(SdfCapsule(Vector3(13, 3, 0), a, b, 2.0f),
                            std::sqrt(9.0f + 9.0f) - 2.0f, 0.001f);
}

ENJIN_TEST(Sdf, ATaperedCapsuleNarrowsAlongItsRun) {
    const Vector3 a(0, 0, 0), b(10, 0, 0);
    // Wide end.
    ENJIN_EXPECT_FLOAT_NEAR(SdfTaperedCapsule(Vector3(0, 0, 0), a, b, 4.0f, 1.0f), -4.0f, 0.001f);
    // Narrow end.
    ENJIN_EXPECT_FLOAT_NEAR(SdfTaperedCapsule(Vector3(10, 0, 0), a, b, 4.0f, 1.0f), -1.0f, 0.001f);
    // Halfway, halfway between.
    ENJIN_EXPECT_FLOAT_NEAR(SdfTaperedCapsule(Vector3(5, 0, 0), a, b, 4.0f, 1.0f), -2.5f, 0.001f);
}

// The melding operator, and the property that makes it safe to use on a
// landscape: it never removes material.
ENJIN_TEST(Sdf, ASmoothUnionOnlyEverAddsMaterialNearTheJoin) {
    const f32 k = 1.0f;
    for (f32 a = -3.0f; a <= 3.0f; a += 0.25f) {
        for (f32 b = -3.0f; b <= 3.0f; b += 0.25f) {
            const f32 hard = SdfUnion(a, b);
            const f32 soft = SdfSmoothUnion(a, b, k);
            // Smaller distance means more solid. A blend that went the other
            // way would eat into a hillside and open holes nobody asked for.
            ENJIN_EXPECT_TRUE(soft <= hard + 1e-4f);
            // And it must not run away: the fillet is bounded by k.
            ENJIN_EXPECT_TRUE(soft >= hard - k);
        }
    }
}

ENJIN_TEST(Sdf, ASmoothUnionWithNoRadiusIsExactlyAHardUnion) {
    ENJIN_EXPECT_FLOAT_NEAR(SdfSmoothUnion(1.0f, -2.0f, 0.0f), -2.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(SdfSmoothUnion(-5.0f, 3.0f, 0.0f), -5.0f, 0.0001f);
}

ENJIN_TEST(Sdf, SubtractingCarvesTheSecondShapeOutOfTheFirst) {
    const Vector3 origin(0, 0, 0);
    // A big solid sphere with a small one taken out of its centre.
    const f32 shell = SdfSubtract(SdfSphere(Vector3(0, 0, 0), origin, 5.0f),
                                  SdfSphere(Vector3(0, 0, 0), origin, 2.0f));
    ENJIN_EXPECT_TRUE(shell > 0.0f);   // the middle is now air

    const f32 wall = SdfSubtract(SdfSphere(Vector3(3.5f, 0, 0), origin, 5.0f),
                                 SdfSphere(Vector3(3.5f, 0, 0), origin, 2.0f));
    ENJIN_EXPECT_TRUE(wall < 0.0f);    // and the rock between them is still there
}

// --------------------------------------------------------------------------
// The mesher
// --------------------------------------------------------------------------

ENJIN_TEST(SurfaceNets, AnEmptyFieldMeshesToNothing) {
    ScalarGrid g = MakeGrid(8, 1.0f, Vector3(0, 0, 0));
    // All air.
    const SurfaceMesh m = BuildSurfaceNet(g, 0.0f);
    ENJIN_EXPECT_TRUE(m.Empty());
}

ENJIN_TEST(SurfaceNets, AGridTooSmallToHoldACellMeshesToNothing) {
    ScalarGrid g = MakeGrid(1, 1.0f, Vector3(0, 0, 0));
    g.values.assign(g.Count(), -1.0f);
    ENJIN_EXPECT_TRUE(BuildSurfaceNet(g, 0.0f).Empty());
}

ENJIN_TEST(SurfaceNets, ASphereMeshesToAClosedSurfaceAtItsOwnRadius) {
    const f32 radius = 6.0f;
    const Vector3 centre(0, 0, 0);
    ScalarGrid g = MakeGrid(33, 0.75f, Vector3(-12, -12, -12));
    SampleField(g, [&](const Vector3& p) { return SdfSphere(p, centre, radius); });

    const SurfaceMesh m = BuildSurfaceNet(g, 0.0f);
    ENJIN_ASSERT_TRUE(!m.Empty());

    // Every vertex sits on the sphere, to within the sampling grid. This is the
    // claim that separates a real mesher from one that emits voxel cubes: the
    // surface lands on the FIELD, not on the grid.
    f32 worst = 0.0f;
    for (const auto& p : m.positions) {
        const f32 r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        worst = std::max(worst, std::fabs(r - radius));
    }
    ENJIN_EXPECT_TRUE(worst < g.voxelSize);

    // Closed: every edge shared by exactly two triangles.
    usize badEdges = 0;
    for (const auto& kv : EdgeUseCounts(m)) {
        if (kv.second != 2) ++badEdges;
    }
    ENJIN_EXPECT_EQ(badEdges, (usize)0);
}

ENJIN_TEST(SurfaceNets, NormalsPointOutOfTheSolid) {
    const Vector3 centre(0, 0, 0);
    ScalarGrid g = MakeGrid(25, 1.0f, Vector3(-12, -12, -12));
    SampleField(g, [&](const Vector3& p) { return SdfSphere(p, centre, 6.0f); });

    const SurfaceMesh m = BuildSurfaceNet(g, 0.0f);
    ENJIN_ASSERT_TRUE(!m.positions.empty());

    // On a sphere centred at the origin, "out" is the position direction.
    // A mesher with its gradient sign flipped would light every cave from the
    // wrong side and look like a material bug.
    usize inward = 0;
    for (usize i = 0; i < m.positions.size(); ++i) {
        const Vector3& p = m.positions[i];
        const Vector3& n = m.normals[i];
        if (p.x * n.x + p.y * n.y + p.z * n.z <= 0.0f) ++inward;
    }
    ENJIN_EXPECT_EQ(inward, (usize)0);
}

ENJIN_TEST(SurfaceNets, TheSurfaceSealsAgainstTheEdgeOfTheVolume) {
    // Solid everywhere: the only surface is where the volume itself stops.
    // If out-of-bounds did not read as air, this would mesh to nothing and a
    // volume's walls would be open -- a collider you fall out through.
    ScalarGrid g = MakeGrid(9, 1.0f, Vector3(0, 0, 0));
    g.values.assign(g.Count(), -1.0f);

    const SurfaceMesh m = BuildSurfaceNet(g, 0.0f);
    ENJIN_ASSERT_TRUE(!m.Empty());

    usize badEdges = 0;
    for (const auto& kv : EdgeUseCounts(m)) {
        if (kv.second != 2) ++badEdges;
    }
    ENJIN_EXPECT_EQ(badEdges, (usize)0);
}

// The whole reason this module exists.
ENJIN_TEST(SurfaceNets, ACaveHasTwoSurfacesAboveTheSamePointAndAHeightmapCannot) {
    // A slab of rock with a passage bored horizontally through the middle.
    ScalarGrid g = MakeGrid(41, 0.5f, Vector3(-10, -5, -10));
    SampleField(g, [](const Vector3& p) {
        const f32 rock = SdfBox(p, Vector3(0, 0, 0), Vector3(8, 4, 8));
        const f32 passage = SdfCapsule(p, Vector3(-9, 0, 0), Vector3(9, 0, 0), 1.5f);
        return SdfSubtract(rock, passage);
    });

    const SurfaceMesh m = BuildSurfaceNet(g, 0.0f);
    ENJIN_ASSERT_TRUE(!m.Empty());

    // Down the middle of the passage, the column of samples goes
    // rock / air / rock: solid, then the bore, then solid again. That is a roof
    // over a floor, which is exactly what one height per column cannot store.
    u32 bands = 0;
    bool prevSolid = false;
    bool first = true;
    const u32 midX = 20, midZ = 20;
    for (u32 y = 0; y < g.dimY; ++y) {
        const bool solid = g.At(midX, y, midZ) < 0.0f;
        if (first) { prevSolid = solid; first = false; continue; }
        if (solid != prevSolid) ++bands;
        prevSolid = solid;
    }
    // air -> rock -> air(bore) -> rock -> air: four sign changes.
    ENJIN_EXPECT_EQ(bands, (u32)4);

    // And it is still one closed solid.
    usize badEdges = 0;
    for (const auto& kv : EdgeUseCounts(m)) {
        if (kv.second != 2) ++badEdges;
    }
    ENJIN_EXPECT_EQ(badEdges, (usize)0);
}

// Melding, end to end: a landscape and a chamber become ONE surface, with no
// seam to hide, because they were never two surfaces.
ENJIN_TEST(SurfaceNets, ATerrainAndAChamberMeshAsASingleClosedSolid) {
    ScalarGrid g = MakeGrid(41, 0.5f, Vector3(-10, -8, -10));
    SampleField(g, [](const Vector3& p) {
        // A gently rolling landscape.
        const f32 surfaceY = 2.0f * std::sin(p.x * 0.25f) * std::cos(p.z * 0.25f);
        const f32 ground = SdfHeightfield(p, surfaceY);
        // A lump of hill sitting on it, blended in rather than stuck on.
        const f32 hill = SdfSphere(p, Vector3(0, 0, 0), 5.0f);
        const f32 land = SdfSmoothUnion(ground, hill, 1.5f);
        // And a chamber hollowed out inside the hill.
        const f32 chamber = SdfSphere(p, Vector3(0, -1.0f, 0), 2.5f);
        return SdfSmoothSubtract(land, chamber, 0.5f);
    });

    const SurfaceMesh m = BuildSurfaceNet(g, 0.0f);
    ENJIN_ASSERT_TRUE(!m.Empty());

    usize badEdges = 0;
    for (const auto& kv : EdgeUseCounts(m)) {
        if (kv.second != 2) ++badEdges;
    }
    ENJIN_EXPECT_EQ(badEdges, (usize)0);

    // The chamber is really hollow: the point at its centre is air, and rock
    // surrounds it. A "meld" that had quietly filled it in would still mesh
    // closed, so closedness alone is not the claim.
    const auto sampleAt = [&](const Vector3& p) {
        const i32 ix = static_cast<i32>(std::lround((p.x - g.origin.x) / g.voxelSize));
        const i32 iy = static_cast<i32>(std::lround((p.y - g.origin.y) / g.voxelSize));
        const i32 iz = static_cast<i32>(std::lround((p.z - g.origin.z) / g.voxelSize));
        return g.At(static_cast<u32>(ix), static_cast<u32>(iy), static_cast<u32>(iz));
    };
    ENJIN_EXPECT_TRUE(sampleAt(Vector3(0.0f, -1.0f, 0.0f)) > 0.0f);   // hollow
    ENJIN_EXPECT_TRUE(sampleAt(Vector3(0.0f, -4.5f, 0.0f)) < 0.0f);   // rock beneath it
}

ENJIN_TEST(SurfaceNets, SamplingAFieldFillsEverySampleInTheGrid) {
    ScalarGrid g = MakeGrid(5, 2.0f, Vector3(1, 2, 3));
    u32 calls = 0;
    SampleField(g, [&](const Vector3&) { ++calls; return -1.0f; });
    ENJIN_EXPECT_EQ(static_cast<usize>(calls), g.Count());

    // And the grid's own coordinate maths agrees with where it said it sampled.
    ENJIN_EXPECT_FLOAT_NEAR(g.PositionOf(0, 0, 0).x, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(g.PositionOf(4, 0, 0).x, 9.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(g.PositionOf(0, 4, 0).y, 10.0f, 0.001f);
}

ENJIN_TEST_MAIN()
