#include "Enjin/Effects/VoronoiMeshFracture.h"
#include "Enjin/Math/Math.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Effects {

ECS::Vertex VoronoiMeshFracture::LerpVertex(const ECS::Vertex& a, const ECS::Vertex& b, f32 t) {
    ECS::Vertex v;
    v.position.x = a.position.x + (b.position.x - a.position.x) * t;
    v.position.y = a.position.y + (b.position.y - a.position.y) * t;
    v.position.z = a.position.z + (b.position.z - a.position.z) * t;

    v.normal.x = a.normal.x + (b.normal.x - a.normal.x) * t;
    v.normal.y = a.normal.y + (b.normal.y - a.normal.y) * t;
    v.normal.z = a.normal.z + (b.normal.z - a.normal.z) * t;
    // Normalize interpolated normal
    f32 len = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y + v.normal.z * v.normal.z);
    if (len > 1e-6f) { v.normal.x /= len; v.normal.y /= len; v.normal.z /= len; }

    v.uv.x = a.uv.x + (b.uv.x - a.uv.x) * t;
    v.uv.y = a.uv.y + (b.uv.y - a.uv.y) * t;

    v.color.x = a.color.x + (b.color.x - a.color.x) * t;
    v.color.y = a.color.y + (b.color.y - a.color.y) * t;
    v.color.z = a.color.z + (b.color.z - a.color.z) * t;
    v.color.w = a.color.w + (b.color.w - a.color.w) * t;

    v.tangent.x = a.tangent.x + (b.tangent.x - a.tangent.x) * t;
    v.tangent.y = a.tangent.y + (b.tangent.y - a.tangent.y) * t;
    v.tangent.z = a.tangent.z + (b.tangent.z - a.tangent.z) * t;
    v.tangent.w = a.tangent.w; // Keep tangent handedness from first vertex

    // Bone weights/indices — lerp weights, keep indices from nearest vertex
    for (int i = 0; i < 4; ++i) {
        v.boneWeights[i] = a.boneWeights[i] + (b.boneWeights[i] - a.boneWeights[i]) * t;
        v.boneIndices[i] = (t < 0.5f) ? a.boneIndices[i] : b.boneIndices[i];
    }

    return v;
}

void VoronoiMeshFracture::ClipMeshByPlane(
    const std::vector<ECS::Vertex>& inVerts,
    const std::vector<u32>& inIndices,
    const Math::Vector3& planeNormal, f32 planeDist,
    std::vector<ECS::Vertex>& outVerts,
    std::vector<u32>& outIndices) {

    outVerts.clear();
    outIndices.clear();

    // Process each triangle independently
    for (usize triIdx = 0; triIdx + 2 < inIndices.size(); triIdx += 3) {
        const ECS::Vertex* tri[3] = {
            &inVerts[inIndices[triIdx]],
            &inVerts[inIndices[triIdx + 1]],
            &inVerts[inIndices[triIdx + 2]]
        };

        // Signed distances to plane
        f32 dist[3];
        for (int i = 0; i < 3; ++i) {
            dist[i] = planeNormal.x * tri[i]->position.x
                     + planeNormal.y * tri[i]->position.y
                     + planeNormal.z * tri[i]->position.z + planeDist;
        }

        bool inside[3] = { dist[0] >= 0.0f, dist[1] >= 0.0f, dist[2] >= 0.0f };
        int insideCount = (inside[0] ? 1 : 0) + (inside[1] ? 1 : 0) + (inside[2] ? 1 : 0);

        if (insideCount == 0) continue; // Fully clipped

        if (insideCount == 3) {
            // Fully inside — keep triangle as-is
            u32 base = static_cast<u32>(outVerts.size());
            outVerts.push_back(*tri[0]);
            outVerts.push_back(*tri[1]);
            outVerts.push_back(*tri[2]);
            outIndices.push_back(base);
            outIndices.push_back(base + 1);
            outIndices.push_back(base + 2);
            continue;
        }

        // Partial clip — Sutherland-Hodgman on the triangle
        std::vector<ECS::Vertex> polygon;
        polygon.reserve(4);

        for (int i = 0; i < 3; ++i) {
            int j = (i + 1) % 3;
            if (inside[i]) {
                polygon.push_back(*tri[i]);
                if (!inside[j]) {
                    // Edge exits — compute intersection
                    f32 t = dist[i] / (dist[i] - dist[j]);
                    polygon.push_back(LerpVertex(*tri[i], *tri[j], t));
                }
            } else {
                if (inside[j]) {
                    // Edge enters — compute intersection
                    f32 t = dist[i] / (dist[i] - dist[j]);
                    polygon.push_back(LerpVertex(*tri[i], *tri[j], t));
                }
            }
        }

        // Triangulate the clipped polygon (fan from first vertex)
        if (polygon.size() >= 3) {
            u32 base = static_cast<u32>(outVerts.size());
            for (auto& v : polygon) outVerts.push_back(v);
            for (u32 i = 1; i + 1 < static_cast<u32>(polygon.size()); ++i) {
                outIndices.push_back(base);
                outIndices.push_back(base + i);
                outIndices.push_back(base + i + 1);
            }
        }
    }
}

