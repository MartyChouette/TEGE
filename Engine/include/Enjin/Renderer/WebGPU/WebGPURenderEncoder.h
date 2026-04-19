#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPURenderEncoder.h"
#include <webgpu/webgpu.h>

namespace Enjin::Renderer {

class WebGPURenderer;
class WebGPUPipelineManager;
class WebGPUBufferManager;
class WebGPUBindGroupManager;

// WebGPU implementation of IRenderEncoder.
// Wraps WGPURenderPassEncoder. SetInlineConstants writes to a reserved
// uniform buffer (WebGPU has no push constants).
class ENJIN_API WebGPURenderEncoder : public IRenderEncoder {
public:
    WebGPURenderEncoder(WebGPURenderer* renderer,
                        WGPURenderPassEncoder passEncoder,
                        WebGPUPipelineManager* pipelineMgr,
                        WebGPUBufferManager* bufferMgr,
                        WebGPUBindGroupManager* bindGroupMgr);
    ~WebGPURenderEncoder() override;

    void BindPipeline(GPUPipelineHandle pipeline) override;
    void SetBindGroup(u32 index, GPUBindGroupHandle group,
                      u32 dynamicOffsetCount, const u32* dynamicOffsets) override;
    void SetVertexBuffer(u32 slot, GPUBufferHandle buffer, u64 offset) override;
    void SetIndexBuffer(GPUBufferHandle buffer, GPUIndexFormat format, u64 offset) override;
    void SetInlineConstants(u32 offset, u32 size, const void* data) override;
    void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;
    void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
                     i32 baseVertex, u32 firstInstance) override;
    void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) override;
    void SetScissor(u32 x, u32 y, u32 width, u32 height) override;

    WGPURenderPassEncoder GetPassEncoder() const { return m_PassEncoder; }

private:
    WebGPURenderer* m_Renderer;
    WGPURenderPassEncoder m_PassEncoder;
    WebGPUPipelineManager* m_PipelineMgr;
    WebGPUBufferManager* m_BufferMgr;
    WebGPUBindGroupManager* m_BindGroupMgr;

    // Reserved UBO for inline constants (push constant emulation)
    // 256 bytes, bound at group 3 binding 0
    WGPUBuffer m_InlineConstantBuffer = nullptr;
    WGPUBindGroup m_InlineConstantBindGroup = nullptr;
    WGPUBindGroupLayout m_InlineConstantLayout = nullptr;
    static constexpr u32 INLINE_CONSTANT_SIZE = 256;
    static constexpr u32 INLINE_CONSTANT_GROUP = 3;

    void CreateInlineConstantResources();
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
