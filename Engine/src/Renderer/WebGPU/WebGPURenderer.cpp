#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderCompiler.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipeline.h"
#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUTextureManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderEncoder.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <webgpu/webgpu.h>
#include <cstring>

// Helper: create a WGPUStringView from a C string literal
static WGPUStringView wgpuStr(const char* s) { return {s, WGPU_STRLEN}; }

namespace Enjin {
namespace Renderer {

WebGPURenderer::WebGPURenderer() = default;

WebGPURenderer::~WebGPURenderer() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

void WebGPURenderer::InitializeAsync(Window* window, std::function<void(bool)> onComplete) {
    if (m_Initialized) {
        if (onComplete) onComplete(true);
        return;
    }
    m_Window = window;
    m_InitCallback = std::move(onComplete);

    // Create WebGPU instance
    WGPUInstanceDescriptor instanceDesc = {};
    m_Instance = wgpuCreateInstance(&instanceDesc);
    if (!m_Instance) {
        ENJIN_LOG_ERROR(Core, "WebGPURenderer: Failed to create WebGPU instance");
        CompleteInit(false);
        return;
    }

    // Request adapter — AllowSpontaneous fires the callback from the browser
    // event loop, so control must return to the browser (no blocking wait)
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;

    WGPURequestAdapterCallbackInfo adapterCB = {};
    adapterCB.mode = WGPUCallbackMode_AllowSpontaneous;
    adapterCB.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* ud1, void* ud2) {
        (void)message; (void)ud2;
        auto* self = static_cast<WebGPURenderer*>(ud1);
        if (status != WGPURequestAdapterStatus_Success || !adapter) {
            ENJIN_LOG_ERROR(Core, "WebGPURenderer: Failed to get WebGPU adapter — browser may not support WebGPU");
            self->CompleteInit(false);
            return;
        }
        self->OnAdapterReady(adapter);
    };
    adapterCB.userdata1 = this;
    wgpuInstanceRequestAdapter(m_Instance, &adapterOpts, adapterCB);
}

void WebGPURenderer::OnAdapterReady(WGPUAdapter adapter) {
    ENJIN_LOG_INFO(Core, "WebGPU adapter acquired");
    m_PendingAdapter = adapter;

    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label = wgpuStr("EnjinDevice");

    WGPURequestDeviceCallbackInfo deviceCB = {};
    deviceCB.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceCB.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* ud1, void* ud2) {
        (void)message; (void)ud2;
        auto* self = static_cast<WebGPURenderer*>(ud1);
        if (status != WGPURequestDeviceStatus_Success || !device) {
            ENJIN_LOG_ERROR(Core, "WebGPURenderer: Failed to get WebGPU device");
            self->CompleteInit(false);
            return;
        }
        self->OnDeviceReady(device);
    };
    deviceCB.userdata1 = this;
    wgpuAdapterRequestDevice(adapter, &deviceDesc, deviceCB);
}

void WebGPURenderer::OnDeviceReady(WGPUDevice device) {
    m_Device = device;
    ENJIN_LOG_INFO(Core, "WebGPU device acquired");

    m_Queue = wgpuDeviceGetQueue(m_Device);
    wgpuAdapterRelease(m_PendingAdapter);
    m_PendingAdapter = nullptr;

    // Get canvas size — read from the actual canvas element if no window provided
    if (m_Window) {
        m_SwapChainWidth = m_Window->GetWidth();
        m_SwapChainHeight = m_Window->GetHeight();
    } else {
        // Read actual canvas pixel dimensions from JS
        m_SwapChainWidth = static_cast<u32>(EM_ASM_INT({
            var c = document.getElementById('game-canvas');
            return c ? (c.width || 1280) : 1280;
        }));
        m_SwapChainHeight = static_cast<u32>(EM_ASM_INT({
            var c = document.getElementById('game-canvas');
            return c ? (c.height || 720) : 720;
        }));
    }

    // Create surface and configure
    CreateSwapChain();
    CreateDepthTexture();

    // Initialize subsystems
    m_ShaderCompiler = std::make_unique<WebGPUShaderCompiler>(m_Device);
    m_PipelineFactory = std::make_unique<WebGPUPipeline>(m_Device);

    // Create abstract sub-managers for IRenderBackend interface
    m_BufferMgr = std::make_unique<WebGPUBufferManager>(this);
    m_TextureMgr = std::make_unique<WebGPUTextureManager>(this);
    m_ShaderMgr = std::make_unique<WebGPUShaderManager>(this);
    m_BindGroupMgr = std::make_unique<WebGPUBindGroupManager>(this, m_BufferMgr.get(), m_TextureMgr.get());
    m_PipelineMgr = std::make_unique<WebGPUPipelineManager>(this, m_ShaderMgr.get(), m_BindGroupMgr.get());

    m_Initialized = true;
    ENJIN_LOG_INFO(Core, "WebGPURenderer initialized (%ux%u)", m_SwapChainWidth, m_SwapChainHeight);
    CompleteInit(true);
}

