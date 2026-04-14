#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

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
    VkBuffer m_InstanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_InstanceMemory = VK_NULL_HANDLE;
    usize m_AllocatedSize = 0;
    u32 m_MaxInstanceCount = 0;  // Allocated capacity

    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkBuffer& outBuffer, VkDeviceMemory& outMemory);
    void FreeBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
