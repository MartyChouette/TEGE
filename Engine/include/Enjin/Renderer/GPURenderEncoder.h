#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"

namespace Enjin::Renderer {

// Abstract render command encoder — records draw commands within a render pass.
// Vulkan impl wraps VkCommandBuffer. WebGPU impl wraps WGPURenderPassEncoder.
// Metal impl will wrap MTLRenderCommandEncoder.
//
// Obtained from IRenderBackend::BeginRenderPass(), valid until EndRenderPass().
// All draw state (pipeline, bind groups, vertex/index buffers) is set through
// this interface. RenderSystem never touches vkCmd* or wgpuRenderPassEncoder* directly.
class ENJIN_API IRenderEncoder {
public:
    virtual ~IRenderEncoder() = default;

    // --- Pipeline ---
    virtual void BindPipeline(GPUPipelineHandle pipeline) = 0;

    // --- Bind groups (descriptor sets) ---
    // dynamicOffsetCount/offsets used for dynamic UBO/SSBO offsets (material SSBO batching)
    virtual void SetBindGroup(u32 index, GPUBindGroupHandle group,
                              u32 dynamicOffsetCount = 0, const u32* dynamicOffsets = nullptr) = 0;

    // --- Vertex / Index buffers ---
    virtual void SetVertexBuffer(u32 slot, GPUBufferHandle buffer, u64 offset = 0) = 0;
    virtual void SetIndexBuffer(GPUBufferHandle buffer, GPUIndexFormat format, u64 offset = 0) = 0;

    // --- Inline constants (push constants on Vulkan, reserved UBO on WebGPU/Metal) ---
    // offset and size in bytes. Max 128 bytes. Data layout must match shader expectations.
    virtual void SetInlineConstants(u32 offset, u32 size, const void* data) = 0;

    // --- Draw commands ---
    virtual void Draw(u32 vertexCount, u32 instanceCount = 1,
                      u32 firstVertex = 0, u32 firstInstance = 0) = 0;
    virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1,
                             u32 firstIndex = 0, i32 baseVertex = 0, u32 firstInstance = 0) = 0;

    // --- Advanced draws (no-op if capability not supported) ---
    virtual void DrawIndexedIndirect(GPUBufferHandle buffer, u64 offset, u32 drawCount) {}
    virtual void DrawIndexedIndirectCount(GPUBufferHandle argsBuffer, u64 argsOffset,
                                          GPUBufferHandle countBuffer, u64 countOffset,
                                          u32 maxDrawCount) {}

    // --- Viewport / Scissor ---
    virtual void SetViewport(f32 x, f32 y, f32 width, f32 height,
                             f32 minDepth = 0.0f, f32 maxDepth = 1.0f) = 0;
    virtual void SetScissor(u32 x, u32 y, u32 width, u32 height) = 0;
};

// Abstract compute command encoder — records dispatches within a compute pass.
// Vulkan binds compute directly via ComputePipelineHelper; WebGPU wraps
// WGPUComputePassEncoder. Obtained from IRenderBackend::BeginComputePass(),
// valid until EndComputePass().
class ENJIN_API IComputeEncoder {
public:
    virtual ~IComputeEncoder() = default;

    // Bind a compute pipeline (created via IGPUPipelineManager::CreateComputePipeline).
    virtual void BindPipeline(GPUPipelineHandle pipeline) = 0;

    // Bind a bind group (storage/uniform buffers the shader reads/writes).
    virtual void SetBindGroup(u32 index, GPUBindGroupHandle group,
                              u32 dynamicOffsetCount = 0, const u32* dynamicOffsets = nullptr) = 0;

    // Dispatch groupsX*groupsY*groupsZ workgroups.
    virtual void Dispatch(u32 groupsX, u32 groupsY = 1, u32 groupsZ = 1) = 0;
};

} // namespace Enjin::Renderer
