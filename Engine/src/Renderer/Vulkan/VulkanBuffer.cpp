#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

VulkanBuffer::VulkanBuffer(VulkanContext* context)
    : m_Context(context) {
}

VulkanBuffer::~VulkanBuffer() {
    Destroy();
}

bool VulkanBuffer::Create(usize size, BufferUsage usage, bool hostVisible) {
    return Create(size, static_cast<VkBufferUsageFlags>(usage), hostVisible);
}

bool VulkanBuffer::Create(usize size, VkBufferUsageFlags usageFlags, bool hostVisible) {
    if (!m_Context || m_Context->GetDevice() == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "Cannot create buffer: null context or device");
        return false;
    }
    if (size == 0) {
        ENJIN_LOG_ERROR(Renderer, "Cannot create zero-size buffer");
        return false;
    }

    m_Size = size;
    m_UsageFlags = usageFlags;
    m_HostVisible = hostVisible;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(m_Context->GetDevice(), &bufferInfo, nullptr, &m_Buffer);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create buffer: %d", result);
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_Buffer, &memRequirements);

    VkMemoryPropertyFlags properties = hostVisible
        ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (!AllocateMemory(properties)) {
        vkDestroyBuffer(m_Context->GetDevice(), m_Buffer, nullptr);
        m_Buffer = VK_NULL_HANDLE;
        m_Size = 0;
        return false;
    }

    result = vkBindBufferMemory(m_Context->GetDevice(), m_Buffer, m_Memory, 0);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind buffer memory: %d", result);
        vkFreeMemory(m_Context->GetDevice(), m_Memory, nullptr);
        m_Memory = VK_NULL_HANDLE;
        vkDestroyBuffer(m_Context->GetDevice(), m_Buffer, nullptr);
        m_Buffer = VK_NULL_HANDLE;
        m_Size = 0;
        m_AllocatedSize = 0;
        return false;
    }

    // Persistently map host-visible + host-coherent buffers to avoid per-upload kernel transitions
    if (m_HostVisible) {
        VkResult mapResult = vkMapMemory(m_Context->GetDevice(), m_Memory, 0, m_Size, 0, &m_PersistentMapped);
        if (mapResult != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "Failed to persistently map buffer, will use per-upload mapping");
            m_PersistentMapped = nullptr;
        }
    }

    return true;
}

void VulkanBuffer::Destroy() {
    if (!m_Context || m_Context->GetDevice() == VK_NULL_HANDLE) {
        // Can't issue Vulkan calls without a valid device — just reset state
        m_PersistentMapped = nullptr;
        m_MappedData = nullptr;
        m_Buffer = VK_NULL_HANDLE;
        m_Memory = VK_NULL_HANDLE;
        m_Size = 0;
        m_AllocatedSize = 0;
        return;
    }

    // Unmap persistent mapping
    if (m_PersistentMapped) {
        vkUnmapMemory(m_Context->GetDevice(), m_Memory);
        m_PersistentMapped = nullptr;
    }
    if (m_MappedData) {
        Unmap();
    }

    if (m_Buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_Context->GetDevice(), m_Buffer, nullptr);
        m_Buffer = VK_NULL_HANDLE;
    }

    if (m_Memory != VK_NULL_HANDLE) {
        if (m_AllocatedSize > 0) {
            m_Context->TrackDeallocation(m_AllocatedSize);
        }
        vkFreeMemory(m_Context->GetDevice(), m_Memory, nullptr);
        m_Memory = VK_NULL_HANDLE;
    }

    m_Size = 0;
    m_AllocatedSize = 0;
}

bool VulkanBuffer::UploadData(const void* data, usize size, usize offset) {
    if (!data) {
        ENJIN_LOG_ERROR(Renderer, "UploadData called with null data pointer");
        return false;
    }
    if (offset > m_Size || size > m_Size - offset) {
        ENJIN_LOG_ERROR(Renderer, "Data size + offset exceeds buffer size");
        return false;
    }

    if (m_HostVisible) {
        // Use persistently mapped pointer if available (avoids vkMapMemory/vkUnmapMemory per upload)
        if (m_PersistentMapped) {
            std::memcpy(static_cast<u8*>(m_PersistentMapped) + offset, data, size);
            return true;
        }
        // Fallback to map/unmap cycle
        void* mapped = Map();
        if (!mapped) {
            return false;
        }
        std::memcpy(static_cast<u8*>(mapped) + offset, data, size);
        Unmap();
        return true;
    }

    // For device-local buffers, we'd need a staging buffer
    // For now, we'll require host-visible buffers for simplicity
    ENJIN_LOG_WARN(Renderer, "UploadData called on device-local buffer - use host-visible buffer");
    return false;
}

void* VulkanBuffer::Map() {
    if (!m_HostVisible) {
        ENJIN_LOG_ERROR(Renderer, "Cannot map device-local buffer");
        return nullptr;
    }

    // Return persistent mapping if active (avoids double vkMapMemory)
    if (m_PersistentMapped) {
        return m_PersistentMapped;
    }

    if (m_MappedData) {
        return m_MappedData;
    }

    VkResult result = vkMapMemory(m_Context->GetDevice(), m_Memory, 0, m_Size, 0, &m_MappedData);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map buffer memory: %d", result);
        return nullptr;
    }

    return m_MappedData;
}

void VulkanBuffer::Unmap() {
    if (m_MappedData) {
        vkUnmapMemory(m_Context->GetDevice(), m_Memory);
        m_MappedData = nullptr;
    }
}

u32 VulkanBuffer::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) {
    // Delegate to VulkanContext's cached memory properties
    return m_Context->FindMemoryType(typeFilter, properties);
}

VkDeviceAddress VulkanBuffer::GetDeviceAddress() const {
    if (m_Buffer == VK_NULL_HANDLE) return 0;

    // Buffer must have been created with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    if (!(m_UsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)) return 0;

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = m_Buffer;

    auto fn = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(
        m_Context->GetDevice(), "vkGetBufferDeviceAddressKHR");
    if (!fn) return 0;
    return fn(m_Context->GetDevice(), &addrInfo);
}

bool VulkanBuffer::AllocateMemory(VkMemoryPropertyFlags properties) {
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_Buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

    // Enable device address if buffer was created with that usage
    if (m_UsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        allocInfo.pNext = &allocFlagsInfo;
    }

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        return false;
    }

    VkResult result = vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_Memory);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate buffer memory: %d", result);
        return false;
    }

    m_AllocatedSize = static_cast<usize>(memRequirements.size);
    if (m_Context && m_AllocatedSize > 0) {
        m_Context->TrackAllocation(m_AllocatedSize);
    }

    return true;
}

} // namespace Renderer
} // namespace Enjin
