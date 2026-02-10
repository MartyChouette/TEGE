#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Enjin {
namespace Renderer {

// Forward declaration
class VulkanContext;

// Buffer usage flags
enum class BufferUsage {
    Vertex = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    Index = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    Uniform = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    Storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    TransferSrc = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    TransferDst = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    IndirectDraw = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
    AccelerationStructureInput = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
    AccelerationStructureStorage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
    ShaderBindingTable = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
    ShaderDeviceAddress = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
};

// Vulkan buffer wrapper
class ENJIN_API VulkanBuffer {
public:
    VulkanBuffer(VulkanContext* context);
    ~VulkanBuffer();

    bool Create(usize size, BufferUsage usage, bool hostVisible = false);
    bool Create(usize size, VkBufferUsageFlags usageFlags, bool hostVisible = false);
    void Destroy();

    bool UploadData(const void* data, usize size, usize offset = 0);
    void* Map();
    void Unmap();

    VkBuffer GetBuffer() const { return m_Buffer; }
    VkDeviceMemory GetMemory() const { return m_Memory; }
    usize GetSize() const { return m_Size; }

    // Get device address (requires buffer created with ShaderDeviceAddress usage)
    VkDeviceAddress GetDeviceAddress() const;

private:
    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties);
    bool AllocateMemory(VkMemoryPropertyFlags properties);

    VulkanContext* m_Context = nullptr;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    usize m_Size = 0;
    usize m_AllocatedSize = 0;  // Actual GPU allocation (may be > m_Size due to alignment)
    VkBufferUsageFlags m_UsageFlags = 0;
    bool m_HostVisible = false;
    void* m_MappedData = nullptr;
};

} // namespace Renderer
} // namespace Enjin
