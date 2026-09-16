#include "EnjinTest.h"
#include "Enjin/AI/Navmesh.h"

#include <cmath>
#include <vector>

using namespace Enjin;
using namespace Enjin::AI;
using namespace Enjin::Math;

// ===========================================================================
// NavmeshUtils — Pure Math
// ===========================================================================

ENJIN_TEST(NavUtils, Distance) {
    f32 d = NavmeshUtils::Distance(Vector3(0, 0, 0), Vector3(3, 4, 0));
    ENJIN_EXPECT_FLOAT_NEAR(d, 5.0f, 0.01f);
}

ENJIN_TEST(NavUtils, DistanceSquared) {
    f32 d2 = NavmeshUtils::DistanceSquared(Vector3(0, 0, 0), Vector3(3, 4, 0));
    ENJIN_EXPECT_FLOAT_NEAR(d2, 25.0f, 0.01f);
}

ENJIN_TEST(NavUtils, ManhattanDistance) {
    f32 d = NavmeshUtils::ManhattanDistance(Vector3(1, 2, 3), Vector3(4, 6, 8));
    ENJIN_EXPECT_FLOAT_NEAR(d, 12.0f, 0.01f); // |3| + |4| + |5|
}

ENJIN_TEST(NavUtils, PolygonAreaTriangle) {
    std::vector<Vector3> tri = {
        Vector3(0, 0, 0), Vector3(4, 0, 0), Vector3(0, 0, 3)
    };
    f32 area = NavmeshUtils::CalculatePolygonArea(tri);
    ENJIN_EXPECT_FLOAT_NEAR(area, 6.0f, 0.1f); // 0.5 * 4 * 3
}

