#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <memory>
#include <mutex>

namespace Enjin {
namespace Renderer {

// Result of allocating geometry into the merged buffer
struct MeshAllocation {
    u32 vertexOffset = 0;   // First vertex in the merged vertex buffer
    u32 vertexCount = 0;    // Number of vertices
    u32 indexOffset = 0;    // First index in the merged index buffer
    u32 indexCount = 0;     // Number of indices
    bool valid = false;     // Whether this allocation is valid
};

// Single large vertex + index buffer for all static 3D meshes.
// Uses a first-fit free-list sub-allocator with coalescing on free.
// Indirect draw commands reference offsets into this buffer pair.
class ENJIN_API MergedGeometryBuffer {
public:
    MergedGeometryBuffer(VulkanContext* context);
    ~MergedGeometryBuffer();

    // Initialize with capacity (element counts, not bytes)
    // Default: 1M vertices, 2M indices
    bool Initialize(u32 maxVertices = 1024 * 1024, u32 maxIndices = 2 * 1024 * 1024);
    void Shutdown();

    // Upload mesh data and return an allocation with offsets.
    // vertexData: pointer to MeshComponent::Vertex array
    // vertexCount: number of vertices
    // indexData: pointer to u32 index array
    // indexCount: number of indices
    MeshAllocation Upload(const void* vertexData, u32 vertexCount,
                          const u32* indexData, u32 indexCount);

    // Return a region to the free list
    void Free(const MeshAllocation& alloc);

    // Reset all allocations (free lists restored to full capacity, GPU buffers kept)
    void Clear();

    // Bind the merged vertex + index buffers to a command buffer
    void BindBuffers(VkCommandBuffer cmd) const;

    // Query
    VulkanBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
    VulkanBuffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }
    u32 GetMaxVertices() const { return m_MaxVertices; }
    u32 GetMaxIndices() const { return m_MaxIndices; }
    u32 GetUsedVertices() const { return m_UsedVertices; }
    u32 GetUsedIndices() const { return m_UsedIndices; }

    // Vertex stride — MUST equal sizeof(MeshComponent::Vertex). The pool packs
    // vertices at this stride and the pipeline reads them back at the same stride,
    // so any mismatch scatters every vertex after the first into garbage.
    // Guarded by a static_assert in RenderSystem.cpp where the Vertex type is visible.
    // 136 = pos(12)+normal(12)+uv(8)+color(16)+tangent(16)+boneWeights(16)+
    //       boneIndices(16)+uv1(8)+boneWeights2(16)+boneIndices2(16).
    static constexpr u32 VERTEX_STRIDE = 136;

private:
    // Free-list block (contiguous range of elements)
    struct FreeBlock {
        u32 offset;
        u32 count;
    };

    // First-fit allocator for either vertex or index space
    u32 Allocate(std::vector<FreeBlock>& freeList, u32 count);
    void Deallocate(std::vector<FreeBlock>& freeList, u32 offset, u32 count);

    VulkanContext* m_Context = nullptr;
    std::unique_ptr<VulkanBuffer> m_VertexBuffer;
    std::unique_ptr<VulkanBuffer> m_IndexBuffer;

    u32 m_MaxVertices = 0;
    u32 m_MaxIndices = 0;
    u32 m_UsedVertices = 0;
    u32 m_UsedIndices = 0;

    std::vector<FreeBlock> m_VertexFreeList;
    std::vector<FreeBlock> m_IndexFreeList;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
