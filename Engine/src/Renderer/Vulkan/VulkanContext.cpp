#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <set>
#include <string>
#include <cstring>
#include <GLFW/glfw3.h>

namespace Enjin {
namespace Renderer {

#ifdef ENJIN_BUILD_DEBUG
const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};
#else
const std::vector<const char*> VALIDATION_LAYERS = {};
#endif

VulkanContext::VulkanContext() {
}

VulkanContext::~VulkanContext() {
    Shutdown();
}

bool VulkanContext::Initialize() {
    ENJIN_LOG_INFO(Renderer, "Initializing Vulkan context...");

    if (!CreateInstance()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create Vulkan instance");
        return false;
    }

#ifdef ENJIN_BUILD_DEBUG
    if (!CreateDebugMessenger()) {
        ENJIN_LOG_WARN(Renderer, "Failed to create debug messenger");
    }
#endif

    if (!SelectPhysicalDevice()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to select physical device");
        return false;
    }

    if (!CreateLogicalDevice()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create logical device");
        return false;
    }

    if (!CreateQueues()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create queues");
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Vulkan context initialized successfully");
    return true;
}

void VulkanContext::Shutdown() {
    if (m_Device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_Device);
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }

#ifdef ENJIN_BUILD_DEBUG
    DestroyDebugMessenger();
#endif

    if (m_Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

bool VulkanContext::CreateInstance() {
    // C++20 Designated Initializer — clean Vulkan application info
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "Enjin Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Enjin Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    auto extensions = GetRequiredExtensions();

    VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<u32>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

#ifdef ENJIN_BUILD_DEBUG
    if (CheckValidationLayerSupport()) {
        createInfo.enabledLayerCount = static_cast<u32>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    } else {
        ENJIN_LOG_WARN(Renderer, "Validation layers requested but not available");
    }
#endif

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create Vulkan instance: %d", result);
        return false;
    }

    return true;
}

bool VulkanContext::SelectPhysicalDevice() {
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        ENJIN_LOG_ERROR(Renderer, "No Vulkan-compatible devices found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (IsDeviceSuitable(device)) {
            m_PhysicalDevice = device;

            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(device, &properties);
            ENJIN_LOG_INFO(Renderer, "Selected physical device: %s", properties.deviceName);

            return true;
        }
    }

    ENJIN_LOG_ERROR(Renderer, "No suitable Vulkan device found");
    return false;
}

bool VulkanContext::CreateLogicalDevice() {
    // Find queue families
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    // Find graphics queue family
    for (u32 i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_GraphicsQueueFamily = i;
            break;
        }
    }

    if (m_GraphicsQueueFamily == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "No graphics queue family found");
        return false;
    }

    // For now, assume present queue is same as graphics queue
    m_PresentQueueFamily = m_GraphicsQueueFamily;

    // Create device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = m_GraphicsQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.fillModeNonSolid = VK_TRUE;

    // Enable wide lines for debug rendering if supported
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &supportedFeatures);
    if (supportedFeatures.wideLines) {
        deviceFeatures.wideLines = VK_TRUE;
    }

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<u32>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures
    };

#ifdef ENJIN_BUILD_DEBUG
    if (CheckValidationLayerSupport()) {
        createInfo.enabledLayerCount = static_cast<u32>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    }
#endif

    VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create logical device: %d", result);
        return false;
    }

    return true;
}

bool VulkanContext::CreateQueues() {
    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    // Present queue will be set when we have a surface
    m_PresentQueueFamily = m_GraphicsQueueFamily;
    m_PresentQueue = m_GraphicsQueue;
    return true;
}

u32 VulkanContext::FindPresentQueueFamily(VkSurfaceKHR surface) const {
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    for (u32 i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            return i;
        }
    }

    return UINT32_MAX;
}

void VulkanContext::SetPresentQueueFamily(u32 queueFamily) {
    m_PresentQueueFamily = queueFamily;
    vkGetDeviceQueue(m_Device, queueFamily, 0, &m_PresentQueue);
}

u32 VulkanContext::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

    for (u32 i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    ENJIN_LOG_ERROR(Renderer, "Failed to find suitable memory type");
    return UINT32_MAX;
}

std::vector<const char*> VulkanContext::GetRequiredExtensions() const {
    u32 glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef ENJIN_BUILD_DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    return extensions;
}

bool VulkanContext::CheckValidationLayerSupport() const {
    u32 layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : VALIDATION_LAYERS) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }
    }

    return true;
}

bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice device) const {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &properties);
    vkGetPhysicalDeviceFeatures(device, &features);

    // Check for required extensions
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    if (!requiredExtensions.empty()) {
        return false; // Missing required extensions
    }

    // Prefer discrete GPU
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return true;
    }

    // Fallback to integrated GPU
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        return true;
    }

    return false;
}

#ifdef ENJIN_BUILD_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    (void)pUserData;
    (void)messageType;

    // Route to appropriate log level based on severity
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ENJIN_LOG_ERROR(Renderer, "[Vulkan Validation] %s", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        ENJIN_LOG_WARN(Renderer, "[Vulkan Validation] %s", pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        ENJIN_LOG_DEBUG(Renderer, "[Vulkan Validation] %s", pCallbackData->pMessage);
    }
    // Verbose messages are ignored to reduce noise

    return VK_FALSE;
}

bool VulkanContext::CreateDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback,
        .pUserData = nullptr
    };

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        m_Instance, "vkCreateDebugUtilsMessengerEXT");

    if (func != nullptr) {
        return func(m_Instance, &createInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS;
    }

    return false;
}

void VulkanContext::DestroyDebugMessenger() {
    if (m_DebugMessenger != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_Instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_Instance, m_DebugMessenger, nullptr);
        }
        m_DebugMessenger = VK_NULL_HANDLE;
    }
}
#endif

} // namespace Renderer
} // namespace Enjin
