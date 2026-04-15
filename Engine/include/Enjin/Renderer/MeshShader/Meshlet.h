#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace Renderer {

// Maximum vertices and primitives per meshlet (hardware-friendly limits)
static constexpr u32 MAX_MESHLET_VERTICES = 64;
static constexpr u32 MAX_MESHLET_PRIMITIVES = 124; // 124 triangles (3 indices each)

// A meshlet represents a small cluster of triangles from a mesh.
// Mesh shaders process one meshlet per workgroup, enabling per-meshlet
// frustum/backface culling before any vertex processing.
struct Meshlet {
    u32 vertexOffset;      // Offset into meshlet vertex index buffer
    u32 vertexCount;       // Number of unique vertices in this meshlet
    u32 primitiveOffset;   // Offset into meshlet primitive index buffer
    u32 primitiveCount;    // Number of triangles

    // Bounding sphere for frustum culling
    Math::Vector3 center;
    f32 radius;

    // Cone for backface culling (normal cone apex + cutoff)
    Math::Vector3 coneAxis;
    f32 coneCutoff;        // cos(cone_half_angle), -1 = disabled
};

// Meshlet data for a single mesh — generated at load time from vertex/index data
struct MeshletData {
    std::vector<Meshlet> meshlets;
    std::vector<u32> meshletVertices;     // Vertex indices (into original vertex buffer)
    std::vector<u8> meshletPrimitives;    // Packed triangle indices (3 bytes per triangle, local to meshlet)
    u32 meshletCount = 0;

    bool IsValid() const { return meshletCount > 0; }
};

// Generate meshlets from a triangle mesh.
// Splits the mesh into small clusters of MAX_MESHLET_VERTICES vertices and
// MAX_MESHLET_PRIMITIVES triangles each. Computes bounding spheres and
// normal cones for per-meshlet culling.
ENJIN_API MeshletData GenerateMeshlets(
    const void* vertices, u32 vertexCount, u32 vertexStride,
    const u32* indices, u32 indexCount,
    u32 positionOffset = 0    // Byte offset of position (vec3) within vertex struct
);

} // namespace Renderer
} // namespace Enjin
