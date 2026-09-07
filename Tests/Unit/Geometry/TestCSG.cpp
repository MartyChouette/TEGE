// Constructive solid geometry on convex brushes.
//
// The maths is proven here before any editor tool wraps it. A CSG system fails
// in ways that look like art bugs -- a wall you can see through, a doorway that
// is still solid to the player, a face wound inside out -- so the properties
// worth asserting are geometric facts, not triangle counts.
#include "EnjinTest.h"
#include "Enjin/Geometry/CSG.h"

using namespace Enjin;
using namespace Enjin::Geometry;
using namespace Enjin::Math;

namespace {

// Does any triangle of the mesh contain this point, projected onto its plane?
// Used to ask "is there surface here", which is what a hole is the absence of.
bool AnyVertexNear(const ECS::MeshComponent& m, const Vector3& p, f32 tol) {
    for (const auto& v : m.vertices) {
        const Vector3 d(v.position.x - p.x, v.position.y - p.y, v.position.z - p.z);
        if (std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z) <= tol) return true;
    }
    return false;
}

Vector3 MeshMin(const ECS::MeshComponent& m) {
    Vector3 mn(1e9f, 1e9f, 1e9f);
    for (const auto& v : m.vertices) {
        mn.x = std::min(mn.x, v.position.x);
        mn.y = std::min(mn.y, v.position.y);
        mn.z = std::min(mn.z, v.position.z);
    }
    return mn;
}
Vector3 MeshMax(const ECS::MeshComponent& m) {
    Vector3 mx(-1e9f, -1e9f, -1e9f);
    for (const auto& v : m.vertices) {
        mx.x = std::max(mx.x, v.position.x);
        mx.y = std::max(mx.y, v.position.y);
        mx.z = std::max(mx.z, v.position.z);
    }
    return mx;
}

// Is the point covered by any triangle lying on the given z plane, tested in XY?
// This is the real question a doorway asks: not "did geometry change" but "can
// you walk through it". A vertex-proximity check would pass on a wall that was
// never cut, because the cutter's own liner faces put vertices there too.
bool SurfaceCoversXY(const ECS::MeshComponent& m, f32 x, f32 y, f32 z, f32 zTol = 0.01f) {
    auto sign = [](f32 ax, f32 ay, f32 bx, f32 by, f32 cx, f32 cy) {
        return (ax - cx) * (by - cy) - (bx - cx) * (ay - cy);
    };
    for (usize i = 0; i + 2 < m.indices.size(); i += 3) {
        const auto& a = m.vertices[m.indices[i]].position;
        const auto& b = m.vertices[m.indices[i + 1]].position;
        const auto& c = m.vertices[m.indices[i + 2]].position;
        if (std::fabs(a.z - z) > zTol || std::fabs(b.z - z) > zTol || std::fabs(c.z - z) > zTol) {
            continue;   // not on this face plane
        }
        const f32 d1 = sign(x, y, a.x, a.y, b.x, b.y);
        const f32 d2 = sign(x, y, b.x, b.y, c.x, c.y);
        const f32 d3 = sign(x, y, c.x, c.y, a.x, a.y);
        const bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        const bool pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        if (!(neg && pos)) return true;   // same side of all three edges
    }
    return false;
}

} // namespace

// ===========================================================================
// Brushes
// ===========================================================================

ENJIN_TEST(CsgBrush, BoxHasSixFaces) {
    Brush b = Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f));
    ENJIN_EXPECT_EQ(b.planes.size(), (usize)6);

    auto faces = BuildFaces(b);
    ENJIN_EXPECT_EQ(faces.size(), (usize)6);
    for (const auto& f : faces) {
        ENJIN_EXPECT_EQ(f.vertices.size(), (usize)4);   // a box face is a quad
    }
}

