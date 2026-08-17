#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPURenderEncoder.h"
#include <webgpu/webgpu.h>

namespace Enjin::Renderer {

class WebGPURenderer;
class WebGPUPipelineManager;
class WebGPUBindGroupManager;

// WebGPU implementation of IComputeEncoder. Wraps a WGPUComputePassEncoder.
// The pass is begun by WebGPURenderer::BeginComputePass() and ended by
// EndComputePass(); this object just records pipeline/bind-group/dispatch.
class ENJIN_API WebGPUComputeEncoder : public IComputeEncoder {
public:
    WebGPUComputeEncoder(WebGPURenderer* renderer,
                         WGPUComputePassEncoder passEncoder,
                         WebGPUPipelineManager* pipelineMgr,
                         WebGPUBindGroupManager* bindGroupMgr);
    ~WebGPUComputeEncoder() override = default;

    void BindPipeline(GPUPipelineHandle pipeline) override;
    void SetBindGroup(u32 index, GPUBindGroupHandle group,
                      u32 dynamicOffsetCount, const u32* dynamicOffsets) override;
    void Dispatch(u32 groupsX, u32 groupsY, u32 groupsZ) override;

    WGPUComputePassEncoder GetPassEncoder() const { return m_PassEncoder; }

private:
    WebGPURenderer* m_Renderer;
    WGPUComputePassEncoder m_PassEncoder;
    WebGPUPipelineManager* m_PipelineMgr;
    WebGPUBindGroupManager* m_BindGroupMgr;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
