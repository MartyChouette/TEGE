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
    u32 _pad0 = 0;             // offset 116 (pad to 16-byte boundary)
    u32 _pad1 = 0;             // offset 120
    u32 _pad2 = 0;             // offset 124

    // Helper to set bounds from BoundingBox
    void SetBounds(const BoundingBox& box) {
        boundsMin = Math::Vector4(box.min.x, box.min.y, box.min.z, 0.0f);
        boundsMax = Math::Vector4(box.max.x, box.max.y, box.max.z, 0.0f);
    }
};

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

private:
    bool CreateComputePipeline();
    bool CreateBuffers();
    void UpdateFrustumPlanes(const Math::Matrix4& viewProj);
    void UpdateDescriptorSet(VkCommandBuffer commandBuffer);

    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    u32 m_ObjectCount = 0;

    VulkanContext* m_Context = nullptr;
    
    // Compute pipeline for culling
    VkPipeline m_CullPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    
    // Buffers
    std::unique_ptr<VulkanBuffer> m_ObjectBuffer;      // Input: Objects to cull
    std::unique_ptr<VulkanBuffer> m_IndirectDrawBuffer; // Output: Indirect draw commands
    std::unique_ptr<VulkanBuffer> m_FrustumBuffer;     // Frustum planes
    std::unique_ptr<VulkanBuffer> m_VisibilityBuffer;   // Per-object visibility
    
    CullingStats m_Stats;
    u32 m_MaxObjects = 100000; // Support up to 100k objects
};

} // namespace Renderer
} // namespace Enjin
