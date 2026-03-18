#include "Enjin/Renderer/AsyncComputeScheduler.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

AsyncComputeScheduler::AsyncComputeScheduler() = default;

AsyncComputeScheduler::~AsyncComputeScheduler() {
    Shutdown();
}

bool AsyncComputeScheduler::Initialize(VulkanContext* ctx, u32 maxFramesInFlight) {
    if (!ctx) return false;
    m_Context = ctx;
    m_MaxFramesInFlight = maxFramesInFlight;

    // Require a dedicated compute queue (different family from graphics)
    if (!ctx->HasDedicatedComputeQueue()) {
        ENJIN_LOG_INFO(Renderer, "AsyncComputeScheduler: No dedicated compute queue — async compute disabled");
        m_Supported = false;
        return false;
    }

    m_ComputeQueue = ctx->GetComputeQueue();
    m_ComputeQueueFamily = ctx->GetComputeQueueFamily();

    if (m_ComputeQueue == VK_NULL_HANDLE) {
        ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Compute queue is VK_NULL_HANDLE");
        m_Supported = false;
        return false;
    }

    if (!CreateCommandResources(maxFramesInFlight)) {
        ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Failed to create command resources");
        m_Supported = false;
        return false;
    }

    if (!CreateSyncResources(maxFramesInFlight)) {
        ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Failed to create sync resources");
        DestroyResources();
        m_Supported = false;
        return false;
    }

    m_Supported = true;
    ENJIN_LOG_INFO(Renderer, "AsyncComputeScheduler initialized (queue family %u, timeline=%s)",
                   m_ComputeQueueFamily, m_HasTimelineSemaphores ? "yes" : "no");
    return true;
}

void AsyncComputeScheduler::Shutdown() {
    if (!m_Context) return;
    DestroyResources();
    m_Supported = false;
    m_Context = nullptr;
}

bool AsyncComputeScheduler::CreateCommandResources(u32 maxFramesInFlight) {
    VkDevice device = m_Context->GetDevice();

    // Create command pool for compute queue family
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_ComputeQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_ComputeCmdPool) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Failed to create compute command pool");
        return false;
    }

    // Allocate per-frame command buffers
    m_ComputeCmdBuffers.resize(maxFramesInFlight);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_ComputeCmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = maxFramesInFlight;

    if (vkAllocateCommandBuffers(device, &allocInfo, m_ComputeCmdBuffers.data()) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Failed to allocate compute command buffers");
        vkDestroyCommandPool(device, m_ComputeCmdPool, nullptr);
        m_ComputeCmdPool = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

bool AsyncComputeScheduler::CreateSyncResources(u32 maxFramesInFlight) {
    VkDevice device = m_Context->GetDevice();

    // Try to create timeline semaphore (Vulkan 1.2 core)
    if (m_Config.enableTimelineSemaphores) {
        VkSemaphoreTypeCreateInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineInfo.initialValue = 0;

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semInfo.pNext = &timelineInfo;

        if (vkCreateSemaphore(device, &semInfo, nullptr, &m_TimelineSemaphore) == VK_SUCCESS) {
            m_HasTimelineSemaphores = true;
            m_TimelineValue = 0;
            m_FrameTimelineValues.resize(maxFramesInFlight, 0);
            ENJIN_LOG_INFO(Renderer, "AsyncComputeScheduler: Timeline semaphore created");
        } else {
            ENJIN_LOG_INFO(Renderer, "AsyncComputeScheduler: Timeline semaphore not supported, using binary");
            m_HasTimelineSemaphores = false;
        }
    }

    // Always create binary semaphores (used as fallback or for graphics submit wait)
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    m_ComputeFinishedSemaphores.resize(maxFramesInFlight);
    for (u32 i = 0; i < maxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &m_ComputeFinishedSemaphores[i]) != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Failed to create compute finished semaphore %u", i);
            return false;
        }
    }

    m_GraphicsToComputeSemaphores.resize(maxFramesInFlight);
    for (u32 i = 0; i < maxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &m_GraphicsToComputeSemaphores[i]) != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Failed to create g2c semaphore %u", i);
            return false;
        }
    }

    return true;
}

