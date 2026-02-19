#include "Enjin/Renderer/Vulkan/VulkanSwapchain.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <algorithm>

namespace Enjin {
namespace Renderer {

VulkanSwapchain::VulkanSwapchain(VulkanContext* context)
    : m_Context(context) {
}

VulkanSwapchain::~VulkanSwapchain() {
    Shutdown();
}

bool VulkanSwapchain::Initialize(VkSurfaceKHR surface, u32 width, u32 height) {
    m_Surface = surface;
    if (!CreateSwapchain(surface, width, height) || !CreateImageViews()) {
        return false;
    }
    if (!CreateDepthResources()) {
        return false;
    }
    return true;
}

void VulkanSwapchain::Shutdown() {
    DestroyFramebuffers();
    DestroyDepthResources();
    DestroyImageViews();

    if (m_Swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_Context->GetDevice(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

bool VulkanSwapchain::Recreate(u32 width, u32 height, bool gpuAlreadyIdle) {
    if (!gpuAlreadyIdle) {
        vkDeviceWaitIdle(m_Context->GetDevice());
    }

    DestroyFramebuffers();
    DestroyDepthResources();
    DestroyImageViews();

    // Destroy old swapchain before creating new one to prevent resource leak
    if (m_Swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_Context->GetDevice(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }

    if (!CreateSwapchain(m_Surface, width, height) || !CreateImageViews()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate swapchain");
        return false;
    }

    if (!CreateDepthResources()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate depth resources");
        return false;
    }

    if (m_RenderPass != VK_NULL_HANDLE) {
        RecreateFramebuffers();
    }
    return true;
}

SwapchainSupportDetails VulkanSwapchain::QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // Prefer SRGB with 32-bit color
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // Fallback to first available
    return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // Log all available present modes for diagnostics (only on first call)
    static bool loggedModes = false;
    if (!loggedModes) {
        for (const auto& mode : availablePresentModes) {
            const char* name = "UNKNOWN";
            switch (mode) {
                case VK_PRESENT_MODE_IMMEDIATE_KHR:    name = "IMMEDIATE"; break;
                case VK_PRESENT_MODE_MAILBOX_KHR:      name = "MAILBOX"; break;
                case VK_PRESENT_MODE_FIFO_KHR:         name = "FIFO"; break;
                case VK_PRESENT_MODE_FIFO_RELAXED_KHR: name = "FIFO_RELAXED"; break;
                default: break;
            }
            ENJIN_LOG_INFO(Renderer, "  Available present mode: %s", name);
        }
        loggedModes = true;
    }

    // If VSync is enabled, use FIFO (guaranteed VSync)
    if (m_VSyncEnabled) {
        ENJIN_LOG_INFO(Renderer, "Selected present mode: FIFO (VSync ON)");
        m_CurrentPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // VSync disabled: prefer mailbox (triple buffering) for low latency without tearing
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            ENJIN_LOG_INFO(Renderer, "Selected present mode: MAILBOX (VSync OFF)");
            m_CurrentPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            return mode;
        }
    }

    // Fallback to IMMEDIATE if MAILBOX not available
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            ENJIN_LOG_INFO(Renderer, "Selected present mode: IMMEDIATE (VSync OFF, MAILBOX unavailable)");
            m_CurrentPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            return mode;
        }
    }

    // Last resort: FIFO (guaranteed to be available)
    ENJIN_LOG_WARN(Renderer, "Selected present mode: FIFO (VSync fallback — no other modes available)");
    m_CurrentPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D extent;
    extent.width = std::clamp(width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return extent;
}

bool VulkanSwapchain::CreateSwapchain(VkSurfaceKHR surface, u32 width, u32 height) {
    SwapchainSupportDetails support = QuerySwapchainSupport(
        m_Context->GetPhysicalDevice(), surface);

    if (support.formats.empty() || support.presentModes.empty()) {
        ENJIN_LOG_ERROR(Renderer, "Swapchain not supported");
        return false;
    }

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(support.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(support.presentModes);
    VkExtent2D extent = ChooseSwapExtent(support.capabilities, width, height);

    u32 imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    u32 queueFamilyIndices[] = {
        m_Context->GetGraphicsQueueFamily(),
        m_Context->GetPresentQueueFamily()
    };

    if (m_Context->GetGraphicsQueueFamily() != m_Context->GetPresentQueueFamily()) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_Context->GetDevice(), &createInfo, nullptr, &m_Swapchain);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create swapchain: %d", result);
        return false;
    }

    result = vkGetSwapchainImagesKHR(m_Context->GetDevice(), m_Swapchain, &imageCount, nullptr);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to get swapchain image count: %d", result);
        return false;
    }
    m_Images.resize(imageCount);
    result = vkGetSwapchainImagesKHR(m_Context->GetDevice(), m_Swapchain, &imageCount, m_Images.data());
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to get swapchain images: %d", result);
        return false;
    }

