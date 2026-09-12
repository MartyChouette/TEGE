#include "Enjin/Acoustics/AcousticBVH.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Enjin {
namespace Acoustics {

namespace {

constexpr u32 kLeafSize = 4;
constexpr u32 kMaxDepth = 40;

// Moller-Trumbore. Returns the distance along the ray, or a negative number.
//
// Double-sided on purpose. A room's walls are often single quads with no
// thickness and no consistent winding -- a floor built by one tool and a wall
// built by another rarely agree on which way is out -- and a backface cull here
// would make half the surfaces in a hand-built room acoustically invisible.
// Which way the normal faces is decided later, against the ray.
f32 RayTriangle(const Math::Vector3& o, const Math::Vector3& d,
                const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c) {
    const Math::Vector3 e1(b.x - a.x, b.y - a.y, b.z - a.z);
    const Math::Vector3 e2(c.x - a.x, c.y - a.y, c.z - a.z);
    const Math::Vector3 p(d.y * e2.z - d.z * e2.y,
                          d.z * e2.x - d.x * e2.z,
                          d.x * e2.y - d.y * e2.x);
    const f32 det = e1.x * p.x + e1.y * p.y + e1.z * p.z;
    if (std::fabs(det) < 1.0e-12f) return -1.0f;   // parallel

    const f32 inv = 1.0f / det;
    const Math::Vector3 t(o.x - a.x, o.y - a.y, o.z - a.z);
    const f32 u = (t.x * p.x + t.y * p.y + t.z * p.z) * inv;
    if (u < 0.0f || u > 1.0f) return -1.0f;

    const Math::Vector3 q(t.y * e1.z - t.z * e1.y,
                          t.z * e1.x - t.x * e1.z,
                          t.x * e1.y - t.y * e1.x);
    const f32 v = (d.x * q.x + d.y * q.y + d.z * q.z) * inv;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;

    return (e2.x * q.x + e2.y * q.y + e2.z * q.z) * inv;
}

// Slab test. `invDir` is precomputed once per ray rather than per node, which
// is most of what makes traversal cheap.
bool RayAABB(const Math::Vector3& o, const Math::Vector3& invDir,
             const Math::Vector3& lo, const Math::Vector3& hi, f32 maxT) {
    f32 t0 = 0.0f, t1 = maxT;
    // A zero component gives an infinite invDir, and inf * 0 is NaN. The
    // comparisons below reject NaN either way, which is why this is written as
    // min/max of both slab ends rather than as a branch on the sign.
    const f32 x0 = (lo.x - o.x) * invDir.x, x1 = (hi.x - o.x) * invDir.x;
    t0 = std::max(t0, std::min(x0, x1));
    t1 = std::min(t1, std::max(x0, x1));
    const f32 y0 = (lo.y - o.y) * invDir.y, y1 = (hi.y - o.y) * invDir.y;
    t0 = std::max(t0, std::min(y0, y1));
    t1 = std::min(t1, std::max(y0, y1));
    const f32 z0 = (lo.z - o.z) * invDir.z, z1 = (hi.z - o.z) * invDir.z;
    t0 = std::max(t0, std::min(z0, z1));
    t1 = std::min(t1, std::max(z0, z1));
    return t0 <= t1;
}

Math::Vector3 Normalize(const Math::Vector3& v) {
    const f32 len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1.0e-12f) return Math::Vector3(0.0f, 1.0f, 0.0f);
    return Math::Vector3(v.x / len, v.y / len, v.z / len);
}

} // namespace

void AcousticBVH::Clear() {
    m_Scene = nullptr;
    m_Triangles.clear();
    m_Nodes.clear();
    m_Centroids.clear();
}