void WebGPURenderer::CompleteInit(bool success) {
    if (!success && m_PendingAdapter) {
        wgpuAdapterRelease(m_PendingAdapter);
        m_PendingAdapter = nullptr;
    }
    if (m_InitCallback) {
        // Move out first — the callback may re-enter (e.g. retry InitializeAsync)
        auto cb = std::move(m_InitCallback);
        m_InitCallback = nullptr;
        cb(success);
    }
}

void WebGPURenderer::Shutdown() {
    if (!m_Initialized) return;

    // Destroy abstract sub-managers
    m_ActiveEncoder.reset();
    m_BindGroupMgr.reset();
    m_PipelineMgr.reset();
    m_ShaderMgr.reset();
    m_TextureMgr.reset();
    m_BufferMgr.reset();

    m_ShaderCompiler.reset();
    m_PipelineFactory.reset();

    if (m_DepthTextureView) { wgpuTextureViewRelease(m_DepthTextureView); m_DepthTextureView = nullptr; }
    if (m_DepthTexture) { wgpuTextureRelease(m_DepthTexture); m_DepthTexture = nullptr; }
    if (m_Surface) { wgpuSurfaceRelease(m_Surface); m_Surface = nullptr; }
    if (m_Queue) { wgpuQueueRelease(m_Queue); m_Queue = nullptr; }
    if (m_Device) { wgpuDeviceRelease(m_Device); m_Device = nullptr; }
    if (m_Instance) { wgpuInstanceRelease(m_Instance); m_Instance = nullptr; }

    m_Initialized = false;
    ENJIN_LOG_INFO(Core, "WebGPURenderer shut down");
}

// ============================================================================
// Frame
// ============================================================================

bool WebGPURenderer::BeginFrameWebGPU() {
    // Clear encoder from any previous frame — callers must not use a stale encoder
    // if this function returns false.
    m_CommandEncoder = nullptr;
    m_SwapchainWritten = false;

    if (!m_Initialized || !m_Surface) return false;

    // Get current surface texture
    WGPUSurfaceTexture surfTex = {};
    wgpuSurfaceGetCurrentTexture(m_Surface, &surfTex);
    if (surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfTex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        ENJIN_LOG_WARN(Core, "WebGPU: Failed to get surface texture");
        return false;
    }

    // Pass nullptr for default view descriptor — Dawn will use texture's own format/dimensions
    m_CurrentSwapChainView = wgpuTextureCreateView(surfTex.texture, nullptr);
    wgpuTextureRelease(surfTex.texture);

    if (!m_CurrentSwapChainView) return false;

    // Create command encoder (render passes started via BeginRenderPass)
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = wgpuStr("FrameEncoder");
    m_CommandEncoder = wgpuDeviceCreateCommandEncoder(m_Device, &encoderDesc);
    return true;
}

void WebGPURenderer::BeginMainRenderPass() {
    if (!m_CommandEncoder || !m_CurrentSwapChainView) return;
    if (m_RenderPassEncoder) return;  // Already active

    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = m_CurrentSwapChainView;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {0.4, 0.5, 0.65, 1.0};

    WGPURenderPassDepthStencilAttachment depthAttachment = {};
    depthAttachment.view = m_DepthTextureView;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthAttachment.stencilStoreOp = WGPUStoreOp_Store;

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    renderPassDesc.depthStencilAttachment = &depthAttachment;
    renderPassDesc.label = wgpuStr("MainPass");

    m_RenderPassEncoder = wgpuCommandEncoderBeginRenderPass(m_CommandEncoder, &renderPassDesc);
    if (m_RenderPassEncoder) m_SwapchainWritten = true;
}