ENJIN_TEST(NavUtils, PolygonCenter) {
    std::vector<Vector3> tri = {
        Vector3(0, 0, 0), Vector3(3, 0, 0), Vector3(0, 0, 3)
    };
    Vector3 c = NavmeshUtils::CalculatePolygonCenter(tri);
    ENJIN_EXPECT_FLOAT_NEAR(c.x, 1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(c.z, 1.0f, 0.01f);
}

ENJIN_TEST(NavUtils, ConvexSquareIsConvex) {
    std::vector<Vector3> square = {
        Vector3(0, 0, 0), Vector3(1, 0, 0),
        Vector3(1, 0, 1), Vector3(0, 0, 1)
    };
    ENJIN_EXPECT_TRUE(NavmeshUtils::IsPolygonConvex(square));
}

ENJIN_TEST(NavUtils, TriangulateConvexQuad) {
    std::vector<Vector3> quad = {
        Vector3(0, 0, 0), Vector3(1, 0, 0),
        Vector3(1, 0, 1), Vector3(0, 0, 1)
    };
    auto indices = NavmeshUtils::TriangulateConvex(quad);
    // A convex quad triangulates to 2 triangles = 6 indices
    ENJIN_EXPECT_EQ(indices.size(), (size_t)6);
}

// ===========================================================================
// NavPolygon Defaults
// ===========================================================================

ENJIN_TEST(NavPolygon, DefaultValues) {
    NavPolygon poly;
    ENJIN_EXPECT_EQ(poly.id, 0u);
    ENJIN_EXPECT_TRUE(poly.walkable);
    ENJIN_EXPECT_EQ(poly.areaType, 0u);
    ENJIN_EXPECT_FLOAT_EQ(poly.cost, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(poly.area, 0.0f);
}

// ===========================================================================
// PathNode
// ===========================================================================

ENJIN_TEST(PathNode, FCostCalculation) {
    PathNode node;
    node.gCost = 3.0f;
    node.hCost = 4.0f;
    ENJIN_EXPECT_FLOAT_EQ(node.fCost(), 7.0f);
}

ENJIN_TEST(PathNode, ComparisonByFCost) {
    PathNode a, b;
    a.gCost = 1.0f; a.hCost = 2.0f; // fCost = 3
    b.gCost = 2.0f; b.hCost = 3.0f; // fCost = 5
    ENJIN_EXPECT_FALSE(a > b); // a.fCost < b.fCost
    ENJIN_EXPECT_TRUE(b > a);
}

// ===========================================================================
// PathResult Defaults
// ===========================================================================

ENJIN_TEST(PathResult, DefaultValues) {
    PathResult result;
    ENJIN_EXPECT_FALSE(result.success);
    ENJIN_EXPECT_EQ(result.waypoints.size(), (size_t)0);
    ENJIN_EXPECT_FLOAT_EQ(result.totalCost, 0.0f);
    ENJIN_EXPECT_EQ(result.nodesExplored, 0u);
}

// ===========================================================================
// NavmeshGenSettings Defaults
// ===========================================================================

ENJIN_TEST(GenSettings, Defaults) {
    NavmeshGenSettings settings;
    ENJIN_EXPECT_FLOAT_NEAR(settings.cellSize, 0.3f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(settings.agentHeight, 2.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(settings.agentRadius, 0.6f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(settings.agentMaxSlope, 45.0f, 0.01f);
    ENJIN_EXPECT_EQ(settings.vertsPerPoly, 6u);
}

// ===========================================================================
// Navmesh — Polygon Management
// ===========================================================================

ENJIN_TEST(Navmesh, AddAndGetPolygon) {
    Navmesh mesh;
    NavPolygon poly;
    poly.vertices = { Vector3(0,0,0), Vector3(1,0,0), Vector3(0,0,1) };
    poly.center = NavmeshUtils::CalculatePolygonCenter(poly.vertices);
    u32 id = mesh.AddPolygon(poly);
    const NavPolygon* got = mesh.GetPolygon(id);
    ENJIN_EXPECT_NOT_NULL(got);
    ENJIN_EXPECT_EQ(got->vertices.size(), (size_t)3);
}

ENJIN_TEST(Navmesh, RemovePolygon) {
    Navmesh mesh;
    NavPolygon poly;
    poly.vertices = { Vector3(0,0,0), Vector3(1,0,0), Vector3(0,0,1) };
    u32 id = mesh.AddPolygon(poly);
    mesh.RemovePolygon(id);
    ENJIN_EXPECT_TRUE(mesh.GetPolygon(id) == nullptr);
}

ENJIN_TEST(Navmesh, ClearRemovesAll) {
    Navmesh mesh;
    NavPolygon p1, p2;
    p1.vertices = { Vector3(0,0,0), Vector3(1,0,0), Vector3(0,0,1) };
    p2.vertices = { Vector3(2,0,0), Vector3(3,0,0), Vector3(2,0,1) };
    mesh.AddPolygon(p1);
    mesh.AddPolygon(p2);
    mesh.Clear();
    ENJIN_EXPECT_EQ(mesh.GetPolygons().size(), (size_t)0);
}

// ===========================================================================
// Navmesh — Grid Generation & Pathfinding
// ===========================================================================

ENJIN_TEST(NavGen, GenerateGrid) {
    NavmeshGenerator gen;
    bool ok = gen.GenerateGrid(Vector3(0, 0, 0), Vector3(10, 0, 10), 2.0f);
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_GT(gen.GetNavmesh().GetPolygons().size(), (size_t)0);
}

ENJIN_TEST(Pathfinding, SimpleGridPath) {
    NavmeshGenerator gen;
    gen.GenerateGrid(Vector3(0, 0, 0), Vector3(10, 0, 10), 2.0f);
    Navmesh& mesh = gen.GetNavmesh();
    mesh.BuildConnectivity();

    Pathfinder pathfinder(&mesh);
    PathResult result = pathfinder.FindPath(Vector3(1, 0, 1), Vector3(9, 0, 9));
    ENJIN_EXPECT_TRUE(result.success);
    ENJIN_EXPECT_GT(result.waypoints.size(), (size_t)0);
    ENJIN_EXPECT_GT(result.totalCost, 0.0f);
}

ENJIN_TEST(Pathfinding, SameStartEndSucceeds) {
    NavmeshGenerator gen;
    gen.GenerateGrid(Vector3(0, 0, 0), Vector3(10, 0, 10), 2.0f);
    Navmesh& mesh = gen.GetNavmesh();
    mesh.BuildConnectivity();

    Pathfinder pathfinder(&mesh);
    PathResult result = pathfinder.FindPath(Vector3(5, 0, 5), Vector3(5, 0, 5));
    ENJIN_EXPECT_TRUE(result.success);
}

ENJIN_TEST(Pathfinding, PathExistsQuery) {
    NavmeshGenerator gen;
    gen.GenerateGrid(Vector3(0, 0, 0), Vector3(10, 0, 10), 2.0f);
    Navmesh& mesh = gen.GetNavmesh();
    mesh.BuildConnectivity();

    Pathfinder pathfinder(&mesh);
    ENJIN_EXPECT_TRUE(pathfinder.PathExists(Vector3(1, 0, 1), Vector3(9, 0, 9)));
}

ENJIN_TEST(Pathfinding, AreaCostMultiplier) {
    Pathfinder pathfinder;
    pathfinder.SetAreaCost(1, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(pathfinder.GetAreaCost(1), 5.0f);
}

// ===========================================================================
// PathFollowerComponent Defaults
// ===========================================================================

ENJIN_TEST(PathFollower, Defaults) {
    PathFollowerComponent follower;
    ENJIN_EXPECT_FLOAT_EQ(follower.speed, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(follower.turnSpeed, 180.0f);
    ENJIN_EXPECT_FLOAT_EQ(follower.arrivalRadius, 0.5f);
    ENJIN_EXPECT_FALSE(follower.isFollowing);
    ENJIN_EXPECT_FALSE(follower.hasArrived);
    ENJIN_EXPECT_TRUE(follower.smoothRotation);
}


// ===========================================================================
// Path smoothing — Math::Spline finally wired to nav
// ===========================================================================
//
// A* returns corners. These pin what smoothing may and may not do to them: it
// may round the route BETWEEN corners, it may not move where the agent starts
// or where it was sent, and it may not hand back nothing.

namespace {
// A right-angle path: straight along +X, then a square turn along +Z. The corner
// is the whole point -- a smoothed route should cut it, a raw one cannot.
std::vector<Vector3> LPath() {
    return { Vector3(0, 0, 0), Vector3(10, 0, 0), Vector3(10, 0, 10) };
}
bool Near(f32 a, f32 b, f32 eps = 0.001f) { return std::fabs(a - b) < eps; }
bool Same(const Vector3& a, const Vector3& b) {
    return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z);
}
} // namespace

ENJIN_TEST(PathSmoothing, TheAgentStillStartsAndEndsWhereItWasTold) {
    // The one thing smoothing must never do. A curve that starts a little off
    // the agent, or stops a little short of the destination, is worse than the
    // jagged path it replaced -- the agent visibly misses.
    const auto raw = LPath();
    const auto smooth = SmoothPath(raw, 1.0f);

    ENJIN_ASSERT_TRUE(smooth.size() >= 2);
    ENJIN_EXPECT_TRUE(Same(smooth.front(), raw.front()));
    ENJIN_EXPECT_TRUE(Same(smooth.back(), raw.back()));
}

ENJIN_TEST(PathSmoothing, TheCornerIsActuallyRounded) {
    // Otherwise this is just a resample. Every raw point lies on one of the two
    // axis-aligned legs, so a rounded route must put at least one sample off
    // both of them.
    const auto smooth = SmoothPath(LPath(), 0.5f);

    bool offTheLegs = false;
    for (const auto& p : smooth) {
        const bool onFirstLeg  = Near(p.z, 0.0f);
        const bool onSecondLeg = Near(p.x, 10.0f);
        if (!onFirstLeg && !onSecondLeg) { offTheLegs = true; break; }
    }
    ENJIN_EXPECT_TRUE(offTheLegs);
}

ENJIN_TEST(PathSmoothing, ADegeneratePathComesBackUnchangedRatherThanEmpty) {
    // An agent handed an empty path stops where it stands, which reads as
    // pathfinding having failed rather than smoothing having declined.
    const std::vector<Vector3> none;
    const std::vector<Vector3> one{ Vector3(1, 2, 3) };
    const std::vector<Vector3> two{ Vector3(0, 0, 0), Vector3(5, 0, 0) };

    ENJIN_EXPECT_TRUE(SmoothPath(none, 1.0f).empty());
    ENJIN_EXPECT_EQ(SmoothPath(one, 1.0f).size(), one.size());
    ENJIN_EXPECT_EQ(SmoothPath(two, 1.0f).size(), two.size());
    // Spacing is a distance, not a hint: zero or negative means do nothing.
    ENJIN_EXPECT_EQ(SmoothPath(LPath(), 0.0f).size(), LPath().size());
    ENJIN_EXPECT_EQ(SmoothPath(LPath(), -1.0f).size(), LPath().size());
}

ENJIN_TEST(PathSmoothing, TighterSpacingGivesMorePointsAndTheSplineKeepsTheCorners) {
    const auto coarse = SmoothPath(LPath(), 4.0f);
    const auto fine   = SmoothPath(LPath(), 0.5f);
    ENJIN_EXPECT_TRUE(fine.size() > coarse.size());

    // Catmull-Rom passes THROUGH its control points, so the route is still the
    // route: the spline can be evaluated back at each original waypoint.
    const Spline s = BuildPathSpline(LPath());
    ENJIN_ASSERT_EQ(s.GetPointCount(), LPath().size());
    ENJIN_EXPECT_TRUE(Same(s.GetPoint(1).position, Vector3(10, 0, 0)));
}
ENJIN_TEST_MAIN()