void AcousticBVH::Build(const Audio::AcousticScene& scene) {
    Clear();
    const usize triCount = scene.TriangleCount();
    if (triCount == 0) return;

    m_Scene = &scene;
    m_Triangles.resize(triCount);
    m_Centroids.resize(triCount);
    for (usize i = 0; i < triCount; ++i) {
        m_Triangles[i] = static_cast<u32>(i);
        const Math::Vector3& a = scene.vertices[static_cast<usize>(scene.indices[i * 3 + 0])];
        const Math::Vector3& b = scene.vertices[static_cast<usize>(scene.indices[i * 3 + 1])];
        const Math::Vector3& c = scene.vertices[static_cast<usize>(scene.indices[i * 3 + 2])];
        m_Centroids[i] = Math::Vector3((a.x + b.x + c.x) / 3.0f,
                                       (a.y + b.y + c.y) / 3.0f,
                                       (a.z + b.z + c.z) / 3.0f);
    }

    m_Nodes.reserve(triCount * 2);
    BuildRange(0, static_cast<u32>(triCount), 0);
    if (!m_Nodes.empty()) {
        m_BoundsMin = m_Nodes[0].lo;
        m_BoundsMax = m_Nodes[0].hi;
    }
}

u32 AcousticBVH::BuildRange(u32 start, u32 count, u32 depth) {
    const u32 self = static_cast<u32>(m_Nodes.size());
    m_Nodes.push_back(Node{});

    Math::Vector3 lo(std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(),
                     std::numeric_limits<f32>::max());
    Math::Vector3 hi(-std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max(),
                     -std::numeric_limits<f32>::max());
    Math::Vector3 clo = lo, chi = hi;

    for (u32 i = start; i < start + count; ++i) {
        const u32 t = m_Triangles[i];
        for (u32 k = 0; k < 3; ++k) {
            const Math::Vector3& v =
                m_Scene->vertices[static_cast<usize>(m_Scene->indices[t * 3 + k])];
            lo.x = std::min(lo.x, v.x); lo.y = std::min(lo.y, v.y); lo.z = std::min(lo.z, v.z);
            hi.x = std::max(hi.x, v.x); hi.y = std::max(hi.y, v.y); hi.z = std::max(hi.z, v.z);
        }
        const Math::Vector3& c = m_Centroids[t];
        clo.x = std::min(clo.x, c.x); clo.y = std::min(clo.y, c.y); clo.z = std::min(clo.z, c.z);
        chi.x = std::max(chi.x, c.x); chi.y = std::max(chi.y, c.y); chi.z = std::max(chi.z, c.z);
    }

    m_Nodes[self].lo = lo;
    m_Nodes[self].hi = hi;

    if (count <= kLeafSize || depth >= kMaxDepth) {
        m_Nodes[self].start = start;
        m_Nodes[self].count = count;
        return self;
    }

    // Split the widest axis of the CENTROID bounds at its middle.
    //
    // Centroid bounds rather than triangle bounds: a few long thin triangles
    // (a floor quad split in two, say) blow up the triangle bounds without
    // telling you anything about where the geometry actually sits.
    const f32 ex = chi.x - clo.x, ey = chi.y - clo.y, ez = chi.z - clo.z;
    const u32 axis = (ex > ey && ex > ez) ? 0u : ((ey > ez) ? 1u : 2u);
    const f32 mid = ((axis == 0) ? (clo.x + chi.x) : (axis == 1) ? (clo.y + chi.y)
                                                                 : (clo.z + chi.z)) * 0.5f;

    auto centroidAxis = [&](u32 tri) {
        const Math::Vector3& c = m_Centroids[tri];
        return (axis == 0) ? c.x : (axis == 1) ? c.y : c.z;
    };

    auto middle = std::partition(m_Triangles.begin() + start,
                                 m_Triangles.begin() + start + count,
                                 [&](u32 tri) { return centroidAxis(tri) < mid; });
    u32 leftCount = static_cast<u32>(middle - (m_Triangles.begin() + start));

    // Everything landing on one side means coincident centroids -- a stack of
    // identical quads, which happens in hand-built rooms. Split down the middle
    // rather than recursing forever on the same set.
    if (leftCount == 0 || leftCount == count) leftCount = count / 2;

    const u32 l = BuildRange(start, leftCount, depth + 1);
    const u32 r = BuildRange(start + leftCount, count - leftCount, depth + 1);
    m_Nodes[self].left = l;
    m_Nodes[self].right = r;
    m_Nodes[self].count = 0;
    return self;
}