WGPURenderPassEncoder WebGPURenderer::BeginDepthOnlyPass(WGPUTextureView depthView, u32 width, u32 height) {
    if (!m_CommandEncoder || !depthView) return nullptr;

    WGPURenderPassDepthStencilAttachment depthAttachment = {};
    depthAttachment.view = depthView;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.stencilLoadOp = WGPULoadOp_Undefined;
    depthAttachment.stencilStoreOp = WGPUStoreOp_Undefined;

    WGPURenderPassDescriptor passDesc = {};
    passDesc.colorAttachmentCount = 0;
    passDesc.colorAttachments = nullptr;
    passDesc.depthStencilAttachment = &depthAttachment;
    passDesc.label = wgpuStr("ShadowPass");

    return wgpuCommandEncoderBeginRenderPass(m_CommandEncoder, &passDesc);
}

void WebGPURenderer::EndFrame() {
    if (!m_Initialized) return;

    if (m_RenderPassEncoder) {
        wgpuRenderPassEncoderEnd(m_RenderPassEncoder);
        wgpuRenderPassEncoderRelease(m_RenderPassEncoder);
        m_RenderPassEncoder = nullptr;
    }

    if (m_CommandEncoder) {
        WGPUCommandBufferDescriptor cmdDesc = {};
        cmdDesc.label = wgpuStr("FrameCommands");
        WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(m_CommandEncoder, &cmdDesc);
        wgpuQueueSubmit(m_Queue, 1, &cmdBuffer);
        wgpuCommandBufferRelease(cmdBuffer);
        wgpuCommandEncoderRelease(m_CommandEncoder);
        m_CommandEncoder = nullptr;
    }

    if (m_CurrentSwapChainView) {
        wgpuTextureViewRelease(m_CurrentSwapChainView);
        m_CurrentSwapChainView = nullptr;
    }

    // Present — emdawnwebgpu auto-presents when requestAnimationFrame returns,
    // so wgpuSurfacePresent is not needed (and unsupported) on web.
#if !ENJIN_PLATFORM_WEB
    wgpuSurfacePresent(m_Surface);
#endif

    m_FrameIndex = (m_FrameIndex + 1) % FRAMES_IN_FLIGHT;
}

// ============================================================================
// Surface & depth
// ============================================================================

void WebGPURenderer::CreateSwapChain() {
    // Create surface from canvas
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
    canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvasDesc.selector = wgpuStr("#game-canvas");

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = &canvasDesc.chain;
    m_Surface = wgpuInstanceCreateSurface(m_Instance, &surfDesc);

    // Configure surface (replaces swap chain in new Dawn API)
    WGPUSurfaceConfiguration config = {};
    config.device = m_Device;
    config.format = GetPreferredSwapChainFormat();
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = m_SwapChainWidth;
    config.height = m_SwapChainHeight;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(m_Surface, &config);
}

void WebGPURenderer::CreateDepthTexture() {
    if (m_DepthTextureView) { wgpuTextureViewRelease(m_DepthTextureView); m_DepthTextureView = nullptr; }
    if (m_DepthTexture) { wgpuTextureRelease(m_DepthTexture); m_DepthTexture = nullptr; }

    WGPUTextureDescriptor depthDesc = {};
    depthDesc.usage = WGPUTextureUsage_RenderAttachment;
    depthDesc.dimension = WGPUTextureDimension_2D;
    depthDesc.size = { m_SwapChainWidth, m_SwapChainHeight, 1 };
    depthDesc.format = GetDepthStencilFormat();
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    m_DepthTexture = wgpuDeviceCreateTexture(m_Device, &depthDesc);

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format = GetDepthStencilFormat();
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;
    m_DepthTextureView = wgpuTextureCreateView(m_DepthTexture, &viewDesc);
}