void AsyncComputeScheduler::DestroyResources() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    // Wait for all work to complete before destroying resources
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    if (m_TimelineSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, m_TimelineSemaphore, nullptr);
        m_TimelineSemaphore = VK_NULL_HANDLE;
    }

    for (auto sem : m_ComputeFinishedSemaphores) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(device, sem, nullptr);
    }
    m_ComputeFinishedSemaphores.clear();

    for (auto sem : m_GraphicsToComputeSemaphores) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(device, sem, nullptr);
    }
    m_GraphicsToComputeSemaphores.clear();

    m_ComputeCmdBuffers.clear();
    if (m_ComputeCmdPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_ComputeCmdPool, nullptr);
        m_ComputeCmdPool = VK_NULL_HANDLE;
    }

    m_FrameTimelineValues.clear();
    m_HasTimelineSemaphores = false;
}

void AsyncComputeScheduler::BeginFrame(u32 frameIndex) {
    m_CurrentFrame = frameIndex;
    m_ComputeSubmittedThisFrame = false;
    m_GraphicsSignaledThisFrame = false;
    m_Recording = false;
}

VkCommandBuffer AsyncComputeScheduler::BeginComputeWork(u32 frameIndex, AsyncComputeWorkType workType) {
    if (!m_Supported) return VK_NULL_HANDLE;
    if (m_Recording) {
        ENJIN_LOG_WARN(Renderer, "AsyncComputeScheduler: Already recording compute work");
        return VK_NULL_HANDLE;
    }
    if (frameIndex >= static_cast<u32>(m_ComputeCmdBuffers.size())) return VK_NULL_HANDLE;

    // Check if this work type should use async compute
    if (!ShouldUseAsync(workType)) return VK_NULL_HANDLE;

    VkCommandBuffer cmd = m_ComputeCmdBuffers[frameIndex];

    // Reset and begin recording
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "AsyncComputeScheduler: Failed to begin compute command buffer");
        return VK_NULL_HANDLE;
    }

    m_Recording = true;
    m_LastWorkType = workType;
    return cmd;
}