void VoronoiMeshFracture::GenerateCutFace(
    const std::vector<ECS::Vertex>& clippedVerts,
    const Math::Vector3& planeNormal,
    f32 planeDist,
    std::vector<ECS::Vertex>& outVerts,
    std::vector<u32>& outIndices) {

    // Find vertices that lie on the cut plane (within tolerance)
    constexpr f32 tolerance = 0.01f;
    std::vector<Math::Vector3> cutPoints;

    for (const auto& v : clippedVerts) {
        f32 d = planeNormal.x * v.position.x + planeNormal.y * v.position.y
              + planeNormal.z * v.position.z + planeDist;
        if (std::abs(d) < tolerance) {
            // Check for duplicates
            bool dupe = false;
            for (const auto& cp : cutPoints) {
                f32 dx = cp.x - v.position.x;
                f32 dy = cp.y - v.position.y;
                f32 dz = cp.z - v.position.z;
                if (dx * dx + dy * dy + dz * dz < tolerance * tolerance) { dupe = true; break; }
            }
            if (!dupe) cutPoints.push_back(v.position);
        }
    }

    if (cutPoints.size() < 3) return;

    // Compute centroid of cut points
    Math::Vector3 centroid(0, 0, 0);
    for (const auto& p : cutPoints) { centroid.x += p.x; centroid.y += p.y; centroid.z += p.z; }
    f32 invN = 1.0f / static_cast<f32>(cutPoints.size());
    centroid.x *= invN; centroid.y *= invN; centroid.z *= invN;

    // Sort points by angle around centroid projected onto the cut plane
    // Build a local 2D coordinate system on the plane
    Math::Vector3 refDir;
    if (std::abs(planeNormal.y) < 0.9f)
        refDir = Math::Vector3(0, 1, 0);
    else
        refDir = Math::Vector3(1, 0, 0);

    // tangentU = normalize(refDir - dot(refDir, normal) * normal)
    f32 d = refDir.x * planeNormal.x + refDir.y * planeNormal.y + refDir.z * planeNormal.z;
    Math::Vector3 tangentU(refDir.x - d * planeNormal.x, refDir.y - d * planeNormal.y, refDir.z - d * planeNormal.z);
    f32 tLen = std::sqrt(tangentU.x * tangentU.x + tangentU.y * tangentU.y + tangentU.z * tangentU.z);
    if (tLen > 1e-6f) { tangentU.x /= tLen; tangentU.y /= tLen; tangentU.z /= tLen; }

    // tangentV = cross(normal, tangentU)
    Math::Vector3 tangentV(
        planeNormal.y * tangentU.z - planeNormal.z * tangentU.y,
        planeNormal.z * tangentU.x - planeNormal.x * tangentU.z,
        planeNormal.x * tangentU.y - planeNormal.y * tangentU.x);

    std::vector<std::pair<f32, usize>> angles;
    angles.reserve(cutPoints.size());
    for (usize i = 0; i < cutPoints.size(); ++i) {
        Math::Vector3 rel(cutPoints[i].x - centroid.x, cutPoints[i].y - centroid.y, cutPoints[i].z - centroid.z);
        f32 u = rel.x * tangentU.x + rel.y * tangentU.y + rel.z * tangentU.z;
        f32 v = rel.x * tangentV.x + rel.y * tangentV.y + rel.z * tangentV.z;
        angles.push_back({std::atan2(v, u), i});
    }
    std::sort(angles.begin(), angles.end());

    // Generate fan triangles from centroid
    u32 base = static_cast<u32>(outVerts.size());

    // Add centroid vertex
    ECS::Vertex centerVert{};
    centerVert.position = centroid;
    centerVert.normal = planeNormal;
    centerVert.uv = Math::Vector2(0.5f, 0.5f);
    centerVert.color = Math::Vector4(0.5f, 0.5f, 0.5f, 1.0f);
    outVerts.push_back(centerVert);

    // Add sorted ring vertices
    for (const auto& [angle, idx] : angles) {
        ECS::Vertex v{};
        v.position = cutPoints[idx];
        v.normal = planeNormal;
        // Planar UV from tangent space
        Math::Vector3 rel(cutPoints[idx].x - centroid.x, cutPoints[idx].y - centroid.y, cutPoints[idx].z - centroid.z);
        v.uv.x = rel.x * tangentU.x + rel.y * tangentU.y + rel.z * tangentU.z;
        v.uv.y = rel.x * tangentV.x + rel.y * tangentV.y + rel.z * tangentV.z;
        v.color = Math::Vector4(0.5f, 0.5f, 0.5f, 1.0f);
        outVerts.push_back(v);
    }

    u32 ringCount = static_cast<u32>(angles.size());
    for (u32 i = 0; i < ringCount; ++i) {
        outIndices.push_back(base);                              // centroid
        outIndices.push_back(base + 1 + i);                     // current
        outIndices.push_back(base + 1 + ((i + 1) % ringCount)); // next
    }
}