void WebGPURenderer::Resize(u32 width, u32 height) {
    if (width == 0 || height == 0) return;
    // Async init may still be waiting on the device; the swap chain is created
    // from live canvas dimensions at device-ready time, so nothing is lost.
    if (!m_Initialized) return;
    m_SwapChainWidth = width;
    m_SwapChainHeight = height;

    if (m_Surface) {
        wgpuSurfaceUnconfigure(m_Surface);
        wgpuSurfaceRelease(m_Surface);
        m_Surface = nullptr;
    }
    CreateSwapChain();
    CreateDepthTexture();
}

// ============================================================================
// Buffer management
// ============================================================================

WebGPUBufferHandle WebGPURenderer::CreateBuffer(u64 size, WGPUBufferUsage usage, const void* data) {
    WebGPUBufferHandle handle;
    handle.size = size;
    handle.usage = usage;

    WGPUBufferDescriptor desc = {};
    desc.size = size;
    desc.usage = usage | WGPUBufferUsage_CopyDst;
    desc.mappedAtCreation = (data != nullptr);
    handle.buffer = wgpuDeviceCreateBuffer(m_Device, &desc);

    if (data && handle.buffer) {
        void* mapped = wgpuBufferGetMappedRange(handle.buffer, 0, size);
        if (mapped) {
            std::memcpy(mapped, data, size);
            wgpuBufferUnmap(handle.buffer);
        }
    }

    return handle;
}

void WebGPURenderer::UpdateBuffer(const WebGPUBufferHandle& buffer, const void* data, u64 size, u64 offset) {
    if (!buffer.buffer || !data || size == 0) return;
    wgpuQueueWriteBuffer(m_Queue, buffer.buffer, offset, data, size);
}

void WebGPURenderer::DestroyBuffer(WebGPUBufferHandle& buffer) {
    if (buffer.buffer) {
        wgpuBufferRelease(buffer.buffer);
        buffer.buffer = nullptr;
    }
    buffer.size = 0;
}

// ============================================================================
// Texture management
// ============================================================================

WebGPUTextureHandle WebGPURenderer::CreateTexture(u32 width, u32 height, WGPUTextureFormat format,
                                                    WGPUTextureUsage usage, const void* pixelData) {
    WebGPUTextureHandle handle;
    handle.width = width;
    handle.height = height;
    handle.format = format;

    WGPUTextureDescriptor texDesc = {};
    texDesc.usage = usage | WGPUTextureUsage_CopyDst;
    texDesc.dimension = WGPUTextureDimension_2D;
    texDesc.size = { width, height, 1 };
    texDesc.format = format;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    handle.texture = wgpuDeviceCreateTexture(m_Device, &texDesc);

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format = format;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 1;
    handle.view = wgpuTextureCreateView(handle.texture, &viewDesc);

    bool isDepth = (format == WGPUTextureFormat_Depth32Float ||
                    format == WGPUTextureFormat_Depth24Plus ||
                    format == WGPUTextureFormat_Depth16Unorm);

    WGPUSamplerDescriptor samplerDesc = {};
    if (isDepth) {
        // Depth textures get a comparison sampler (for shadow mapping)
        samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
        samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
        samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
        samplerDesc.magFilter = WGPUFilterMode_Linear;
        samplerDesc.minFilter = WGPUFilterMode_Linear;
        samplerDesc.compare = WGPUCompareFunction_LessEqual;
        samplerDesc.maxAnisotropy = 1;
    } else {
        samplerDesc.addressModeU = WGPUAddressMode_Repeat;
        samplerDesc.addressModeV = WGPUAddressMode_Repeat;
        samplerDesc.addressModeW = WGPUAddressMode_Repeat;
        samplerDesc.magFilter = WGPUFilterMode_Linear;
        samplerDesc.minFilter = WGPUFilterMode_Linear;
        samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
        samplerDesc.maxAnisotropy = 1;
    }
    handle.sampler = wgpuDeviceCreateSampler(m_Device, &samplerDesc);

    if (pixelData) {
        UploadTexture(handle, pixelData, width, height);
    }

    return handle;
}

