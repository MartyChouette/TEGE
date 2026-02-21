#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderCompiler.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipeline.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <cstring>

namespace Enjin {
namespace Renderer {

WebGPURenderer::~WebGPURenderer() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool WebGPURenderer::Initialize(Window* window) {
    if (m_Initialized) return true;
    m_Window = window;

    // Obtain the WebGPU device from Emscripten (pre-created by the browser)
    m_Device = emscripten_webgpu_get_device();
    if (!m_Device) {
        ENJIN_LOG_ERROR(Core, "WebGPURenderer: Failed to get WebGPU device");
        return false;
    }

    m_Queue = wgpuDeviceGetQueue(m_Device);

    // Set error callback
    wgpuDeviceSetUncapturedErrorCallback(m_Device,
        [](WGPUErrorType type, const char* message, void* userdata) {
            (void)userdata;
            ENJIN_LOG_ERROR(Core, "WebGPU error (type %d): %s", static_cast<int>(type), message);
        }, nullptr);

    // Get canvas size
    m_SwapChainWidth = window ? window->GetWidth() : 800;
    m_SwapChainHeight = window ? window->GetHeight() : 600;

    // Create swap chain (surface)
    CreateSwapChain();
    CreateDepthTexture();

    // Initialize subsystems
    m_ShaderCompiler = std::make_unique<WebGPUShaderCompiler>(m_Device);
    m_PipelineFactory = std::make_unique<WebGPUPipeline>(m_Device);

    m_Initialized = true;
    ENJIN_LOG_INFO(Core, "WebGPURenderer initialized (%ux%u)", m_SwapChainWidth, m_SwapChainHeight);
    return true;
}

void WebGPURenderer::Shutdown() {
    if (!m_Initialized) return;

    m_ShaderCompiler.reset();
    m_PipelineFactory.reset();

    if (m_DepthTextureView) { wgpuTextureViewRelease(m_DepthTextureView); m_DepthTextureView = nullptr; }
    if (m_DepthTexture) { wgpuTextureRelease(m_DepthTexture); m_DepthTexture = nullptr; }
    if (m_SwapChain) { wgpuSwapChainRelease(m_SwapChain); m_SwapChain = nullptr; }
    if (m_Queue) { wgpuQueueRelease(m_Queue); m_Queue = nullptr; }
    if (m_Device) { wgpuDeviceRelease(m_Device); m_Device = nullptr; }

    m_Initialized = false;
    ENJIN_LOG_INFO(Core, "WebGPURenderer shut down");
}

// ============================================================================
// Frame
// ============================================================================

bool WebGPURenderer::BeginFrame() {
    if (!m_Initialized) return false;

    // Acquire swap chain texture
    m_CurrentSwapChainView = wgpuSwapChainGetCurrentTextureView(m_SwapChain);
    if (!m_CurrentSwapChainView) {
        ENJIN_LOG_WARN(Core, "WebGPU: Failed to acquire swap chain texture view");
        return false;
    }

    // Create command encoder
    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = "FrameEncoder";
    m_CommandEncoder = wgpuDeviceCreateCommandEncoder(m_Device, &encoderDesc);

    // Begin render pass
    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = m_CurrentSwapChainView;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {0.1, 0.1, 0.12, 1.0};

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
    renderPassDesc.label = "MainPass";

    m_RenderPassEncoder = wgpuCommandEncoderBeginRenderPass(m_CommandEncoder, &renderPassDesc);
    return true;
}

void WebGPURenderer::EndFrame() {
    if (!m_Initialized) return;

    // End render pass
    if (m_RenderPassEncoder) {
        wgpuRenderPassEncoderEnd(m_RenderPassEncoder);
        wgpuRenderPassEncoderRelease(m_RenderPassEncoder);
        m_RenderPassEncoder = nullptr;
    }

    // Finish command buffer and submit
    if (m_CommandEncoder) {
        WGPUCommandBufferDescriptor cmdDesc = {};
        cmdDesc.label = "FrameCommands";
        WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(m_CommandEncoder, &cmdDesc);
        wgpuQueueSubmit(m_Queue, 1, &cmdBuffer);
        wgpuCommandBufferRelease(cmdBuffer);
        wgpuCommandEncoderRelease(m_CommandEncoder);
        m_CommandEncoder = nullptr;
    }

    // Release swap chain view
    if (m_CurrentSwapChainView) {
        wgpuTextureViewRelease(m_CurrentSwapChainView);
        m_CurrentSwapChainView = nullptr;
    }

    m_FrameIndex = (m_FrameIndex + 1) % FRAMES_IN_FLIGHT;
}

// ============================================================================
// Swap chain & depth
// ============================================================================

void WebGPURenderer::CreateSwapChain() {
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc = {};
    canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc.selector = "#game-canvas";

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = &canvasDesc.chain;
    m_Surface = wgpuInstanceCreateSurface(nullptr, &surfDesc);

    WGPUSwapChainDescriptor swapDesc = {};
    swapDesc.usage = WGPUTextureUsage_RenderAttachment;
    swapDesc.format = GetPreferredSwapChainFormat();
    swapDesc.width = m_SwapChainWidth;
    swapDesc.height = m_SwapChainHeight;
    swapDesc.presentMode = WGPUPresentMode_Fifo;
    m_SwapChain = wgpuDeviceCreateSwapChain(m_Device, m_Surface, &swapDesc);
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
    m_SwapChainWidth = width;
    m_SwapChainHeight = height;

    if (m_SwapChain) { wgpuSwapChainRelease(m_SwapChain); m_SwapChain = nullptr; }
    CreateSwapChain();
    CreateDepthTexture();
}

// ============================================================================
// Buffer management
// ============================================================================

WebGPUBufferHandle WebGPURenderer::CreateBuffer(u64 size, WGPUBufferUsageFlags usage, const void* data) {
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
                                                    WGPUTextureUsageFlags usage, const void* pixelData) {
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

    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_Repeat;
    samplerDesc.addressModeV = WGPUAddressMode_Repeat;
    samplerDesc.addressModeW = WGPUAddressMode_Repeat;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.maxAnisotropy = 1;
    handle.sampler = wgpuDeviceCreateSampler(m_Device, &samplerDesc);

    if (pixelData) {
        UploadTexture(handle, pixelData, width, height);
    }

    return handle;
}

void WebGPURenderer::UploadTexture(const WebGPUTextureHandle& texture, const void* data,
                                     u32 width, u32 height) {
    if (!texture.texture || !data) return;

    WGPUImageCopyTexture dst = {};
    dst.texture = texture.texture;
    dst.mipLevel = 0;
    dst.origin = { 0, 0, 0 };
    dst.aspect = WGPUTextureAspect_All;

    u32 bytesPerPixel = 4; // Assume RGBA8
    WGPUTextureDataLayout layout = {};
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
    if (m_RenderPassEncoder && pipeline) {
        wgpuRenderPassEncoderSetPipeline(m_RenderPassEncoder, pipeline);
    }
}

void WebGPURenderer::SetBindGroup(u32 groupIndex, WGPUBindGroup group) {
    if (m_RenderPassEncoder && group) {
        wgpuRenderPassEncoderSetBindGroup(m_RenderPassEncoder, groupIndex, group, 0, nullptr);
    }
}

void WebGPURenderer::SetVertexBuffer(u32 slot, WGPUBuffer buffer, u64 offset, u64 size) {
    if (m_RenderPassEncoder && buffer) {
        wgpuRenderPassEncoderSetVertexBuffer(m_RenderPassEncoder, slot, buffer, offset,
                                              size == 0 ? WGPU_WHOLE_SIZE : size);
    }
}

void WebGPURenderer::SetIndexBuffer(WGPUBuffer buffer, WGPUIndexFormat format, u64 offset, u64 size) {
    if (m_RenderPassEncoder && buffer) {
        wgpuRenderPassEncoderSetIndexBuffer(m_RenderPassEncoder, buffer, format, offset,
                                             size == 0 ? WGPU_WHOLE_SIZE : size);
    }
}

void WebGPURenderer::Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
    if (m_RenderPassEncoder) {
        wgpuRenderPassEncoderDraw(m_RenderPassEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

void WebGPURenderer::DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
                                   i32 baseVertex, u32 firstInstance) {
    if (m_RenderPassEncoder) {
        wgpuRenderPassEncoderDrawIndexed(m_RenderPassEncoder, indexCount, instanceCount,
                                          firstIndex, baseVertex, firstInstance);
    }
}

void WebGPURenderer::SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) {
    if (m_RenderPassEncoder) {
        wgpuRenderPassEncoderSetViewport(m_RenderPassEncoder, x, y, width, height, minDepth, maxDepth);
    }
}

void WebGPURenderer::SetScissor(u32 x, u32 y, u32 width, u32 height) {
    if (m_RenderPassEncoder) {
        wgpuRenderPassEncoderSetScissorRect(m_RenderPassEncoder, x, y, width, height);
    }
}

void WebGPURenderer::WaitForAllFrames() {
    // WebGPU doesn't have an explicit fence-wait like Vulkan.
    // Work is submitted asynchronously; the browser ensures ordering.
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