void VoronoiMeshFracture::ComputeFragmentProperties(FractureFragment& frag) {
    if (frag.vertices.empty()) {
        frag.centroid = Math::Vector3(0, 0, 0);
        frag.aabbMin = frag.aabbMax = Math::Vector3(0, 0, 0);
        frag.volume = 0.0f;
        return;
    }

    // Compute centroid and AABB
    frag.centroid = Math::Vector3(0, 0, 0);
    frag.aabbMin = frag.vertices[0].position;
    frag.aabbMax = frag.vertices[0].position;

    for (const auto& v : frag.vertices) {
        frag.centroid.x += v.position.x;
        frag.centroid.y += v.position.y;
        frag.centroid.z += v.position.z;

        frag.aabbMin.x = std::min(frag.aabbMin.x, v.position.x);
        frag.aabbMin.y = std::min(frag.aabbMin.y, v.position.y);
        frag.aabbMin.z = std::min(frag.aabbMin.z, v.position.z);
        frag.aabbMax.x = std::max(frag.aabbMax.x, v.position.x);
        frag.aabbMax.y = std::max(frag.aabbMax.y, v.position.y);
        frag.aabbMax.z = std::max(frag.aabbMax.z, v.position.z);
    }

    f32 invN = 1.0f / static_cast<f32>(frag.vertices.size());
    frag.centroid.x *= invN;
    frag.centroid.y *= invN;
    frag.centroid.z *= invN;

    // Approximate volume from AABB
    Math::Vector3 size(frag.aabbMax.x - frag.aabbMin.x,
                       frag.aabbMax.y - frag.aabbMin.y,
                       frag.aabbMax.z - frag.aabbMin.z);
    frag.volume = size.x * size.y * size.z * 0.5f; // ~50% of AABB for convex fragment
}

