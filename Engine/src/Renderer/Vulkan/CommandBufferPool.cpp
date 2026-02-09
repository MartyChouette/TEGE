#include "Enjin/Renderer/Vulkan/CommandBufferPool.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

bool CommandBufferPool::Initialize(VulkanContext* context, u32 threadCount, u32 framesInFlight) {
    m_Context = context;
    m_ThreadCount = threadCount;
    m_FramesInFlight = framesInFlight;

    u32 totalPools = threadCount * framesInFlight;
    m_Pools.resize(totalPools, VK_NULL_HANDLE);
    m_PoolData.resize(totalPools);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // Short-lived buffers, reset each frame

    for (u32 i = 0; i < totalPools; ++i) {
        VkResult result = vkCreateCommandPool(context->GetDevice(), &poolInfo, nullptr, &m_Pools[i]);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create command pool %u: %d", i, result);
            Shutdown();
            return false;
        }
    }

    ENJIN_LOG_INFO(Renderer, "CommandBufferPool initialized: %u threads x %u frames = %u pools",
                   threadCount, framesInFlight, totalPools);
    return true;
}

void CommandBufferPool::Shutdown() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();
    for (auto pool : m_Pools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, pool, nullptr);
        }
    }
    m_Pools.clear();
    m_PoolData.clear();
    m_ThreadCount = 0;
    m_FramesInFlight = 0;
}

VkCommandBuffer CommandBufferPool::Allocate(u32 threadIndex, u32 frameIndex) {
    u32 idx = GetPoolIndex(threadIndex, frameIndex);
    auto& data = m_PoolData[idx];

    // If we have a pre-allocated buffer available, reuse it
    if (data.nextIndex < static_cast<u32>(data.buffers.size())) {
        return data.buffers[data.nextIndex++];
    }

    // Allocate a new secondary command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_Pools[idx];
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer buffer = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, &buffer);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate secondary command buffer: %d", result);
        return VK_NULL_HANDLE;
    }

    data.buffers.push_back(buffer);
    data.nextIndex = static_cast<u32>(data.buffers.size());
    return buffer;
}

void CommandBufferPool::ResetFrame(u32 frameIndex) {
    for (u32 t = 0; t < m_ThreadCount; ++t) {
        u32 idx = GetPoolIndex(t, frameIndex);
        vkResetCommandPool(m_Context->GetDevice(), m_Pools[idx], 0);
        m_PoolData[idx].nextIndex = 0; // All previously allocated buffers are now available
    }
}

} // namespace Renderer
} // namespace Enjin
