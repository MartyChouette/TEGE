#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/RayTracing/RTCapabilities.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <atomic>
#include <functional>

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

    // Fence-based GPU synchronization (preferred over vkDeviceWaitIdle for mid-frame use).
    // When a VulkanRenderer is active, waits on in-flight fences (fast, graphics-queue-only).
    // Falls back to vkDeviceWaitIdle when no renderer is registered (init/shutdown).
    void WaitForGPU() {
        if (m_FenceWaitFn) m_FenceWaitFn();
        else if (m_Device != VK_NULL_HANDLE) vkDeviceWaitIdle(m_Device);
    }

    // Called by VulkanRenderer to register its fence-based wait
    void SetFenceWaitFunction(std::function<void()> fn) { m_FenceWaitFn = std::move(fn); }

    // Ray tracing capabilities
    const RTCapabilities& GetRTCapabilities() const { return m_RTCapabilities; }
    bool IsRayTracingSupported() const { return m_RTCapabilities.supported; }

    // GPU timestamp query support
    f32 GetTimestampPeriod() const { return m_TimestampPeriod; }
    u32 GetTimestampValidBits() const { return m_TimestampValidBits; }

    // Variable Rate Shading support (VK_KHR_fragment_shading_rate)
    bool IsVRSSupported() const { return m_VRSSupported; }

    // Device Generated Commands support (VK_EXT or VK_NV)
    bool IsDGCSupported() const { return m_DGCExtSupported || m_DGCNVSupported; }
    bool IsDGCExtSupported() const { return m_DGCExtSupported; }
    bool IsDGCNVSupported() const { return m_DGCNVSupported; }

    // Query the maximum MSAA sample count supported by both color and depth framebuffers
    VkSampleCountFlagBits GetMaxUsableSampleCount() const;

    // GPU memory queries (uses VK_EXT_memory_budget when available)
    u64 GetGPUMemoryUsed() const;
    u64 GetGPUMemoryBudget() const;

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

    // Fence-based wait function registered by VulkanRenderer
    std::function<void()> m_FenceWaitFn;

    // Cached physical device memory properties (populated during SelectPhysicalDevice)
    VkPhysicalDeviceMemoryProperties m_MemoryProperties{};

    // Ray tracing capabilities (populated during SelectPhysicalDevice)
    RTCapabilities m_RTCapabilities;

    // GPU timestamp query support (populated during SelectPhysicalDevice)
    f32 m_TimestampPeriod = 0.0f;
    u32 m_TimestampValidBits = 0;

    // Variable Rate Shading support (populated during SelectPhysicalDevice)
    bool m_VRSSupported = false;

    // Device Generated Commands support (populated during SelectPhysicalDevice)
    bool m_DGCExtSupported = false;  // VK_EXT_device_generated_commands
    bool m_DGCNVSupported = false;   // VK_NV_device_generated_commands (fallback)
    bool m_Maintenance5Supported = false;  // VK_KHR_maintenance5 (hard dependency of DGC EXT)

    // Mesh shader support (populated during SelectPhysicalDevice)
    bool m_MeshShaderSupported = false;
public:
    bool IsMeshShaderSupported() const { return m_MeshShaderSupported; }
private:

#ifdef ENJIN_BUILD_DEBUG
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    bool CreateDebugMessenger();
    void DestroyDebugMessenger();
#endif
};

} // namespace Renderer
} // namespace Enjin