std::vector<FractureFragment> VoronoiMeshFracture::Fracture(
    const std::vector<ECS::Vertex>& vertices,
    const std::vector<u32>& indices,
    const Config& config) {

    std::vector<FractureFragment> fragments;
    if (vertices.empty() || indices.empty() || config.fragmentCount < 2) return fragments;

    RNG rng(config.randomSeed);

    // Compute mesh AABB
    Math::Vector3 meshMin = vertices[0].position;
    Math::Vector3 meshMax = vertices[0].position;
    for (const auto& v : vertices) {
        meshMin.x = std::min(meshMin.x, v.position.x);
        meshMin.y = std::min(meshMin.y, v.position.y);
        meshMin.z = std::min(meshMin.z, v.position.z);
        meshMax.x = std::max(meshMax.x, v.position.x);
        meshMax.y = std::max(meshMax.y, v.position.y);
        meshMax.z = std::max(meshMax.z, v.position.z);
    }

    Math::Vector3 meshSize(meshMax.x - meshMin.x, meshMax.y - meshMin.y, meshMax.z - meshMin.z);
    Math::Vector3 meshCenter((meshMin.x + meshMax.x) * 0.5f,
                             (meshMin.y + meshMax.y) * 0.5f,
                             (meshMin.z + meshMax.z) * 0.5f);

    // Generate seed points (biased toward impact point)
    std::vector<Math::Vector3> seeds;
    seeds.reserve(config.fragmentCount);

    for (u32 i = 0; i < config.fragmentCount; ++i) {
        Math::Vector3 pt;
        pt.x = rng.Range(meshMin.x, meshMax.x);
        pt.y = rng.Range(meshMin.y, meshMax.y);
        pt.z = rng.Range(meshMin.z, meshMax.z);

        // Bias toward impact point
        if (config.impactBias > 0.0f) {
            f32 bias = config.impactBias;
            pt.x = pt.x * (1.0f - bias) + config.impactPoint.x * bias;
            pt.y = pt.y * (1.0f - bias) + config.impactPoint.y * bias;
            pt.z = pt.z * (1.0f - bias) + config.impactPoint.z * bias;
            // Clamp to AABB
            pt.x = Math::Clamp(pt.x, meshMin.x, meshMax.x);
            pt.y = Math::Clamp(pt.y, meshMin.y, meshMax.y);
            pt.z = Math::Clamp(pt.z, meshMin.z, meshMax.z);
        }

        seeds.push_back(pt);
    }

    // For each seed, clip the source mesh against all bisector planes to produce the Voronoi cell
    for (u32 i = 0; i < config.fragmentCount; ++i) {
        std::vector<ECS::Vertex> currentVerts = vertices;
        std::vector<u32> currentIndices = indices;

        bool empty = false;
        for (u32 j = 0; j < config.fragmentCount; ++j) {
            if (i == j) continue;

            // Bisector plane between seed i and seed j
            // Plane passes through midpoint, normal points from j toward i
            Math::Vector3 mid((seeds[i].x + seeds[j].x) * 0.5f,
                              (seeds[i].y + seeds[j].y) * 0.5f,
                              (seeds[i].z + seeds[j].z) * 0.5f);

            Math::Vector3 normal(seeds[i].x - seeds[j].x,
                                 seeds[i].y - seeds[j].y,
                                 seeds[i].z - seeds[j].z);
            f32 nLen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (nLen < 1e-8f) continue;
            normal.x /= nLen; normal.y /= nLen; normal.z /= nLen;

            // Plane equation: dot(normal, p) + d >= 0 (inside)
            f32 d = -(normal.x * mid.x + normal.y * mid.y + normal.z * mid.z);

            std::vector<ECS::Vertex> clippedVerts;
            std::vector<u32> clippedIndices;

            ClipMeshByPlane(currentVerts, currentIndices, normal, d, clippedVerts, clippedIndices);

            if (clippedVerts.empty() || clippedIndices.empty()) {
                empty = true;
                break;
            }

            // Generate cut face for this clip plane
            GenerateCutFace(clippedVerts, normal, d, clippedVerts, clippedIndices);

            currentVerts = std::move(clippedVerts);
            currentIndices = std::move(clippedIndices);
        }

        if (empty || currentVerts.empty()) continue;

        FractureFragment frag;
        frag.vertices = std::move(currentVerts);
        frag.indices = std::move(currentIndices);
        ComputeFragmentProperties(frag);

        if (frag.volume > 1e-6f) {
            fragments.push_back(std::move(frag));
        }
    }

    return fragments;
}

} // namespace Effects
} // namespace Enjin
