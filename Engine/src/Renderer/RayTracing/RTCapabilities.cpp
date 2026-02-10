#include "Enjin/Renderer/RayTracing/RTCapabilities.h"
#include "Enjin/Logging/Log.h"
#include <vector>
#include <string>
#include <set>

namespace Enjin {
namespace Renderer {

static const std::vector<const char*> s_RTExtensions = {
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
};

const std::vector<const char*>& RTCapabilities::GetRequiredExtensions() {
    return s_RTExtensions;
}

RTCapabilities RTCapabilities::Query(VkPhysicalDevice device) {
    RTCapabilities caps;

    if (device == VK_NULL_HANDLE) return caps;

    // Enumerate available device extensions
    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());

    std::set<std::string> extensionSet;
    for (const auto& ext : available) {
        extensionSet.insert(ext.extensionName);
    }

    // Check each required extension
    caps.accelerationStructure = extensionSet.count(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) > 0;
    caps.rayTracingPipeline = extensionSet.count(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) > 0;
    caps.deferredHostOperations = extensionSet.count(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) > 0;
    caps.bufferDeviceAddress = extensionSet.count(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) > 0;

    caps.supported = caps.accelerationStructure &&
                     caps.rayTracingPipeline &&
                     caps.deferredHostOperations &&
                     caps.bufferDeviceAddress;

    if (!caps.supported) {
        ENJIN_LOG_INFO(Renderer, "Ray tracing not supported: AS=%s, RTPipeline=%s, DeferredOps=%s, BDA=%s",
            caps.accelerationStructure ? "yes" : "no",
            caps.rayTracingPipeline ? "yes" : "no",
            caps.deferredHostOperations ? "yes" : "no",
            caps.bufferDeviceAddress ? "yes" : "no");
        return caps;
    }

    // Query ray tracing pipeline properties
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelineProps{};
    rtPipelineProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProps{};
    asProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    rtPipelineProps.pNext = &asProps;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtPipelineProps;
    vkGetPhysicalDeviceProperties2(device, &props2);

    caps.maxRayRecursionDepth = rtPipelineProps.maxRayRecursionDepth;
    caps.shaderGroupHandleSize = rtPipelineProps.shaderGroupHandleSize;
    caps.shaderGroupHandleAlignment = rtPipelineProps.shaderGroupHandleAlignment;
    caps.shaderGroupBaseAlignment = rtPipelineProps.shaderGroupBaseAlignment;
    caps.maxRayDispatchInvocationCount = rtPipelineProps.maxRayDispatchInvocationCount;

    caps.maxGeometryCount = asProps.maxGeometryCount;
    caps.maxInstanceCount = asProps.maxInstanceCount;
    caps.maxPrimitiveCount = asProps.maxPrimitiveCount;

    ENJIN_LOG_INFO(Renderer, "Ray tracing supported: maxRecursion=%u, handleSize=%u, handleAlign=%u, baseAlign=%u",
        caps.maxRayRecursionDepth, caps.shaderGroupHandleSize,
        caps.shaderGroupHandleAlignment, caps.shaderGroupBaseAlignment);
    ENJIN_LOG_INFO(Renderer, "  AS limits: maxGeometry=%llu, maxInstance=%llu, maxPrimitive=%llu",
        caps.maxGeometryCount, caps.maxInstanceCount, caps.maxPrimitiveCount);

    return caps;
}

} // namespace Renderer
} // namespace Enjin