WebGPUTextureHandle WebGPURenderer::CreateCubemapTexture(u32 size, WGPUTextureFormat format, WGPUTextureUsage usage) {
    WebGPUTextureHandle handle;
    handle.width = size;
    handle.height = size;
    handle.format = format;

    WGPUTextureDescriptor texDesc = {};
    texDesc.usage = usage;
    texDesc.dimension = WGPUTextureDimension_2D;
    texDesc.size = { size, size, 6 };  // 6 array layers = cubemap
    texDesc.format = format;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    handle.texture = wgpuDeviceCreateTexture(m_Device, &texDesc);

    // Cubemap view (all 6 faces)
    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format = format;
    viewDesc.dimension = WGPUTextureViewDimension_Cube;
    viewDesc.mipLevelCount = 1;
    viewDesc.arrayLayerCount = 6;
    handle.view = wgpuTextureCreateView(handle.texture, &viewDesc);

    // Comparison sampler for shadow mapping
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDesc.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.compare = WGPUCompareFunction_LessEqual;
    samplerDesc.maxAnisotropy = 1;
    handle.sampler = wgpuDeviceCreateSampler(m_Device, &samplerDesc);

    return handle;
}

WGPUTextureView WebGPURenderer::CreateCubeFaceView(WGPUTexture texture, WGPUTextureFormat format, u32 faceIndex) {
    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.format = format;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = faceIndex;
    viewDesc.arrayLayerCount = 1;
    return wgpuTextureCreateView(texture, &viewDesc);
}

void WebGPURenderer::UploadTexture(const WebGPUTextureHandle& texture, const void* data,
                                     u32 width, u32 height) {
    if (!texture.texture || !data) return;

    WGPUTexelCopyTextureInfo dst = {};
    dst.texture = texture.texture;
    dst.mipLevel = 0;
    dst.origin = { 0, 0, 0 };
    dst.aspect = WGPUTextureAspect_All;

    u32 bytesPerPixel = 4;
    WGPUTexelCopyBufferLayout layout = {};
    layout.offset = 0;
    layout.bytesPerRow = width * bytesPerPixel;
    layout.rowsPerImage = height;

    WGPUExtent3D extent = { width, height, 1 };
    wgpuQueueWriteTexture(m_Queue, &dst, data, width * height * bytesPerPixel, &layout, &extent);
}

void WebGPURenderer::DestroyTexture(WebGPUTextureHandle& texture) {
    if (texture.sampler) { wgpuSamplerRelease(texture.sampler); texture.sampler = nullptr; }
    if (texture.view) { wgpuTextureViewRelease(texture.view); texture.view = nullptr; }
    if (texture.texture) { wgpuTextureRelease(texture.texture); texture.texture = nullptr; }
    texture.width = 0;
    texture.height = 0;
}

// ============================================================================
// Pipeline creation
// ============================================================================

WGPURenderPipeline WebGPURenderer::CreatePipeline(WGPUShaderModule vertexShader,
                                                     WGPUShaderModule fragmentShader,
                                                     WGPUPipelineLayout layout) {
    WebGPURenderPipelineDesc desc;
    desc.vertexShader = vertexShader;
    desc.fragmentShader = fragmentShader;
    desc.layout = layout;
    return m_PipelineFactory->CreateRenderPipeline(desc);
}

// ============================================================================
// Bind groups
// ============================================================================

WGPUBindGroup WebGPURenderer::CreateBindGroup(WGPUBindGroupLayout layout,
                                                const std::vector<WGPUBindGroupEntry>& entries) {
    WGPUBindGroupDescriptor desc = {};
    desc.layout = layout;
    desc.entryCount = static_cast<u32>(entries.size());
    desc.entries = entries.data();
    return wgpuDeviceCreateBindGroup(m_Device, &desc);
}

// ============================================================================
// Render pass commands
// ============================================================================

void WebGPURenderer::SetPipeline(WGPURenderPipeline pipeline) {
    if (m_RenderPassEncoder && pipeline)
        wgpuRenderPassEncoderSetPipeline(m_RenderPassEncoder, pipeline);
}

void WebGPURenderer::SetBindGroup(u32 groupIndex, WGPUBindGroup group) {
    if (m_RenderPassEncoder && group)
        wgpuRenderPassEncoderSetBindGroup(m_RenderPassEncoder, groupIndex, group, 0, nullptr);
}

