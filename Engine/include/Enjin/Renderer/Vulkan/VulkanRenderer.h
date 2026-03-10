#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanSwapchain.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <functional>

/**
 * @file VulkanRenderer.h
 * @brief Main Vulkan rendering interface
 * @author Enjin Engine Team
 * @date 2025
 */

namespace Enjin {
namespace Renderer {

/**
 * @brief Vulkan renderer - main rendering interface
 * 
 * Manages the Vulkan rendering state, swapchain, and command buffers.
 */
class ENJIN_API VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    /**
     * @brief Initialize the renderer
     * @param window Pointer to the window to render to
     * @return true if initialization succeeded, false otherwise
     */
    bool Initialize(Window* window);

    /**
     * @brief Shutdown the renderer
     */
    void Shutdown();

    /**
     * @brief Begin a new frame
     * Acquire next image and begin command buffer recording.
     * Does NOT start the main render pass - call BeginMainRenderPass() for that.
     * @return true if a new frame was started, false otherwise
     */
    bool BeginFrame();

    /**
     * @brief Begin the main render pass
     * Call this after BeginFrame() and any pre-render passes (shadow, etc.)
     */
    void BeginMainRenderPass();

    /**
     * @brief End the current frame
     * End command buffer recording and submit to queue
     */
    void EndFrame();

    bool IsMainRenderPassActive() const { return m_IsMainRenderPassActive; }

    VkCommandBuffer GetCurrentCommandBuffer() const;
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VulkanContext* GetContext() const { return m_Context.get(); }
    VulkanSwapchain* GetSwapchain() const { return m_Swapchain.get(); }
    VkExtent2D GetSwapchainExtent() const { return m_Swapchain ? m_Swapchain->GetExtent() : VkExtent2D{0, 0}; }
    u32 GetCurrentFrameIndex() const { return m_CurrentFrame; }

    void OnWindowResize(u32 width, u32 height);

    // Wait for all in-flight frames to complete (fence-based, graphics queue only).
    // Preferred over vkDeviceWaitIdle() for mid-frame synchronization.
    void WaitForAllFrames();

    bool IsDeviceLost() const { return m_DeviceLost; }

    // Flag set by window resize callback to trigger swapchain recreation
    void SetFramebufferResized(bool resized) { m_FramebufferResized = resized; }

    // Deferred VSync change — safe to call mid-frame, applied at end of EndFrame()
    void RequestVSyncChange(bool enabled);
    bool IsVSyncEnabled() const;

    // Register a callback to be notified after swapchain recreation (e.g., PostProcessing)
    using ResizeCallback = std::function<void(u32, u32)>;
    void AddResizeCallback(ResizeCallback callback) { m_ResizeCallbacks.push_back(std::move(callback)); }

    // Async compute: begin/end compute command buffer, submit to compute queue
    VkCommandBuffer GetCurrentComputeCommandBuffer() const;
    bool BeginComputeCommandBuffer();
    void EndComputeCommandBuffer();
    void SubmitCompute();  // Submit compute queue, signal semaphore
    bool HasAsyncCompute() const { return m_ComputeCommandPool != VK_NULL_HANDLE; }

    // Insert a pipeline barrier in the graphics command buffer that waits for the
    // compute semaphore. Use this for mid-frame synchronization when graphics work
    // depends on compute results (e.g., shadow pass finishes → wait for culling).
    // This is a lighter-weight alternative to waiting at submit time.
    void InsertComputeToGraphicsBarrier(VkCommandBuffer graphicsCmd, VkPipelineStageFlags dstStage);

    // Signal from graphics to compute: allows compute queue to wait until a specific
    // graphics stage completes (e.g., depth pass → compute can read depth for HiZ).
    void SignalGraphicsToCompute(VkCommandBuffer graphicsCmd);
    bool WasComputeSubmittedThisFrame() const { return m_ComputeSubmittedThisFrame; }

    // Variable Rate Shading: set the shading rate image for the next render pass
    void SetShadingRateImage(VkImageView imageView, VkExtent2D tileSize) {
        m_ShadingRateImageView = imageView;
        m_ShadingRateTileSize = tileSize;
    }
    void ClearShadingRateImage() { m_ShadingRateImageView = VK_NULL_HANDLE; }

private:
    bool CreateSurface();
    bool CreateRenderPass();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
    void DestroySyncObjects();
    bool AcquireNextImage();
    void SubmitCommandBuffer();

    Window* m_Window = nullptr;
    std::unique_ptr<VulkanContext> m_Context;
    std::unique_ptr<VulkanSwapchain> m_Swapchain;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;

    u32 m_CurrentFrame = 0;
    u32 m_CurrentImageIndex = 0;
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    bool m_IsFrameStarted = false;
    bool m_IsMainRenderPassActive = false;
    bool m_FramebufferResized = false;
    bool m_DeviceLost = false;
    bool m_VSyncChangeRequested = false;
    bool m_VSyncDesiredState = false;

    std::vector<ResizeCallback> m_ResizeCallbacks;

    // Async compute queue resources
    VkCommandPool m_ComputeCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_ComputeCommandBuffers;
    std::vector<VkSemaphore> m_ComputeFinishedSemaphores;
    std::vector<VkSemaphore> m_GraphicsToComputeSemaphores; // Graphics signals compute can start
    bool m_ComputeRecording = false;
    bool m_ComputeSubmittedThisFrame = false; // True if SubmitCompute was called
    bool m_GraphicsToComputeSignaled = false; // True if SignalGraphicsToCompute was called

    // VRS shading rate image (set by RenderSystem when VRS is active)
    VkImageView m_ShadingRateImageView = VK_NULL_HANDLE;
    VkExtent2D m_ShadingRateTileSize = { 16, 16 };

    bool CreateComputeResources();
    void DestroyComputeResources();
};

} // namespace Renderer
} // namespace Enjin