ENJIN_TEST(CsgBrush, BoxFacesLandOnItsBounds) {
    Brush b = Brush::Box(Vector3(0.0f), Vector3(2.0f, 3.0f, 4.0f));
    ECS::MeshComponent m = ToMesh(BuildFaces(b));

    Vector3 mn = MeshMin(m), mx = MeshMax(m);
    ENJIN_EXPECT_FLOAT_NEAR(mn.x, -2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mx.x,  2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mn.y, -3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mx.y,  3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mn.z, -4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(mx.z,  4.0f, 0.001f);
}

ENJIN_TEST(CsgBrush, ContainsAgreesWithTheBounds) {
    Brush b = Brush::Box(Vector3(5.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));
    ENJIN_EXPECT_TRUE(b.Contains(Vector3(5.0f, 0.0f, 0.0f)));
    ENJIN_EXPECT_TRUE(b.Contains(Vector3(5.9f, 0.9f, 0.9f)));
    ENJIN_EXPECT_FALSE(b.Contains(Vector3(7.0f, 0.0f, 0.0f)));
    ENJIN_EXPECT_FALSE(b.Contains(Vector3(0.0f, 0.0f, 0.0f)));
}

// A rotated box is still exactly a box, not an approximation of one: the planes
// are built from the rotated axes rather than from world axes.
ENJIN_TEST(CsgBrush, RotatedBoxStaysABox) {
    Brush b = Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f),
                         Quaternion::FromEulerDegrees(Vector3(0.0f, 45.0f, 0.0f)));
    auto faces = BuildFaces(b);
    ENJIN_EXPECT_EQ(faces.size(), (usize)6);

    // Turned 45 degrees about Y, the corners reach sqrt(2) on X and Z and the
    // height is untouched.
    ECS::MeshComponent m = ToMesh(faces);
    ENJIN_EXPECT_FLOAT_NEAR(MeshMax(m).x, 1.41421f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(MeshMax(m).y, 1.0f, 0.001f);
}

ENJIN_TEST(CsgBrush, PrismHasSidesPlusCaps) {
    Brush b = Brush::Prism(Vector3(0.0f), 1.0f, 2.0f, 8);
    ENJIN_EXPECT_EQ(b.planes.size(), (usize)10);   // 8 sides + top + bottom
    ENJIN_EXPECT_EQ(BuildFaces(b).size(), (usize)10);
}

// Planes that do not enclose anything must produce nothing, rather than a
// scaffold quad 4096 units across leaking into the level.
ENJIN_TEST(CsgBrush, UnboundedPlanesProduceNothing) {
    Brush b;
    b.planes.push_back(Plane::FromPointNormal(Vector3(0.0f), Vector3(0, 1, 0)));
    b.planes.push_back(Plane::FromPointNormal(Vector3(0.0f), Vector3(0, -1, 0)));
    ENJIN_EXPECT_TRUE(BuildFaces(b).empty());
}

// ===========================================================================
// Clipping
// ===========================================================================

ENJIN_TEST(CsgClip, BrushFaceFullyOutsideSurvivesWhole) {
    Brush cutter = Brush::Box(Vector3(100.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f));
    BrushFace p = BuildFaces(Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f)))[0];

    auto pieces = ClipFaceOutsideBrush(p, cutter);
    ENJIN_ASSERT_EQ(pieces.size(), (usize)1);
    ENJIN_EXPECT_EQ(pieces[0].vertices.size(), p.vertices.size());
}

ENJIN_TEST(CsgClip, BrushFaceFullyInsideIsRemoved) {
    Brush big = Brush::Box(Vector3(0.0f), Vector3(10.0f, 10.0f, 10.0f));
    BrushFace p = BuildFaces(Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f)))[0];

    ENJIN_EXPECT_TRUE(ClipFaceOutsideBrush(p, big).empty());
}

// ===========================================================================
// The operation
// ===========================================================================

ENJIN_TEST(CsgSolid, AddAloneIsJustTheBrush) {
    std::vector<BrushEntry> brushes;
    brushes.push_back({ Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f)), BrushOp::Add });

    ENJIN_EXPECT_EQ(BuildSolid(brushes).size(), (usize)6);
}

// The doorway. A wall with a hole cut through it must have no surface where the
// hole is, and must still reach its original extents everywhere else.
ENJIN_TEST(CsgSolid, SubtractCutsAHoleThroughAWall) {
    std::vector<BrushEntry> brushes;
    brushes.push_back({ Brush::Box(Vector3(0.0f), Vector3(4.0f, 3.0f, 0.25f)), BrushOp::Add });
    // A doorway through the middle, deeper than the wall so it cuts clean through.
    brushes.push_back({ Brush::Box(Vector3(0.0f, -1.0f, 0.0f), Vector3(1.0f, 2.0f, 1.0f)),
                        BrushOp::Subtract });

    ECS::MeshComponent m = BuildMesh(brushes);
    ENJIN_ASSERT_TRUE(m.vertices.size() > 0);

    // The wall still spans its full width and height.
    ENJIN_EXPECT_FLOAT_NEAR(MeshMin(m).x, -4.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(MeshMax(m).x,  4.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(MeshMax(m).y,  3.0f, 0.01f);

    // The actual question: is the front face gone where the doorway is, and
    // still there where it is not. Anything less than this passes on a wall
    // that was never cut, because the cutter's liner faces leave vertices at
    // the opening either way.
    ENJIN_EXPECT_FALSE(SurfaceCoversXY(m, 0.0f, -1.0f, 0.25f));   // through the doorway
    ENJIN_EXPECT_TRUE(SurfaceCoversXY(m, 3.0f, 0.0f, 0.25f));     // wall beside it
    ENJIN_EXPECT_TRUE(SurfaceCoversXY(m, 0.0f, 2.5f, 0.25f));     // lintel above it
}

// The artefact this exists to prevent: a subtraction that removes the front
// face and leaves you looking into the back of the wall. The cutter's own faces
// must come back, flipped, as the inner surface of the hole.
ENJIN_TEST(CsgSolid, SubtractLeavesAnInnerSurface) {
    std::vector<BrushEntry> plain;
    plain.push_back({ Brush::Box(Vector3(0.0f), Vector3(4.0f, 3.0f, 0.25f)), BrushOp::Add });
    const usize plainFaces = BuildSolid(plain).size();

    std::vector<BrushEntry> cut = plain;
    cut.push_back({ Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f)), BrushOp::Subtract });

    ENJIN_EXPECT_TRUE(BuildSolid(cut).size() > plainFaces);
}

