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
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Enjin Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Enjin Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<u32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef ENJIN_BUILD_DEBUG
    if (CheckValidationLayerSupport()) {
        createInfo.enabledLayerCount = static_cast<u32>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
    } else {
        ENJIN_LOG_WARN(Renderer, "Validation layers requested but not available");
    }
#else
    createInfo.enabledLayerCount = 0;
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

    // Log all available devices
    ENJIN_LOG_INFO(Renderer, "Found %u Vulkan device(s):", deviceCount);
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        const char* typeStr = "Unknown";
        switch (props.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   typeStr = "Discrete GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeStr = "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    typeStr = "Virtual GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:            typeStr = "CPU"; break;
            default: break;
        }
        ENJIN_LOG_INFO(Renderer, "  - %s (%s)", props.deviceName, typeStr);
    }

    // Score devices: prefer discrete GPUs over integrated
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = -1;

    for (const auto& device : devices) {
        if (!IsDeviceSuitable(device)) continue;

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        int score = 0;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 10000;
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
        }

        // Bonus for ray tracing support
        RTCapabilities rtCaps = RTCapabilities::Query(device);
        if (rtCaps.supported) {
            score += 1000;
        }

        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "No suitable Vulkan device found");
        return false;
    }

    m_PhysicalDevice = bestDevice;
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(bestDevice, &properties);
    ENJIN_LOG_INFO(Renderer, "Selected physical device: %s", properties.deviceName);

    // Cache memory properties (avoids repeated vkGetPhysicalDeviceMemoryProperties calls)
    vkGetPhysicalDeviceMemoryProperties(bestDevice, &m_MemoryProperties);

    // Query ray tracing capabilities
    m_RTCapabilities = RTCapabilities::Query(bestDevice);

    return true;
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
    // This will need to be updated when we integrate with windowing system
    m_PresentQueueFamily = m_GraphicsQueueFamily;

    // Search for dedicated compute queue family (COMPUTE_BIT && !GRAPHICS_BIT)
    m_ComputeQueueFamily = m_GraphicsQueueFamily; // Fallback to graphics family
    for (u32 i = 0; i < queueFamilyCount; ++i) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            m_ComputeQueueFamily = i;
            ENJIN_LOG_INFO(Renderer, "Found dedicated compute queue family: %u", i);
            break;
        }
    }
    if (m_ComputeQueueFamily == m_GraphicsQueueFamily) {
        ENJIN_LOG_INFO(Renderer, "No dedicated compute queue, using graphics queue family for compute");
    }

    // Create device queues
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    VkDeviceQueueCreateInfo graphicsQueueInfo{};
    graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphicsQueueInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    graphicsQueueInfo.queueCount = 1;
    graphicsQueueInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(graphicsQueueInfo);

    // Add separate compute queue create info if dedicated
    if (m_ComputeQueueFamily != m_GraphicsQueueFamily) {
        VkDeviceQueueCreateInfo computeQueueInfo{};
        computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        computeQueueInfo.queueFamilyIndex = m_ComputeQueueFamily;
        computeQueueInfo.queueCount = 1;
        computeQueueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(computeQueueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.fillModeNonSolid = VK_TRUE;

    // Enable wide lines for debug rendering if supported
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &supportedFeatures);
    if (supportedFeatures.wideLines) {
        deviceFeatures.wideLines = VK_TRUE;
    }

    // Enable multi-draw indirect for GPU-driven rendering pipeline
    if (supportedFeatures.multiDrawIndirect) {
        deviceFeatures.multiDrawIndirect = VK_TRUE;
    }
    if (supportedFeatures.drawIndirectFirstInstance) {
        deviceFeatures.drawIndirectFirstInstance = VK_TRUE;
    }

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Add ray tracing extensions if supported
    if (m_RTCapabilities.supported) {
        const auto& rtExts = RTCapabilities::GetRequiredExtensions();
        deviceExtensions.insert(deviceExtensions.end(), rtExts.begin(), rtExts.end());
        ENJIN_LOG_INFO(Renderer, "Enabling %zu ray tracing device extensions", rtExts.size());
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());

    // Build feature chain for RT support
    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bdaFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;

    if (m_RTCapabilities.supported) {
        // Chain: deviceFeatures2 -> bdaFeatures -> asFeatures -> rtPipelineFeatures
        deviceFeatures2.pNext = &bdaFeatures;
        bdaFeatures.pNext = &asFeatures;
        asFeatures.pNext = &rtPipelineFeatures;

        createInfo.pNext = &deviceFeatures2;
        createInfo.pEnabledFeatures = nullptr;  // Use pNext chain instead
    } else {
        createInfo.pEnabledFeatures = &deviceFeatures;
    }

    createInfo.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

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
    // Get compute queue (may be same as graphics if no dedicated compute family)
    vkGetDeviceQueue(m_Device, m_ComputeQueueFamily, 0, &m_ComputeQueue);
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
    // Use cached memory properties instead of querying the driver each time
    for (u32 i = 0; i < m_MemoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (m_MemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
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
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

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
