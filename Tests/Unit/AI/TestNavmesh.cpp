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

ENJIN_TEST_MAIN()
