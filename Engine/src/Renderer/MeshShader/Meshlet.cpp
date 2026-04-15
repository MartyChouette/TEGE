#include "Enjin/Renderer/MeshShader/Meshlet.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <cmath>

namespace Enjin {
namespace Renderer {

MeshletData GenerateMeshlets(
    const void* vertices, u32 vertexCount, u32 vertexStride,
    const u32* indices, u32 indexCount,
    u32 positionOffset)
{
    MeshletData result;
    if (!vertices || !indices || indexCount == 0 || vertexCount == 0) return result;

    u32 triangleCount = indexCount / 3;
    const u8* vertexData = static_cast<const u8*>(vertices);

    // Simple greedy meshlet builder: pack triangles sequentially into meshlets
    // until vertex or primitive limits are reached.
    // A production implementation would use spatial clustering (e.g., meshoptimizer).

    std::vector<u32> meshletVertexMap(vertexCount, UINT32_MAX);
    u32 currentMeshletVertexCount = 0;
    u32 currentMeshletPrimCount = 0;
    u32 meshletVertexStart = static_cast<u32>(result.meshletVertices.size());
    u32 meshletPrimStart = static_cast<u32>(result.meshletPrimitives.size());

    auto flushMeshlet = [&]() {
        if (currentMeshletPrimCount == 0) return;

        Meshlet meshlet;
        meshlet.vertexOffset = meshletVertexStart;
        meshlet.vertexCount = currentMeshletVertexCount;
        meshlet.primitiveOffset = meshletPrimStart;
        meshlet.primitiveCount = currentMeshletPrimCount;

        // Compute bounding sphere from meshlet vertices
        Math::Vector3 center(0.0f);
        for (u32 i = 0; i < currentMeshletVertexCount; ++i) {
            u32 vi = result.meshletVertices[meshletVertexStart + i];
            const f32* pos = reinterpret_cast<const f32*>(vertexData + vi * vertexStride + positionOffset);
            center = center + Math::Vector3(pos[0], pos[1], pos[2]);
        }
        if (currentMeshletVertexCount > 0)
            center = center * (1.0f / static_cast<f32>(currentMeshletVertexCount));

        f32 radius = 0.0f;
        for (u32 i = 0; i < currentMeshletVertexCount; ++i) {
            u32 vi = result.meshletVertices[meshletVertexStart + i];
            const f32* pos = reinterpret_cast<const f32*>(vertexData + vi * vertexStride + positionOffset);
            Math::Vector3 p(pos[0], pos[1], pos[2]);
            f32 dist = (p - center).Length();
            if (dist > radius) radius = dist;
        }

        meshlet.center = center;
        meshlet.radius = radius;
        meshlet.coneAxis = Math::Vector3(0.0f, 1.0f, 0.0f);
        meshlet.coneCutoff = -1.0f; // Disabled for now

        result.meshlets.push_back(meshlet);
        result.meshletCount++;

        // Reset for next meshlet
        for (u32 i = 0; i < currentMeshletVertexCount; ++i) {
            meshletVertexMap[result.meshletVertices[meshletVertexStart + i]] = UINT32_MAX;
        }
        currentMeshletVertexCount = 0;
        currentMeshletPrimCount = 0;
        meshletVertexStart = static_cast<u32>(result.meshletVertices.size());
        meshletPrimStart = static_cast<u32>(result.meshletPrimitives.size());
    };

    for (u32 tri = 0; tri < triangleCount; ++tri) {
        u32 i0 = indices[tri * 3 + 0];
        u32 i1 = indices[tri * 3 + 1];
        u32 i2 = indices[tri * 3 + 2];

        // Count new vertices this triangle would add
        u32 newVerts = 0;
        if (meshletVertexMap[i0] == UINT32_MAX) newVerts++;
        if (meshletVertexMap[i1] == UINT32_MAX) newVerts++;
        if (meshletVertexMap[i2] == UINT32_MAX) newVerts++;

        // Check if adding this triangle would exceed meshlet limits
        if (currentMeshletVertexCount + newVerts > MAX_MESHLET_VERTICES ||
            currentMeshletPrimCount + 1 > MAX_MESHLET_PRIMITIVES) {
            flushMeshlet();
        }

        // Add vertices to meshlet
        auto addVertex = [&](u32 vi) -> u8 {
            if (meshletVertexMap[vi] == UINT32_MAX) {
                meshletVertexMap[vi] = currentMeshletVertexCount;
                result.meshletVertices.push_back(vi);
                currentMeshletVertexCount++;
            }
            return static_cast<u8>(meshletVertexMap[vi]);
        };

        u8 local0 = addVertex(i0);
        u8 local1 = addVertex(i1);
        u8 local2 = addVertex(i2);

        // Pack triangle as 3 local indices
        result.meshletPrimitives.push_back(local0);
        result.meshletPrimitives.push_back(local1);
        result.meshletPrimitives.push_back(local2);
        currentMeshletPrimCount++;
    }

    // Flush last meshlet
    flushMeshlet();

    ENJIN_LOG_INFO(Renderer, "Generated %u meshlets from %u triangles (%u vertices)",
                   result.meshletCount, triangleCount, vertexCount);
    return result;
}

} // namespace Renderer
} // namespace Enjin
