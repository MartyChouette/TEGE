#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <atomic>

namespace Enjin {
namespace Renderer {

// Vulkan context - manages Vulkan instance, device, and queues
class ENJIN_API VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool Initialize();
    void Shutdown();

    VkInstance GetInstance() const { return m_Instance; }
    VkDevice GetDevice() const { return m_Device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue GetPresentQueue() const { return m_PresentQueue; }
    VkQueue GetComputeQueue() const { return m_ComputeQueue; }
    u32 GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    u32 GetPresentQueueFamily() const { return m_PresentQueueFamily; }
    u32 GetComputeQueueFamily() const { return m_ComputeQueueFamily; }
    bool HasDedicatedComputeQueue() const { return m_ComputeQueueFamily != m_GraphicsQueueFamily; }

    // Find present queue family for a surface
    u32 FindPresentQueueFamily(VkSurfaceKHR surface) const;
    void SetPresentQueueFamily(u32 queueFamily);
    
    // Find memory type for allocation
    u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;

    // GPU memory tracking
    void TrackAllocation(usize bytes) { m_TotalGPUAllocatedBytes.fetch_add(bytes, std::memory_order_relaxed); }
    void TrackDeallocation(usize bytes) { m_TotalGPUAllocatedBytes.fetch_sub(bytes, std::memory_order_relaxed); }
    usize GetTotalGPUAllocatedBytes() const { return m_TotalGPUAllocatedBytes.load(std::memory_order_relaxed); }

protected:
    friend class VulkanRenderer;
    bool CreateInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateQueues();

    std::vector<const char*> GetRequiredExtensions() const;
    bool CheckValidationLayerSupport() const;
    bool IsDeviceSuitable(VkPhysicalDevice device) const;

    VkInstance m_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    VkQueue m_ComputeQueue = VK_NULL_HANDLE;
    u32 m_GraphicsQueueFamily = UINT32_MAX;
    u32 m_PresentQueueFamily = UINT32_MAX;
    u32 m_ComputeQueueFamily = UINT32_MAX;

    std::atomic<usize> m_TotalGPUAllocatedBytes{0};

#ifdef ENJIN_BUILD_DEBUG
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    bool CreateDebugMessenger();
    void DestroyDebugMessenger();
#endif
};

} // namespace Renderer
} // namespace Enjin
