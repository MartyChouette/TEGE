#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <set>
#include <string>
#include <cstring>
#include <GLFW/glfw3.h>
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#endif

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

    // Check if Vulkan is available before attempting to create an instance.
    // glfwVulkanSupported() tries to load the Vulkan loader (vulkan-1.dll / libvulkan.so).
    // If it fails, any vkCreateInstance call would crash or return an error.
    if (!glfwVulkanSupported()) {
        ENJIN_LOG_FATAL(Renderer, "Vulkan is not available on this system");
#ifdef _WIN32
        MessageBoxA(nullptr,
            "Vulkan is not available on this system.\n\n"
            "This usually means your GPU drivers are outdated or your graphics card does not support Vulkan.\n\n"
            "Please update your GPU drivers and try again.\n\n"
            "Check 'enjin.log' for details.",
            "TEGE - Startup Error",
            MB_OK | MB_ICONERROR);
#endif
        return false;
    }

    if (!CreateInstance()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create Vulkan instance");
#ifdef _WIN32
        MessageBoxA(nullptr,
            "Failed to initialize Vulkan.\n\n"
            "Your GPU drivers may be outdated or your system may not fully support Vulkan 1.3.\n\n"
            "Please update your GPU drivers and try again.\n\n"
            "Check 'enjin.log' for details.",
            "TEGE - Startup Error",
            MB_OK | MB_ICONERROR);
#endif
        return false;
    }

#ifdef ENJIN_BUILD_DEBUG
    if (!CreateDebugMessenger()) {
        ENJIN_LOG_WARN(Renderer, "Failed to create debug messenger");
    }
