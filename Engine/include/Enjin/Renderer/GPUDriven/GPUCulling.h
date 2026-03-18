#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Memory/Memory.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <cfloat>

namespace Enjin {
namespace Renderer {

class HiZPyramid; // Forward declaration

// Bounding box for GPU culling
struct ENJIN_API BoundingBox {
    Math::Vector3 min;
    Math::Vector3 max;
    
    BoundingBox() : min(Math::Vector3(1e30f)), max(Math::Vector3(-1e30f)) {}
    BoundingBox(const Math::Vector3& min, const Math::Vector3& max) : min(min), max(max) {}
    
    Math::Vector3 GetCenter() const {
        return (min + max) * 0.5f;
    }
    
    Math::Vector3 GetExtent() const {
        return max - min;
    }
};

// Object data for GPU culling (layout matches GLSL std430)
struct CullableObject {
    // Bounds as vec4 for GPU alignment (xyz = value, w = padding)
    Math::Vector4 boundsMin;   // offset 0,  16 bytes
    Math::Vector4 boundsMax;   // offset 16, 16 bytes
    Math::Matrix4 transform;   // offset 32, 64 bytes
    u32 meshIndex = 0;         // offset 96
    u32 materialIndex = 0;     // offset 100
    u32 indexCount = 0;        // offset 104
    u32 indexOffset = 0;       // offset 108
    u32 vertexOffset = 0;      // offset 112
    u32 indirectEligible = 0;  // offset 116 — 1 = emit indirect draw command, 0 = visibility only
    u32 _pad1 = 0;             // offset 120
    u32 _pad2 = 0;             // offset 124

    // Helper to set bounds from BoundingBox
    void SetBounds(const BoundingBox& box) {
        boundsMin = Math::Vector4(box.min.x, box.min.y, box.min.z, 0.0f);
        boundsMax = Math::Vector4(box.max.x, box.max.y, box.max.z, 0.0f);
    }
};

// Size of ObjectDataGPU (defined in RenderSystem.h) in bytes.
// Kept as a constant here to avoid a circular include (RenderSystem.h includes GPUCulling.h).
// Must stay in sync with the static_assert in RenderSystem.h.
static constexpr usize OBJECT_DATA_GPU_SIZE = 192;

// GPU frustum culling system
// INNOVATION: Move culling to GPU, reducing CPU overhead
class VulkanBuffer; // Forward declaration

class ENJIN_API GPUCullingSystem {
public:
    GPUCullingSystem(VulkanContext* context);
    ~GPUCullingSystem();

    bool Initialize();
    void Shutdown();

    // Submit objects for culling
    void SubmitObjects(const std::vector<CullableObject>& objects);
    
    // Execute culling on GPU
    // Returns indirect draw commands for visible objects
    bool ExecuteCulling(
        const Math::Matrix4& viewMatrix,
        const Math::Matrix4& projectionMatrix,
        VkCommandBuffer commandBuffer,
        VkBuffer& outIndirectDrawBuffer,
        u32& outDrawCount
    );

    // Get culling statistics
    struct CullingStats {
        u32 totalObjects = 0;
        u32 visibleObjects = 0;
        u32 culledObjects = 0;
    };
    CullingStats GetStats() const { return m_Stats; }

    // Check if object at given index is visible (after ExecuteCulling)
    // Returns true if index is out of range or culling hasn't run yet
    bool IsVisible(u32 objectIndex) const {
        if (objectIndex >= m_CachedVisibility.size()) return true;
        return m_CachedVisibility[objectIndex] != 0;
    }

    // Get the compacted indirect draw buffer (only visible objects, tightly packed)
    VkBuffer GetIndirectDrawBuffer() const;
    // Get the draw count buffer (atomic counter written by GPU)
    VkBuffer GetDrawCountBuffer() const;
    // Get the ObjectData SSBO (per-object material/transform for indirect draws)
    VkBuffer GetObjectDataBuffer() const;
    u32 GetMaxObjects() const { return m_MaxObjects; }

    // Upload per-object data to the ObjectData SSBO
    bool UploadObjectData(const void* data, usize sizeBytes);

    // Set the Hi-Z pyramid for occlusion culling (optional — falls back to frustum-only)
    void SetHiZPyramid(HiZPyramid* hiz) { m_HiZPyramid = hiz; }
    bool HasHiZ() const { return m_HiZPyramid != nullptr; }

    // Two-phase Hi-Z occlusion culling:
    // Phase 0: Frustum + HiZ cull using previous frame's depth pyramid.
    //          Definitely-visible objects are drawn immediately; potentially-occluded flagged.
    // Between phases: Partial HiZ pyramid regenerated from phase 0 visible objects.
    // Phase 1: Re-test flagged objects against updated HiZ. Newly-visible appended.
    // Returns true if two-phase was executed (HiZ available + pipeline valid).
    bool ExecuteTwoPhase(
        const Math::Matrix4& viewMatrix,
        const Math::Matrix4& projectionMatrix,
        VkCommandBuffer commandBuffer,
        VkBuffer& outIndirectDrawBuffer,
        u32& outDrawCount
    );

    // Get the occlusion flag buffer (used between phases for HiZ partial regeneration)
    VkBuffer GetOcclusionFlagBuffer() const;

private:
    bool CreateComputePipeline();
    bool CreateBuffers();
    void UpdateFrustumPlanes(const Math::Matrix4& viewProj);
    void UpdateDescriptorSet(VkCommandBuffer commandBuffer);
    void UpdateDescriptorSetTwoPhase(VkCommandBuffer commandBuffer);

    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    u32 m_ObjectCount = 0;

    VulkanContext* m_Context = nullptr;

    // Compute pipeline for culling (single-phase / phase 0+1 with push constant)
    VkPipeline m_CullPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;

    // Two-phase Hi-Z pipeline (uses same shader with push constant phase selector)
    VkPipeline m_CullHiZPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_HiZPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_HiZDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_HiZDescriptorSet = VK_NULL_HANDLE;

    // Buffers
    std::unique_ptr<VulkanBuffer> m_ObjectBuffer;      // Input: Objects to cull
    std::unique_ptr<VulkanBuffer> m_IndirectDrawBuffer; // Output: Indirect draw commands (compacted)
    std::unique_ptr<VulkanBuffer> m_FrustumBuffer;     // Frustum planes
    std::unique_ptr<VulkanBuffer> m_VisibilityBuffer;   // Per-object visibility
    std::unique_ptr<VulkanBuffer> m_DrawCountBuffer;    // Atomic draw count (4 bytes)
    std::unique_ptr<VulkanBuffer> m_ObjectDataBuffer;   // Per-object material/transform SSBO

    CullingStats m_Stats;
    u32 m_MaxObjects = 100000; // Support up to 100k objects
    std::vector<u32> m_CachedVisibility; // Per-object visibility from last ExecuteCulling
    HiZPyramid* m_HiZPyramid = nullptr; // Optional Hi-Z pyramid for occlusion culling

    // Two-phase buffers
    std::unique_ptr<VulkanBuffer> m_OcclusionFlagBuffer;  // Per-object occlusion flags (phase 0 → phase 1)
    bool m_HiZPipelineCreated = false;
    bool CreateHiZComputePipeline();
};

} // namespace Renderer
} // namespace Enjin
