#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Logging/Log.h"

namespace Enjin::Renderer {

static WGPUStringView wgpuStr(const char* s) { return {s, WGPU_STRLEN}; }

WebGPUPipelineManager::WebGPUPipelineManager(WebGPURenderer* renderer, WebGPUShaderManager* shaderMgr,
                                               WebGPUBindGroupManager* bindGroupMgr)
    : m_Renderer(renderer), m_ShaderMgr(shaderMgr), m_BindGroupMgr(bindGroupMgr) {}

WebGPUPipelineManager::~WebGPUPipelineManager() {
    Shutdown();
}

void WebGPUPipelineManager::Shutdown() {
    m_Pool.ForEach([](WebGPUPipelineSlot& slot) {
        if (slot.pipeline) { wgpuRenderPipelineRelease(slot.pipeline); slot.pipeline = nullptr; }
        if (slot.computePipeline) { wgpuComputePipelineRelease(slot.computePipeline); slot.computePipeline = nullptr; }
    });
    m_Pool.Clear();
}

WGPUVertexFormat WebGPUPipelineManager::TranslateVertexFormat(GPUVertexFormat fmt) {
    switch (fmt) {
        case GPUVertexFormat::Float32:    return WGPUVertexFormat_Float32;
        case GPUVertexFormat::Float32x2:  return WGPUVertexFormat_Float32x2;
        case GPUVertexFormat::Float32x3:  return WGPUVertexFormat_Float32x3;
        case GPUVertexFormat::Float32x4:  return WGPUVertexFormat_Float32x4;
        case GPUVertexFormat::Uint32:     return WGPUVertexFormat_Uint32;
        case GPUVertexFormat::Sint32:     return WGPUVertexFormat_Sint32;
        case GPUVertexFormat::Uint8x4:    return WGPUVertexFormat_Uint8x4;
        case GPUVertexFormat::Sint8x4:    return WGPUVertexFormat_Sint8x4;
        case GPUVertexFormat::Uint16x2:   return WGPUVertexFormat_Uint16x2;
        case GPUVertexFormat::Sint16x2:   return WGPUVertexFormat_Sint16x2;
        case GPUVertexFormat::Uint16x4:   return WGPUVertexFormat_Uint16x4;
        case GPUVertexFormat::Sint16x4:   return WGPUVertexFormat_Sint16x4;
        case GPUVertexFormat::Uint32x2:   return WGPUVertexFormat_Uint32x2;
        case GPUVertexFormat::Uint32x4:   return WGPUVertexFormat_Uint32x4;
    }
    return WGPUVertexFormat_Float32x3;
}

WGPUPrimitiveTopology WebGPUPipelineManager::TranslateTopology(GPUPrimitiveTopology topo) {
    switch (topo) {
        case GPUPrimitiveTopology::TriangleList:  return WGPUPrimitiveTopology_TriangleList;
        case GPUPrimitiveTopology::TriangleStrip: return WGPUPrimitiveTopology_TriangleStrip;
        case GPUPrimitiveTopology::LineList:      return WGPUPrimitiveTopology_LineList;
        case GPUPrimitiveTopology::LineStrip:     return WGPUPrimitiveTopology_LineStrip;
        case GPUPrimitiveTopology::PointList:     return WGPUPrimitiveTopology_PointList;
    }
    return WGPUPrimitiveTopology_TriangleList;
}

WGPUCullMode WebGPUPipelineManager::TranslateCullMode(GPUCullMode mode) {
    switch (mode) {
        case GPUCullMode::None:  return WGPUCullMode_None;
        case GPUCullMode::Front: return WGPUCullMode_Front;
        case GPUCullMode::Back:  return WGPUCullMode_Back;
    }
    return WGPUCullMode_None;
}

WGPUFrontFace WebGPUPipelineManager::TranslateFrontFace(GPUFrontFace face) {
    return (face == GPUFrontFace::CCW) ? WGPUFrontFace_CCW : WGPUFrontFace_CW;
}

WGPUCompareFunction WebGPUPipelineManager::TranslateCompare(GPUCompareFunction fn) {
    switch (fn) {
        case GPUCompareFunction::Never:        return WGPUCompareFunction_Never;
        case GPUCompareFunction::Less:         return WGPUCompareFunction_Less;
        case GPUCompareFunction::LessEqual:    return WGPUCompareFunction_LessEqual;
        case GPUCompareFunction::Greater:      return WGPUCompareFunction_Greater;
        case GPUCompareFunction::GreaterEqual: return WGPUCompareFunction_GreaterEqual;
        case GPUCompareFunction::Equal:        return WGPUCompareFunction_Equal;
        case GPUCompareFunction::NotEqual:     return WGPUCompareFunction_NotEqual;
        case GPUCompareFunction::Always:       return WGPUCompareFunction_Always;
    }
    return WGPUCompareFunction_Less;
}

WGPUTextureFormat WebGPUPipelineManager::TranslateTextureFormat(GPUTextureFormat fmt) {
    switch (fmt) {
        case GPUTextureFormat::RGBA8Unorm:            return WGPUTextureFormat_RGBA8Unorm;
        case GPUTextureFormat::RGBA8Srgb:             return WGPUTextureFormat_RGBA8UnormSrgb;
        case GPUTextureFormat::BGRA8Unorm:            return WGPUTextureFormat_BGRA8Unorm;
        case GPUTextureFormat::BGRA8Srgb:             return WGPUTextureFormat_BGRA8UnormSrgb;
        case GPUTextureFormat::RGBA16Float:           return WGPUTextureFormat_RGBA16Float;
        case GPUTextureFormat::RGBA32Float:           return WGPUTextureFormat_RGBA32Float;
        case GPUTextureFormat::R8Unorm:               return WGPUTextureFormat_R8Unorm;
        case GPUTextureFormat::RG8Unorm:              return WGPUTextureFormat_RG8Unorm;
        case GPUTextureFormat::Depth24PlusStencil8:   return WGPUTextureFormat_Depth24PlusStencil8;
        case GPUTextureFormat::Depth32Float:          return WGPUTextureFormat_Depth32Float;
        case GPUTextureFormat::Depth16Unorm:          return WGPUTextureFormat_Depth16Unorm;
    }
    return WGPUTextureFormat_BGRA8Unorm;
}

WGPUBlendFactor WebGPUPipelineManager::TranslateBlendFactor(GPUBlendFactor f) {
    switch (f) {
        case GPUBlendFactor::Zero:              return WGPUBlendFactor_Zero;
        case GPUBlendFactor::One:               return WGPUBlendFactor_One;
        case GPUBlendFactor::SrcAlpha:          return WGPUBlendFactor_SrcAlpha;
        case GPUBlendFactor::OneMinusSrcAlpha:  return WGPUBlendFactor_OneMinusSrcAlpha;
        case GPUBlendFactor::DstAlpha:          return WGPUBlendFactor_DstAlpha;
        case GPUBlendFactor::OneMinusDstAlpha:  return WGPUBlendFactor_OneMinusDstAlpha;
        case GPUBlendFactor::SrcColor:          return WGPUBlendFactor_Src;
        case GPUBlendFactor::OneMinusSrcColor:  return WGPUBlendFactor_OneMinusSrc;
        case GPUBlendFactor::DstColor:          return WGPUBlendFactor_Dst;
        case GPUBlendFactor::OneMinusDstColor:  return WGPUBlendFactor_OneMinusDst;
    }
    return WGPUBlendFactor_One;
}

WGPUBlendOperation WebGPUPipelineManager::TranslateBlendOp(GPUBlendOp op) {
    switch (op) {
        case GPUBlendOp::Add:             return WGPUBlendOperation_Add;
        case GPUBlendOp::Subtract:        return WGPUBlendOperation_Subtract;
        case GPUBlendOp::ReverseSubtract: return WGPUBlendOperation_ReverseSubtract;
        case GPUBlendOp::Min:             return WGPUBlendOperation_Min;
        case GPUBlendOp::Max:             return WGPUBlendOperation_Max;
    }
    return WGPUBlendOperation_Add;
}

GPUPipelineHandle WebGPUPipelineManager::CreateRenderPipeline(const GPURenderPipelineDesc& desc) {
    WGPUShaderModule vsModule = m_ShaderMgr->GetNativeShader(desc.vertexShader);
    WGPUShaderModule fsModule = desc.fragmentShader.IsValid() ? m_ShaderMgr->GetNativeShader(desc.fragmentShader) : nullptr;
    if (!vsModule) {
        ENJIN_LOG_ERROR(Core, "WebGPUPipelineManager: Invalid vertex shader handle");
        return {};
    }
    if (!fsModule && desc.hasColorAttachment) {
        ENJIN_LOG_ERROR(Core, "WebGPUPipelineManager: Fragment shader required for color attachment");
        return {};
    }

    // Build vertex buffer layouts
    std::vector<WGPUVertexBufferLayout> vbLayouts;
    std::vector<std::vector<WGPUVertexAttribute>> attrStorage;
    for (const auto& vb : desc.vertexBuffers) {
        auto& attrs = attrStorage.emplace_back();
        for (const auto& attr : vb.attributes) {
            WGPUVertexAttribute wa = {};
            wa.format = TranslateVertexFormat(attr.format);
            wa.offset = attr.offset;
            wa.shaderLocation = attr.shaderLocation;
            attrs.push_back(wa);
        }
        WGPUVertexBufferLayout layout = {};
        layout.arrayStride = vb.stride;
        layout.stepMode = vb.perInstance ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex;
        layout.attributeCount = static_cast<u32>(attrs.size());
        layout.attributes = attrs.data();
        vbLayouts.push_back(layout);
    }

    // Colour targets. The blend states live in their own vector rather than as
    // locals because WGPUColorTargetState holds a POINTER to one: a vector that
    // reallocated, or a local that went out of scope, would leave every target
    // pointing at freed memory. Both are reserved up front for that reason.
    const usize targetCount = desc.hasColorAttachment ? (1 + desc.extraColorTargets.size()) : 0;
    std::vector<WGPUColorTargetState> colorTargets;
    std::vector<WGPUBlendState> blendStates;
    colorTargets.reserve(targetCount);
    blendStates.reserve(targetCount);

    auto pushTarget = [&](GPUTextureFormat format, bool alphaBlend, const GPUBlendState& bs) {
        WGPUColorTargetState target = {};
        target.format = TranslateTextureFormat(format);
        target.writeMask = WGPUColorWriteMask_All;
        if (alphaBlend) {
            WGPUBlendState blend = {};
            blend.color.srcFactor = TranslateBlendFactor(bs.srcColor);
            blend.color.dstFactor = TranslateBlendFactor(bs.dstColor);
            blend.color.operation = TranslateBlendOp(bs.colorOp);
            blend.alpha.srcFactor = TranslateBlendFactor(bs.srcAlpha);
            blend.alpha.dstFactor = TranslateBlendFactor(bs.dstAlpha);
            blend.alpha.operation = TranslateBlendOp(bs.alphaOp);
            blendStates.push_back(blend);
            target.blend = &blendStates.back();
        }
        colorTargets.push_back(target);
    };

    if (desc.hasColorAttachment) {
        pushTarget(desc.colorFormat, desc.alphaBlend, desc.blendState);
        for (const auto& extra : desc.extraColorTargets) {
            pushTarget(extra.format, extra.alphaBlend, extra.blendState);
        }
    }

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = wgpuStr(desc.fragmentEntryPoint ? desc.fragmentEntryPoint : "fs_main");
    fragmentState.targetCount = static_cast<u32>(colorTargets.size());
    fragmentState.targets = colorTargets.empty() ? nullptr : colorTargets.data();

    WGPUDepthStencilState depthStencil = {};
    depthStencil.format = TranslateTextureFormat(desc.depthFormat);
    depthStencil.depthWriteEnabled = desc.depthWrite ? WGPUOptionalBool_True : WGPUOptionalBool_False;
    depthStencil.depthCompare = TranslateCompare(desc.depthCompare);
    if (desc.depthBiasEnable) {
        depthStencil.depthBias = static_cast<i32>(desc.depthBiasConstant);
        depthStencil.depthBiasSlopeScale = desc.depthBiasSlope;
    }

    // Create explicit pipeline layout from bind group layouts
    WGPUPipelineLayout pipelineLayout = nullptr;
    if (m_BindGroupMgr && !desc.bindGroupLayouts.empty()) {
        std::vector<WGPUBindGroupLayout> nativeLayouts;
        for (const auto& layoutHandle : desc.bindGroupLayouts) {
            WGPUBindGroupLayout native = m_BindGroupMgr->GetNativeLayout(layoutHandle);
            if (native) nativeLayouts.push_back(native);
        }
        if (nativeLayouts.size() == desc.bindGroupLayouts.size()) {
            WGPUPipelineLayoutDescriptor layoutDesc = {};
            layoutDesc.bindGroupLayoutCount = static_cast<u32>(nativeLayouts.size());
            layoutDesc.bindGroupLayouts = nativeLayouts.data();
            pipelineLayout = wgpuDeviceCreatePipelineLayout(m_Renderer->GetDevice(), &layoutDesc);
        }
    }

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout;  // nullptr = auto layout (fallback)
    pipelineDesc.vertex.module = vsModule;
    pipelineDesc.vertex.entryPoint = wgpuStr("vs_main");
    pipelineDesc.vertex.bufferCount = static_cast<u32>(vbLayouts.size());
    pipelineDesc.vertex.buffers = vbLayouts.empty() ? nullptr : vbLayouts.data();
    pipelineDesc.fragment = fsModule ? &fragmentState : nullptr;
    pipelineDesc.depthStencil = desc.depthTest ? &depthStencil : nullptr;
    pipelineDesc.primitive.topology = TranslateTopology(desc.topology);
    pipelineDesc.primitive.frontFace = TranslateFrontFace(desc.frontFace);
    pipelineDesc.primitive.cullMode = TranslateCullMode(desc.cullMode);
    pipelineDesc.multisample.count = desc.sampleCount;
    pipelineDesc.multisample.mask = 0xFFFFFFFF;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(m_Renderer->GetDevice(), &pipelineDesc);
    if (pipelineLayout) wgpuPipelineLayoutRelease(pipelineLayout);
    if (!pipeline) {
        ENJIN_LOG_ERROR(Core, "WebGPUPipelineManager: Pipeline creation failed%s%s",
                        desc.label ? ": " : "", desc.label ? desc.label : "");
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->pipeline = pipeline;
    m_PipelineLabels[handle] = desc.label ? desc.label : "(unlabelled render)";
    return GPUPipelineHandle{handle};
}

GPUPipelineHandle WebGPUPipelineManager::CreateComputePipeline(const GPUComputePipelineDesc& desc) {
    WGPUShaderModule csModule = m_ShaderMgr->GetNativeShader(desc.computeShader);
    if (!csModule) {
        ENJIN_LOG_ERROR(Core, "WebGPUPipelineManager: Invalid compute shader handle");
        return {};
    }

    // Explicit pipeline layout from the bind group layouts the shader reads.
    WGPUPipelineLayout pipelineLayout = nullptr;
    if (m_BindGroupMgr && !desc.bindGroupLayouts.empty()) {
        std::vector<WGPUBindGroupLayout> nativeLayouts;
        for (const auto& layoutHandle : desc.bindGroupLayouts) {
            WGPUBindGroupLayout native = m_BindGroupMgr->GetNativeLayout(layoutHandle);
            if (native) nativeLayouts.push_back(native);
        }
        if (nativeLayouts.size() == desc.bindGroupLayouts.size()) {
            WGPUPipelineLayoutDescriptor layoutDesc = {};
            layoutDesc.bindGroupLayoutCount = static_cast<u32>(nativeLayouts.size());
            layoutDesc.bindGroupLayouts = nativeLayouts.data();
            pipelineLayout = wgpuDeviceCreatePipelineLayout(m_Renderer->GetDevice(), &layoutDesc);
        }
    }

    WGPUComputePipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout;  // nullptr = auto layout (fallback)
    pipelineDesc.compute.module = csModule;
    pipelineDesc.compute.entryPoint = wgpuStr(desc.entryPoint ? desc.entryPoint : "main");
    if (desc.label) pipelineDesc.label = wgpuStr(desc.label);

    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(m_Renderer->GetDevice(), &pipelineDesc);
    if (pipelineLayout) wgpuPipelineLayoutRelease(pipelineLayout);
    if (!pipeline) {
        ENJIN_LOG_ERROR(Core, "WebGPUPipelineManager: Compute pipeline creation failed%s%s",
                        desc.label ? ": " : "", desc.label ? desc.label : "");
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->computePipeline = pipeline;
    m_PipelineLabels[handle] = desc.label ? desc.label : "(unlabelled compute)";
    return GPUPipelineHandle{handle};
}

void WebGPUPipelineManager::DestroyPipeline(GPUPipelineHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    if (slot->pipeline) { wgpuRenderPipelineRelease(slot->pipeline); slot->pipeline = nullptr; }
    if (slot->computePipeline) { wgpuComputePipelineRelease(slot->computePipeline); slot->computePipeline = nullptr; }
    m_Pool.Free(handle.id);
}

bool WebGPUPipelineManager::IsValid(GPUPipelineHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

void WebGPUPipelineManager::ReportUnusedPipelines() const {
    u32 unused = 0;
    for (const auto& kv : m_PipelineLabels) {
        if (m_UsedPipelines.count(kv.first)) continue;
        ++unused;
        ENJIN_LOG_WARN(Renderer, "Pipeline never bound: %s", kv.second.c_str());
    }
    if (unused == 0) {
        ENJIN_LOG_INFO(Renderer, "Pipelines: all %zu bound at least once",
                       m_PipelineLabels.size());
    }
}

WGPURenderPipeline WebGPUPipelineManager::GetNativePipeline(GPUPipelineHandle handle) {
    m_UsedPipelines.insert(handle.id);
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->pipeline : nullptr;
}

WGPUComputePipeline WebGPUPipelineManager::GetNativeComputePipeline(GPUPipelineHandle handle) {
    m_UsedPipelines.insert(handle.id);
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->computePipeline : nullptr;
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