ENJIN_TEST(CsgSolid, SubtractingSomethingMissingChangesNothingButTheLiner) {
    std::vector<BrushEntry> brushes;
    brushes.push_back({ Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f)), BrushOp::Add });
    const usize before = BuildSolid(brushes).size();

    brushes.push_back({ Brush::Box(Vector3(50.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f)),
                        BrushOp::Subtract });
    ECS::MeshComponent m = BuildMesh(brushes);

    // The original solid is untouched: it still spans exactly its own bounds and
    // has not been clipped by a cutter nowhere near it.
    ENJIN_EXPECT_FLOAT_NEAR(MeshMin(m).x, -1.0f, 0.01f);
    ENJIN_EXPECT_TRUE(BuildSolid(brushes).size() >= before);
}

ENJIN_TEST(CsgSolid, IntersectKeepsOnlyTheOverlap) {
    std::vector<BrushEntry> brushes;
    brushes.push_back({ Brush::Box(Vector3(0.0f), Vector3(2.0f, 2.0f, 2.0f)), BrushOp::Add });
    brushes.push_back({ Brush::Box(Vector3(1.0f, 0.0f, 0.0f), Vector3(2.0f, 2.0f, 2.0f)),
                        BrushOp::Intersect });

    ECS::MeshComponent m = BuildMesh(brushes);
    ENJIN_ASSERT_TRUE(m.vertices.size() > 0);

    // Overlap of [-2,2] and [-1,3] on X is [-1,2].
    ENJIN_EXPECT_FLOAT_NEAR(MeshMin(m).x, -1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(MeshMax(m).x,  2.0f, 0.01f);
}

// ===========================================================================
// Meshing
// ===========================================================================

ENJIN_TEST(CsgMesh, NormalsComeFromThePlanesAndAreUnit) {
    ECS::MeshComponent m = ToMesh(BuildFaces(Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f))));
    ENJIN_ASSERT_TRUE(m.vertices.size() > 0);

    for (const auto& v : m.vertices) {
        const f32 len = std::sqrt(v.normal.x * v.normal.x +
                                  v.normal.y * v.normal.y +
                                  v.normal.z * v.normal.z);
        ENJIN_EXPECT_FLOAT_NEAR(len, 1.0f, 0.001f);
    }
}

ENJIN_TEST(CsgMesh, IndicesAreInRangeAndTriangulated) {
    ECS::MeshComponent m = BuildMesh({
        { Brush::Box(Vector3(0.0f), Vector3(2.0f, 2.0f, 0.5f)), BrushOp::Add },
        { Brush::Box(Vector3(0.0f), Vector3(0.5f, 0.5f, 2.0f)), BrushOp::Subtract },
    });

    ENJIN_ASSERT_TRUE(m.indices.size() > 0);
    ENJIN_EXPECT_EQ(m.indices.size() % 3, (usize)0);
    for (u32 i : m.indices) {
        ENJIN_EXPECT_TRUE(i < static_cast<u32>(m.vertices.size()));
    }
    ENJIN_ASSERT_EQ(m.subMeshes.size(), (usize)1);
    ENJIN_EXPECT_EQ(m.subMeshes[0].indexCount, (u32)m.indices.size());
}

ENJIN_TEST(CsgMesh, EmptyBrushListMakesAnEmptyMeshNotACrash) {
    ECS::MeshComponent m = BuildMesh({});
    ENJIN_EXPECT_EQ(m.vertices.size(), (usize)0);
    ENJIN_EXPECT_EQ(m.indices.size(), (usize)0);
    ENJIN_EXPECT_EQ(m.subMeshes.size(), (usize)0);
}

ENJIN_TEST_MAIN()
