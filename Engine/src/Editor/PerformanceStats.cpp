#include "Enjin/Editor/PerformanceStats.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <vulkan/vulkan.h>

#if defined(ENJIN_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <Psapi.h>
#elif defined(__linux__)
    #include <fstream>
    #include <string>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <mach/mach.h>
    #include <sys/sysctl.h>
#endif

namespace Enjin {
namespace Editor {

void PerformanceStats::UpdateSystemMemory(PerformanceMetrics& metrics) {
#if defined(ENJIN_PLATFORM_WINDOWS)
    // Process memory
    PROCESS_MEMORY_COUNTERS_EX pmc;
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        metrics.processMemoryBytes = static_cast<usize>(pmc.WorkingSetSize);
    }

    // System memory
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        metrics.totalPhysicalMemory = static_cast<usize>(memStatus.ullTotalPhys);
        metrics.availablePhysicalMemory = static_cast<usize>(memStatus.ullAvailPhys);
    }
#elif defined(__linux__)
    // Process memory from /proc/self/status
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmRSS:") == 0) {
            usize kb = 0;
            sscanf(line.c_str(), "VmRSS: %zu kB", &kb);
            metrics.processMemoryBytes = kb * 1024;
            break;
        }
    }

    // System memory from /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    while (std::getline(meminfo, line)) {
        usize kb = 0;
        if (line.find("MemTotal:") == 0) {
            sscanf(line.c_str(), "MemTotal: %zu kB", &kb);
            metrics.totalPhysicalMemory = kb * 1024;
        } else if (line.find("MemAvailable:") == 0) {
            sscanf(line.c_str(), "MemAvailable: %zu kB", &kb);
            metrics.availablePhysicalMemory = kb * 1024;
        }
    }
#elif defined(__APPLE__)
    // Process memory
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS) {
        metrics.processMemoryBytes = static_cast<usize>(info.resident_size);
    }

    // System memory
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    u64 memSize = 0;
    size_t len = sizeof(memSize);
    sysctl(mib, 2, &memSize, &len, nullptr, 0);
    metrics.totalPhysicalMemory = static_cast<usize>(memSize);
    metrics.availablePhysicalMemory = 0; // Not easily available on macOS
#endif
}

void PerformanceStats::QueryGPUMemory(Renderer::VulkanContext* ctx, PerformanceMetrics& metrics) {
    if (!ctx || ctx->GetPhysicalDevice() == VK_NULL_HANDLE) return;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(ctx->GetPhysicalDevice(), &memProps);

    metrics.gpuTotalBytes = 0;
    for (u32 i = 0; i < memProps.memoryHeapCount; ++i) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            metrics.gpuTotalBytes += static_cast<usize>(memProps.memoryHeaps[i].size);
        }
    }

    // GPU allocated comes from VulkanContext tracking
    metrics.gpuAllocatedBytes = ctx->GetTotalGPUAllocatedBytes();
}

} // namespace Editor
} // namespace Enjin
