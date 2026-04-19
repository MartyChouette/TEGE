#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/GPUPipeline.h"
#include "Enjin/Renderer/GPUResourcePool.h"
#include <webgpu/webgpu.h>

namespace Enjin::Renderer {

class WebGPURenderer;
class WebGPUShaderManager;
class WebGPUBindGroupManager;

struct WebGPUPipelineSlot {
    WGPURenderPipeline pipeline = nullptr;
};

class ENJIN_API WebGPUPipelineManager : public IGPUPipelineManager {
public:
    WebGPUPipelineManager(WebGPURenderer* renderer, WebGPUShaderManager* shaderMgr,
                          WebGPUBindGroupManager* bindGroupMgr = nullptr);
    ~WebGPUPipelineManager() override;

    GPUPipelineHandle CreateRenderPipeline(const GPURenderPipelineDesc& desc) override;
    void DestroyPipeline(GPUPipelineHandle handle) override;
    bool IsValid(GPUPipelineHandle handle) const override;

    // Access native WGPURenderPipeline
    WGPURenderPipeline GetNativePipeline(GPUPipelineHandle handle);

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

    WebGPURenderer* m_Renderer = nullptr;
    WebGPUShaderManager* m_ShaderMgr = nullptr;
    WebGPUBindGroupManager* m_BindGroupMgr = nullptr;
    GPUResourcePool<WebGPUPipelineSlot> m_Pool;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
