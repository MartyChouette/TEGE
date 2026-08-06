#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// Low-level BLAS (Bottom-Level Acceleration Structure) wrapper
class ENJIN_API BLAS {
public:
    BLAS(VulkanContext* context);
    ~BLAS();

    // Build from vertex+index data. geometryFlags: VK_GEOMETRY_OPAQUE_BIT_KHR etc.
    bool Build(VkCommandBuffer cmd,
               VkDeviceAddress vertexBufferAddress, u32 vertexCount, u32 vertexStride,
               VkDeviceAddress indexBufferAddress, u32 indexCount,
               VkGeometryFlagsKHR geometryFlags = VK_GEOMETRY_OPAQUE_BIT_KHR);

    // Compact after build to save memory (call after build fence completes)
    bool Compact(VkCommandBuffer cmd);

    void Destroy();

    VkAccelerationStructureKHR GetHandle() const { return m_Handle; }
    VkDeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }
    bool IsValid() const { return m_Handle != VK_NULL_HANDLE; }
    bool IsCompacted() const { return m_Compacted; }

private:
    VulkanContext* m_Context = nullptr;
    VkAccelerationStructureKHR m_Handle = VK_NULL_HANDLE;
    VkDeviceAddress m_DeviceAddress = 0;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkBuffer m_ScratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_ScratchMemory = VK_NULL_HANDLE;
    usize m_AllocatedSize = 0;
    bool m_Compacted = false;

    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkBuffer& outBuffer, VkDeviceMemory& outMemory);
    void FreeBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
};

// Top-Level Acceleration Structure wrapper
class ENJIN_API TLAS {
public:
    TLAS(VulkanContext* context);
    ~TLAS();

    // Build or update from instance array.
    // updateOnly=true: faster refit for transform-only changes
    bool Build(VkCommandBuffer cmd,
               const VkAccelerationStructureInstanceKHR* instances, u32 instanceCount,
               bool updateOnly = false);

    void Destroy();

    VkAccelerationStructureKHR GetHandle() const { return m_Handle; }
    bool IsValid() const { return m_Handle != VK_NULL_HANDLE; }

private:
    VulkanContext* m_Context = nullptr;
    VkAccelerationStructureKHR m_Handle = VK_NULL_HANDLE;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    VkBuffer m_ScratchBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_ScratchMemory = VK_NULL_HANDLE;
    VkDeviceSize m_ScratchSize = 0;  // Allocated scratch size (reused when large enough)
    VkBuffer m_InstanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_InstanceMemory = VK_NULL_HANDLE;
    usize m_AllocatedSize = 0;
    u32 m_MaxInstanceCount = 0;  // Allocated capacity

    // Deferred destruction: a full rebuild replaces the TLAS while the previous
    // frame's command buffer may still reference the old one (VUID-02442).
    // Replaced resources are retired here and freed after enough Build() calls
    // (one per frame) have passed for all in-flight frames to complete.
    struct RetiredResources {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        u32 age = 0;
    };
    std::vector<RetiredResources> m_Retired;
    void RetireCurrent();          // Move handle/buffer/memory into m_Retired
    void RetireScratch();          // Move scratch buffer/memory into m_Retired
    void AgeAndFreeRetired();      // Called per Build(); frees entries older than in-flight depth
    void FlushRetired();           // Immediate free (shutdown, after wait-idle)

    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkBuffer& outBuffer, VkDeviceMemory& outMemory);
    void FreeBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
