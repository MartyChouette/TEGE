#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"

namespace Enjin::Renderer {

// Abstract pipeline manager — backend implementations create native render pipelines
// from backend-agnostic descriptors.
class ENJIN_API IGPUPipelineManager {
public:
    virtual ~IGPUPipelineManager() = default;

    // Create a render pipeline from a backend-agnostic descriptor.
    // The descriptor references shader handles and bind group layout handles
    // that must have been created via the shader/bind group managers.
    virtual GPUPipelineHandle CreateRenderPipeline(const GPURenderPipelineDesc& desc) = 0;

    // Create a compute pipeline from a backend-agnostic descriptor. Default returns an
    // invalid handle; backends that support GPU compute (WebGPU) override this. The
    // Vulkan path uses ComputePipelineHelper directly and does not need it.
    virtual GPUPipelineHandle CreateComputePipeline(const GPUComputePipelineDesc& /*desc*/) { return {}; }

    // Destroy a pipeline.
    virtual void DestroyPipeline(GPUPipelineHandle handle) = 0;

    // Check if a handle is valid.
    virtual bool IsValid(GPUPipelineHandle handle) const = 0;
};

} // namespace Enjin::Renderer
