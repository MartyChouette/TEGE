#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace Math {

// Spline types
enum class SplineType : u8 {
    Linear,         // Straight lines between points
    Bezier,         // Cubic Bezier curves
    CatmullRom,     // Catmull-Rom (smooth through all points)
    BSpline         // B-Spline (smooth, doesn't pass through control points)
};

// A single point on a spline with optional tangent data
struct SplinePoint {
    Vector3 position;
    Vector3 tangentIn;   // Incoming tangent (for Bezier)
    Vector3 tangentOut;  // Outgoing tangent (for Bezier)
    f32 roll = 0.0f;     // Roll angle at this point (for camera rails)

    SplinePoint() = default;
    SplinePoint(const Vector3& pos) : position(pos), tangentIn(0,0,0), tangentOut(0,0,0) {}
    SplinePoint(const Vector3& pos, const Vector3& tanIn, const Vector3& tanOut)
        : position(pos), tangentIn(tanIn), tangentOut(tanOut) {}
};

// Spline class - represents a path through 3D space
class ENJIN_API Spline {
public:
    Spline(SplineType type = SplineType::CatmullRom);
    ~Spline() = default;

    // Control points
    void AddPoint(const Vector3& point);
    void AddPoint(const SplinePoint& point);
    void InsertPoint(usize index, const Vector3& point);
    void RemovePoint(usize index);
    void SetPoint(usize index, const Vector3& point);
    void Clear();

    usize GetPointCount() const { return m_Points.size(); }
    const SplinePoint& GetPoint(usize index) const { return m_Points[index]; }
    SplinePoint& GetPoint(usize index) { return m_Points[index]; }
    const std::vector<SplinePoint>& GetPoints() const { return m_Points; }

    // Spline properties
    void SetType(SplineType type) { m_Type = type; m_Dirty = true; }
    SplineType GetType() const { return m_Type; }

    void SetClosed(bool closed) { m_Closed = closed; m_Dirty = true; }
    bool IsClosed() const { return m_Closed; }

    void SetTension(f32 tension) { m_Tension = tension; m_Dirty = true; }
    f32 GetTension() const { return m_Tension; }

    // Evaluation (t = 0 to 1 along entire spline)
    Vector3 Evaluate(f32 t) const;
    Vector3 EvaluateTangent(f32 t) const;
    Vector3 EvaluateNormal(f32 t) const;
    f32 EvaluateRoll(f32 t) const;

    // Get a full transform at point t (position, forward, up, right)
    void EvaluateFrame(f32 t, Vector3& position, Vector3& forward, Vector3& up, Vector3& right) const;

    // Segment-based evaluation (segment index + local t)
    Vector3 EvaluateSegment(usize segment, f32 t) const;
    Vector3 EvaluateSegmentTangent(usize segment, f32 t) const;

    // Length and arc-length parameterization
    f32 GetTotalLength() const;
    f32 GetSegmentLength(usize segment) const;
    f32 TToDistance(f32 t) const;
    f32 DistanceToT(f32 distance) const;

    // Find closest point on spline to a position
    f32 FindClosestT(const Vector3& point, usize samples = 100) const;
    Vector3 FindClosestPoint(const Vector3& point, usize samples = 100) const;

    // Generate points along the spline for rendering/collision
    std::vector<Vector3> GeneratePoints(usize pointsPerSegment = 10) const;
    std::vector<Vector3> GeneratePointsByDistance(f32 spacing) const;

    // Auto-calculate tangents for Bezier mode
    void AutoCalculateTangents(f32 smoothness = 0.5f);

private:
    void UpdateCache() const;
    Vector3 EvaluateLinear(usize segment, f32 t) const;
    Vector3 EvaluateBezier(usize segment, f32 t) const;
    Vector3 EvaluateCatmullRom(usize segment, f32 t) const;
    Vector3 EvaluateBSpline(usize segment, f32 t) const;

    Vector3 EvaluateLinearTangent(usize segment, f32 t) const;
    Vector3 EvaluateBezierTangent(usize segment, f32 t) const;
    Vector3 EvaluateCatmullRomTangent(usize segment, f32 t) const;
    Vector3 EvaluateBSplineTangent(usize segment, f32 t) const;

    std::vector<SplinePoint> m_Points;
    SplineType m_Type = SplineType::CatmullRom;
    bool m_Closed = false;
    f32 m_Tension = 0.5f;  // For Catmull-Rom (0 = tight, 1 = loose)

    // Cached data for arc-length parameterization
    mutable bool m_Dirty = true;
    mutable std::vector<f32> m_SegmentLengths;
    mutable f32 m_TotalLength = 0.0f;
    mutable std::vector<f32> m_LengthTable;  // Cumulative lengths
};

// 2D Spline variant (for top-down/side-scroller paths)
class ENJIN_API Spline2D {
public:
    Spline2D(SplineType type = SplineType::CatmullRom);

    void AddPoint(const Vector2& point);
    void SetPoint(usize index, const Vector2& point);
    void Clear();

    usize GetPointCount() const { return m_Points.size(); }
    Vector2 GetPoint(usize index) const { return m_Points[index]; }

    void SetClosed(bool closed) { m_Closed = closed; }
    bool IsClosed() const { return m_Closed; }

    Vector2 Evaluate(f32 t) const;
    Vector2 EvaluateTangent(f32 t) const;
    Vector2 EvaluateNormal(f32 t) const;

    f32 GetTotalLength() const;
    f32 FindClosestT(const Vector2& point, usize samples = 100) const;

    std::vector<Vector2> GeneratePoints(usize pointsPerSegment = 10) const;

private:
    std::vector<Vector2> m_Points;
    SplineType m_Type;
    bool m_Closed = false;
    f32 m_Tension = 0.5f;
};

// Utility functions
namespace SplineUtils {
    // Bezier helper functions
    ENJIN_API Vector3 CubicBezier(const Vector3& p0, const Vector3& p1,
                                   const Vector3& p2, const Vector3& p3, f32 t);
    ENJIN_API Vector3 CubicBezierDerivative(const Vector3& p0, const Vector3& p1,
                                             const Vector3& p2, const Vector3& p3, f32 t);

    // Catmull-Rom helper
    ENJIN_API Vector3 CatmullRom(const Vector3& p0, const Vector3& p1,
                                  const Vector3& p2, const Vector3& p3, f32 t, f32 tension = 0.5f);
    ENJIN_API Vector3 CatmullRomDerivative(const Vector3& p0, const Vector3& p1,
                                            const Vector3& p2, const Vector3& p3, f32 t, f32 tension = 0.5f);
}

} // namespace Math
} // namespace Enjin
