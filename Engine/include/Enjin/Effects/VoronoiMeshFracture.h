#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <vector>

namespace Enjin {
namespace Effects {

// Result of a single Voronoi fracture cell
struct FractureFragment {
    std::vector<ECS::Vertex> vertices;
    std::vector<u32> indices;
    Math::Vector3 centroid;          // Center of mass (average of vertices)
    Math::Vector3 aabbMin;
    Math::Vector3 aabbMax;
    f32 volume;                      // Approximate volume for mass calculation
};

// 3D Voronoi mesh fracture using Sutherland-Hodgman clipping
class ENJIN_API VoronoiMeshFracture {
public:
    struct Config {
        u32 fragmentCount = 8;
        f32 impactBias = 0.5f;       // 0 = uniform seeds, 1 = biased toward impact
        Math::Vector3 impactPoint;
        u32 randomSeed = 12345;
    };

    // Fracture a mesh into Voronoi fragments
    // Input: source mesh vertices/indices + config
    // Output: vector of fragments (one per Voronoi cell that intersects the mesh)
    static std::vector<FractureFragment> Fracture(
        const std::vector<ECS::Vertex>& vertices,
        const std::vector<u32>& indices,
        const Config& config);

private:
    // Simple LCG random number generator
    struct RNG {
        u32 state;
        RNG(u32 seed) : state(seed) {}
        u32 Next() { state = state * 1664525u + 1013904223u; return state; }
        f32 Float01() { return static_cast<f32>(Next() & 0xFFFF) / 65536.0f; }
        f32 Range(f32 min, f32 max) { return min + Float01() * (max - min); }
    };

    // Clip a set of triangles against a plane (Sutherland-Hodgman)
    // plane: normal + distance (dot(normal, p) + d > 0 means inside)
    static void ClipMeshByPlane(
        const std::vector<ECS::Vertex>& inVerts,
        const std::vector<u32>& inIndices,
        const Math::Vector3& planeNormal, f32 planeDist,
        std::vector<ECS::Vertex>& outVerts,
        std::vector<u32>& outIndices);

    // Interpolate vertex attributes
    static ECS::Vertex LerpVertex(const ECS::Vertex& a, const ECS::Vertex& b, f32 t);

    // Generate cap face for cut plane
    static void GenerateCutFace(
        const std::vector<ECS::Vertex>& clippedVerts,
        const Math::Vector3& planeNormal,
        f32 planeDist,
        std::vector<ECS::Vertex>& outVerts,
        std::vector<u32>& outIndices);

    // Compute fragment properties (centroid, AABB, volume)
    static void ComputeFragmentProperties(FractureFragment& frag);
};

} // namespace Effects
} // namespace Enjin
