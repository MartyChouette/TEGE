#include "Enjin/Math/Spline.h"
#include "Enjin/Math/Math.h"
#include <algorithm>

namespace Enjin {
namespace Math {

// ============================================================================
// SplineUtils
// ============================================================================

Vector3 SplineUtils::CubicBezier(const Vector3& p0, const Vector3& p1,
                                  const Vector3& p2, const Vector3& p3, f32 t) {
    f32 t2 = t * t;
    f32 t3 = t2 * t;
    f32 mt = 1.0f - t;
    f32 mt2 = mt * mt;
    f32 mt3 = mt2 * mt;

    return p0 * mt3 + p1 * (3.0f * mt2 * t) + p2 * (3.0f * mt * t2) + p3 * t3;
}

Vector3 SplineUtils::CubicBezierDerivative(const Vector3& p0, const Vector3& p1,
                                            const Vector3& p2, const Vector3& p3, f32 t) {
    f32 t2 = t * t;
    f32 mt = 1.0f - t;
    f32 mt2 = mt * mt;

    return (p1 - p0) * (3.0f * mt2) +
           (p2 - p1) * (6.0f * mt * t) +
           (p3 - p2) * (3.0f * t2);
}

Vector3 SplineUtils::CatmullRom(const Vector3& p0, const Vector3& p1,
                                 const Vector3& p2, const Vector3& p3, f32 t, f32 tension) {
    f32 t2 = t * t;
    f32 t3 = t2 * t;

    // Catmull-Rom basis with tension parameter
    f32 s = (1.0f - tension) * 0.5f;

    f32 b0 = -s * t3 + 2.0f * s * t2 - s * t;
    f32 b1 = (2.0f - s) * t3 + (s - 3.0f) * t2 + 1.0f;
    f32 b2 = (s - 2.0f) * t3 + (3.0f - 2.0f * s) * t2 + s * t;
    f32 b3 = s * t3 - s * t2;

    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

Vector3 SplineUtils::CatmullRomDerivative(const Vector3& p0, const Vector3& p1,
                                           const Vector3& p2, const Vector3& p3, f32 t, f32 tension) {
    f32 t2 = t * t;
    f32 s = (1.0f - tension) * 0.5f;

    f32 b0 = -3.0f * s * t2 + 4.0f * s * t - s;
    f32 b1 = 3.0f * (2.0f - s) * t2 + 2.0f * (s - 3.0f) * t;
    f32 b2 = 3.0f * (s - 2.0f) * t2 + 2.0f * (3.0f - 2.0f * s) * t + s;
    f32 b3 = 3.0f * s * t2 - 2.0f * s * t;

    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

// ============================================================================
// Spline (3D)
// ============================================================================

Spline::Spline(SplineType type) : m_Type(type) {
}

void Spline::AddPoint(const Vector3& point) {
    m_Points.push_back(SplinePoint(point));
    m_Dirty = true;
}

void Spline::AddPoint(const SplinePoint& point) {
    m_Points.push_back(point);
    m_Dirty = true;
}

void Spline::InsertPoint(usize index, const Vector3& point) {
    if (index <= m_Points.size()) {
        m_Points.insert(m_Points.begin() + static_cast<std::ptrdiff_t>(index), SplinePoint(point));
        m_Dirty = true;
    }
}

void Spline::RemovePoint(usize index) {
    if (index < m_Points.size()) {
        m_Points.erase(m_Points.begin() + static_cast<std::ptrdiff_t>(index));
        m_Dirty = true;
    }
}

void Spline::SetPoint(usize index, const Vector3& point) {
    if (index < m_Points.size()) {
        m_Points[index].position = point;
        m_Dirty = true;
    }
}

void Spline::Clear() {
    m_Points.clear();
    m_Dirty = true;
}

Vector3 Spline::Evaluate(f32 t) const {
    if (m_Points.size() < 2) {
        return m_Points.empty() ? Vector3(0, 0, 0) : m_Points[0].position;
    }

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    f32 scaledT = t * static_cast<f32>(numSegments);
    usize segment = static_cast<usize>(scaledT);
    f32 localT = scaledT - static_cast<f32>(segment);

    // Clamp to valid range
    if (segment >= numSegments) {
        segment = numSegments - 1;
        localT = 1.0f;
    }

    return EvaluateSegment(segment, localT);
}

Vector3 Spline::EvaluateTangent(f32 t) const {
    if (m_Points.size() < 2) {
        return Vector3(0, 0, 1);
    }

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    f32 scaledT = t * static_cast<f32>(numSegments);
    usize segment = static_cast<usize>(scaledT);
    f32 localT = scaledT - static_cast<f32>(segment);

    if (segment >= numSegments) {
        segment = numSegments - 1;
        localT = 1.0f;
    }

    return EvaluateSegmentTangent(segment, localT).Normalized();
}

Vector3 Spline::EvaluateNormal(f32 t) const {
    Vector3 tangent = EvaluateTangent(t);

    // Create a normal perpendicular to the tangent
    Vector3 up(0, 1, 0);
    if (Abs(tangent.Dot(up)) > 0.99f) {
        up = Vector3(1, 0, 0);
    }

    Vector3 right = tangent.Cross(up).Normalized();
    return up.Cross(tangent).Normalized();
}

f32 Spline::EvaluateRoll(f32 t) const {
    if (m_Points.size() < 2) {
        return 0.0f;
    }

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    f32 scaledT = t * static_cast<f32>(numSegments);
    usize segment = static_cast<usize>(scaledT);
    f32 localT = scaledT - static_cast<f32>(segment);

    if (segment >= numSegments) {
        segment = numSegments - 1;
        localT = 1.0f;
    }

    // Interpolate roll between segment endpoints
    usize i0 = segment;
    usize i1 = (segment + 1) % m_Points.size();

    return Lerp(m_Points[i0].roll, m_Points[i1].roll, localT);
}

void Spline::EvaluateFrame(f32 t, Vector3& position, Vector3& forward, Vector3& up, Vector3& right) const {
    position = Evaluate(t);
    forward = EvaluateTangent(t);
    f32 roll = EvaluateRoll(t);

    // Create coordinate frame with roll
    Vector3 worldUp(0, 1, 0);
    if (Abs(forward.Dot(worldUp)) > 0.99f) {
        worldUp = Vector3(0, 0, 1);
    }

    right = forward.Cross(worldUp).Normalized();
    up = right.Cross(forward).Normalized();

    // Apply roll rotation around forward axis
    if (Abs(roll) > 0.001f) {
        f32 cosRoll = Cos(roll);
        f32 sinRoll = Sin(roll);
        Vector3 newRight = right * cosRoll + up * sinRoll;
        Vector3 newUp = up * cosRoll - right * sinRoll;
        right = newRight;
        up = newUp;
    }
}

Vector3 Spline::EvaluateSegment(usize segment, f32 t) const {
    switch (m_Type) {
        case SplineType::Linear:
            return EvaluateLinear(segment, t);
        case SplineType::Bezier:
            return EvaluateBezier(segment, t);
        case SplineType::CatmullRom:
            return EvaluateCatmullRom(segment, t);
        case SplineType::BSpline:
            return EvaluateBSpline(segment, t);
        default:
            return EvaluateLinear(segment, t);
    }
}

Vector3 Spline::EvaluateSegmentTangent(usize segment, f32 t) const {
    switch (m_Type) {
        case SplineType::Linear:
            return EvaluateLinearTangent(segment, t);
        case SplineType::Bezier:
            return EvaluateBezierTangent(segment, t);
        case SplineType::CatmullRom:
            return EvaluateCatmullRomTangent(segment, t);
        case SplineType::BSpline:
            return EvaluateBSplineTangent(segment, t);
        default:
            return EvaluateLinearTangent(segment, t);
    }
}

Vector3 Spline::EvaluateLinear(usize segment, f32 t) const {
    usize i0 = segment;
    usize i1 = (segment + 1) % m_Points.size();
    return Lerp(m_Points[i0].position, m_Points[i1].position, t);
}

Vector3 Spline::EvaluateLinearTangent(usize segment, f32 t) const {
    (void)t;
    usize i0 = segment;
    usize i1 = (segment + 1) % m_Points.size();
    return (m_Points[i1].position - m_Points[i0].position).Normalized();
}

Vector3 Spline::EvaluateBezier(usize segment, f32 t) const {
    usize i0 = segment;
    usize i1 = (segment + 1) % m_Points.size();

    const Vector3& p0 = m_Points[i0].position;
    const Vector3& p3 = m_Points[i1].position;
    Vector3 p1 = p0 + m_Points[i0].tangentOut;
    Vector3 p2 = p3 + m_Points[i1].tangentIn;

    return SplineUtils::CubicBezier(p0, p1, p2, p3, t);
}

Vector3 Spline::EvaluateBezierTangent(usize segment, f32 t) const {
    usize i0 = segment;
    usize i1 = (segment + 1) % m_Points.size();

    const Vector3& p0 = m_Points[i0].position;
    const Vector3& p3 = m_Points[i1].position;
    Vector3 p1 = p0 + m_Points[i0].tangentOut;
    Vector3 p2 = p3 + m_Points[i1].tangentIn;

    return SplineUtils::CubicBezierDerivative(p0, p1, p2, p3, t);
}

Vector3 Spline::EvaluateCatmullRom(usize segment, f32 t) const {
    usize n = m_Points.size();

    // Get 4 control points for Catmull-Rom
    usize i1 = segment;
    usize i2 = (segment + 1) % n;
    usize i0, i3;

    if (m_Closed) {
        i0 = (segment + n - 1) % n;
        i3 = (segment + 2) % n;
    } else {
        i0 = segment > 0 ? segment - 1 : 0;
        i3 = segment + 2 < n ? segment + 2 : n - 1;
    }

    return SplineUtils::CatmullRom(
        m_Points[i0].position, m_Points[i1].position,
        m_Points[i2].position, m_Points[i3].position,
        t, m_Tension
    );
}

Vector3 Spline::EvaluateCatmullRomTangent(usize segment, f32 t) const {
    usize n = m_Points.size();

    usize i1 = segment;
    usize i2 = (segment + 1) % n;
    usize i0, i3;

    if (m_Closed) {
        i0 = (segment + n - 1) % n;
        i3 = (segment + 2) % n;
    } else {
        i0 = segment > 0 ? segment - 1 : 0;
        i3 = segment + 2 < n ? segment + 2 : n - 1;
    }

    return SplineUtils::CatmullRomDerivative(
        m_Points[i0].position, m_Points[i1].position,
        m_Points[i2].position, m_Points[i3].position,
        t, m_Tension
    );
}

Vector3 Spline::EvaluateBSpline(usize segment, f32 t) const {
    // B-Spline uses same control point selection as Catmull-Rom
    // but with different basis functions
    usize n = m_Points.size();

    usize i1 = segment;
    usize i2 = (segment + 1) % n;
    usize i0, i3;

    if (m_Closed) {
        i0 = (segment + n - 1) % n;
        i3 = (segment + 2) % n;
    } else {
        i0 = segment > 0 ? segment - 1 : 0;
        i3 = segment + 2 < n ? segment + 2 : n - 1;
    }

    const Vector3& p0 = m_Points[i0].position;
    const Vector3& p1 = m_Points[i1].position;
    const Vector3& p2 = m_Points[i2].position;
    const Vector3& p3 = m_Points[i3].position;

    // Uniform cubic B-Spline basis
    f32 t2 = t * t;
    f32 t3 = t2 * t;

    f32 b0 = (-t3 + 3.0f * t2 - 3.0f * t + 1.0f) / 6.0f;
    f32 b1 = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
    f32 b2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f;
    f32 b3 = t3 / 6.0f;

    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

Vector3 Spline::EvaluateBSplineTangent(usize segment, f32 t) const {
    usize n = m_Points.size();

    usize i1 = segment;
    usize i2 = (segment + 1) % n;
    usize i0, i3;

    if (m_Closed) {
        i0 = (segment + n - 1) % n;
        i3 = (segment + 2) % n;
    } else {
        i0 = segment > 0 ? segment - 1 : 0;
        i3 = segment + 2 < n ? segment + 2 : n - 1;
    }

    const Vector3& p0 = m_Points[i0].position;
    const Vector3& p1 = m_Points[i1].position;
    const Vector3& p2 = m_Points[i2].position;
    const Vector3& p3 = m_Points[i3].position;

    f32 t2 = t * t;

    f32 b0 = (-3.0f * t2 + 6.0f * t - 3.0f) / 6.0f;
    f32 b1 = (9.0f * t2 - 12.0f * t) / 6.0f;
    f32 b2 = (-9.0f * t2 + 6.0f * t + 3.0f) / 6.0f;
    f32 b3 = 3.0f * t2 / 6.0f;

    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

void Spline::UpdateCache() const {
    if (!m_Dirty || m_Points.size() < 2) {
        return;
    }

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    m_SegmentLengths.resize(numSegments);
    m_LengthTable.resize(numSegments + 1);

    m_TotalLength = 0.0f;
    m_LengthTable[0] = 0.0f;

    // Calculate length of each segment using numerical integration
    constexpr usize samplesPerSegment = 20;
    for (usize seg = 0; seg < numSegments; ++seg) {
        f32 segmentLength = 0.0f;
        Vector3 prevPoint = EvaluateSegment(seg, 0.0f);

        for (usize i = 1; i <= samplesPerSegment; ++i) {
            f32 t = static_cast<f32>(i) / static_cast<f32>(samplesPerSegment);
            Vector3 point = EvaluateSegment(seg, t);
            segmentLength += (point - prevPoint).Length();
            prevPoint = point;
        }

        m_SegmentLengths[seg] = segmentLength;
        m_TotalLength += segmentLength;
        m_LengthTable[seg + 1] = m_TotalLength;
    }

    m_Dirty = false;
}

f32 Spline::GetTotalLength() const {
    UpdateCache();
    return m_TotalLength;
}

f32 Spline::GetSegmentLength(usize segment) const {
    UpdateCache();
    if (segment < m_SegmentLengths.size()) {
        return m_SegmentLengths[segment];
    }
    return 0.0f;
}

f32 Spline::TToDistance(f32 t) const {
    return t * GetTotalLength();
}

f32 Spline::DistanceToT(f32 distance) const {
    UpdateCache();
    if (m_LengthTable.size() < 2 || m_TotalLength < 0.0001f) {
        return 0.0f;
    }

    distance = Clamp(distance, 0.0f, m_TotalLength);

    // Binary search for segment
    usize lo = 0;
    usize hi = m_LengthTable.size() - 1;
    while (lo < hi - 1) {
        usize mid = (lo + hi) / 2;
        if (m_LengthTable[mid] <= distance) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    // Interpolate within segment
    f32 segStart = m_LengthTable[lo];
    f32 segEnd = m_LengthTable[lo + 1];
    f32 segLen = segEnd - segStart;

    f32 localT = (segLen > 0.0001f) ? (distance - segStart) / segLen : 0.0f;

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    return (static_cast<f32>(lo) + localT) / static_cast<f32>(numSegments);
}

f32 Spline::FindClosestT(const Vector3& point, usize samples) const {
    if (m_Points.empty()) {
        return 0.0f;
    }

    f32 bestT = 0.0f;
    f32 bestDistSq = (Evaluate(0.0f) - point).LengthSquared();

    for (usize i = 1; i <= samples; ++i) {
        f32 t = static_cast<f32>(i) / static_cast<f32>(samples);
        f32 distSq = (Evaluate(t) - point).LengthSquared();
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestT = t;
        }
    }

    return bestT;
}

Vector3 Spline::FindClosestPoint(const Vector3& point, usize samples) const {
    return Evaluate(FindClosestT(point, samples));
}

std::vector<Vector3> Spline::GeneratePoints(usize pointsPerSegment) const {
    std::vector<Vector3> result;

    if (m_Points.size() < 2) {
        if (!m_Points.empty()) {
            result.push_back(m_Points[0].position);
        }
        return result;
    }

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    usize totalPoints = numSegments * pointsPerSegment + (m_Closed ? 0 : 1);
    result.reserve(totalPoints);

    for (usize seg = 0; seg < numSegments; ++seg) {
        for (usize i = 0; i < pointsPerSegment; ++i) {
            f32 t = static_cast<f32>(i) / static_cast<f32>(pointsPerSegment);
            result.push_back(EvaluateSegment(seg, t));
        }
    }

    if (!m_Closed) {
        result.push_back(m_Points.back().position);
    }

    return result;
}

std::vector<Vector3> Spline::GeneratePointsByDistance(f32 spacing) const {
    std::vector<Vector3> result;
    f32 totalLen = GetTotalLength();

    if (totalLen < 0.0001f || spacing < 0.0001f) {
        if (!m_Points.empty()) {
            result.push_back(m_Points[0].position);
        }
        return result;
    }

    f32 distance = 0.0f;
    while (distance <= totalLen) {
        f32 t = DistanceToT(distance);
        result.push_back(Evaluate(t));
        distance += spacing;
    }

    // Always include end point
    if (!result.empty()) {
        Vector3 endPoint = Evaluate(1.0f);
        if ((result.back() - endPoint).LengthSquared() > 0.0001f) {
            result.push_back(endPoint);
        }
    }

    return result;
}

void Spline::AutoCalculateTangents(f32 smoothness) {
    if (m_Points.size() < 2) {
        return;
    }

    for (usize i = 0; i < m_Points.size(); ++i) {
        Vector3 tangent;

        if (m_Closed) {
            usize prev = (i + m_Points.size() - 1) % m_Points.size();
            usize next = (i + 1) % m_Points.size();
            tangent = (m_Points[next].position - m_Points[prev].position) * smoothness;
        } else {
            if (i == 0) {
                tangent = (m_Points[1].position - m_Points[0].position) * smoothness;
            } else if (i == m_Points.size() - 1) {
                tangent = (m_Points[i].position - m_Points[i - 1].position) * smoothness;
            } else {
                tangent = (m_Points[i + 1].position - m_Points[i - 1].position) * smoothness * 0.5f;
            }
        }

        m_Points[i].tangentIn = tangent * -1.0f;
        m_Points[i].tangentOut = tangent;
    }

    m_Dirty = true;
}

// ============================================================================
// Spline2D
// ============================================================================

Spline2D::Spline2D(SplineType type) : m_Type(type) {
}

void Spline2D::AddPoint(const Vector2& point) {
    m_Points.push_back(point);
}

void Spline2D::SetPoint(usize index, const Vector2& point) {
    if (index < m_Points.size()) {
        m_Points[index] = point;
    }
}

void Spline2D::Clear() {
    m_Points.clear();
}

Vector2 Spline2D::Evaluate(f32 t) const {
    if (m_Points.size() < 2) {
        return m_Points.empty() ? Vector2(0, 0) : m_Points[0];
    }

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    f32 scaledT = t * static_cast<f32>(numSegments);
    usize segment = static_cast<usize>(scaledT);
    f32 localT = scaledT - static_cast<f32>(segment);

    if (segment >= numSegments) {
        segment = numSegments - 1;
        localT = 1.0f;
    }

    // Get control points
    usize n = m_Points.size();
    usize i1 = segment;
    usize i2 = (segment + 1) % n;

    if (m_Type == SplineType::Linear) {
        return Lerp(m_Points[i1], m_Points[i2], localT);
    }

    // Catmull-Rom
    usize i0, i3;
    if (m_Closed) {
        i0 = (segment + n - 1) % n;
        i3 = (segment + 2) % n;
    } else {
        i0 = segment > 0 ? segment - 1 : 0;
        i3 = segment + 2 < n ? segment + 2 : n - 1;
    }

    // Convert to 3D, evaluate, convert back
    Vector3 result = SplineUtils::CatmullRom(
        Vector3(m_Points[i0].x, m_Points[i0].y, 0),
        Vector3(m_Points[i1].x, m_Points[i1].y, 0),
        Vector3(m_Points[i2].x, m_Points[i2].y, 0),
        Vector3(m_Points[i3].x, m_Points[i3].y, 0),
        localT, m_Tension
    );

    return Vector2(result.x, result.y);
}

Vector2 Spline2D::EvaluateTangent(f32 t) const {
    // Numerical derivative
    constexpr f32 h = 0.001f;
    f32 t0 = Max(t - h, 0.0f);
    f32 t1 = Min(t + h, 1.0f);
    return (Evaluate(t1) - Evaluate(t0)).Normalized();
}

Vector2 Spline2D::EvaluateNormal(f32 t) const {
    Vector2 tangent = EvaluateTangent(t);
    return Vector2(-tangent.y, tangent.x);  // Perpendicular in 2D
}

f32 Spline2D::GetTotalLength() const {
    if (m_Points.size() < 2) {
        return 0.0f;
    }

    f32 length = 0.0f;
    constexpr usize samples = 100;
    Vector2 prevPoint = Evaluate(0.0f);

    for (usize i = 1; i <= samples; ++i) {
        f32 t = static_cast<f32>(i) / static_cast<f32>(samples);
        Vector2 point = Evaluate(t);
        length += (point - prevPoint).Length();
        prevPoint = point;
    }

    return length;
}

f32 Spline2D::FindClosestT(const Vector2& point, usize samples) const {
    if (m_Points.empty()) {
        return 0.0f;
    }

    f32 bestT = 0.0f;
    f32 bestDistSq = (Evaluate(0.0f) - point).LengthSquared();

    for (usize i = 1; i <= samples; ++i) {
        f32 t = static_cast<f32>(i) / static_cast<f32>(samples);
        f32 distSq = (Evaluate(t) - point).LengthSquared();
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestT = t;
        }
    }

    return bestT;
}

std::vector<Vector2> Spline2D::GeneratePoints(usize pointsPerSegment) const {
    std::vector<Vector2> result;

    if (m_Points.size() < 2) {
        if (!m_Points.empty()) {
            result.push_back(m_Points[0]);
        }
        return result;
    }

    usize numSegments = m_Closed ? m_Points.size() : m_Points.size() - 1;
    usize totalSamples = numSegments * pointsPerSegment;

    result.reserve(totalSamples + 1);

    for (usize i = 0; i <= totalSamples; ++i) {
        f32 t = static_cast<f32>(i) / static_cast<f32>(totalSamples);
        result.push_back(Evaluate(t));
    }

    return result;
}

} // namespace Math
} // namespace Enjin
