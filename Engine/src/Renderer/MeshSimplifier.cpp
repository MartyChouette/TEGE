#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <queue>

namespace Enjin {
namespace Renderer {

// Simple spatial hash for vertex welding
struct VertexHash {
    usize operator()(const Math::Vector3& v) const {
        // Quantize to grid for fast lookup
        auto h = [](f32 f) -> usize {
            i32 q = static_cast<i32>(f * 1000.0f);
            return std::hash<i32>()(q);
        };
        return h(v.x) ^ (h(v.y) << 11) ^ (h(v.z) << 22);
    }
};

struct VertexEqual {
    bool operator()(const Math::Vector3& a, const Math::Vector3& b) const {
        return std::abs(a.x - b.x) < 1e-5f &&
               std::abs(a.y - b.y) < 1e-5f &&
               std::abs(a.z - b.z) < 1e-5f;
    }
};

// Symmetric 4x4 error quadric (Garland-Heckbert). Stored as 10 doubles for the
// upper triangle: layout [0 1 2 3 / 1 4 5 6 / 2 5 7 8 / 3 6 8 9]. Doubles because
// accumulating hundreds of planes in float loses the precision the solve needs.
struct Quadric {
    double q[10] = {0,0,0,0,0,0,0,0,0,0};
    void addPlane(double a, double b, double c, double d, double w = 1.0) {
        q[0]+=w*a*a; q[1]+=w*a*b; q[2]+=w*a*c; q[3]+=w*a*d;
        q[4]+=w*b*b; q[5]+=w*b*c; q[6]+=w*b*d;
        q[7]+=w*c*c; q[8]+=w*c*d;
        q[9]+=w*d*d;
    }
    void add(const Quadric& o) { for (int i = 0; i < 10; ++i) q[i] += o.q[i]; }
    // v^T Q v for the homogeneous point (x,y,z,1) — the squared distance to the
    // set of planes this quadric represents.
    double error(double x, double y, double z) const {
        return q[0]*x*x + 2*q[1]*x*y + 2*q[2]*x*z + 2*q[3]*x
             + q[4]*y*y + 2*q[5]*y*z + 2*q[6]*y
             + q[7]*z*z + 2*q[8]*z + q[9];
    }
    // Position minimizing the error (solve the 3x3 from the quadric's gradient).
    // Returns false if the system is singular (flat/degenerate) — caller falls back.
    bool solveOptimal(Math::Vector3& out) const {
        double a=q[0], b=q[1], c=q[2], e=q[4], f=q[5], i=q[7];
        double det = a*(e*i - f*f) - b*(b*i - f*c) + c*(b*f - e*c);
        if (std::abs(det) < 1e-12) return false;
        double bx=-q[3], by=-q[6], bz=-q[8], inv = 1.0/det;
        double x = (bx*(e*i-f*f) - b*(by*i-f*bz) + c*(by*f-e*bz)) * inv;
        double y = (a*(by*i-f*bz) - bx*(b*i-f*c) + c*(b*bz-by*c)) * inv;
        double z = (a*(e*bz-by*f) - b*(b*bz-by*c) + bx*(b*f-e*c)) * inv;
        out = Math::Vector3(static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(z));
        return true;
    }
};

// Edge collapse data
struct EdgeCollapse {
    u32 v0, v1;           // v1 collapses into v0 (v0 survives)
    f32 cost;             // Quadric error of the collapse
    Math::Vector3 target; // Optimal collapse position
    u32 ver0, ver1;       // Endpoint versions when queued — stale entries are skipped on pop

