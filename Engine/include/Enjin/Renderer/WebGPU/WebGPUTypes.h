#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#if ENJIN_PLATFORM_WEB

#include <webgpu/webgpu.h>
#include <memory>

namespace Enjin {
namespace Renderer {

// ============================================================================
// RAII wrappers for WebGPU handles
// ============================================================================

// Custom deleters for WebGPU reference-counted objects
struct WGPUDeviceDeleter {
    void operator()(WGPUDevice d) const { if (d) wgpuDeviceRelease(d); }
};

struct WGPUQueueDeleter {
    void operator()(WGPUQueue q) const { if (q) wgpuQueueRelease(q); }
};

struct WGPUBufferDeleter {
    void operator()(WGPUBuffer b) const { if (b) wgpuBufferRelease(b); }
};

struct WGPUTextureDeleter {
    void operator()(WGPUTexture t) const { if (t) wgpuTextureRelease(t); }
};

struct WGPUTextureViewDeleter {
    void operator()(WGPUTextureView v) const { if (v) wgpuTextureViewRelease(v); }
};

struct WGPUSamplerDeleter {
    void operator()(WGPUSampler s) const { if (s) wgpuSamplerRelease(s); }
};

struct WGPURenderPipelineDeleter {
    void operator()(WGPURenderPipeline p) const { if (p) wgpuRenderPipelineRelease(p); }
};

struct WGPUComputePipelineDeleter {
    void operator()(WGPUComputePipeline p) const { if (p) wgpuComputePipelineRelease(p); }
};

struct WGPUShaderModuleDeleter {
    void operator()(WGPUShaderModule m) const { if (m) wgpuShaderModuleRelease(m); }
};

struct WGPUBindGroupDeleter {
    void operator()(WGPUBindGroup g) const { if (g) wgpuBindGroupRelease(g); }
};

struct WGPUBindGroupLayoutDeleter {
    void operator()(WGPUBindGroupLayout l) const { if (l) wgpuBindGroupLayoutRelease(l); }
};

struct WGPUPipelineLayoutDeleter {
    void operator()(WGPUPipelineLayout l) const { if (l) wgpuPipelineLayoutRelease(l); }
};

struct WGPUCommandEncoderDeleter {
    void operator()(WGPUCommandEncoder e) const { if (e) wgpuCommandEncoderRelease(e); }
};

struct WGPUCommandBufferDeleter {
    void operator()(WGPUCommandBuffer b) const { if (b) wgpuCommandBufferRelease(b); }
};

struct WGPURenderPassEncoderDeleter {
    void operator()(WGPURenderPassEncoder e) const { if (e) wgpuRenderPassEncoderRelease(e); }
};

// WGPUSwapChain removed in Dawn WebGPU — surface configuration is used instead.
// struct WGPUSwapChainDeleter { ... };

// Unique-ownership handles — drop-in replacements for raw WebGPU handles
// with automatic release on scope exit.
using WGPUDevicePtr         = std::unique_ptr<std::remove_pointer_t<WGPUDevice>, WGPUDeviceDeleter>;
using WGPUQueuePtr          = std::unique_ptr<std::remove_pointer_t<WGPUQueue>, WGPUQueueDeleter>;
using WGPUBufferPtr         = std::unique_ptr<std::remove_pointer_t<WGPUBuffer>, WGPUBufferDeleter>;
using WGPUTexturePtr        = std::unique_ptr<std::remove_pointer_t<WGPUTexture>, WGPUTextureDeleter>;
using WGPUTextureViewPtr    = std::unique_ptr<std::remove_pointer_t<WGPUTextureView>, WGPUTextureViewDeleter>;
using WGPUSamplerPtr        = std::unique_ptr<std::remove_pointer_t<WGPUSampler>, WGPUSamplerDeleter>;
using WGPURenderPipelinePtr = std::unique_ptr<std::remove_pointer_t<WGPURenderPipeline>, WGPURenderPipelineDeleter>;
using WGPUComputePipelinePtr = std::unique_ptr<std::remove_pointer_t<WGPUComputePipeline>, WGPUComputePipelineDeleter>;
using WGPUShaderModulePtr   = std::unique_ptr<std::remove_pointer_t<WGPUShaderModule>, WGPUShaderModuleDeleter>;
using WGPUBindGroupPtr      = std::unique_ptr<std::remove_pointer_t<WGPUBindGroup>, WGPUBindGroupDeleter>;
using WGPUBindGroupLayoutPtr = std::unique_ptr<std::remove_pointer_t<WGPUBindGroupLayout>, WGPUBindGroupLayoutDeleter>;
using WGPUPipelineLayoutPtr = std::unique_ptr<std::remove_pointer_t<WGPUPipelineLayout>, WGPUPipelineLayoutDeleter>;
// WGPUSwapChainPtr removed — Dawn uses surface configuration instead

// ============================================================================
// Descriptor helpers
// ============================================================================

// Vertex attribute format mapping (mirrors Vulkan VkFormat for common types)
enum class WebGPUVertexFormat : u8 {
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Uint32,
    Sint32,
};

inline WGPUVertexFormat ToWGPUVertexFormat(WebGPUVertexFormat fmt) {
    switch (fmt) {
        case WebGPUVertexFormat::Float32:   return WGPUVertexFormat_Float32;
        case WebGPUVertexFormat::Float32x2: return WGPUVertexFormat_Float32x2;
        case WebGPUVertexFormat::Float32x3: return WGPUVertexFormat_Float32x3;
        case WebGPUVertexFormat::Float32x4: return WGPUVertexFormat_Float32x4;
        case WebGPUVertexFormat::Uint32:    return WGPUVertexFormat_Uint32;
        case WebGPUVertexFormat::Sint32:    return WGPUVertexFormat_Sint32;
    }
    return WGPUVertexFormat_Float32;
}

// Texture format helpers
inline WGPUTextureFormat GetPreferredSwapChainFormat() {
    return WGPUTextureFormat_BGRA8Unorm;
}

inline WGPUTextureFormat GetDepthStencilFormat() {
    return WGPUTextureFormat_Depth24PlusStencil8;
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
