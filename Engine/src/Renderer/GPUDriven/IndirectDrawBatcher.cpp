#include "Enjin/Renderer/GPUDriven/IndirectDrawBatcher.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>

namespace Enjin {
namespace Renderer {

IndirectDrawBatcher::IndirectDrawBatcher() = default;

IndirectDrawBatcher::~IndirectDrawBatcher() {
    Shutdown();
}

bool IndirectDrawBatcher::Initialize(VulkanContext* ctx, u32 maxObjects) {
    if (!ctx) return false;
    m_Context = ctx;
    m_MaxObjects = maxObjects;

    // Pre-allocate CPU-side storage
    m_PendingEntities.reserve(maxObjects);
    m_Commands.reserve(maxObjects);
    m_Batches.reserve(256); // Typical scene has far fewer unique texture sets

    // Create GPU buffer for indirect draw commands.
    // Sized for maxObjects * sizeof(VkDrawIndexedIndirectCommand).
    // Host-visible for simplicity (CPU writes, GPU reads indirectly).
    m_IndirectBuffer = std::make_unique<VulkanBuffer>(ctx);
    usize bufferSize = static_cast<usize>(maxObjects) * sizeof(VkDrawIndexedIndirectCommand);
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (!m_IndirectBuffer->Create(bufferSize, usage, true /* hostVisible */)) {
        ENJIN_LOG_WARN(Renderer, "IndirectDrawBatcher: Failed to create indirect buffer (%zu bytes)", bufferSize);
        m_IndirectBuffer.reset();
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "IndirectDrawBatcher initialized (max %u objects, %zu KB buffer)",
                   maxObjects, bufferSize / 1024);
    return true;
}

void IndirectDrawBatcher::Shutdown() {
    m_IndirectBuffer.reset();
    m_PendingEntities.clear();
    m_Batches.clear();
    m_Commands.clear();
    m_Context = nullptr;
}

void IndirectDrawBatcher::Reset() {
    m_PendingEntities.clear();
    m_Batches.clear();
    m_Commands.clear();
    m_TotalCommands = 0;
}

void IndirectDrawBatcher::AddEntity(u32 entity, u64 textureHash,
                                     u32 indexCount, u32 indexOffset, u32 vertexOffset,
                                     u32 objectDataIndex) {
    if (m_PendingEntities.size() >= m_MaxObjects) return;

    PendingEntity pe;
    pe.entity = entity;
    pe.textureHash = textureHash;
    pe.indexCount = indexCount;
    pe.indexOffset = indexOffset;
    pe.vertexOffset = vertexOffset;
    pe.objectDataIndex = objectDataIndex;
    m_PendingEntities.push_back(pe);
}

bool IndirectDrawBatcher::BuildBatches() {
    m_Batches.clear();
    m_Commands.clear();
    m_TotalCommands = 0;

    if (m_PendingEntities.empty()) return false;

    // Sort pending entities by texture hash to group them contiguously.
    // This is O(n log n) but n is typically small (hundreds, not millions).
    std::sort(m_PendingEntities.begin(), m_PendingEntities.end(),
        [](const PendingEntity& a, const PendingEntity& b) {
            return a.textureHash < b.textureHash;
        });

    // Build contiguous command ranges per texture group
    m_Commands.reserve(m_PendingEntities.size());
    u64 currentHash = m_PendingEntities[0].textureHash;
    u32 batchStart = 0;

    for (usize i = 0; i < m_PendingEntities.size(); ++i) {
        const PendingEntity& pe = m_PendingEntities[i];

        // Start a new batch when texture hash changes
        if (pe.textureHash != currentHash) {
            TextureDrawBatch batch;
            batch.textureHash = currentHash;
            batch.firstCommand = batchStart;
            batch.commandCount = static_cast<u32>(m_Commands.size()) - batchStart;
            batch.representativeEntity = m_PendingEntities[batchStart].entity;
            m_Batches.push_back(batch);

            batchStart = static_cast<u32>(m_Commands.size());
            currentHash = pe.textureHash;
        }

        // Emit indirect draw command for this entity
        VkDrawIndexedIndirectCommand cmd{};
        cmd.indexCount = pe.indexCount;
        cmd.instanceCount = 1;
        cmd.firstIndex = pe.indexOffset;
        cmd.vertexOffset = static_cast<i32>(pe.vertexOffset);
        cmd.firstInstance = pe.objectDataIndex; // Encodes SSBO index via gl_InstanceIndex
        m_Commands.push_back(cmd);
    }

    // Final batch
    if (batchStart < static_cast<u32>(m_Commands.size())) {
        TextureDrawBatch batch;
        batch.textureHash = currentHash;
        batch.firstCommand = batchStart;
        batch.commandCount = static_cast<u32>(m_Commands.size()) - batchStart;
        batch.representativeEntity = m_PendingEntities[batchStart].entity;
        m_Batches.push_back(batch);
    }

    m_TotalCommands = static_cast<u32>(m_Commands.size());
    return !m_Batches.empty();
}

bool IndirectDrawBatcher::UploadCommands() {
    if (!m_IndirectBuffer || m_Commands.empty()) return false;

    usize uploadSize = m_Commands.size() * sizeof(VkDrawIndexedIndirectCommand);
    if (uploadSize > m_IndirectBuffer->GetSize()) {
        ENJIN_LOG_WARN(Renderer, "IndirectDrawBatcher: Command buffer overflow (%zu > %zu bytes)",
                       uploadSize, m_IndirectBuffer->GetSize());
        return false;
    }

    return m_IndirectBuffer->UploadData(m_Commands.data(), uploadSize);
}

VkBuffer IndirectDrawBatcher::GetIndirectBuffer() const {
    if (!m_IndirectBuffer) return VK_NULL_HANDLE;
    return m_IndirectBuffer->GetBuffer();
}

} // namespace Renderer
} // namespace Enjin
