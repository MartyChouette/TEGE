#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Enjin {
namespace Renderer {

// Ray tracing hardware capability detection and properties
struct ENJIN_API RTCapabilities {
    bool supported = false;                  // All required RT extensions available
    bool accelerationStructure = false;       // VK_KHR_acceleration_structure
    bool rayTracingPipeline = false;          // VK_KHR_ray_tracing_pipeline
    bool deferredHostOperations = false;      // VK_KHR_deferred_host_operations
    bool bufferDeviceAddress = false;         // VK_KHR_buffer_device_address

    // Pipeline properties (valid only when supported=true)
    u32 maxRayRecursionDepth = 0;
    u32 shaderGroupHandleSize = 0;
    u32 shaderGroupHandleAlignment = 0;
    u32 shaderGroupBaseAlignment = 0;
    u32 maxRayDispatchInvocationCount = 0;

    // Acceleration structure properties
    u64 maxGeometryCount = 0;
    u64 maxInstanceCount = 0;
    u64 maxPrimitiveCount = 0;

    // Query capabilities of a physical device (call before logical device creation)
    static RTCapabilities Query(VkPhysicalDevice device);

    // Get the list of device extensions to enable when RT is supported
    static const std::vector<const char*>& GetRequiredExtensions();
};

} // namespace Renderer
} // namespace Enjin
