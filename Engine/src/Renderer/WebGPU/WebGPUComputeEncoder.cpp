#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUComputeEncoder.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"

namespace Enjin::Renderer {

WebGPUComputeEncoder::WebGPUComputeEncoder(WebGPURenderer* renderer,
                                           WGPUComputePassEncoder passEncoder,
                                           WebGPUPipelineManager* pipelineMgr,
                                           WebGPUBindGroupManager* bindGroupMgr)
    : m_Renderer(renderer), m_PassEncoder(passEncoder),
      m_PipelineMgr(pipelineMgr), m_BindGroupMgr(bindGroupMgr) {}

void WebGPUComputeEncoder::BindPipeline(GPUPipelineHandle pipeline) {
    WGPUComputePipeline native = m_PipelineMgr->GetNativeComputePipeline(pipeline);
    if (!native) return;
    wgpuComputePassEncoderSetPipeline(m_PassEncoder, native);
}

void WebGPUComputeEncoder::SetBindGroup(u32 index, GPUBindGroupHandle group,
                                        u32 dynamicOffsetCount, const u32* dynamicOffsets) {
    WGPUBindGroup native = m_BindGroupMgr->GetNativeGroup(group);
    if (!native) return;
    wgpuComputePassEncoderSetBindGroup(m_PassEncoder, index, native,
                                       dynamicOffsetCount, dynamicOffsets);
}

void WebGPUComputeEncoder::Dispatch(u32 groupsX, u32 groupsY, u32 groupsZ) {
    wgpuComputePassEncoderDispatchWorkgroups(m_PassEncoder, groupsX, groupsY, groupsZ);
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
