#pragma once

#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin::Renderer {

// ============================================================================
// Opaque GPU resource handles — typed wrappers over u64 generation IDs.
// Backend implementations resolve these to native objects internally.
// ============================================================================

struct GPUBufferHandle           { u64 id = 0; bool IsValid() const { return id != 0; } };
struct GPUTextureHandle          { u64 id = 0; bool IsValid() const { return id != 0; } };
struct GPUPipelineHandle         { u64 id = 0; bool IsValid() const { return id != 0; } };
struct GPUShaderHandle           { u64 id = 0; bool IsValid() const { return id != 0; } };
struct GPUBindGroupHandle        { u64 id = 0; bool IsValid() const { return id != 0; } };
struct GPUBindGroupLayoutHandle  { u64 id = 0; bool IsValid() const { return id != 0; } };

// ============================================================================
// Backend-agnostic enums
// ============================================================================

enum class GPUBufferUsage : u32 {
    Vertex          = 1 << 0,
    Index           = 1 << 1,
    Uniform         = 1 << 2,
    Storage         = 1 << 3,
    Indirect        = 1 << 4,
    CopySrc         = 1 << 5,
    CopyDst         = 1 << 6,
};
inline GPUBufferUsage operator|(GPUBufferUsage a, GPUBufferUsage b) {
    return static_cast<GPUBufferUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline bool operator&(GPUBufferUsage a, GPUBufferUsage b) {
    return (static_cast<u32>(a) & static_cast<u32>(b)) != 0;
}

enum class GPUTextureFormat : u8 {
    RGBA8Unorm,
    RGBA8Srgb,
    BGRA8Unorm,
    BGRA8Srgb,
    RGBA16Float,
    RGBA32Float,
    R8Unorm,
    RG8Unorm,
    Depth24PlusStencil8,
    Depth32Float,
    Depth16Unorm,
};

enum class GPUTextureUsage : u32 {
    Sampled         = 1 << 0,
    Storage         = 1 << 1,
    RenderAttachment = 1 << 2,
    CopySrc         = 1 << 3,
    CopyDst         = 1 << 4,
};
inline GPUTextureUsage operator|(GPUTextureUsage a, GPUTextureUsage b) {
    return static_cast<GPUTextureUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}

enum class GPUPrimitiveTopology : u8 {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
};

enum class GPUCullMode : u8 {
    None,
    Front,
    Back,
};

enum class GPUFrontFace : u8 {
    CCW,
    CW,
};

enum class GPUPolygonMode : u8 {
    Fill,
    Line,
    Point,
};

enum class GPUCompareFunction : u8 {
    Never,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Always,
};

enum class GPUBlendFactor : u8 {
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
};

enum class GPUBlendOp : u8 {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class GPUShaderStage : u32 {
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
};
inline GPUShaderStage operator|(GPUShaderStage a, GPUShaderStage b) {
    return static_cast<GPUShaderStage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline bool operator&(GPUShaderStage a, GPUShaderStage b) {
    return (static_cast<u32>(a) & static_cast<u32>(b)) != 0;
}

enum class GPULoadOp : u8 {
    Load,
    Clear,
    DontCare,
};

enum class GPUStoreOp : u8 {
    Store,
    Discard,
};

enum class GPUBindingType : u8 {
    UniformBuffer,
    StorageBuffer,
    StorageBufferReadOnly,
    SampledTexture,
    StorageTexture,
    Sampler,
    ComparisonSampler,
    DepthTexture,
    DepthTextureCube,
};

enum class GPUIndexFormat : u8 {
    Uint16,
    Uint32,
};

enum class GPUVertexFormat : u8 {
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Uint32,
    Sint32,
    Uint8x4,
    Sint8x4,
    Uint16x2,
    Sint16x2,
    Uint16x4,
    Sint16x4,
    Uint32x2,
    Uint32x4,
};

// ============================================================================
// Descriptor structs used by manager interfaces
// ============================================================================

struct GPUBufferDesc {
    u64 size = 0;
    GPUBufferUsage usage = GPUBufferUsage::Vertex;
    bool hostVisible = false;
    const char* label = nullptr;
};

struct GPUTextureDesc {
    u32 width = 1;
    u32 height = 1;
    u32 depth = 1;
    u32 mipLevels = 1;
    u32 arrayLayers = 1;
    GPUTextureFormat format = GPUTextureFormat::RGBA8Unorm;
    GPUTextureUsage usage = GPUTextureUsage::Sampled;
    u32 sampleCount = 1;
    const char* label = nullptr;
};

struct GPUVertexAttribute {
    GPUVertexFormat format;
    u32 offset;
    u32 shaderLocation;
};

struct GPUVertexBufferLayoutDesc {
    u32 stride;
    bool perInstance = false;  // false = per-vertex, true = per-instance
    std::vector<GPUVertexAttribute> attributes;
};

struct GPUBlendState {
    GPUBlendFactor srcColor = GPUBlendFactor::One;
    GPUBlendFactor dstColor = GPUBlendFactor::Zero;
    GPUBlendOp colorOp = GPUBlendOp::Add;
    GPUBlendFactor srcAlpha = GPUBlendFactor::One;
    GPUBlendFactor dstAlpha = GPUBlendFactor::Zero;
    GPUBlendOp alphaOp = GPUBlendOp::Add;
};

struct GPURenderPipelineDesc {
    GPUShaderHandle vertexShader;
    GPUShaderHandle fragmentShader;
    std::vector<GPUVertexBufferLayoutDesc> vertexBuffers;
    std::vector<GPUBindGroupLayoutHandle> bindGroupLayouts;
    GPUPrimitiveTopology topology = GPUPrimitiveTopology::TriangleList;
    GPUCullMode cullMode = GPUCullMode::Back;
    GPUFrontFace frontFace = GPUFrontFace::CCW;
    GPUPolygonMode polygonMode = GPUPolygonMode::Fill;
    bool depthTest = true;
    bool depthWrite = true;
    GPUCompareFunction depthCompare = GPUCompareFunction::Less;
    bool depthBiasEnable = false;
    f32 depthBiasConstant = 0.0f;
    f32 depthBiasSlope = 0.0f;
    u32 colorAttachmentCount = 1;
    GPUTextureFormat colorFormat = GPUTextureFormat::BGRA8Srgb;
    GPUTextureFormat depthFormat = GPUTextureFormat::Depth24PlusStencil8;
    bool alphaBlend = false;
    GPUBlendState blendState;
    u32 sampleCount = 1;
    bool hasColorAttachment = true;
    const char* label = nullptr;
};

struct GPUBindGroupLayoutEntry {
    u32 binding;
    GPUBindingType type;
    GPUShaderStage visibility;
    u64 minBufferBindingSize = 0;  // for buffer bindings
};

struct GPUBindGroupLayoutDesc {
    std::vector<GPUBindGroupLayoutEntry> entries;
    const char* label = nullptr;
};

struct GPUBindGroupEntry {
    u32 binding;
    GPUBufferHandle buffer;       // for buffer bindings
    u64 bufferOffset = 0;
    u64 bufferSize = 0;
    GPUTextureHandle texture;     // for texture bindings
    GPUTextureHandle sampler;     // for sampler bindings (reuses texture handle for now)
};

struct GPUBindGroupDesc {
    GPUBindGroupLayoutHandle layout;
    std::vector<GPUBindGroupEntry> entries;
    const char* label = nullptr;
};

// Render pass configuration
struct GPUColorAttachment {
    GPUTextureHandle texture;  // 0 = swapchain
    GPULoadOp loadOp = GPULoadOp::Clear;
    GPUStoreOp storeOp = GPUStoreOp::Store;
    f32 clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct GPUDepthAttachment {
    GPUTextureHandle texture;  // 0 = default depth
    GPULoadOp loadOp = GPULoadOp::Clear;
    GPUStoreOp storeOp = GPUStoreOp::Store;
    f32 clearDepth = 1.0f;
    u32 clearStencil = 0;
};

struct GPURenderPassDesc {
    std::vector<GPUColorAttachment> colorAttachments;
    GPUDepthAttachment depthAttachment;
    u32 width = 0;   // 0 = swapchain dimensions
    u32 height = 0;
    const char* label = nullptr;
};

} // namespace Enjin::Renderer