void WebGPURenderer::SetVertexBuffer(u32 slot, WGPUBuffer buffer, u64 offset, u64 size) {
    if (m_RenderPassEncoder && buffer)
        wgpuRenderPassEncoderSetVertexBuffer(m_RenderPassEncoder, slot, buffer, offset,
                                              size == 0 ? WGPU_WHOLE_SIZE : size);
}

void WebGPURenderer::SetIndexBuffer(WGPUBuffer buffer, WGPUIndexFormat format, u64 offset, u64 size) {
    if (m_RenderPassEncoder && buffer)
        wgpuRenderPassEncoderSetIndexBuffer(m_RenderPassEncoder, buffer, format, offset,
                                             size == 0 ? WGPU_WHOLE_SIZE : size);
}

void WebGPURenderer::Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
    if (m_RenderPassEncoder)
        wgpuRenderPassEncoderDraw(m_RenderPassEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
}

void WebGPURenderer::DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
                                   i32 baseVertex, u32 firstInstance) {
    if (m_RenderPassEncoder)
        wgpuRenderPassEncoderDrawIndexed(m_RenderPassEncoder, indexCount, instanceCount,
                                          firstIndex, baseVertex, firstInstance);
}

void WebGPURenderer::SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) {
    if (m_RenderPassEncoder)
        wgpuRenderPassEncoderSetViewport(m_RenderPassEncoder, x, y, width, height, minDepth, maxDepth);
}

void WebGPURenderer::SetScissor(u32 x, u32 y, u32 width, u32 height) {
    if (m_RenderPassEncoder)
        wgpuRenderPassEncoderSetScissorRect(m_RenderPassEncoder, x, y, width, height);
}

void WebGPURenderer::WaitForAllFrames() {
    // WebGPU doesn't have explicit fence-wait — browser ensures ordering.
}

// ============================================================================
// IRenderBackend overrides
// ============================================================================

bool WebGPURenderer::Initialize(u32 /*width*/, u32 /*height*/) {
    ENJIN_LOG_ERROR(Core, "WebGPURenderer::Initialize(u32,u32) not supported — use Initialize(Window*)");
    return false;
}

void WebGPURenderer::BeginFrame() {
    BeginFrameWebGPU();
}

void WebGPURenderer::Present() {
    // WebGPU presents in EndFrame — no-op here
}

PlatformCapabilities WebGPURenderer::GetCapabilities() const {
    PlatformCapabilities caps;
    caps.hasWebGPU = true;
    caps.maxTextureSize = 8192;
    caps.preferredCompression = TextureCompression::ASTC;
    return caps;
}

GPUCapabilities WebGPURenderer::GetGPUCapabilities() const {
    return MakeWebGPUCapabilities();
}

IGPUBufferManager* WebGPURenderer::GetBufferManager() { return m_BufferMgr.get(); }
IGPUTextureManager* WebGPURenderer::GetTextureManager() { return m_TextureMgr.get(); }
IGPUPipelineManager* WebGPURenderer::GetPipelineManager() { return m_PipelineMgr.get(); }
IGPUShaderManager* WebGPURenderer::GetShaderManager() { return m_ShaderMgr.get(); }
IGPUBindGroupManager* WebGPURenderer::GetBindGroupManager() { return m_BindGroupMgr.get(); }

IRenderEncoder* WebGPURenderer::BeginRenderPass(const GPURenderPassDesc& desc) {
    // Default desc (width=0) = main swapchain render pass
    if (desc.width == 0 && desc.height == 0) {
        if (!m_RenderPassEncoder) BeginMainRenderPass();
        if (!m_RenderPassEncoder) return nullptr;
        m_ActiveEncoder = std::make_unique<WebGPURenderEncoder>(
            this, m_RenderPassEncoder,
            m_PipelineMgr.get(), m_BufferMgr.get(), m_BindGroupMgr.get());
        return m_ActiveEncoder.get();
    }
    return nullptr;
}

void WebGPURenderer::EndRenderPass(IRenderEncoder* encoder) {
    if (m_ActiveEncoder.get() == encoder) {
        m_ActiveEncoder.reset();
    }
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