#endif

    if (!SelectPhysicalDevice()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to select physical device");
#ifdef _WIN32
        MessageBoxA(nullptr,
            "No compatible GPU found.\n\n"
            "TEGE requires a Vulkan-capable graphics card with swapchain support.\n\n"
            "Please ensure your GPU drivers are up to date.\n\n"
            "Check 'enjin.log' for details.",
            "TEGE - Startup Error",
            MB_OK | MB_ICONERROR);
#endif
        return false;
    }

    if (!CreateLogicalDevice()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create logical device");
#ifdef _WIN32
        MessageBoxA(nullptr,
            "Failed to create a Vulkan device.\n\n"
            "Your GPU may not support the required Vulkan features.\n\n"
            "Please update your GPU drivers and try again.\n\n"
            "Check 'enjin.log' for details.",
            "TEGE - Startup Error",
            MB_OK | MB_ICONERROR);
#endif
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
        VkResult waitResult = vkDeviceWaitIdle(m_Device);
        if (waitResult != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "vkDeviceWaitIdle failed during shutdown: %d", waitResult);
        }
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
    VkResult enumResult = vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    if (enumResult != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to enumerate physical devices: %d", enumResult);
        return false;
    }

    if (deviceCount == 0) {
        ENJIN_LOG_ERROR(Renderer, "No Vulkan-compatible devices found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    enumResult = vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());
    if (enumResult != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to enumerate physical devices (fetch): %d", enumResult);
        return false;
    }

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
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            score += 1;   // software rasterizer: strictly last resort
        }

        // Bonus for ray tracing support. Not granted to CPU rasterizers: lavapipe
        // advertises the RT extensions, and the bonus would rank it above a real
        // integrated GPU (CPU ray tracing is unusably slow).
        if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) {
            RTCapabilities rtCaps = RTCapabilities::Query(device);
            if (rtCaps.supported) {
                score += 1000;
            }
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

    // Cache timestamp query support from device limits
    m_TimestampPeriod = properties.limits.timestampPeriod;
    m_TimestampValidBits = properties.limits.timestampComputeAndGraphics ? 64 : 0;
    if (m_TimestampPeriod > 0.0f) {
        ENJIN_LOG_INFO(Renderer, "GPU timestamp period: %.2f ns (valid bits: %u)", m_TimestampPeriod, m_TimestampValidBits);
    } else {
        ENJIN_LOG_WARN(Renderer, "GPU does not support timestamp queries (period=0)");
    }

    // Cache memory properties (avoids repeated vkGetPhysicalDeviceMemoryProperties calls)
    vkGetPhysicalDeviceMemoryProperties(bestDevice, &m_MemoryProperties);

    // Query ray tracing capabilities
    m_RTCapabilities = RTCapabilities::Query(bestDevice);

    // Probe device extension support for optional features (VRS, DGC)
    {
        u32 extCount = 0;
        vkEnumerateDeviceExtensionProperties(bestDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(bestDevice, nullptr, &extCount, exts.data());
        for (const auto& ext : exts) {
#ifdef ENJIN_VRS
            if (strcmp(ext.extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0) {
                m_VRSSupported = true;
            }
#endif
            if (strcmp(ext.extensionName, "VK_EXT_device_generated_commands") == 0) {
                m_DGCExtSupported = true;
            }
            if (strcmp(ext.extensionName, "VK_NV_device_generated_commands") == 0) {
                m_DGCNVSupported = true;
            }
            if (strcmp(ext.extensionName, "VK_EXT_mesh_shader") == 0) {
                m_MeshShaderSupported = true;
            }
            if (strcmp(ext.extensionName, "VK_KHR_maintenance5") == 0) {
                m_Maintenance5Supported = true;
            }
        }

        // VK_EXT_device_generated_commands requires VK_KHR_maintenance5 in the
        // enabled extension list (VUID-vkCreateDevice-...-01387). If the device
        // somehow exposes DGC EXT without maintenance5, drop to the NV path or
        // multi-draw indirect rather than create the device with a broken list.
        if (m_DGCExtSupported && !m_Maintenance5Supported) {
            ENJIN_LOG_INFO(Renderer, "VK_EXT_device_generated_commands present but VK_KHR_maintenance5 is not — disabling DGC EXT");
            m_DGCExtSupported = false;
        }

        if (m_MeshShaderSupported) {
            ENJIN_LOG_INFO(Renderer, "VK_EXT_mesh_shader supported — mesh/task shaders available");
        }

#ifdef ENJIN_VRS
        if (m_VRSSupported) {
            ENJIN_LOG_INFO(Renderer, "VK_KHR_fragment_shading_rate supported");
        } else {
            ENJIN_LOG_INFO(Renderer, "VK_KHR_fragment_shading_rate not supported — VRS disabled");
        }
#endif

        if (m_DGCExtSupported) {
            ENJIN_LOG_INFO(Renderer, "VK_EXT_device_generated_commands supported");
        } else if (m_DGCNVSupported) {
            ENJIN_LOG_INFO(Renderer, "VK_NV_device_generated_commands supported (EXT not available)");
        } else {
            ENJIN_LOG_INFO(Renderer, "Device generated commands not supported — using multi-draw indirect");
        }
    }

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
    if (supportedFeatures.samplerAnisotropy) {
        deviceFeatures.samplerAnisotropy = VK_TRUE;
    }
    // Required for MRT pipelines with different blend states per attachment
    // (e.g., color attachment with full write mask + velocity attachment with write disabled)
    if (supportedFeatures.independentBlend) {
        deviceFeatures.independentBlend = VK_TRUE;
    }
    // Point/spot shadow cube arrays and the probe cubemap use samplerCubeArray /
    // VK_IMAGE_VIEW_TYPE_CUBE_ARRAY, which require this feature (validation 08740/01004).
    if (supportedFeatures.imageCubeArray) {
        deviceFeatures.imageCubeArray = VK_TRUE;
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

    // Add device generated commands extension if supported. DGC EXT has a hard
    // dependency on VK_KHR_maintenance5, which must appear in the list too.
    if (m_DGCExtSupported) {
        deviceExtensions.push_back("VK_KHR_maintenance5");
        deviceExtensions.push_back("VK_EXT_device_generated_commands");
        ENJIN_LOG_INFO(Renderer, "Enabling VK_EXT_device_generated_commands (+ VK_KHR_maintenance5) extension");
    } else if (m_DGCNVSupported) {
        deviceExtensions.push_back("VK_NV_device_generated_commands");
        ENJIN_LOG_INFO(Renderer, "Enabling VK_NV_device_generated_commands extension");
    }

    // Add mesh shader extension if supported
    if (m_MeshShaderSupported) {
        deviceExtensions.push_back("VK_EXT_mesh_shader");
        ENJIN_LOG_INFO(Renderer, "Enabling VK_EXT_mesh_shader extension");
    }

    // Add VRS extension if supported
#ifdef ENJIN_VRS
    if (m_VRSSupported) {
        deviceExtensions.push_back(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
        ENJIN_LOG_INFO(Renderer, "Enabling VK_KHR_fragment_shading_rate extension");
    }
#endif

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());

    // Build feature chain — used when RT or DGC requires pNext features
    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bdaFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    // DGC feature structs — guarded for Vulkan SDK version compatibility.
    // VK_EXT_device_generated_commands was introduced in Vulkan 1.3.275 (VK_HEADER_VERSION 275).
    // VK_NV_device_generated_commands is available in older SDKs.
#if defined(VK_EXT_device_generated_commands)
    VkPhysicalDeviceDeviceGeneratedCommandsFeaturesEXT dgcExtFeatures{};
    dgcExtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_EXT;
    dgcExtFeatures.deviceGeneratedCommands = VK_TRUE;
#endif

#if defined(VK_NV_device_generated_commands)
    VkPhysicalDeviceDeviceGeneratedCommandsFeaturesNV dgcNVFeatures{};
    dgcNVFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEVICE_GENERATED_COMMANDS_FEATURES_NV;
    dgcNVFeatures.deviceGeneratedCommands = VK_TRUE;
#endif

    // Descriptor indexing features (required for bindless rendering)
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;  // a bindless layout uses it on an SSBO binding (validation 03008)
    descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;

    // Timeline semaphores are created by the engine; the feature must be enabled
    // (validation 03252). Core in Vulkan 1.2; this feature struct is the 1.1 path.
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures{};
    timelineSemaphoreFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;

    // Determine whether we need the pNext feature chain
    // Always true now — bindless descriptors require descriptor indexing features
    bool needsFeatureChain = true;

    if (needsFeatureChain) {
        // Build the pNext chain dynamically
        void** chainTail = &deviceFeatures2.pNext;

        // Descriptor indexing (bindless) — always chain
        *chainTail = &descriptorIndexingFeatures;
        chainTail = &descriptorIndexingFeatures.pNext;

        // Timeline semaphore — always chain (engine creates timeline semaphores)
        *chainTail = &timelineSemaphoreFeatures;
        chainTail = &timelineSemaphoreFeatures.pNext;

        // BDA is required by RT and by DGC (EXT variant uses device addresses)
        if (m_RTCapabilities.supported || m_DGCExtSupported) {
            *chainTail = &bdaFeatures;
            chainTail = &bdaFeatures.pNext;
        }

        if (m_RTCapabilities.supported) {
            *chainTail = &asFeatures;
            chainTail = &asFeatures.pNext;
            *chainTail = &rtPipelineFeatures;
            chainTail = &rtPipelineFeatures.pNext;
        }

        // Chain DGC feature structs (prefer EXT over NV)
        bool dgcChained = false;
#if defined(VK_EXT_device_generated_commands)
        if (m_DGCExtSupported && !dgcChained) {
            *chainTail = &dgcExtFeatures;
            chainTail = &dgcExtFeatures.pNext;
            dgcChained = true;
        }
#endif
#if defined(VK_NV_device_generated_commands)
        if (m_DGCNVSupported && !dgcChained) {
            *chainTail = &dgcNVFeatures;
            chainTail = &dgcNVFeatures.pNext;
            dgcChained = true;
        }
#endif
        (void)chainTail; (void)dgcChained;

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
    if (m_GraphicsQueue == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "Graphics queue is null after vkGetDeviceQueue");
        return false;
    }
    // Present queue will be set when we have a surface
    m_PresentQueueFamily = m_GraphicsQueueFamily;
    m_PresentQueue = m_GraphicsQueue;
    // Get compute queue (may be same as graphics if no dedicated compute family)
    vkGetDeviceQueue(m_Device, m_ComputeQueueFamily, 0, &m_ComputeQueue);
    if (m_ComputeQueue == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "Compute queue is null after vkGetDeviceQueue");
        return false;
    }
    return true;
}

u32 VulkanContext::FindPresentQueueFamily(VkSurfaceKHR surface) const {
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    for (u32 i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = false;
        VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, surface, &presentSupport);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "vkGetPhysicalDeviceSurfaceSupportKHR failed for family %u: %d", i, result);
            continue;
        }
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

    // glfwGetRequiredInstanceExtensions returns nullptr if Vulkan is not supported
    // (e.g., missing vulkan-1.dll or no Vulkan ICD installed)
    if (!glfwExtensions || glfwExtensionCount == 0) {
        // Surface WHY: GLFW records a human-readable reason (loader missing,
        // vkGetInstanceProcAddr not found, no WSI extensions...).
        const char* glfwErr = nullptr;
        int glfwCode = glfwGetError(&glfwErr);
        ENJIN_LOG_ERROR(Renderer, "GLFW returned no Vulkan instance extensions (Vulkan may not be available)");
        ENJIN_LOG_ERROR(Renderer, "GLFW error 0x%x: %s", glfwCode, glfwErr ? glfwErr : "(no detail)");
        ENJIN_LOG_ERROR(Renderer, "glfwVulkanSupported = %d", glfwVulkanSupported());
        return {};
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef ENJIN_BUILD_DEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    return extensions;
}

bool VulkanContext::CheckValidationLayerSupport() const {
    u32 layerCount = 0;
    VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (result != VK_SUCCESS || layerCount == 0) {
        return false;
    }

    std::vector<VkLayerProperties> availableLayers(layerCount);
    result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    if (result != VK_SUCCESS) {
        return false;
    }

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
    u32 extensionCount = 0;
    VkResult extResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    if (extResult != VK_SUCCESS || extensionCount == 0) {
        return false;
    }
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    extResult = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    if (extResult != VK_SUCCESS) {
        return false;
    }

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

    // Last resort: software rasterizers (lavapipe / SwiftShader report
    // VK_PHYSICAL_DEVICE_TYPE_CPU). Slow, but lets headless environments with no
    // GPU (CI runners, VMs, remote desktops) boot and render. Scoring in
    // SelectPhysicalDevice keeps any real GPU ahead of these.
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
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
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
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

VkSampleCountFlagBits VulkanContext::GetMaxUsableSampleCount() const {
    if (m_PhysicalDevice == VK_NULL_HANDLE) return VK_SAMPLE_COUNT_1_BIT;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);

    VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts
                              & props.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

u64 VulkanContext::GetGPUMemoryUsed() const {
    if (m_PhysicalDevice == VK_NULL_HANDLE) return 0;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProps);

    // Sum all device-local heap sizes as a rough "total available"
    // Actual used requires VK_EXT_memory_budget, fall back to 0
    // if not available. The budget query below is more useful.
    return 0; // Can't query used without memory_budget extension
}

u64 VulkanContext::GetGPUMemoryBudget() const {
    if (m_PhysicalDevice == VK_NULL_HANDLE) return 0;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProps);

    // Sum device-local heap sizes as the total VRAM budget
    u64 totalDeviceLocal = 0;
    for (u32 i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalDeviceLocal += memProps.memoryHeaps[i].size;
        }
    }
    return totalDeviceLocal;
}

} // namespace Renderer
} // namespace Enjin
