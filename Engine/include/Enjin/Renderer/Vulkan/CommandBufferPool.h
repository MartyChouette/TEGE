#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>

namespace Enjin {
namespace Renderer {

// Per-thread, per-frame VkCommandPool + secondary VkCommandBuffer cache.
// Each thread gets its own command pool (Vulkan pools are not thread-safe).
class ENJIN_API CommandBufferPool {
public:
    CommandBufferPool() = default;
    ~CommandBufferPool() { Shutdown(); }

    bool Initialize(VulkanContext* context, u32 threadCount, u32 framesInFlight);
    void Shutdown();

    // Allocate a secondary command buffer for the given thread and frame.
    // The buffer is reset (via pool reset) at the start of each frame.
    VkCommandBuffer Allocate(u32 threadIndex, u32 frameIndex);

    // Reset all command pools for the given frame index (called at frame start)
    void ResetFrame(u32 frameIndex);

    u32 GetThreadCount() const { return m_ThreadCount; }
    u32 GetFramesInFlight() const { return m_FramesInFlight; }

private:
    VulkanContext* m_Context = nullptr;
    u32 m_ThreadCount = 0;
    u32 m_FramesInFlight = 0;

    // Flat array: index = threadIndex * framesInFlight + frameIndex
    std::vector<VkCommandPool> m_Pools;

    // Per-pool allocated secondary command buffers (grows as needed)
    struct PoolData {
        std::vector<VkCommandBuffer> buffers;
        u32 nextIndex = 0; // Next available buffer index
    };
    std::vector<PoolData> m_PoolData;

    u32 GetPoolIndex(u32 threadIndex, u32 frameIndex) const {
        return threadIndex * m_FramesInFlight + frameIndex;
    }
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
