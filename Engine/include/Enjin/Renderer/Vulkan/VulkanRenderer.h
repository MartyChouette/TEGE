#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanSwapchain.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

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
     * Acquire next image and begin command buffer recording
     * @return true if a new frame was started, false otherwise
     */
    bool BeginFrame();

    /**
     * @brief End the current frame
     * End command buffer recording and submit to queue
     */
    void EndFrame();

    VkCommandBuffer GetCurrentCommandBuffer() const;
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VulkanContext* GetContext() const { return m_Context.get(); }
    VulkanSwapchain* GetSwapchain() const { return m_Swapchain.get(); }
    VkExtent2D GetSwapchainExtent() const { return m_Swapchain ? m_Swapchain->GetExtent() : VkExtent2D{0, 0}; }
    u32 GetCurrentFrameIndex() const { return m_CurrentFrame; }

    void OnWindowResize(u32 width, u32 height);

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
};

} // namespace Renderer
} // namespace Enjin
