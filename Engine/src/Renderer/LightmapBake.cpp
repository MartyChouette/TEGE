#include "Enjin/Renderer/LightmapBake.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

namespace {

inline Math::Vector3 Sub(const Math::Vector3& a, const Math::Vector3& b) {
    return Math::Vector3(a.x - b.x, a.y - b.y, a.z - b.z);
}
inline Math::Vector3 Add(const Math::Vector3& a, const Math::Vector3& b) {
    return Math::Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
}
inline Math::Vector3 Mul(const Math::Vector3& a, f32 s) {
    return Math::Vector3(a.x * s, a.y * s, a.z * s);
}
inline f32 Dot(const Math::Vector3& a, const Math::Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Math::Vector3 Cross(const Math::Vector3& a, const Math::Vector3& b) {
    return Math::Vector3(a.y * b.z - a.z * b.y,
                         a.z * b.x - a.x * b.z,
                         a.x * b.y - a.y * b.x);
}
inline Math::Vector3 Normalize(const Math::Vector3& v) {
    const f32 l = std::sqrt(Dot(v, v));
    return (l > 1e-12f) ? Mul(v, 1.0f / l) : Math::Vector3(0.0f, 0.0f, 1.0f);
}

// A small deterministic generator. std::mt19937 would do, but a bake has to
// produce identical bytes on every platform it runs on or a rebake shows up as
// a diff nobody can review, and the standard distributions are not specified
// tightly enough to promise that.
struct Rng {
    u32 s;
    explicit Rng(u32 seed) : s(seed ? seed : 1u) {}
    u32 Next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    f32 Unit() { return static_cast<f32>(Next() & 0xFFFFFFu) / 16777216.0f; }
};

// A direction in the hemisphere around `n`, cosine-weighted -- more samples
// near the axis, which is where a surface actually gathers most of its light.
Math::Vector3 CosineHemisphere(const Math::Vector3& n, Rng& rng) {
    const f32 u1 = rng.Unit();
    const f32 u2 = rng.Unit();
    const f32 r = std::sqrt(u1);
    const f32 theta = 6.28318530718f * u2;
    const f32 x = r * std::cos(theta);
    const f32 y = r * std::sin(theta);
    const f32 z = std::sqrt(std::max(0.0f, 1.0f - u1));

    // Any frame around n will do; this one avoids the degenerate case where the
    // helper axis is parallel to n.
    const Math::Vector3 helper = (std::fabs(n.z) < 0.9f) ? Math::Vector3(0, 0, 1)
                                                         : Math::Vector3(1, 0, 0);
    const Math::Vector3 t = Normalize(Cross(helper, n));
    const Math::Vector3 b = Cross(n, t);
    return Normalize(Add(Add(Mul(t, x), Mul(b, y)), Mul(n, z)));
}

inline u8 ToByte(f32 v) {
    const f32 c = v <= 0.0f ? 0.0f : (v >= 1.0f ? 1.0f : v);
    return static_cast<u8>(c * 255.0f + 0.5f);
}

} // namespace

// --- Ray scene ---------------------------------------------------------------

u32 BakeRayScene::BuildRange(u32 first, u32 count, u32 depth) {
    const u32 nodeIndex = static_cast<u32>(m_Nodes.size());
    m_Nodes.push_back(Node{});

    Math::Vector3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
    for (u32 i = first; i < first + count; ++i) {
        const Tri& t = m_Tris[i];
        const Math::Vector3 p1 = Add(t.p0, t.e1);
        const Math::Vector3 p2 = Add(t.p0, t.e2);
        for (const Math::Vector3& p : { t.p0, p1, p2 }) {
            lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
            hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
        }
    }
    m_Nodes[nodeIndex].lo = lo;
    m_Nodes[nodeIndex].hi = hi;

    // Leaf. Four triangles is small enough that the slab tests stop paying for
    // themselves, and depth is capped so a pathological split cannot recurse
    // forever on coincident centroids.
    if (count <= 4 || depth > 32) {
        m_Nodes[nodeIndex].first = first;
        m_Nodes[nodeIndex].count = count;
        return nodeIndex;
    }

    // Split on the widest axis at the median centroid. Median rather than the
    // midpoint so a cluster of geometry in one corner still divides evenly.
    const Math::Vector3 extent = Sub(hi, lo);
    const int axis = (extent.x > extent.y) ? ((extent.x > extent.z) ? 0 : 2)
                                           : ((extent.y > extent.z) ? 1 : 2);
    const auto centroid = [axis](const Tri& t) {
        const Math::Vector3 c = Add(t.p0, Mul(Add(t.e1, t.e2), 1.0f / 3.0f));
        return axis == 0 ? c.x : (axis == 1 ? c.y : c.z);
    };
    const u32 mid = count / 2;
    std::nth_element(m_Tris.begin() + first, m_Tris.begin() + first + mid,
                     m_Tris.begin() + first + count,
                     [&](const Tri& a, const Tri& b) { return centroid(a) < centroid(b); });

    BuildRange(first, mid, depth + 1);
    m_Nodes[nodeIndex].right = BuildRange(first + mid, count - mid, depth + 1);
    m_Nodes[nodeIndex].count = 0;
    return nodeIndex;
}

void BakeRayScene::Build(const std::vector<BakeTriangle>& triangles) {
    m_Tris.clear();
    m_Nodes.clear();
    m_Tris.reserve(triangles.size());
    for (const BakeTriangle& t : triangles) {
        Tri tri;
        tri.p0 = t.position[0];
        tri.e1 = Sub(t.position[1], t.position[0]);
        tri.e2 = Sub(t.position[2], t.position[0]);
        // Degenerate triangles cannot occlude anything and would divide by zero
        // in the intersection test.
        if (Dot(Cross(tri.e1, tri.e2), Cross(tri.e1, tri.e2)) < 1e-16f) continue;
        m_Tris.push_back(tri);
    }
    if (m_Tris.empty()) return;
    m_Nodes.reserve(m_Tris.size() * 2);
    BuildRange(0, static_cast<u32>(m_Tris.size()), 0);
}

bool BakeRayScene::Occluded(const Math::Vector3& origin, const Math::Vector3& direction,
                            f32 maxDistance) const {
    if (m_Nodes.empty()) return false;

    const Math::Vector3 inv(
        1.0f / (std::fabs(direction.x) > 1e-12f ? direction.x : 1e-12f),
        1.0f / (std::fabs(direction.y) > 1e-12f ? direction.y : 1e-12f),
        1.0f / (std::fabs(direction.z) > 1e-12f ? direction.z : 1e-12f));

    u32 stack[64];
    u32 sp = 0;
    stack[sp++] = 0;

    while (sp > 0) {
        const Node& n = m_Nodes[stack[--sp]];

        // Slab test against the node's box.
        f32 t0 = 0.0f, t1 = maxDistance;
        for (int a = 0; a < 3; ++a) {
            const f32 o = (a == 0) ? origin.x : (a == 1 ? origin.y : origin.z);
            const f32 iv = (a == 0) ? inv.x : (a == 1 ? inv.y : inv.z);
            const f32 lo = (a == 0) ? n.lo.x : (a == 1 ? n.lo.y : n.lo.z);
            const f32 hi = (a == 0) ? n.hi.x : (a == 1 ? n.hi.y : n.hi.z);
            f32 tNear = (lo - o) * iv;
            f32 tFar = (hi - o) * iv;
            if (tNear > tFar) std::swap(tNear, tFar);
            t0 = std::max(t0, tNear);
            t1 = std::min(t1, tFar);
        }
        if (t0 > t1) continue;

        if (n.count > 0) {
            for (u32 i = n.first; i < n.first + n.count; ++i) {
                // Moller-Trumbore.
                const Tri& tri = m_Tris[i];
                const Math::Vector3 pv = Cross(direction, tri.e2);
                const f32 det = Dot(tri.e1, pv);
                if (std::fabs(det) < 1e-12f) continue;
                const f32 invDet = 1.0f / det;
                const Math::Vector3 tv = Sub(origin, tri.p0);
                const f32 u = Dot(tv, pv) * invDet;
                if (u < 0.0f || u > 1.0f) continue;
                const Math::Vector3 qv = Cross(tv, tri.e1);
                const f32 v = Dot(direction, qv) * invDet;
                if (v < 0.0f || u + v > 1.0f) continue;
                const f32 t = Dot(tri.e2, qv) * invDet;
                if (t > 1e-4f && t < maxDistance) return true;
            }
        } else {
            // The left child always sits immediately after its parent, because
            // BuildRange pushes a node and then recurses left before right.
            // The right index is stored because it is the one that moves.
            const u32 self = static_cast<u32>(&n - m_Nodes.data());
            if (sp + 2 <= 64) {
                stack[sp++] = self + 1;
                stack[sp++] = n.right;
            }
        }
    }
    return false;
}

// --- The bake ----------------------------------------------------------------

LightmapBakeResult BakeLightmap(const std::vector<BakeTriangle>& triangles,
                                const std::vector<BakeLight>& lights,
                                const LightmapBakeOptions& options) {
    LightmapBakeResult result;
    if (triangles.empty()) {
        result.error = "nothing to bake";
        return result;
    }
    if (options.atlasSize == 0) {
        result.error = "atlas size must be positive";
        return result;
    }

    const u32 size = options.atlasSize;
    const usize pixels = static_cast<usize>(size) * size;
    for (u32 b = 0; b < kRNMBasisCount; ++b) result.basis[b].assign(pixels * 3, 0);
    std::vector<u8> covered(pixels, 0);

    BakeRayScene scene;
    scene.Build(triangles);

    const Math::Vector3* rnm = RNMBasis();
    Rng rng(options.seed);
    u32 rayCasts = 0;

    for (const BakeTriangle& tri : triangles) {
        // The island's texel bounds, clamped to the atlas.
        f32 minU = tri.uv1[0].x, maxU = tri.uv1[0].x;
        f32 minV = tri.uv1[0].y, maxV = tri.uv1[0].y;
        for (int k = 1; k < 3; ++k) {
            minU = std::min(minU, tri.uv1[k].x); maxU = std::max(maxU, tri.uv1[k].x);
            minV = std::min(minV, tri.uv1[k].y); maxV = std::max(maxV, tri.uv1[k].y);
        }
        const i32 x0 = std::max(0, static_cast<i32>(std::floor(minU * size)) - 1);
        const i32 x1 = std::min(static_cast<i32>(size) - 1, static_cast<i32>(std::ceil(maxU * size)) + 1);
        const i32 y0 = std::max(0, static_cast<i32>(std::floor(minV * size)) - 1);
        const i32 y1 = std::min(static_cast<i32>(size) - 1, static_cast<i32>(std::ceil(maxV * size)) + 1);

        // Barycentric setup in UV space, so a texel can be tested for being
        // inside the triangle and interpolated in one step.
        const f32 ax = tri.uv1[0].x, ay = tri.uv1[0].y;
        const f32 bx = tri.uv1[1].x, by = tri.uv1[1].y;
        const f32 cx = tri.uv1[2].x, cy = tri.uv1[2].y;
        const f32 denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);
        if (std::fabs(denom) < 1e-16f) continue;   // island with no area

        for (i32 y = y0; y <= y1; ++y) {
            for (i32 x = x0; x <= x1; ++x) {
                const f32 px = (static_cast<f32>(x) + 0.5f) / static_cast<f32>(size);
                const f32 py = (static_cast<f32>(y) + 0.5f) / static_cast<f32>(size);

                f32 w0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / denom;
                f32 w1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / denom;
                f32 w2 = 1.0f - w0 - w1;
                // A small tolerance so texels straddling an edge are still
                // lit -- an untouched edge texel reads as a black outline once
                // the atlas is filtered.
                const f32 tol = -0.25f;
                if (w0 < tol || w1 < tol || w2 < tol) continue;
                w0 = std::max(0.0f, w0); w1 = std::max(0.0f, w1); w2 = std::max(0.0f, w2);
                const f32 wsum = w0 + w1 + w2;
                if (wsum <= 0.0f) continue;
                w0 /= wsum; w1 /= wsum; w2 /= wsum;

                const Math::Vector3 pos(
                    tri.position[0].x * w0 + tri.position[1].x * w1 + tri.position[2].x * w2,
                    tri.position[0].y * w0 + tri.position[1].y * w1 + tri.position[2].y * w2,
                    tri.position[0].z * w0 + tri.position[1].z * w1 + tri.position[2].z * w2);
                const Math::Vector3 nrm = Normalize(Math::Vector3(
                    tri.normal[0].x * w0 + tri.normal[1].x * w1 + tri.normal[2].x * w2,
                    tri.normal[0].y * w0 + tri.normal[1].y * w1 + tri.normal[2].y * w2,
                    tri.normal[0].z * w0 + tri.normal[1].z * w1 + tri.normal[2].z * w2));
                Math::Vector3 tan(
                    tri.tangent[0].x * w0 + tri.tangent[1].x * w1 + tri.tangent[2].x * w2,
                    tri.tangent[0].y * w0 + tri.tangent[1].y * w1 + tri.tangent[2].y * w2,
                    tri.tangent[0].z * w0 + tri.tangent[1].z * w1 + tri.tangent[2].z * w2);
                // Gram-Schmidt, then a fallback: a mesh with no tangents at all
                // would otherwise give a zero frame and bake three identical
                // maps, which silently turns the technique off.
                tan = Sub(tan, Mul(nrm, Dot(nrm, tan)));
                if (Dot(tan, tan) < 1e-12f) {
                    const Math::Vector3 helper = (std::fabs(nrm.z) < 0.9f) ? Math::Vector3(0, 0, 1)
                                                                           : Math::Vector3(1, 0, 0);
                    tan = Cross(helper, nrm);
                }
                tan = Normalize(tan);
                const f32 handed = tri.tangent[0].w < 0.0f ? -1.0f : 1.0f;
                const Math::Vector3 bit = Mul(Cross(nrm, tan), handed);

                const Math::Vector3 origin = Add(pos, Mul(nrm, options.rayBias));
                const usize texel = static_cast<usize>(y) * size + static_cast<usize>(x);
                covered[texel] = 1;

                for (u32 b = 0; b < kRNMBasisCount; ++b) {
                    // The basis direction, carried from tangent space into the
                    // world by this texel's own frame.
                    const Math::Vector3 dir = Normalize(Math::Vector3(
                        tan.x * rnm[b].x + bit.x * rnm[b].y + nrm.x * rnm[b].z,
                        tan.y * rnm[b].x + bit.y * rnm[b].y + nrm.y * rnm[b].z,
                        tan.z * rnm[b].x + bit.z * rnm[b].y + nrm.z * rnm[b].z));

                    Math::Vector3 sum(0.0f, 0.0f, 0.0f);

                    for (const BakeLight& L : lights) {
                        Math::Vector3 toLight;
                        f32 distance = 1e30f;
                        f32 atten = 1.0f;
                        if (L.type == BakeLight::Type::Directional) {
                            toLight = Normalize(Mul(L.vector, -1.0f));
                        } else {
                            const Math::Vector3 d = Sub(L.vector, pos);
                            distance = std::sqrt(Dot(d, d));
                            if (distance > L.range || distance < 1e-6f) continue;
                            toLight = Mul(d, 1.0f / distance);
                            const f32 f = 1.0f - distance / L.range;
                            atten = f * f;
                        }
                        const f32 ndl = Dot(dir, toLight);
                        if (ndl <= 0.0f) continue;
                        if (L.castsShadow) {
                            ++rayCasts;
                            if (scene.Occluded(origin, toLight, distance * 0.999f)) continue;
                        }
                        const f32 k = ndl * L.intensity * atten;
                        sum = Add(sum, Mul(L.color, k));
                    }

                    // Sky. Unoccluded rays see the sky above the horizon and the
                    // ground bounce below it; occluded ones see nothing, which is
                    // where the darkening in corners comes from -- ambient
                    // occlusion falling out of the same loop rather than being a
                    // separate effect bolted on.
                    if (options.skySamples > 0 && options.skyIntensity > 0.0f) {
                        Math::Vector3 sky(0.0f, 0.0f, 0.0f);
                        for (u32 s = 0; s < options.skySamples; ++s) {
                            const Math::Vector3 rd = CosineHemisphere(dir, rng);
                            ++rayCasts;
                            if (scene.Occluded(origin, rd, 1e4f)) continue;
                            sky = Add(sky, rd.y > 0.0f ? options.skyColor : options.groundColor);
                        }
                        sum = Add(sum, Mul(sky, options.skyIntensity /
                                                static_cast<f32>(options.skySamples)));
                    }

                    u8* out = result.basis[b].data() + texel * 3;
                    out[0] = ToByte(sum.x);
                    out[1] = ToByte(sum.y);
                    out[2] = ToByte(sum.z);
                }
            }
        }
    }

