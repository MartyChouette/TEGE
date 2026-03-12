#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Enjin {
namespace Renderer {

// Swapchain support details
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Vulkan swapchain - manages swapchain images and presentation
class ENJIN_API VulkanSwapchain {
public:
    VulkanSwapchain(VulkanContext* context);
    ~VulkanSwapchain();

    bool Initialize(VkSurfaceKHR surface, u32 width, u32 height);
    void Shutdown();

    bool Recreate(u32 width, u32 height, bool gpuAlreadyIdle = false);

    VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
    VkFormat GetImageFormat() const { return m_ImageFormat; }
    VkExtent2D GetExtent() const { return m_Extent; }
    u32 GetImageCount() const { return static_cast<u32>(m_Images.size()); }
    const std::vector<VkImage>& GetImages() const { return m_Images; }
    const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }
    const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_Framebuffers; }
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    VkFormat GetDepthFormat() const { return m_DepthFormat; }

    // Velocity buffer for TAA / temporal upscaling (RG16F per-pixel motion vectors)
    VkImageView GetVelocityImageView() const { return m_VelocityImageView; }
    static constexpr VkFormat VELOCITY_FORMAT = VK_FORMAT_R16G16_SFLOAT;

    VkFramebuffer GetFramebuffer(u32 index) const {
        return index < m_Framebuffers.size() ? m_Framebuffers[index] : VK_NULL_HANDLE;
    }

    void SetRenderPass(VkRenderPass renderPass) { m_RenderPass = renderPass; }
    void RecreateFramebuffers();

    // VSync control
    void SetVSyncEnabled(bool enabled);
    bool IsVSyncEnabled() const { return m_VSyncEnabled; }
    VkPresentModeKHR GetCurrentPresentMode() const { return m_CurrentPresentMode; }

private:
    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height);
    bool CreateSwapchain(VkSurfaceKHR surface, u32 width, u32 height);
    bool CreateImageViews();
    void DestroyImageViews();
    bool CreateDepthResources();
    void DestroyDepthResources();
    bool CreateVelocityResources();
    void DestroyVelocityResources();
    VkFormat FindDepthFormat();
    void DestroyFramebuffers();

    VulkanContext* m_Context = nullptr;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_Extent{};
    std::vector<VkImage> m_Images;
    std::vector<VkImageView> m_ImageViews;
    std::vector<VkFramebuffer> m_Framebuffers;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    
    // Depth buffer resources
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;
    VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;

    // Velocity buffer resources (RG16F for TAA motion vectors)
    VkImage m_VelocityImage = VK_NULL_HANDLE;
    VkDeviceMemory m_VelocityImageMemory = VK_NULL_HANDLE;
    VkImageView m_VelocityImageView = VK_NULL_HANDLE;

    // VSync state
    bool m_VSyncEnabled = false;
    VkPresentModeKHR m_CurrentPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
};

} // namespace Renderer
} // namespace Enjin
