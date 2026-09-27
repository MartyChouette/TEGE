#include "Enjin/Physics/Polygon2D.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Physics {

namespace {

f32 Cross(const Math::Vector2& o, const Math::Vector2& a, const Math::Vector2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Andrew's monotone chain; counter-clockwise, no collinear points.
std::vector<Math::Vector2> ConvexHull(std::vector<Math::Vector2> pts) {
    std::sort(pts.begin(), pts.end(), [](const Math::Vector2& a, const Math::Vector2& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    pts.erase(std::unique(pts.begin(), pts.end(), [](const Math::Vector2& a, const Math::Vector2& b) {
        return a.x == b.x && a.y == b.y;
    }), pts.end());
    if (pts.size() < 3) return pts;

    std::vector<Math::Vector2> hull(2 * pts.size());
    usize k = 0;
    for (usize i = 0; i < pts.size(); ++i) {
        while (k >= 2 && Cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) --k;
        hull[k++] = pts[i];
    }
    for (usize i = pts.size() - 1, t = k + 1; i > 0; --i) {
        while (k >= t && Cross(hull[k - 2], hull[k - 1], pts[i - 1]) <= 0.0f) --k;
        hull[k++] = pts[i - 1];
    }
    hull.resize(k - 1);
    return hull;
}

} // namespace

std::vector<Math::Vector2> FitBox2DPolygon(const std::vector<Math::Vector2>& points, usize maxVertices) {
    std::vector<Math::Vector2> hull = ConvexHull(points);
    maxVertices = std::max<usize>(maxVertices, 3);
    // Each pass drops the corner that takes the least area with it. The hull
    // stays convex: removing a vertex of a convex polygon cannot make it
    // concave.
    while (hull.size() > maxVertices) {
        usize best = 0;
        f32 bestArea = INFINITY;
        const usize n = hull.size();
        for (usize i = 0; i < n; ++i) {
            const f32 area = std::abs(Cross(hull[(i + n - 1) % n], hull[i], hull[(i + 1) % n]));
            if (area < bestArea) { bestArea = area; best = i; }
        }
        hull.erase(hull.begin() + static_cast<std::ptrdiff_t>(best));
    }
    return hull;
}

} // namespace Physics
} // namespace Enjin
