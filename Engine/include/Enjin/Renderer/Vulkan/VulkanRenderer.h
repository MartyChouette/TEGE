#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Renderer/RenderBackend.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanSwapchain.h"
#include <vulkan/vulkan.h>
#include <atomic>
#include <vector>
#include <memory>
#include <functional>

/**
 * @file VulkanRenderer.h
 * @brief Main Vulkan rendering interface
 * @author Enjin Engine Team
 * @date 2025
 */

namespace Enjin {
namespace Renderer {

// Forward declarations for abstract manager implementations
class VulkanBufferManager;
class VulkanTextureManager;
class VulkanPipelineManager;
class VulkanShaderManager;
class VulkanBindGroupManager;
class VulkanRenderEncoder;

/**
 * @brief GPU timestamp query indices for per-pass timing
 */
enum GPUTimestampQuery : u32 {
    GPU_TS_SHADOW_BEGIN = 0, GPU_TS_SHADOW_END = 1,
    GPU_TS_MAIN_BEGIN = 2, GPU_TS_MAIN_END = 3,
    GPU_TS_RT_BEGIN = 4, GPU_TS_RT_END = 5,
    GPU_TS_POSTPROCESS_BEGIN = 6, GPU_TS_POSTPROCESS_END = 7,
    GPU_TS_COUNT = 8
};

/**
 * @brief Vulkan renderer - main rendering interface
 *
 * Manages the Vulkan rendering state, swapchain, and command buffers.
 */
class ENJIN_API VulkanRenderer : public IRenderBackend {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    // Initialize with a window (primary entry point for applications)
    bool Initialize(Window* window);

    // Begin a new frame — acquires swapchain image and begins command buffer recording.
    // Does NOT start the main render pass — call BeginMainRenderPass() for that.
    // Returns false if the frame could not be started (e.g. swapchain lost).
    bool BeginFrameVulkan();

    // Begin the main render pass (call after BeginFrameVulkan and pre-render passes)
    void BeginMainRenderPass();

    bool IsMainRenderPassActive() const { return m_IsMainRenderPassActive; }

    VkCommandBuffer GetCurrentCommandBuffer() const;
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VulkanContext* GetContext() const { return m_Context.get(); }
    VulkanSwapchain* GetSwapchain() const { return m_Swapchain.get(); }
    VkExtent2D GetSwapchainExtent() const { return m_Swapchain ? m_Swapchain->GetExtent() : VkExtent2D{0, 0}; }

    void OnWindowResize(u32 width, u32 height);

    bool IsDeviceLost() const { return m_DeviceLost; }

    // Flag set by window resize callback to trigger swapchain recreation
    void SetFramebufferResized(bool resized) { m_FramebufferResized = resized; }

    // Deferred VSync change — safe to call mid-frame, applied at end of EndFrame()
    void RequestVSyncChange(bool enabled);
    bool IsVSyncEnabled() const;

    // HDR output — recreates swapchain, render pass, and framebuffers
    void SetHDREnabled(bool enabled);
    bool IsHDREnabled() const;
    u32 GetHDROutputMode() const;
    // Recreate the main render pass (e.g., after swapchain format change or MSAA toggle)
    bool RecreateRenderPass();

    // MSAA: set the multisample count (1, 2, 4, or 8). Triggers render pass + framebuffer
    // recreation. The caller must also recreate all pipelines that reference this render pass.
    // Returns false if the requested sample count is not supported by the hardware.
    bool SetMSAASamples(VkSampleCountFlagBits samples);
    VkSampleCountFlagBits GetMSAASamples() const { return m_MSAASamples; }

    // Register a callback to be notified after swapchain recreation (e.g., PostProcessing)
    using ResizeCallback = std::function<void(u32, u32)>;
    void AddResizeCallback(ResizeCallback callback) { m_ResizeCallbacks.push_back(std::move(callback)); }

    // Async compute: begin/end compute command buffer, submit to compute queue
    VkCommandBuffer GetCurrentComputeCommandBuffer() const;
    bool BeginComputeCommandBuffer();
    void EndComputeCommandBuffer();
    void SubmitCompute();  // Submit compute queue, signal semaphore
    bool HasAsyncCompute() const { return m_ComputeCommandPool != VK_NULL_HANDLE; }

    // Insert a pipeline barrier in the graphics command buffer that waits for the
    // compute semaphore. Use this for mid-frame synchronization when graphics work
    // depends on compute results (e.g., shadow pass finishes → wait for culling).
    // This is a lighter-weight alternative to waiting at submit time.
    void InsertComputeToGraphicsBarrier(VkCommandBuffer graphicsCmd, VkPipelineStageFlags dstStage);

    // Signal from graphics to compute: allows compute queue to wait until a specific
    // graphics stage completes (e.g., depth pass → compute can read depth for HiZ).
    void SignalGraphicsToCompute(VkCommandBuffer graphicsCmd);
    bool WasComputeSubmittedThisFrame() const { return m_ComputeSubmittedThisFrame; }

    // External semaphore wait: allows an external system (e.g., AsyncComputeScheduler)
    // to add a semaphore that the graphics queue submit should wait on.
    // The semaphore is consumed on the next SubmitCommandBuffer() call.
    void AddExternalWaitSemaphore(VkSemaphore semaphore, VkPipelineStageFlags waitStage);

