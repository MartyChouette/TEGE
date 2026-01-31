#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {

namespace Renderer {
    class VulkanContext;
}

namespace Editor {

struct PerformanceMetrics {
    usize processMemoryBytes = 0;
    usize totalPhysicalMemory = 0;
    usize availablePhysicalMemory = 0;
    usize gpuTotalBytes = 0;
    usize gpuAllocatedBytes = 0;
    u32 drawCallCount = 0;
    u32 triangleCount = 0;
};

class ENJIN_API PerformanceStats {
public:
    static void UpdateSystemMemory(PerformanceMetrics& metrics);
    static void QueryGPUMemory(Renderer::VulkanContext* ctx, PerformanceMetrics& metrics);
};

} // namespace Editor
} // namespace Enjin
