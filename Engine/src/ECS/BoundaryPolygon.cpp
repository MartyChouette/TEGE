#include "Enjin/ECS/Components/BoundaryPolygon.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace Enjin {
namespace ECS {

namespace {

f32 Cross(const Math::Vector2& o, const Math::Vector2& a, const Math::Vector2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool InTriangle(const Math::Vector2& p, const Math::Vector2& a,
                const Math::Vector2& b, const Math::Vector2& c) {
    const f32 c1 = Cross(a, b, p), c2 = Cross(b, c, p), c3 = Cross(c, a, p);
    const bool neg = (c1 < 0) || (c2 < 0) || (c3 < 0);
    const bool pos = (c1 > 0) || (c2 > 0) || (c3 > 0);
    return !(neg && pos);
}

f32 DistanceToSegment(const Math::Vector2& p, const Math::Vector2& a, const Math::Vector2& b) {
    const f32 dx = b.x - a.x, dy = b.y - a.y;
    const f32 len2 = dx * dx + dy * dy;
    f32 t = (len2 > 1e-12f) ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    const f32 ex = a.x + dx * t - p.x, ey = a.y + dy * t - p.y;
    return std::sqrt(ex * ex + ey * ey);
}

} // namespace

bool BoundaryPolygonComponent::BuildSurface(const std::vector<Math::Vector2>& ring, f32 maxEdge,
                                            std::vector<Math::Vector2>& positions,
                                            std::vector<f32>& shore,
                                            std::vector<u32>& indices) {
    positions.clear(); shore.clear(); indices.clear();
    const usize n = ring.size();
    if (n < 3) return false;

    // Ear clipping wants one winding; flip the index order rather than the data.
    f32 area = 0.0f;
    for (usize i = 0; i < n; ++i) {
        const auto& p = ring[i]; const auto& q = ring[(i + 1) % n];
        area += p.x * q.y - q.x * p.y;
    }
    if (std::fabs(area) < 1e-6f) return false;

    std::vector<u32> idx(n);
    for (usize i = 0; i < n; ++i) idx[i] = static_cast<u32>(i);
    if (area < 0.0f) std::reverse(idx.begin(), idx.end());

    positions = ring;
    std::vector<u32> tris;
    usize remaining = n;
    while (remaining > 3) {
        bool clipped = false;
        for (usize i = 0; i < remaining; ++i) {
            const u32 i0 = idx[(i + remaining - 1) % remaining], i1 = idx[i], i2 = idx[(i + 1) % remaining];
            const auto& a = ring[i0]; const auto& b = ring[i1]; const auto& c = ring[i2];
            if (Cross(a, b, c) <= 1e-9f) continue;   // reflex or degenerate
            bool contains = false;
            for (usize j = 0; j < remaining && !contains; ++j) {
                const u32 pj = idx[j];
                if (pj != i0 && pj != i1 && pj != i2 && InTriangle(ring[pj], a, b, c)) contains = true;
            }
            if (contains) continue;
            tris.insert(tris.end(), {i0, i1, i2});
            idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
            --remaining;
            clipped = true;
            break;
        }
        // A ring dragged across itself has no ear left. What was clipped so far
        // is still water; the knot is not.
        if (!clipped) break;
    }
    if (remaining == 3) tris.insert(tris.end(), {idx[0], idx[1], idx[2]});
    if (tris.empty()) return false;

    // Split long edges at their midpoints.
    //
    // The decision is made per EDGE, never per triangle. Splitting a triangle
    // because its longest edge was too long also cut its short edges, and the
    // neighbour across a short edge had no reason to split: its triangle kept
    // the whole edge while this side had a vertex in the middle of it. That is
    // a T-junction, and the foam read a different shore distance on each side
    // of the seam. An edge's length is the same from both triangles, so marking
    // edges and then splitting each triangle by how many of its edges are
    // marked keeps every shared edge shared.
    const f32 limit = std::max(0.25f, maxEdge);
    auto len = [&](u32 a, u32 b) {
        const Math::Vector2 d = positions[a] - positions[b];
        return std::sqrt(d.x * d.x + d.y * d.y);
    };
    auto key = [](u32 a, u32 b) { return std::make_pair(std::min(a, b), std::max(a, b)); };
    // A lake the size of a county stops here. Counted as the splits are made,
    // so one pass cannot run past it the way a per-pass check would.
    constexpr usize kMaxTriangles = 20000;
    usize triCount = tris.size() / 3;
    for (int pass = 0; pass < 12; ++pass) {
        std::map<std::pair<u32, u32>, u32> mids;
        for (usize t = 0; t + 2 < tris.size(); t += 3) {
            for (int e = 0; e < 3; ++e) {
                const u32 a = tris[t + e], b = tris[t + (e + 1) % 3];
                const auto k = key(a, b);
                if (mids.count(k) || len(a, b) <= limit) continue;
                // A split edge adds a triangle on each side of it.
                if (triCount + 2 > kMaxTriangles) break;
                mids.emplace(k, static_cast<u32>(positions.size()));
                positions.push_back((positions[a] + positions[b]) * 0.5f);
                triCount += 2;
            }
        }
        if (mids.empty()) break;

        std::vector<u32> next;
        next.reserve(tris.size() * 2);
        for (usize t = 0; t + 2 < tris.size(); t += 3) {
            u32 v[3] = {tris[t], tris[t + 1], tris[t + 2]};
            u32 m[3];
            bool on[3];
            int count = 0;
            for (int e = 0; e < 3; ++e) {
                auto it = mids.find(key(v[e], v[(e + 1) % 3]));
                on[e] = it != mids.end();
                m[e] = on[e] ? it->second : 0;
                count += on[e] ? 1 : 0;
            }
            // Rotate so the marked edges come first (one: edge 0; two: edges 0
            // and 1). Rotation keeps the winding.
            auto rotate = [&]() {
                std::rotate(v, v + 1, v + 3);
                std::rotate(m, m + 1, m + 3);
                std::rotate(on, on + 1, on + 3);
            };
            if (count == 1) { while (!on[0]) rotate(); }
            if (count == 2) { while (on[2]) rotate(); }

            if (count == 0) {
                next.insert(next.end(), {v[0], v[1], v[2]});
            } else if (count == 1) {
                next.insert(next.end(), {v[0], m[0], v[2], m[0], v[1], v[2]});
            } else if (count == 2) {
                // The corner between the two split edges, then the quad left over
                // cut along its shorter diagonal.
                next.insert(next.end(), {m[0], v[1], m[1]});
                if (len(v[0], m[1]) <= len(m[0], v[2])) {
                    next.insert(next.end(), {v[0], m[0], m[1], v[0], m[1], v[2]});
                } else {
                    next.insert(next.end(), {v[0], m[0], v[2], m[0], m[1], v[2]});
                }
            } else {
                next.insert(next.end(), {v[0], m[0], m[2], m[0], v[1], m[1],
                                         m[2], m[1], v[2], m[0], m[1], m[2]});
            }
        }
        tris.swap(next);
    }

    shore.resize(positions.size());
    f32 furthest = 0.0f;
    for (usize v = 0; v < positions.size(); ++v) {
        f32 d = 1e30f;
        for (usize i = 0; i < n; ++i) d = std::min(d, DistanceToSegment(positions[v], ring[i], ring[(i + 1) % n]));
        shore[v] = d;
        furthest = std::max(furthest, d);
    }
    for (f32& s : shore) s = (furthest > 1e-6f) ? s / furthest : 0.0f;

    indices = std::move(tris);
    return true;
}

} // namespace ECS
} // namespace Enjin
