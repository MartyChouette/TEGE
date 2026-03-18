#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <vulkan/vulkan.h>
#include <vector>

/**
 * @file AsyncComputeScheduler.h
 * @brief Manages async compute overlap for GPU-driven work scheduling.
 *
 * Enables overlapping compute work (RT dispatches, denoising, culling) with
 * rasterization on the graphics queue. Uses timeline semaphores for fine-grained
 * synchronization between queues.
 *
 * Falls back gracefully to single-queue execution when no dedicated compute queue
 * is available (e.g. integrated GPUs with a single queue family).
 */

namespace Enjin {
namespace Renderer {

class VulkanRenderer; // Forward declaration

/// Work items that can be scheduled on the async compute queue.
/// Each type has different synchronization requirements.
enum class AsyncComputeWorkType : u8 {
    GPUCulling,       // Frustum/occlusion culling — runs before rasterization
    RTDispatch,       // Ray tracing effects — overlaps with main geometry raster
    Denoise,          // SVGF/OIDN/OptiX denoiser — overlaps with post-processing
    ClusteredLighting // Light assignment compute — runs before main pass
};

/// Configuration for async compute behavior
struct AsyncComputeConfig {
    bool enableAsyncRT = true;         // Overlap RT dispatches with rasterization
    bool enableAsyncDenoise = true;    // Overlap denoiser with post-processing
    bool enableAsyncCulling = true;    // GPU culling on compute queue (existing behavior)
    bool enableTimelineSemaphores = true; // Use timeline semaphores (Vulkan 1.2+)
};

/// Synchronization point between graphics and compute queues.
/// Each point represents a dependency edge in the frame graph.
struct ComputeSyncPoint {
    u64 timelineValue = 0;   // Timeline semaphore value for this sync point
    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;
};

/**
 * @brief Orchestrates async compute work across graphics and compute queues.
 *
 * Frame flow with async compute enabled:
 *   1. Shadow passes run on graphics queue
 *   2. GPU culling dispatches on compute queue (overlaps shadow tail end)
 *   3. Main geometry rasterization begins on graphics queue
 *   4. RT effects dispatch on compute queue (overlaps with main geometry)
 *   5. Graphics queue finishes geometry, signals compute can read depth
 *   6. Denoiser runs on compute queue (overlaps post-processing start)
 *   7. Post-processing waits for denoiser completion, composites RT results
 *
 * When async compute is unavailable, all work runs sequentially on the
 * graphics queue using the existing single-queue path.
 */
class ENJIN_API AsyncComputeScheduler {
public:
    AsyncComputeScheduler();
    ~AsyncComputeScheduler();

    /// Initialize the scheduler. Returns true if async compute is available.
    /// @param ctx Vulkan context with queue family info
    /// @param maxFramesInFlight Number of frames in flight (typically 2)
    bool Initialize(VulkanContext* ctx, u32 maxFramesInFlight);

    /// Shut down and destroy all Vulkan resources.
    void Shutdown();

    /// Whether async compute is supported and initialized.
    bool IsSupported() const { return m_Supported; }

    /// Whether timeline semaphores are available.
    bool HasTimelineSemaphores() const { return m_HasTimelineSemaphores; }

    /// Get the compute queue (separate from graphics).
    VkQueue GetComputeQueue() const { return m_ComputeQueue; }

    /// Get the compute queue family index.
    u32 GetComputeQueueFamily() const { return m_ComputeQueueFamily; }

    // --- Per-frame lifecycle ---

    /// Begin a new frame. Resets per-frame state and increments timeline.
    void BeginFrame(u32 frameIndex);

    /// Begin recording compute commands for the given work type.
    /// @return Command buffer to record into, or VK_NULL_HANDLE if unavailable.
    VkCommandBuffer BeginComputeWork(u32 frameIndex, AsyncComputeWorkType workType);

    /// End recording and submit the compute command buffer.
    /// @param frameIndex Current frame-in-flight index
    /// @param waitOnGraphics If true, compute waits for graphics to reach a sync point
    /// @param signalForGraphics If true, signals a semaphore that graphics can wait on
    void SubmitComputeWork(u32 frameIndex, bool waitOnGraphics = false, bool signalForGraphics = true);

    /// Insert a barrier in the graphics command buffer to wait for compute results.
    /// Call this before the graphics queue needs to read compute output.
    /// @param graphicsCmd The graphics command buffer
    /// @param dstStage Pipeline stage that will consume compute results
    void WaitForCompute(VkCommandBuffer graphicsCmd, VkPipelineStageFlags dstStage);

    /// Signal from graphics queue that compute can proceed.
    /// Used when compute work depends on graphics output (e.g., depth buffer for HiZ).
    void SignalFromGraphics(u32 frameIndex);

    /// Get the binary semaphore to wait on during graphics queue submit.
    /// Returns VK_NULL_HANDLE if no compute work was submitted this frame.
    VkSemaphore GetComputeFinishedSemaphore(u32 frameIndex) const;

    /// Get the timeline semaphore (if available) for fine-grained sync.
    VkSemaphore GetTimelineSemaphore() const { return m_TimelineSemaphore; }

    /// Get the current timeline value for the given frame.
    u64 GetCurrentTimelineValue(u32 frameIndex) const;

    /// Check if compute work was submitted this frame.
    bool WasComputeSubmitted() const { return m_ComputeSubmittedThisFrame; }

    /// What type of work was last submitted.
    AsyncComputeWorkType GetLastWorkType() const { return m_LastWorkType; }

    /// End-of-frame cleanup. Call after graphics submit.
    void EndFrame();

    /// Runtime configuration
    void SetConfig(const AsyncComputeConfig& config) { m_Config = config; }
    const AsyncComputeConfig& GetConfig() const { return m_Config; }

    /// Check if a specific work type should use async compute.
    bool ShouldUseAsync(AsyncComputeWorkType type) const;

private:
    bool CreateCommandResources(u32 maxFramesInFlight);
    bool CreateSyncResources(u32 maxFramesInFlight);
    void DestroyResources();

    VulkanContext* m_Context = nullptr;

    // Queue state
    bool m_Supported = false;
    bool m_HasTimelineSemaphores = false;
    VkQueue m_ComputeQueue = VK_NULL_HANDLE;
    u32 m_ComputeQueueFamily = UINT32_MAX;

    // Command resources (per frame in flight)
    VkCommandPool m_ComputeCmdPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_ComputeCmdBuffers;
    bool m_Recording = false;

    // Binary semaphores (per frame in flight) — fallback when timeline not available
    std::vector<VkSemaphore> m_ComputeFinishedSemaphores;
    std::vector<VkSemaphore> m_GraphicsToComputeSemaphores;

    // Timeline semaphore (single, shared across frames — Vulkan 1.2+)
    VkSemaphore m_TimelineSemaphore = VK_NULL_HANDLE;
    u64 m_TimelineValue = 0; // Monotonically increasing
    std::vector<u64> m_FrameTimelineValues; // Value signaled per frame slot

    // Per-frame state
    u32 m_CurrentFrame = 0;
    bool m_ComputeSubmittedThisFrame = false;
    bool m_GraphicsSignaledThisFrame = false;
    AsyncComputeWorkType m_LastWorkType = AsyncComputeWorkType::GPUCulling;
    u32 m_MaxFramesInFlight = 2;

    AsyncComputeConfig m_Config;
};

} // namespace Renderer
} // namespace Enjin