void AsyncComputeScheduler::SubmitComputeWork(u32 frameIndex, bool waitOnGraphics, bool signalForGraphics) {
    if (!m_Supported || !m_Recording) return;
    if (frameIndex >= static_cast<u32>(m_ComputeCmdBuffers.size())) return;

    VkCommandBuffer cmd = m_ComputeCmdBuffers[frameIndex];

    // End recording
    vkEndCommandBuffer(cmd);
    m_Recording = false;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    // Signal semaphore so graphics queue can wait on compute completion
    VkSemaphore signalSem = VK_NULL_HANDLE;
    if (signalForGraphics) {
        if (m_HasTimelineSemaphores) {
            // Increment timeline value and signal it
            m_TimelineValue++;
            m_FrameTimelineValues[frameIndex] = m_TimelineValue;

            VkTimelineSemaphoreSubmitInfo timelineSubmit{};
            timelineSubmit.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timelineSubmit.signalSemaphoreValueCount = 1;
            timelineSubmit.pSignalSemaphoreValues = &m_TimelineValue;

            // Also signal binary semaphore for graphics queue submit wait
            VkSemaphore signalSemaphores[2] = {
                m_ComputeFinishedSemaphores[frameIndex],
                m_TimelineSemaphore
            };
            submitInfo.signalSemaphoreCount = 2;
            submitInfo.pSignalSemaphores = signalSemaphores;

            u64 dummySignalValue = 0; // Binary semaphore doesn't use timeline value
            u64 signalValues[2] = { dummySignalValue, m_TimelineValue };
            timelineSubmit.signalSemaphoreValueCount = 2;
            timelineSubmit.pSignalSemaphoreValues = signalValues;

            submitInfo.pNext = &timelineSubmit;

            // Wait on graphics→compute semaphore if requested
            VkSemaphore waitSem = VK_NULL_HANDLE;
            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            u64 waitValue = 0;
            if (waitOnGraphics && m_GraphicsSignaledThisFrame) {
                waitSem = m_GraphicsToComputeSemaphores[frameIndex];
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = &waitSem;
                submitInfo.pWaitDstStageMask = &waitStage;
                timelineSubmit.waitSemaphoreValueCount = 1;
                timelineSubmit.pWaitSemaphoreValues = &waitValue;
                m_GraphicsSignaledThisFrame = false;
            }

            VkResult result = vkQueueSubmit(m_ComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
            if (result == VK_ERROR_DEVICE_LOST) {
                ENJIN_LOG_FATAL(Renderer, "Device lost during async compute submit");
                return;
            } else if (result != VK_SUCCESS) {
                ENJIN_LOG_ERROR(Renderer, "AsyncComputeScheduler: Failed to submit compute work: %d", result);
                return;
            }
        } else {
            // Binary semaphore path
            signalSem = m_ComputeFinishedSemaphores[frameIndex];
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &signalSem;

            VkSemaphore waitSem = VK_NULL_HANDLE;
            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            if (waitOnGraphics && m_GraphicsSignaledThisFrame) {
                waitSem = m_GraphicsToComputeSemaphores[frameIndex];
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = &waitSem;
                submitInfo.pWaitDstStageMask = &waitStage;
                m_GraphicsSignaledThisFrame = false;
            }

            VkResult result = vkQueueSubmit(m_ComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
            if (result == VK_ERROR_DEVICE_LOST) {
                ENJIN_LOG_FATAL(Renderer, "Device lost during async compute submit");
                return;
            } else if (result != VK_SUCCESS) {
                ENJIN_LOG_ERROR(Renderer, "AsyncComputeScheduler: Failed to submit compute work: %d", result);
                return;
            }
        }
    } else {
        // Fire-and-forget (no signal)
        VkResult result = vkQueueSubmit(m_ComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "AsyncComputeScheduler: Failed to submit compute work: %d", result);
            return;
        }
    }

    m_ComputeSubmittedThisFrame = true;
}

void AsyncComputeScheduler::WaitForCompute(VkCommandBuffer graphicsCmd, VkPipelineStageFlags dstStage) {
    if (!m_ComputeSubmittedThisFrame) return;

    // Insert a memory barrier on the graphics command buffer.
    // The actual semaphore wait happens during graphics queue submit (VulkanRenderer
    // adds our ComputeFinishedSemaphore to the wait list).
    // This barrier ensures compute writes are visible to the graphics pipeline.
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

    vkCmdPipelineBarrier(graphicsCmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, dstStage,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void AsyncComputeScheduler::SignalFromGraphics(u32 frameIndex) {
    if (!m_Supported) return;
    m_GraphicsSignaledThisFrame = true;
    // The actual semaphore signal is added to the graphics queue submit by VulkanRenderer
}

VkSemaphore AsyncComputeScheduler::GetComputeFinishedSemaphore(u32 frameIndex) const {
    if (!m_ComputeSubmittedThisFrame) return VK_NULL_HANDLE;
    if (frameIndex >= static_cast<u32>(m_ComputeFinishedSemaphores.size())) return VK_NULL_HANDLE;
    return m_ComputeFinishedSemaphores[frameIndex];
}

u64 AsyncComputeScheduler::GetCurrentTimelineValue(u32 frameIndex) const {
    if (!m_HasTimelineSemaphores) return 0;
    if (frameIndex >= static_cast<u32>(m_FrameTimelineValues.size())) return 0;
    return m_FrameTimelineValues[frameIndex];
}

void AsyncComputeScheduler::EndFrame() {
    m_ComputeSubmittedThisFrame = false;
    m_GraphicsSignaledThisFrame = false;
    m_Recording = false;
}

bool AsyncComputeScheduler::ShouldUseAsync(AsyncComputeWorkType type) const {
    if (!m_Supported) return false;
    switch (type) {
        case AsyncComputeWorkType::GPUCulling:       return m_Config.enableAsyncCulling;
        case AsyncComputeWorkType::RTDispatch:       return m_Config.enableAsyncRT;
        case AsyncComputeWorkType::Denoise:          return m_Config.enableAsyncDenoise;
        case AsyncComputeWorkType::ClusteredLighting: return m_Config.enableAsyncCulling;
    }
    return false;
}

} // namespace Renderer
} // namespace Enjin