    bool operator>(const EdgeCollapse& other) const { return cost > other.cost; }
};

ECS::MeshComponent MeshSimplifier::Simplify(const ECS::MeshComponent& source, f32 ratio) {
    if (ratio >= 1.0f || source.vertices.size() < 12 || source.indices.size() < 12) {
        return source; // No simplification needed
    }

    ratio = Math::Clamp(ratio, 0.01f, 1.0f);
    u32 targetVertCount = static_cast<u32>(source.vertices.size() * ratio);
    if (targetVertCount < 4) targetVertCount = 4;

    // Build adjacency: for each vertex, track which triangles use it
    u32 vertCount = static_cast<u32>(source.vertices.size());
    u32 triCount = static_cast<u32>(source.indices.size() / 3);

    // Validate indices — skip simplification if any index is out of range
    for (u32 i = 0; i < source.indices.size(); ++i) {
        if (source.indices[i] >= vertCount) {
            ENJIN_LOG_WARN(Asset, "MeshSimplifier: index %u out of range (vertCount=%u), skipping", source.indices[i], vertCount);
            return source;
        }
    }

    // Working copies
    std::vector<ECS::MeshComponent::Vertex> vertices = source.vertices;
    std::vector<u32> indices = source.indices;

    auto cross = [](const Math::Vector3& u, const Math::Vector3& v) {
        return Math::Vector3(u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x);
    };
    auto edgeKey = [](u32 a, u32 b) -> u64 {
        u32 lo = a < b ? a : b, hi = a < b ? b : a;
        return static_cast<u64>(lo) | (static_cast<u64>(hi) << 32);
    };

    // --- Per-vertex error quadrics from area-weighted face planes (Garland-Heckbert).
    // Each face contributes its plane to its three vertices; larger faces weigh more,
    // so flat regions accrue low error (cheap to collapse) and detail stays.
    std::vector<Quadric> Q(vertCount);
    std::unordered_map<u64, u32> edgeFaceCount;
    edgeFaceCount.reserve(triCount * 3);
    for (u32 t = 0; t < triCount; ++t) {
        u32 i0 = indices[t*3+0], i1 = indices[t*3+1], i2 = indices[t*3+2];
        const Math::Vector3& p0 = vertices[i0].position;
        const Math::Vector3& p1 = vertices[i1].position;
        const Math::Vector3& p2 = vertices[i2].position;
        Math::Vector3 n = cross(p1 - p0, p2 - p0);
        f32 len = n.Length();
        if (len < 1e-12f) continue; // degenerate face has no plane
        n = n * (1.0f / len);
        double d = -static_cast<double>(n.x*p0.x + n.y*p0.y + n.z*p0.z);
        double w = 0.5 * static_cast<double>(len); // triangle area
        Q[i0].addPlane(n.x, n.y, n.z, d, w);
        Q[i1].addPlane(n.x, n.y, n.z, d, w);
        Q[i2].addPlane(n.x, n.y, n.z, d, w);
        edgeFaceCount[edgeKey(i0,i1)]++;
        edgeFaceCount[edgeKey(i1,i2)]++;
        edgeFaceCount[edgeKey(i2,i0)]++;
    }

    // Boundary preservation: an edge used by a single triangle is an open border.
    // Add a plane perpendicular to that triangle through the edge, weighted heavily,
    // so simplification can't erode the silhouette inward.
    for (u32 t = 0; t < triCount; ++t) {
        u32 tri[3] = { indices[t*3+0], indices[t*3+1], indices[t*3+2] };
        Math::Vector3 fn = cross(vertices[tri[1]].position - vertices[tri[0]].position,
                                 vertices[tri[2]].position - vertices[tri[0]].position);
        if (fn.Length() < 1e-12f) continue;
        fn = fn * (1.0f / fn.Length());
        for (int e = 0; e < 3; ++e) {
            u32 a = tri[e], b = tri[(e+1)%3];
            if (edgeFaceCount[edgeKey(a,b)] != 1) continue; // interior edge
            Math::Vector3 bn = cross(vertices[b].position - vertices[a].position, fn);
            f32 bl = bn.Length();
            if (bl < 1e-12f) continue;
            bn = bn * (1.0f / bl);
            double d = -static_cast<double>(bn.x*vertices[a].position.x + bn.y*vertices[a].position.y + bn.z*vertices[a].position.z);
            constexpr double kBoundary = 1000.0;
            Q[a].addPlane(bn.x, bn.y, bn.z, d, kBoundary);
            Q[b].addPlane(bn.x, bn.y, bn.z, d, kBoundary);
        }
    }

    // Adjacency (neighbor sets), kept current across collapses so we can re-cost.
    std::vector<std::unordered_set<u32>> adj(vertCount);
    for (u32 t = 0; t < triCount; ++t) {
        u32 i0 = indices[t*3+0], i1 = indices[t*3+1], i2 = indices[t*3+2];
        if (i0==i1||i1==i2||i0==i2) continue;
        adj[i0].insert(i1); adj[i1].insert(i0);
        adj[i1].insert(i2); adj[i2].insert(i1);
        adj[i2].insert(i0); adj[i0].insert(i2);
    }

    std::vector<u32> vertVersion(vertCount, 0);
    std::vector<bool> vertRemoved(vertCount, false);
    std::vector<u32> remap(vertCount);
    for (u32 i = 0; i < vertCount; ++i) remap[i] = i;

    std::priority_queue<EdgeCollapse, std::vector<EdgeCollapse>, std::greater<EdgeCollapse>> pq;
    auto makeCollapse = [&](u32 a, u32 b) -> EdgeCollapse {
        Quadric qs = Q[a]; qs.add(Q[b]);
        Math::Vector3 opt;
        if (!qs.solveOptimal(opt)) {
            // Singular (flat/degenerate) — take the cheapest of the two endpoints or the midpoint.
            Math::Vector3 pa = vertices[a].position, pb = vertices[b].position, mid = (pa + pb) * 0.5f;
            double ea = qs.error(pa.x,pa.y,pa.z), eb = qs.error(pb.x,pb.y,pb.z), em = qs.error(mid.x,mid.y,mid.z);
            opt = (ea <= eb && ea <= em) ? pa : (eb <= em ? pb : mid);
        }
        double c = qs.error(opt.x, opt.y, opt.z);
        EdgeCollapse ec;
        ec.v0 = a; ec.v1 = b; ec.cost = static_cast<f32>(c < 0.0 ? 0.0 : c); ec.target = opt;
        ec.ver0 = vertVersion[a]; ec.ver1 = vertVersion[b];
        return ec;
    };

    {
        std::unordered_set<u64> seen;
        seen.reserve(triCount * 3);
        for (u32 v = 0; v < vertCount; ++v)
            for (u32 n : adj[v])
                if (seen.insert(edgeKey(v, n)).second) pq.push(makeCollapse(v, n));
    }

    u32 activeVerts = vertCount;
    while (activeVerts > targetVertCount && !pq.empty()) {
        EdgeCollapse ec = pq.top(); pq.pop();
        u32 a = ec.v0, b = ec.v1;
        if (a == b || vertRemoved[a] || vertRemoved[b]) continue;
        if (vertVersion[a] != ec.ver0 || vertVersion[b] != ec.ver1) continue; // stale cost

        // Collapse b into a at the optimal point; merge quadrics + blend attributes.
        Q[a].add(Q[b]);
        vertices[a].position = ec.target;
        Math::Vector3 nrm = vertices[a].normal + vertices[b].normal;
        f32 nl = nrm.Length();
        vertices[a].normal = (nl > 1e-6f) ? nrm * (1.0f/nl) : vertices[a].normal;
        vertices[a].uv = (vertices[a].uv + vertices[b].uv) * 0.5f;

        remap[b] = a;
        vertRemoved[b] = true;
        --activeVerts;

        for (u32 n : adj[b]) {           // rewire b's neighbors onto a
            if (n == a) continue;
            adj[n].erase(b); adj[n].insert(a); adj[a].insert(n);
        }
        adj[a].erase(b);
        adj[b].clear();

        ++vertVersion[a];                // a changed — old queued edges are now stale
        for (u32 n : adj[a])
            if (!vertRemoved[n]) pq.push(makeCollapse(a, n));
    }

    // --- Rebuild: compact survivors, remap triangles, drop degenerates ---
    auto findRoot = [&](u32 v) -> u32 { while (remap[v] != v) v = remap[v]; return v; };
    ECS::MeshComponent result;
    std::vector<u32> vertexMap(vertCount, UINT32_MAX);
    for (u32 i = 0; i < vertCount; ++i) {
        u32 root = findRoot(i);
        if (vertexMap[root] == UINT32_MAX) {
            vertexMap[root] = static_cast<u32>(result.vertices.size());
            result.vertices.push_back(vertices[root]);
        }
    }
    for (u32 t = 0; t < triCount; ++t) {
        u32 i0 = findRoot(indices[t*3+0]), i1 = findRoot(indices[t*3+1]), i2 = findRoot(indices[t*3+2]);
        if (i0==i1 || i1==i2 || i0==i2) continue;
        u32 n0 = vertexMap[i0], n1 = vertexMap[i1], n2 = vertexMap[i2];
        if (n0==UINT32_MAX||n1==UINT32_MAX||n2==UINT32_MAX) continue;
        result.indices.push_back(n0);
        result.indices.push_back(n1);
        result.indices.push_back(n2);
    }

    return result;
}

void MeshSimplifier::GenerateLODs(const ECS::MeshComponent& sourceMesh, ECS::LODComponent& lod) {
    if (!sourceMesh.IsValid()) return;

    u32 sourceVerts = static_cast<u32>(sourceMesh.vertices.size());
    u32 sourceTris = static_cast<u32>(sourceMesh.indices.size() / 3);

    // Stable source size for the LOD selection metric (largest AABB extent of the
    // ORIGINAL mesh). Computed once here so the render-time metric never depends on
    // whichever LOD mesh is currently swapped in (that feedback loop is what made
    // LOD flicker and chug to a crash).
    {
        Math::Vector3 mn(1e30f, 1e30f, 1e30f), mx(-1e30f, -1e30f, -1e30f);
        for (const auto& v : sourceMesh.vertices) {
            mn.x = std::min(mn.x, v.position.x); mn.y = std::min(mn.y, v.position.y); mn.z = std::min(mn.z, v.position.z);
            mx.x = std::max(mx.x, v.position.x); mx.y = std::max(mx.y, v.position.y); mx.z = std::max(mx.z, v.position.z);
        }
        Math::Vector3 ext = mx - mn;
        lod.sourceMaxExtent = Math::Max(Math::Max(ext.x, ext.y), ext.z);
        if (!(lod.sourceMaxExtent > 0.0f)) lod.sourceMaxExtent = 0.0f; // NaN/empty guard
    }

    // Skip LOD generation for very large meshes to avoid long hangs
    if (sourceVerts > 100000) {
        ENJIN_LOG_WARN(Asset, "Skipping LOD generation for mesh with %u vertices (too large)", sourceVerts);
        lod.levelCount = 1;
        lod.levels[0].mesh = sourceMesh;
        lod.levels[0].vertexCount = sourceVerts;
        lod.levels[0].triangleCount = sourceTris;
        lod.levels[0].reductionRatio = 1.0f;
        lod.levels[0].maxDistance = lod.baseDistance;
        lod.activeLOD = 0;
        return;
    }

    lod.levelCount = 0;

    // A LOD level below this triangle count is useless (LOD 4 came out at 0 tris on
    // a bad mesh, rendering nothing). Stop before emitting a destroyed level.
    constexpr u32 kMinLODTriangles = 16;

    for (int i = 0; i < ECS::LODComponent::MAX_LEVELS; ++i) {
        f32 ratio = lod.reductionRatios[i];

        ECS::MeshComponent lvlMesh = (i == 0) ? sourceMesh : Simplify(sourceMesh, ratio);
        u32 tris = static_cast<u32>(lvlMesh.indices.size() / 3);

        // Reject a level the simplifier destroyed. An empty/near-empty mesh renders
        // as nothing, and its collapse to a tiny bound also drives LOD flicker. Keep
        // the last good level as the coarsest LOD instead of shipping garbage.
        if (i > 0 && tris < kMinLODTriangles) break;

        ECS::LODComponent::LODLevel& level = lod.levels[i];
        level.reductionRatio = ratio;
        level.mesh = std::move(lvlMesh);
        level.vertexCount = static_cast<u32>(level.mesh.vertices.size());
        level.triangleCount = tris;
        // Distance thresholds: base * multiplier^level
        level.maxDistance = lod.baseDistance * std::pow(lod.distanceMultiplier, static_cast<f32>(i));

        lod.levelCount++;

        // Don't generate further levels if mesh is already very simple
        if (level.vertexCount <= 8 || level.triangleCount <= kMinLODTriangles) {
            break;
        }
    }

    lod.autoGenerated = true;
    lod.activeLOD = 0;

    ENJIN_LOG_INFO(Asset, "Generated %d LOD levels: ", lod.levelCount);
    for (int i = 0; i < lod.levelCount; ++i) {
        ENJIN_LOG_INFO(Asset, "  LOD %d: %u verts, %u tris (%.0f%% of original, dist %.1f)",
            i, lod.levels[i].vertexCount, lod.levels[i].triangleCount,
            lod.levels[i].reductionRatio * 100.0f, lod.levels[i].maxDistance);
    }
}

} // namespace Renderer
} // namespace Enjin
