#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPUPipeline.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include <webgpu/webgpu.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Enjin::Renderer {

class WebGPURenderer;
class WebGPUShaderManager;
class WebGPUBindGroupManager;

struct WebGPUPipelineSlot {
    WGPURenderPipeline pipeline = nullptr;
    WGPUComputePipeline computePipeline = nullptr;  // set instead of pipeline for compute
};

class ENJIN_API WebGPUPipelineManager : public IGPUPipelineManager {
public:
    WebGPUPipelineManager(WebGPURenderer* renderer, WebGPUShaderManager* shaderMgr,
                          WebGPUBindGroupManager* bindGroupMgr = nullptr);
    ~WebGPUPipelineManager() override;

    GPUPipelineHandle CreateRenderPipeline(const GPURenderPipelineDesc& desc) override;
    GPUPipelineHandle CreateComputePipeline(const GPUComputePipelineDesc& desc) override;
    void DestroyPipeline(GPUPipelineHandle handle) override;
    bool IsValid(GPUPipelineHandle handle) const override;

    // Access native WGPURenderPipeline
    WGPURenderPipeline GetNativePipeline(GPUPipelineHandle handle);
    // Access native WGPUComputePipeline (for handles created via CreateComputePipeline)
    WGPUComputePipeline GetNativeComputePipeline(GPUPipelineHandle handle);

    // Say which pipelines were built and never drawn with.
    //
    // A pipeline that is created, logged as initialized, and never bound looks
    // exactly like a working feature from the outside - m_WebGrassPipeline and
    // m_WebTreePipeline were built every boot for months while the vegetation
    // actually came from somewhere else, and edits to their shaders changed
    // nothing. Anything in this report is either dead or not reached yet.
    void ReportUnusedPipelines() const;

    void Shutdown();

private:
    WGPUVertexFormat TranslateVertexFormat(GPUVertexFormat fmt);
    WGPUPrimitiveTopology TranslateTopology(GPUPrimitiveTopology topo);
    WGPUCullMode TranslateCullMode(GPUCullMode mode);
    WGPUFrontFace TranslateFrontFace(GPUFrontFace face);
    WGPUCompareFunction TranslateCompare(GPUCompareFunction fn);
    WGPUTextureFormat TranslateTextureFormat(GPUTextureFormat fmt);
    WGPUBlendFactor TranslateBlendFactor(GPUBlendFactor f);
    WGPUBlendOperation TranslateBlendOp(GPUBlendOp op);

    // label by handle id, and the ids that have been bound at least once
    std::unordered_map<u64, std::string> m_PipelineLabels;
    mutable std::unordered_set<u64> m_UsedPipelines;

    WebGPURenderer* m_Renderer = nullptr;
    WebGPUShaderManager* m_ShaderMgr = nullptr;
    WebGPUBindGroupManager* m_BindGroupMgr = nullptr;
    GPUResourcePool<WebGPUPipelineSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
