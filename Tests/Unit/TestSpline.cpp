#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Spline.h"
#include "Enjin/Math/Math.h"

using namespace Enjin;
using namespace Enjin::Math;

// ============================================================================
// Linear Spline
// ============================================================================

ENJIN_TEST(SplineLinear, EvaluateEndpoints) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(10, 0, 0));

    auto p0 = s.Evaluate(0.0f);
    auto p1 = s.Evaluate(1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(p0.x, 0.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(p1.x, 10.0f, 0.01f);
}

ENJIN_TEST(SplineLinear, EvaluateMidpoint) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(10, 0, 0));

    auto mid = s.Evaluate(0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.x, 5.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.y, 0.0f, 0.01f);
}

ENJIN_TEST(SplineLinear, MultiSegment) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(10, 0, 0));
    s.AddPoint(Vector3(10, 10, 0));

    // t=0.5 should be at the junction of two segments
    auto mid = s.Evaluate(0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.x, 10.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.y, 0.0f, 0.01f);
}

ENJIN_TEST(SplineLinear, TotalLength) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(3, 4, 0));  // Length = 5

    f32 len = s.GetTotalLength();
    ENJIN_EXPECT_FLOAT_NEAR(len, 5.0f, 0.1f);
}

ENJIN_TEST(SplineLinear, TangentDirection) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(10, 0, 0));

    auto tangent = s.EvaluateTangent(0.5f);
    auto norm = tangent.Normalized();
    ENJIN_EXPECT_FLOAT_NEAR(norm.x, 1.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(norm.y, 0.0f, 0.01f);
}

// ============================================================================
// CatmullRom Spline
// ============================================================================

ENJIN_TEST(SplineCatmullRom, PassesThroughControlPoints) {
    Spline s(SplineType::CatmullRom);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(5, 5, 0));
    s.AddPoint(Vector3(10, 0, 0));
    s.AddPoint(Vector3(15, 5, 0));

    // CatmullRom should pass through interior points
    auto start = s.Evaluate(0.0f);
    auto end = s.Evaluate(1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(start.x, 0.0f, 0.1f);
    ENJIN_EXPECT_FLOAT_NEAR(end.x, 15.0f, 0.1f);
}

ENJIN_TEST(SplineCatmullRom, LengthPositive) {
    Spline s(SplineType::CatmullRom);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(5, 5, 0));
    s.AddPoint(Vector3(10, 0, 0));

    f32 len = s.GetTotalLength();
    ENJIN_EXPECT_GT(len, 0.0f);
}

// ============================================================================
// Arc Length Parameterization
// ============================================================================

ENJIN_TEST(SplineArcLength, DistanceToTRoundTrip) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(10, 0, 0));

    f32 totalLen = s.GetTotalLength();
    f32 halfDist = totalLen * 0.5f;
    f32 t = s.DistanceToT(halfDist);
    ENJIN_EXPECT_FLOAT_NEAR(t, 0.5f, 0.05f);
}

ENJIN_TEST(SplineArcLength, TToDistanceRoundTrip) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(10, 0, 0));

    f32 dist = s.TToDistance(0.5f);
    f32 totalLen = s.GetTotalLength();
    ENJIN_EXPECT_FLOAT_NEAR(dist, totalLen * 0.5f, 0.1f);
}

// ============================================================================
// FindClosest
// ============================================================================

ENJIN_TEST(SplineClosest, FindClosestOnLine) {
    Spline s(SplineType::Linear);
    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(10, 0, 0));

    auto closest = s.FindClosestPoint(Vector3(5, 3, 0));
    ENJIN_EXPECT_FLOAT_NEAR(closest.x, 5.0f, 0.2f);
    ENJIN_EXPECT_FLOAT_NEAR(closest.y, 0.0f, 0.01f);
}

// ============================================================================
// Point Count / Clear
// ============================================================================

ENJIN_TEST(SplineOps, PointManagement) {
    Spline s(SplineType::Linear);
    ENJIN_EXPECT_EQ(s.GetPointCount(), (usize)0);

    s.AddPoint(Vector3(0, 0, 0));
    s.AddPoint(Vector3(1, 0, 0));
    ENJIN_EXPECT_EQ(s.GetPointCount(), (usize)2);

    s.RemovePoint(0);
    ENJIN_EXPECT_EQ(s.GetPointCount(), (usize)1);

    s.Clear();
    ENJIN_EXPECT_EQ(s.GetPointCount(), (usize)0);
}

// ============================================================================
// 2D Spline
// ============================================================================

ENJIN_TEST(Spline2D, LinearEvaluate) {
    Spline2D s(SplineType::Linear);
    s.AddPoint(Vector2(0, 0));
    s.AddPoint(Vector2(10, 10));

    auto mid = s.Evaluate(0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.x, 5.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(mid.y, 5.0f, 0.01f);
}

ENJIN_TEST(Spline2D, TotalLength) {
    Spline2D s(SplineType::Linear);
    s.AddPoint(Vector2(0, 0));
    s.AddPoint(Vector2(3, 4));

    f32 len = s.GetTotalLength();
    ENJIN_EXPECT_FLOAT_NEAR(len, 5.0f, 0.1f);
}

// ============================================================================
// Bezier helpers
// ============================================================================

ENJIN_TEST(SplineBezierHelper, CubicBezierEndpoints) {
    Vector3 p0(0, 0, 0), p1(1, 2, 0), p2(3, 2, 0), p3(4, 0, 0);

    auto start = SplineUtils::CubicBezier(p0, p1, p2, p3, 0.0f);
    auto end = SplineUtils::CubicBezier(p0, p1, p2, p3, 1.0f);
    ENJIN_EXPECT_VEC3_EQ(start, 0.0f, 0.0f, 0.0f);
    ENJIN_EXPECT_VEC3_EQ(end, 4.0f, 0.0f, 0.0f);
}

ENJIN_TEST(SplineBezierHelper, CatmullRomEndpoints) {
    Vector3 p0(0, 0, 0), p1(2, 0, 0), p2(4, 0, 0), p3(6, 0, 0);

    auto start = SplineUtils::CatmullRom(p0, p1, p2, p3, 0.0f);
    auto end = SplineUtils::CatmullRom(p0, p1, p2, p3, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(start.x, 2.0f, 0.01f);  // CatmullRom returns p1 at t=0
    ENJIN_EXPECT_FLOAT_NEAR(end.x, 4.0f, 0.01f);     // p2 at t=1
}

ENJIN_TEST_MAIN()
