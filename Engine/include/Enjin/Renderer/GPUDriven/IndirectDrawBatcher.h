#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUDriven/GPUCulling.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

/**
 * @file IndirectDrawBatcher.h
 * @brief Groups visible entities by texture set for batched indirect draws.
 *
 * After GPU culling produces a visibility list, this system groups visible
 * entities by their texture set hash (from MaterialComponent::cachedSortKey).
 * Each group shares the same descriptor set state (same textures bound),
 * enabling a single vkCmdDrawIndexedIndirectCount per texture group instead
 * of one draw call per entity.
 *
 * Flow:
 *   1. GPU culling produces per-object visibility (m_CachedVisibility)
 *   2. IndirectDrawBatcher iterates visible entities, groups by texture hash
 *   3. Each group's indirect draw commands are placed contiguously in the buffer
 *   4. RenderSystem issues one indirect draw per group, binding textures once per group
 *
 * Non-textured entities continue to use the existing single-batch indirect draw.
 * This system handles the textured entities that were previously drawn per-entity.
 */

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

/// A batch of entities sharing the same texture set, ready for indirect draw.
struct TextureDrawBatch {
    u64 textureHash = 0;             // Texture set hash (from sort key bits 16-55)
    u32 firstCommand = 0;            // Offset into the indirect draw buffer (in commands)
    u32 commandCount = 0;            // Number of indirect draw commands in this batch
    u32 representativeEntity = 0;    // Any entity from this group (for descriptor binding)
};

/**
 * @brief CPU-side grouping of visible textured entities into indirect draw batches.
 *
 * This is the CPU-side counterpart to GPU culling's indirect draw emission.
 * While GPU culling emits indirect commands for non-textured pool entities,
 * this batcher handles textured pool entities that need per-group descriptor binding.
 */
class ENJIN_API IndirectDrawBatcher {
public:
    IndirectDrawBatcher();
    ~IndirectDrawBatcher();

    /// Initialize GPU buffers for batched indirect draws.
    /// @param ctx Vulkan context
    /// @param maxObjects Maximum number of objects to batch
    bool Initialize(VulkanContext* ctx, u32 maxObjects);

    /// Shut down and release GPU resources.
    void Shutdown();

    /// Clear all batches for a new frame.
    void Reset();

    /// Add a visible textured entity to the batcher.
    /// @param entity Entity ID
    /// @param textureHash Texture set hash from the material sort key
    /// @param indexCount Number of indices for this mesh
    /// @param indexOffset Offset into the merged index buffer
    /// @param vertexOffset Offset into the merged vertex buffer
    /// @param objectDataIndex Index into the ObjectData SSBO (for firstInstance)
    void AddEntity(u32 entity, u64 textureHash,
                   u32 indexCount, u32 indexOffset, u32 vertexOffset,
                   u32 objectDataIndex);

    /// Build the indirect draw command buffer from accumulated entities.
    /// Groups entities by texture hash and writes contiguous command ranges.
    /// @return true if any batches were created
    bool BuildBatches();

    /// Upload the indirect draw commands to the GPU buffer.
    bool UploadCommands();

    /// Get the list of texture draw batches (sorted by texture hash).
    const std::vector<TextureDrawBatch>& GetBatches() const { return m_Batches; }

    /// Get the indirect draw buffer for vkCmdDrawIndexedIndirect.
    VkBuffer GetIndirectBuffer() const;

    /// Get the total number of draw commands across all batches.
    u32 GetTotalCommandCount() const { return m_TotalCommands; }

    /// Whether any textured batches exist this frame.
    bool HasBatches() const { return !m_Batches.empty(); }

private:
    VulkanContext* m_Context = nullptr;

    /// Per-entity data accumulated before batching
    struct PendingEntity {
        u32 entity;
        u64 textureHash;
        u32 indexCount;
        u32 indexOffset;
        u32 vertexOffset;
        u32 objectDataIndex;
    };
    std::vector<PendingEntity> m_PendingEntities;

    /// Built batches (sorted by texture hash)
    std::vector<TextureDrawBatch> m_Batches;

    /// CPU-side indirect draw commands (uploaded to GPU after BuildBatches)
    std::vector<VkDrawIndexedIndirectCommand> m_Commands;

    /// GPU buffer for indirect draw commands
    std::unique_ptr<VulkanBuffer> m_IndirectBuffer;

    u32 m_MaxObjects = 0;
    u32 m_TotalCommands = 0;
};

} // namespace Renderer
} // namespace Enjin
