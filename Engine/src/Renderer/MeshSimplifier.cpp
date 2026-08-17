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

// Edge collapse data
struct EdgeCollapse {
    u32 v0, v1;       // Vertices to collapse
    f32 cost;          // Error cost of this collapse
    Math::Vector3 target; // Optimal collapse position

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
    std::vector<bool> vertRemoved(vertCount, false);
    std::vector<bool> triRemoved(triCount, false);

    // Build edge set and compute collapse costs
    // Use simple edge-length-based cost (approximation of QEM)
    struct Edge {
        u32 v0, v1;
        bool operator==(const Edge& o) const {
            return (v0 == o.v0 && v1 == o.v1) || (v0 == o.v1 && v1 == o.v0);
        }
    };
    struct EdgeHash {
        usize operator()(const Edge& e) const {
            u32 lo = e.v0 < e.v1 ? e.v0 : e.v1;
            u32 hi = e.v0 < e.v1 ? e.v1 : e.v0;
            return std::hash<u64>()(static_cast<u64>(lo) | (static_cast<u64>(hi) << 32));
        }
    };

    std::unordered_set<u64> edgeSet;
    std::priority_queue<EdgeCollapse, std::vector<EdgeCollapse>, std::greater<EdgeCollapse>> pq;

    auto edgeKey = [](u32 a, u32 b) -> u64 {
        u32 lo = a < b ? a : b;
        u32 hi = a < b ? b : a;
        return static_cast<u64>(lo) | (static_cast<u64>(hi) << 32);
    };

    // Collect all edges from triangles
    for (u32 t = 0; t < triCount; ++t) {
        u32 i0 = indices[t * 3 + 0];
        u32 i1 = indices[t * 3 + 1];
        u32 i2 = indices[t * 3 + 2];

        u64 edges[3] = { edgeKey(i0, i1), edgeKey(i1, i2), edgeKey(i2, i0) };
        u32 pairs[3][2] = { {i0, i1}, {i1, i2}, {i2, i0} };

        for (int e = 0; e < 3; ++e) {
            if (edgeSet.count(edges[e]) == 0) {
                edgeSet.insert(edges[e]);

                u32 v0 = pairs[e][0], v1 = pairs[e][1];
                Math::Vector3 mid = (vertices[v0].position + vertices[v1].position) * 0.5f;
                f32 edgeLen = (vertices[v0].position - vertices[v1].position).Length();

                EdgeCollapse ec;
                ec.v0 = v0;
                ec.v1 = v1;
                ec.cost = edgeLen;
                ec.target = mid;
                pq.push(ec);
            }
        }
    }

    // Remap table: vertex i -> its current representative
    std::vector<u32> remap(vertCount);
    for (u32 i = 0; i < vertCount; ++i) remap[i] = i;

    auto findRoot = [&](u32 v) -> u32 {
        while (remap[v] != v) v = remap[v];
        return v;
    };

    u32 activeVerts = vertCount;

    // Collapse edges until we hit target
    while (activeVerts > targetVertCount && !pq.empty()) {
        EdgeCollapse ec = pq.top();
        pq.pop();

        u32 rv0 = findRoot(ec.v0);
        u32 rv1 = findRoot(ec.v1);

        // Skip if already collapsed to same vertex
        if (rv0 == rv1) continue;
        if (vertRemoved[rv0] || vertRemoved[rv1]) continue;

        // Collapse v1 into v0: move v0 to midpoint, redirect v1
        vertices[rv0].position = ec.target;
        // Blend normals
        vertices[rv0].normal = (vertices[rv0].normal + vertices[rv1].normal);
        f32 nLen = vertices[rv0].normal.Length();
        if (nLen > 1e-6f) vertices[rv0].normal = vertices[rv0].normal * (1.0f / nLen);
        // Average UVs
        vertices[rv0].uv = (vertices[rv0].uv + vertices[rv1].uv) * 0.5f;

        remap[rv1] = rv0;
        vertRemoved[rv1] = true;
        activeVerts--;
    }

    // Rebuild mesh with remapped indices, removing degenerate triangles
    ECS::MeshComponent result;

    // Build compacted vertex list
    std::vector<u32> vertexMap(vertCount, UINT32_MAX);
    u32 newVertIdx = 0;
    for (u32 i = 0; i < vertCount; ++i) {
        u32 root = findRoot(i);
        if (vertexMap[root] == UINT32_MAX) {
            vertexMap[root] = newVertIdx++;
            result.vertices.push_back(vertices[root]);
        }
    }

    // Remap and add non-degenerate triangles
    for (u32 t = 0; t < triCount; ++t) {
        u32 i0 = findRoot(indices[t * 3 + 0]);
        u32 i1 = findRoot(indices[t * 3 + 1]);
        u32 i2 = findRoot(indices[t * 3 + 2]);

        // Skip degenerate triangles
        if (i0 == i1 || i1 == i2 || i0 == i2) continue;

        u32 ni0 = vertexMap[i0];
        u32 ni1 = vertexMap[i1];
        u32 ni2 = vertexMap[i2];

        if (ni0 == UINT32_MAX || ni1 == UINT32_MAX || ni2 == UINT32_MAX) continue;

        result.indices.push_back(ni0);
        result.indices.push_back(ni1);
        result.indices.push_back(ni2);
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