    // Variable Rate Shading: set the shading rate image for the next render pass
    void SetShadingRateImage(VkImageView imageView, VkExtent2D tileSize) {
        m_ShadingRateImageView = imageView;
        m_ShadingRateTileSize = tileSize;
    }
    void ClearShadingRateImage() { m_ShadingRateImageView = VK_NULL_HANDLE; }

    // GPU timestamp queries — per-pass GPU timing in milliseconds
    VkQueryPool GetTimestampPool(u32 frameIdx) const { return m_TimestampPools[frameIdx % MAX_FRAMES_IN_FLIGHT]; }
    const f32* GetGPUPassTimes() const { return m_GPUPassTimes; }

    // --- IRenderBackend overrides ---
    bool Initialize(u32 width, u32 height) override;
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Resize(u32 width, u32 height) override;
    const char* GetBackendName() const override { return "Vulkan"; }
    PlatformCapabilities GetCapabilities() const override;
    GPUCapabilities GetGPUCapabilities() const override;
    u32 GetCurrentFrameIndex() const override { return m_CurrentFrame; }
    u32 GetFramesInFlight() const override { return MAX_FRAMES_IN_FLIGHT; }
    u32 GetSwapchainWidth() const override;
    u32 GetSwapchainHeight() const override;
    void WaitForAllFrames() override;
    IGPUBufferManager* GetBufferManager() override;
    IGPUTextureManager* GetTextureManager() override;
    IGPUPipelineManager* GetPipelineManager() override;
    IGPUShaderManager* GetShaderManager() override;
    IGPUBindGroupManager* GetBindGroupManager() override;
    IRenderEncoder* BeginRenderPass(const GPURenderPassDesc& desc) override;
    void EndRenderPass(IRenderEncoder* encoder) override;

private:
    bool CreateSurface();
    bool CreateRenderPass();
    bool CreateCommandPool();
    bool CreateCommandBuffers();
    bool CreateSyncObjects();
    void DestroySyncObjects();
    bool AcquireNextImage();
    void SubmitCommandBuffer();

    Window* m_Window = nullptr;
    std::unique_ptr<VulkanContext> m_Context;
    std::unique_ptr<VulkanSwapchain> m_Swapchain;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_CommandBuffers;

    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    std::vector<VkFence> m_InFlightFences;
    std::vector<VkFence> m_ImagesInFlight;

    u32 m_CurrentFrame = 0;
    u32 m_CurrentImageIndex = 0;
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    f32 m_FenceWaitMs = 0.0f;       // Time spent waiting for GPU fence (spike indicator)
    f32 m_AcquireMs = 0.0f;         // Time spent acquiring swapchain image
public:
    f32 GetFenceWaitMs() const { return m_FenceWaitMs; }
    f32 GetAcquireMs() const { return m_AcquireMs; }
private:
    bool m_IsFrameStarted = false;
    bool m_IsMainRenderPassActive = false;
    bool m_FramebufferResized = false;
    std::atomic<bool> m_DeviceLost{false};
    bool m_VSyncChangeRequested = false;
    bool m_VSyncDesiredState = false;

    std::vector<ResizeCallback> m_ResizeCallbacks;

    // Async compute queue resources
    VkCommandPool m_ComputeCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_ComputeCommandBuffers;
    std::vector<VkSemaphore> m_ComputeFinishedSemaphores;
    std::vector<VkSemaphore> m_GraphicsToComputeSemaphores; // Graphics signals compute can start
    bool m_ComputeRecording = false;
    bool m_ComputeSubmittedThisFrame = false; // True if SubmitCompute was called
    bool m_GraphicsToComputeSignaled = false; // True if SignalGraphicsToCompute was called

    // External semaphores to wait on during graphics submit
    struct ExternalWait {
        VkSemaphore semaphore = VK_NULL_HANDLE;
        VkPipelineStageFlags waitStage = 0;
    };
    std::vector<ExternalWait> m_ExternalWaitSemaphores;

    // MSAA sample count (VK_SAMPLE_COUNT_1_BIT = no MSAA)
    VkSampleCountFlagBits m_MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // VRS shading rate image (set by RenderSystem when VRS is active)
    VkImageView m_ShadingRateImageView = VK_NULL_HANDLE;
    VkExtent2D m_ShadingRateTileSize = { 16, 16 };

    bool CreateComputeResources();
    void DestroyComputeResources();

    // GPU timestamp query pools (one per frame in flight) and results
    VkQueryPool m_TimestampPools[2] = {};
    u64 m_TimestampResults[GPU_TS_COUNT] = {};
    f32 m_GPUPassTimes[4] = {};  // shadow, main, RT, postprocess (ms)

    // --- IRenderBackend sub-managers (owned, created in Initialize) ---
    std::unique_ptr<VulkanBufferManager> m_BufferMgr;
    std::unique_ptr<VulkanTextureManager> m_TextureMgr;
    std::unique_ptr<VulkanShaderManager> m_ShaderMgr;
    std::unique_ptr<VulkanPipelineManager> m_PipelineMgr;
    std::unique_ptr<VulkanBindGroupManager> m_BindGroupMgr;
    std::unique_ptr<VulkanRenderEncoder> m_ActiveEncoder;
};

} // namespace Renderer
} // namespace Enjin