    // Dilate: push lit texels outward into the padding. Without this a bilinear
    // read at an island's edge mixes in the black margin, which draws a dark
    // outline around every triangle -- the other classic lightmap artifact.
    for (u32 pass = 0; pass < options.dilate; ++pass) {
        std::vector<u8> grown = covered;
        for (i32 y = 0; y < static_cast<i32>(size); ++y) {
            for (i32 x = 0; x < static_cast<i32>(size); ++x) {
                const usize t = static_cast<usize>(y) * size + static_cast<usize>(x);
                if (covered[t]) continue;
                for (i32 dy = -1; dy <= 1 && !grown[t]; ++dy) {
                    for (i32 dx = -1; dx <= 1; ++dx) {
                        const i32 nx = x + dx, ny = y + dy;
                        if (nx < 0 || ny < 0 || nx >= static_cast<i32>(size) ||
                            ny >= static_cast<i32>(size)) continue;
                        const usize n = static_cast<usize>(ny) * size + static_cast<usize>(nx);
                        if (!covered[n]) continue;
                        for (u32 b = 0; b < kRNMBasisCount; ++b) {
                            u8* dst = result.basis[b].data() + t * 3;
                            const u8* src = result.basis[b].data() + n * 3;
                            dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
                        }
                        grown[t] = 1;
                        break;
                    }
                }
            }
        }
        covered.swap(grown);
    }

    for (u8 c : covered) if (c) ++result.litTexels;
    result.rayCasts = rayCasts;
    result.ok = true;
    return result;
}

} // namespace Renderer
} // namespace Enjin