RayHit AcousticBVH::Raycast(const Math::Vector3& origin, const Math::Vector3& direction,
                            f32 maxDistance) const {
    RayHit best;
    if (!IsBuilt()) return best;

    const Math::Vector3 dir = Normalize(direction);
    const Math::Vector3 invDir(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);

    f32 nearest = maxDistance;
    u32 stack[64];
    u32 depth = 0;
    stack[depth++] = 0;

    while (depth > 0) {
        const Node& node = m_Nodes[stack[--depth]];
        if (!RayAABB(origin, invDir, node.lo, node.hi, nearest)) continue;

        if (node.IsLeaf()) {
            for (u32 i = node.start; i < node.start + node.count; ++i) {
                const u32 t = m_Triangles[i];
                const Math::Vector3& a =
                    m_Scene->vertices[static_cast<usize>(m_Scene->indices[t * 3 + 0])];
                const Math::Vector3& b =
                    m_Scene->vertices[static_cast<usize>(m_Scene->indices[t * 3 + 1])];
                const Math::Vector3& c =
                    m_Scene->vertices[static_cast<usize>(m_Scene->indices[t * 3 + 2])];
                const f32 hitT = RayTriangle(origin, dir, a, b, c);
                if (hitT <= 1.0e-4f || hitT >= nearest) continue;

                nearest = hitT;
                best.hit = true;
                best.distance = hitT;
                best.triangle = t;
                best.material = (t < m_Scene->materialIndices.size())
                                    ? m_Scene->materialIndices[t] : 0;
                best.point = Math::Vector3(origin.x + dir.x * hitT,
                                           origin.y + dir.y * hitT,
                                           origin.z + dir.z * hitT);
                const Math::Vector3 e1(b.x - a.x, b.y - a.y, b.z - a.z);
                const Math::Vector3 e2(c.x - a.x, c.y - a.y, c.z - a.z);
                Math::Vector3 n = Normalize(Math::Vector3(e1.y * e2.z - e1.z * e2.y,
                                                          e1.z * e2.x - e1.x * e2.z,
                                                          e1.x * e2.y - e1.y * e2.x));
                // Faced back along the ray. A room's surfaces have no reliable
                // winding, so "outward" is decided by where the sound came
                // from, not by how the triangle was authored.
                if (n.x * dir.x + n.y * dir.y + n.z * dir.z > 0.0f) {
                    n = Math::Vector3(-n.x, -n.y, -n.z);
                }
                best.normal = n;
            }
        } else {
            if (depth + 2 <= 64) {
                stack[depth++] = node.left;
                stack[depth++] = node.right;
            }
        }
    }
    return best;
}

bool AcousticBVH::Occluded(const Math::Vector3& from, const Math::Vector3& to,
                           f32 epsilon) const {
    if (!IsBuilt()) return false;

    const Math::Vector3 delta(to.x - from.x, to.y - from.y, to.z - from.z);
    const f32 length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (length < epsilon) return false;

    const Math::Vector3 dir(delta.x / length, delta.y / length, delta.z / length);
    const Math::Vector3 invDir(1.0f / dir.x, 1.0f / dir.y, 1.0f / dir.z);

    // Pulled in at both ends, because both endpoints usually SIT on a surface:
    // a reflection point is on a wall by construction, and without the epsilon
    // every such test reports the wall occluding itself.
    const f32 limit = length - epsilon;

    u32 stack[64];
    u32 depth = 0;
    stack[depth++] = 0;

    while (depth > 0) {
        const Node& node = m_Nodes[stack[--depth]];
        if (!RayAABB(from, invDir, node.lo, node.hi, limit)) continue;

        if (node.IsLeaf()) {
            for (u32 i = node.start; i < node.start + node.count; ++i) {
                const u32 t = m_Triangles[i];
                const Math::Vector3& a =
                    m_Scene->vertices[static_cast<usize>(m_Scene->indices[t * 3 + 0])];
                const Math::Vector3& b =
                    m_Scene->vertices[static_cast<usize>(m_Scene->indices[t * 3 + 1])];
                const Math::Vector3& c =
                    m_Scene->vertices[static_cast<usize>(m_Scene->indices[t * 3 + 2])];
                const f32 hitT = RayTriangle(from, dir, a, b, c);
                // Any hit at all ends it. Finding the nearest would be most of
                // the cost for an answer nobody asked for.
                if (hitT > epsilon && hitT < limit) return true;
            }
        } else if (depth + 2 <= 64) {
            stack[depth++] = node.left;
            stack[depth++] = node.right;
        }
    }
    return false;
}

} // namespace Acoustics
} // namespace Enjin
