#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/WebGPU/WebGPUTypes.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderCompiler.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipeline.h"
#include "Enjin/Renderer/RenderBackend.h"
#include <memory>
#include <vector>
#include <string>

namespace Enjin {

class Window;

namespace Renderer {

class WebGPUShaderCompiler;
class WebGPUPipeline;
class WebGPUBufferManager;
class WebGPUTextureManager;
class WebGPUShaderManager;
class WebGPUPipelineManager;
class WebGPUBindGroupManager;
class WebGPURenderEncoder;

// WebGPU buffer handle returned by CreateBuffer — wraps GPU buffer with metadata.
struct WebGPUBufferHandle {
    WGPUBuffer buffer = nullptr;
    u64 size = 0;
    WGPUBufferUsage usage = 0;
};

// WebGPU texture handle returned by CreateTexture.
struct WebGPUTextureHandle {
    WGPUTexture texture = nullptr;
    WGPUTextureView view = nullptr;
    WGPUSampler sampler = nullptr;
    u32 width = 0;
    u32 height = 0;
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
};

// Main WebGPU renderer — parallel to VulkanRenderer but targeting the WebGPU API.
// Manages device, swap chain, command encoding, and GPU resource creation.
//
// Public interface matches VulkanRenderer's usage patterns in RenderSystem
// so the system can switch between them with compile-time #if guards.
class ENJIN_API WebGPURenderer : public IRenderBackend {
public:
    WebGPURenderer();
    ~WebGPURenderer();

    // --- Lifecycle (Window-based, primary entry point) ---
    bool Initialize(Window* window);

    // Begin a new frame — acquires swapchain + creates command encoder. No render pass yet.
    bool BeginFrameWebGPU();

    // Begin the main swapchain render pass (call after BeginFrameWebGPU and any pre-passes)
    void BeginMainRenderPass();

    // Begin a depth-only render pass for shadow mapping. Returns raw encoder (caller ends it).
    WGPURenderPassEncoder BeginDepthOnlyPass(WGPUTextureView depthView, u32 width, u32 height);

    // --- Swap chain ---
    u32 GetSwapChainWidth() const { return m_SwapChainWidth; }
    u32 GetSwapChainHeight() const { return m_SwapChainHeight; }

    // --- Buffer management ---
    WebGPUBufferHandle CreateBuffer(u64 size, WGPUBufferUsage usage, const void* data = nullptr);
    void UpdateBuffer(const WebGPUBufferHandle& buffer, const void* data, u64 size, u64 offset = 0);
    void DestroyBuffer(WebGPUBufferHandle& buffer);

    // --- Texture management ---
    WebGPUTextureHandle CreateTexture(u32 width, u32 height, WGPUTextureFormat format,
                                      WGPUTextureUsage usage, const void* pixelData = nullptr);
    void UploadTexture(const WebGPUTextureHandle& texture, const void* data, u32 width, u32 height);
    void DestroyTexture(WebGPUTextureHandle& texture);

    // --- Pipeline ---
    WGPURenderPipeline CreatePipeline(WGPUShaderModule vertexShader, WGPUShaderModule fragmentShader,
                                       WGPUPipelineLayout layout);

    // --- Bind groups ---
    WGPUBindGroup CreateBindGroup(WGPUBindGroupLayout layout,
                                   const std::vector<WGPUBindGroupEntry>& entries);

    // --- Render pass commands (valid between BeginFrame/EndFrame) ---
    void SetPipeline(WGPURenderPipeline pipeline);
    void SetBindGroup(u32 groupIndex, WGPUBindGroup group);
    void SetVertexBuffer(u32 slot, WGPUBuffer buffer, u64 offset = 0, u64 size = 0);
    void SetIndexBuffer(WGPUBuffer buffer, WGPUIndexFormat format, u64 offset = 0, u64 size = 0);
    void Draw(u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0, u32 firstInstance = 0);
    void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 firstIndex = 0,
                     i32 baseVertex = 0, u32 firstInstance = 0);
    void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth = 0.0f, f32 maxDepth = 1.0f);
    void SetScissor(u32 x, u32 y, u32 width, u32 height);

    // --- Accessors ---
    WGPUDevice GetDevice() const { return m_Device; }
    WGPUQueue GetQueue() const { return m_Queue; }
    u32 GetCurrentFrame() const { return m_FrameIndex; }

    // Depth texture for shadow passes
    WGPUTextureView GetDepthTextureView() const { return m_DepthTextureView; }

    // Subsystem access
    WebGPUShaderCompiler* GetShaderCompiler() { return m_ShaderCompiler.get(); }
    WebGPUPipeline* GetPipelineFactory() { return m_PipelineFactory.get(); }

    // --- IRenderBackend overrides ---
    bool Initialize(u32 width, u32 height) override;
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Resize(u32 width, u32 height) override;
    const char* GetBackendName() const override { return "WebGPU"; }
    PlatformCapabilities GetCapabilities() const override;
    GPUCapabilities GetGPUCapabilities() const override;
    u32 GetCurrentFrameIndex() const override { return m_FrameIndex; }
    u32 GetFramesInFlight() const override { return FRAMES_IN_FLIGHT; }
    u32 GetSwapchainWidth() const override { return m_SwapChainWidth; }
    u32 GetSwapchainHeight() const override { return m_SwapChainHeight; }
    void WaitForAllFrames() override;
    IGPUBufferManager* GetBufferManager() override;
    IGPUTextureManager* GetTextureManager() override;
    IGPUPipelineManager* GetPipelineManager() override;
    IGPUShaderManager* GetShaderManager() override;
    IGPUBindGroupManager* GetBindGroupManager() override;
    IRenderEncoder* BeginRenderPass(const GPURenderPassDesc& desc) override;
    void EndRenderPass(IRenderEncoder* encoder) override;

private:
    void CreateSwapChain();
    void CreateDepthTexture();

    static constexpr u32 FRAMES_IN_FLIGHT = 2;

    // Core WebGPU objects
    WGPUDevice m_Device = nullptr;
    WGPUQueue m_Queue = nullptr;
    WGPUInstance m_Instance = nullptr;
    WGPUSurface m_Surface = nullptr;

    // Per-frame state
    WGPUCommandEncoder m_CommandEncoder = nullptr;
    WGPURenderPassEncoder m_RenderPassEncoder = nullptr;
    WGPUTextureView m_CurrentSwapChainView = nullptr;
    u32 m_FrameIndex = 0;

    // Depth buffer
    WGPUTexture m_DepthTexture = nullptr;
    WGPUTextureView m_DepthTextureView = nullptr;

    // Swap chain dimensions
    u32 m_SwapChainWidth = 0;
    u32 m_SwapChainHeight = 0;

    // Subsystems
    std::unique_ptr<WebGPUShaderCompiler> m_ShaderCompiler;
    std::unique_ptr<WebGPUPipeline> m_PipelineFactory;

    Window* m_Window = nullptr;
    bool m_Initialized = false;

    // --- IRenderBackend sub-managers (owned, created in Initialize) ---
    std::unique_ptr<WebGPUBufferManager> m_BufferMgr;
    std::unique_ptr<WebGPUTextureManager> m_TextureMgr;
    std::unique_ptr<WebGPUShaderManager> m_ShaderMgr;
    std::unique_ptr<WebGPUPipelineManager> m_PipelineMgr;
    std::unique_ptr<WebGPUBindGroupManager> m_BindGroupMgr;
    std::unique_ptr<WebGPURenderEncoder> m_ActiveEncoder;
};

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