    m_ImageFormat = surfaceFormat.format;
    m_Extent = extent;

    ENJIN_LOG_INFO(Renderer, "Swapchain created: %dx%d, format: %d, images: %u",
        extent.width, extent.height, m_ImageFormat, imageCount);

    return true;
}

bool VulkanSwapchain::CreateImageViews() {
    m_ImageViews.resize(m_Images.size());

    for (usize i = 0; i < m_Images.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_Images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_ImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(m_Context->GetDevice(), &createInfo, nullptr, &m_ImageViews[i]);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create image view %zu: %d", i, result);
            return false;
        }
    }

    return true;
}

void VulkanSwapchain::DestroyImageViews() {
    for (auto& imageView : m_ImageViews) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_Context->GetDevice(), imageView, nullptr);
        }
    }
    m_ImageViews.clear();
}

void VulkanSwapchain::RecreateFramebuffers() {
    if (m_RenderPass == VK_NULL_HANDLE) {
        return;
    }

    DestroyFramebuffers();
    m_Framebuffers.resize(m_ImageViews.size());

    std::vector<VkImageView> attachments(2);
    attachments[1] = m_DepthImageView; // Depth attachment is same for all framebuffers

    for (usize i = 0; i < m_ImageViews.size(); ++i) {
        attachments[0] = m_ImageViews[i]; // Color attachment
        
        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = m_RenderPass;
        createInfo.attachmentCount = static_cast<u32>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = m_Extent.width;
        createInfo.height = m_Extent.height;
        createInfo.layers = 1;

        VkResult result = vkCreateFramebuffer(m_Context->GetDevice(), &createInfo, nullptr, &m_Framebuffers[i]);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create framebuffer %zu: %d", i, result);
        }
    }
}

void VulkanSwapchain::DestroyFramebuffers() {
    for (auto& framebuffer : m_Framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_Context->GetDevice(), framebuffer, nullptr);
        }
    }
    m_Framebuffers.clear();
}

VkFormat VulkanSwapchain::FindDepthFormat() {
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_Context->GetPhysicalDevice(), format, &props);
        
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    // Fallback
    return VK_FORMAT_D32_SFLOAT;
}

bool VulkanSwapchain::CreateDepthResources() {
    m_DepthFormat = FindDepthFormat();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_DepthFormat;
    imageInfo.extent.width = m_Extent.width;
    imageInfo.extent.height = m_Extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(m_Context->GetDevice(), &imageInfo, nullptr, &m_DepthImage);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create depth image: %d", result);
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_Context->GetDevice(), m_DepthImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "Failed to find suitable memory type for depth buffer");
        vkDestroyImage(m_Context->GetDevice(), m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
        return false;
    }

    result = vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_DepthImageMemory);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate depth image memory: %d", result);
        vkDestroyImage(m_Context->GetDevice(), m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
        return false;
    }

    result = vkBindImageMemory(m_Context->GetDevice(), m_DepthImage, m_DepthImageMemory, 0);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind depth image memory: %d", result);
        vkFreeMemory(m_Context->GetDevice(), m_DepthImageMemory, nullptr);
        vkDestroyImage(m_Context->GetDevice(), m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
        m_DepthImageMemory = VK_NULL_HANDLE;
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_DepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_DepthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_DepthImageView);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create depth image view: %d", result);
        vkFreeMemory(m_Context->GetDevice(), m_DepthImageMemory, nullptr);
        vkDestroyImage(m_Context->GetDevice(), m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
        m_DepthImageMemory = VK_NULL_HANDLE;
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Depth buffer created: format %d, %dx%d", m_DepthFormat, m_Extent.width, m_Extent.height);
    return true;
}

void VulkanSwapchain::DestroyDepthResources() {
    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Context->GetDevice(), m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
    }

    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_Context->GetDevice(), m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }

    if (m_DepthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Context->GetDevice(), m_DepthImageMemory, nullptr);
        m_DepthImageMemory = VK_NULL_HANDLE;
    }
}

void VulkanSwapchain::SetVSyncEnabled(bool enabled) {
    if (m_VSyncEnabled == enabled) return;

    m_VSyncEnabled = enabled;
    ENJIN_LOG_INFO(Renderer, "VSync %s, recreating swapchain...", enabled ? "enabled" : "disabled");

    // Recreate swapchain with new present mode
    if (!Recreate(m_Extent.width, m_Extent.height)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate swapchain for VSync change");
    }
}

} // namespace Renderer
} // namespace Enjin
