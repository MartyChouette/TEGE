#include "Enjin/Renderer/GPUDriven/MergedGeometryBuffer.h"
#include "Enjin/Renderer/RayTracing/RTCapabilities.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Renderer {

MergedGeometryBuffer::MergedGeometryBuffer(VulkanContext* context)
    : m_Context(context) {
}

MergedGeometryBuffer::~MergedGeometryBuffer() {
    Shutdown();
}

bool MergedGeometryBuffer::Initialize(u32 maxVertices, u32 maxIndices) {
    m_MaxVertices = maxVertices;
    m_MaxIndices = maxIndices;

    // Create vertex buffer (device-local with host-visible for uploads)
    usize vertexBufferSize = static_cast<usize>(maxVertices) * VERTEX_STRIDE;
    m_VertexBuffer = std::make_unique<VulkanBuffer>(m_Context);
    // STORAGE bit: DDGI voxelization reads this pool as a readonly SSBO.
    VkBufferUsageFlags vertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    // Add RT usage flags when ray tracing is supported (needed for BLAS building)
    if (m_Context->IsRayTracingSupported()) {
        vertexUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    if (!m_VertexBuffer->Create(vertexBufferSize, vertexUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "MergedGeometryBuffer: Failed to create vertex buffer (%u vertices, %zu bytes)",
                        maxVertices, vertexBufferSize);
        return false;
    }

    // Create index buffer
    usize indexBufferSize = static_cast<usize>(maxIndices) * sizeof(u32);
    m_IndexBuffer = std::make_unique<VulkanBuffer>(m_Context);
    VkBufferUsageFlags indexUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;   // DDGI voxelization SSBO
    if (m_Context->IsRayTracingSupported()) {
        indexUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }
    if (!m_IndexBuffer->Create(indexBufferSize, indexUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "MergedGeometryBuffer: Failed to create index buffer (%u indices, %zu bytes)",
                        maxIndices, indexBufferSize);
        m_VertexBuffer.reset();
        return false;
    }

    // Initialize free lists with one block covering the entire range
    m_VertexFreeList.push_back({ 0, maxVertices });
    m_IndexFreeList.push_back({ 0, maxIndices });
    m_UsedVertices = 0;
    m_UsedIndices = 0;

    ENJIN_LOG_INFO(Renderer, "MergedGeometryBuffer initialized: %u vertices (%.1f MB), %u indices (%.1f MB)",
                   maxVertices, vertexBufferSize / (1024.0f * 1024.0f),
                   maxIndices, indexBufferSize / (1024.0f * 1024.0f));
    return true;
}

void MergedGeometryBuffer::Shutdown() {
    m_VertexBuffer.reset();
    m_IndexBuffer.reset();
    m_VertexFreeList.clear();
    m_IndexFreeList.clear();
    m_UsedVertices = 0;
    m_UsedIndices = 0;
}

u32 MergedGeometryBuffer::Allocate(std::vector<FreeBlock>& freeList, u32 count) {
    // First-fit search
    for (auto it = freeList.begin(); it != freeList.end(); ++it) {
        if (it->count >= count) {
            u32 offset = it->offset;
            if (it->count == count) {
                freeList.erase(it);
            } else {
                it->offset += count;
                it->count -= count;
            }
            return offset;
        }
    }
    return UINT32_MAX; // Allocation failed
}

void MergedGeometryBuffer::Deallocate(std::vector<FreeBlock>& freeList, u32 offset, u32 count) {
    if (count == 0) return;

    // Find insertion point (keep sorted by offset)
    auto insertPos = std::lower_bound(freeList.begin(), freeList.end(), offset,
        [](const FreeBlock& block, u32 off) { return block.offset < off; });

    // Insert new free block
    auto newIt = freeList.insert(insertPos, { offset, count });

    // Coalesce with next block
    auto nextIt = newIt + 1;
    if (nextIt != freeList.end() && newIt->offset + newIt->count == nextIt->offset) {
        newIt->count += nextIt->count;
        freeList.erase(nextIt);
    }

    // Coalesce with previous block
    if (newIt != freeList.begin()) {
        auto prevIt = newIt - 1;
        if (prevIt->offset + prevIt->count == newIt->offset) {
            prevIt->count += newIt->count;
            freeList.erase(newIt);
        }
    }
}

MeshAllocation MergedGeometryBuffer::Upload(const void* vertexData, u32 vertexCount,
                                             const u32* indexData, u32 indexCount) {
    MeshAllocation alloc;

    if (!m_VertexBuffer || !m_IndexBuffer) return alloc;
    if (vertexCount == 0 || indexCount == 0) return alloc;

    // Allocate vertex space
    u32 vOffset = Allocate(m_VertexFreeList, vertexCount);
    if (vOffset == UINT32_MAX) {
        ENJIN_LOG_WARN(Renderer, "MergedGeometryBuffer: vertex space exhausted (%u/%u used, requested %u)",
                       m_UsedVertices, m_MaxVertices, vertexCount);
        return alloc;
    }

    // Allocate index space
    u32 iOffset = Allocate(m_IndexFreeList, indexCount);
    if (iOffset == UINT32_MAX) {
        // Roll back vertex allocation
        Deallocate(m_VertexFreeList, vOffset, vertexCount);
        ENJIN_LOG_WARN(Renderer, "MergedGeometryBuffer: index space exhausted (%u/%u used, requested %u)",
                       m_UsedIndices, m_MaxIndices, indexCount);
        return alloc;
    }

    // Upload vertex data at offset
    usize vertexByteOffset = static_cast<usize>(vOffset) * VERTEX_STRIDE;
    usize vertexByteSize = static_cast<usize>(vertexCount) * VERTEX_STRIDE;
    if (!m_VertexBuffer->UploadData(vertexData, vertexByteSize, vertexByteOffset)) {
        Deallocate(m_VertexFreeList, vOffset, vertexCount);
        Deallocate(m_IndexFreeList, iOffset, indexCount);
        ENJIN_LOG_ERROR(Renderer, "MergedGeometryBuffer: vertex upload failed");
        return alloc;
    }

    // Upload index data at offset
    usize indexByteOffset = static_cast<usize>(iOffset) * sizeof(u32);
    usize indexByteSize = static_cast<usize>(indexCount) * sizeof(u32);
    if (!m_IndexBuffer->UploadData(indexData, indexByteSize, indexByteOffset)) {
        Deallocate(m_VertexFreeList, vOffset, vertexCount);
        Deallocate(m_IndexFreeList, iOffset, indexCount);
        ENJIN_LOG_ERROR(Renderer, "MergedGeometryBuffer: index upload failed");
        return alloc;
    }

    m_UsedVertices += vertexCount;
    m_UsedIndices += indexCount;

    alloc.vertexOffset = vOffset;
    alloc.vertexCount = vertexCount;
    alloc.indexOffset = iOffset;
    alloc.indexCount = indexCount;
    alloc.valid = true;
    return alloc;
}

void MergedGeometryBuffer::Free(const MeshAllocation& alloc) {
    if (!alloc.valid) return;

    Deallocate(m_VertexFreeList, alloc.vertexOffset, alloc.vertexCount);
    Deallocate(m_IndexFreeList, alloc.indexOffset, alloc.indexCount);

    m_UsedVertices -= alloc.vertexCount;
    m_UsedIndices -= alloc.indexCount;
}

void MergedGeometryBuffer::Clear() {
    m_VertexFreeList.clear();
    m_IndexFreeList.clear();
    if (m_MaxVertices > 0) m_VertexFreeList.push_back({ 0, m_MaxVertices });
    if (m_MaxIndices > 0) m_IndexFreeList.push_back({ 0, m_MaxIndices });
    m_UsedVertices = 0;
    m_UsedIndices = 0;
}

void MergedGeometryBuffer::BindBuffers(VkCommandBuffer cmd) const {
    if (!m_VertexBuffer || !m_IndexBuffer) return;

    VkBuffer vertexBuffers[] = { m_VertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

} // namespace Renderer
} // namespace Enjin
