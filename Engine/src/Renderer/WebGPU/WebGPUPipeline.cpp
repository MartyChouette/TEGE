#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUPipeline.h"
#include "Enjin/Logging/Log.h"
#include <vector>

namespace Enjin {
namespace Renderer {

WebGPUPipeline::WebGPUPipeline(WGPUDevice device) : m_Device(device) {}

WGPURenderPipeline WebGPUPipeline::CreateRenderPipeline(const WebGPURenderPipelineDesc& desc) {
    // --- Vertex state ---
    std::vector<WGPUVertexBufferLayout> wgpuBufferLayouts;
    // We need to keep attribute arrays alive until pipeline creation
    std::vector<std::vector<WGPUVertexAttribute>> attrStorage;

    for (const auto& vb : desc.vertexBuffers) {
        std::vector<WGPUVertexAttribute> attrs;
        for (const auto& a : vb.attributes) {
            WGPUVertexAttribute wa = {};
            wa.format = ToWGPUVertexFormat(a.format);
            wa.offset = a.offset;
            wa.shaderLocation = a.shaderLocation;
            attrs.push_back(wa);
        }
        attrStorage.push_back(std::move(attrs));

        WGPUVertexBufferLayout layout = {};
        layout.arrayStride = vb.stride;
        layout.stepMode = vb.perInstance ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex;
        layout.attributeCount = static_cast<u32>(attrStorage.back().size());
        layout.attributes = attrStorage.back().data();
        wgpuBufferLayouts.push_back(layout);
    }

    WGPUVertexState vertexState = {};
    vertexState.module = desc.vertexShader;
    vertexState.entryPoint = desc.vertexEntryPoint;
    vertexState.bufferCount = static_cast<u32>(wgpuBufferLayouts.size());
    vertexState.buffers = wgpuBufferLayouts.data();

    // --- Fragment state ---
    WGPUBlendState blendState = {};
    blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.color.operation = WGPUBlendOperation_Add;
    blendState.alpha.srcFactor = WGPUBlendFactor_One;
    blendState.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blendState.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = desc.colorFormat;
    colorTarget.blend = desc.blendEnabled ? &blendState : nullptr;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = desc.fragmentShader;
    fragmentState.entryPoint = desc.fragmentEntryPoint;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    // --- Primitive state ---
    WGPUPrimitiveState primitiveState = {};
    primitiveState.topology = desc.topology;
    primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitiveState.frontFace = desc.frontFace;
    primitiveState.cullMode = desc.cullMode;

    // --- Depth/stencil state ---
    WGPUDepthStencilState depthStencil = {};
    depthStencil.format = GetDepthStencilFormat();
    depthStencil.depthWriteEnabled = desc.depthWrite;
    depthStencil.depthCompare = desc.depthCompare;
    depthStencil.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencil.stencilBack.compare = WGPUCompareFunction_Always;

    // --- Multisample state ---
    WGPUMultisampleState multisample = {};
    multisample.count = 1;
    multisample.mask = ~0u;
    multisample.alphaToCoverageEnabled = false;

    // --- Pipeline descriptor ---
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = desc.label;
    pipelineDesc.layout = desc.layout;
    pipelineDesc.vertex = vertexState;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive = primitiveState;
    pipelineDesc.depthStencil = desc.depthTest ? &depthStencil : nullptr;
    pipelineDesc.multisample = multisample;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(m_Device, &pipelineDesc);
    if (!pipeline) {
        ENJIN_LOG_ERROR(Core, "WebGPU: Failed to create render pipeline '%s'",
                        desc.label ? desc.label : "unnamed");
    }
    return pipeline;
}

WGPUBindGroupLayout WebGPUPipeline::CreateStandardBindGroupLayout(u32 group) {
    std::vector<WGPUBindGroupLayoutEntry> entries;

    if (group == 0) {
        // Group 0: ViewProj UBO (binding 0) + Lighting UBO (binding 1)
        WGPUBindGroupLayoutEntry viewProj = {};
        viewProj.binding = 0;
        viewProj.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        viewProj.buffer.type = WGPUBufferBindingType_Uniform;
        viewProj.buffer.minBindingSize = 0;
        entries.push_back(viewProj);

        WGPUBindGroupLayoutEntry lighting = {};
        lighting.binding = 1;
        lighting.visibility = WGPUShaderStage_Fragment;
        lighting.buffer.type = WGPUBufferBindingType_Uniform;
        lighting.buffer.minBindingSize = 0;
        entries.push_back(lighting);
    } else if (group == 1) {
        // Group 1: Material UBO (binding 0)
        WGPUBindGroupLayoutEntry material = {};
        material.binding = 0;
        material.visibility = WGPUShaderStage_Fragment;
        material.buffer.type = WGPUBufferBindingType_Uniform;
        material.buffer.minBindingSize = 0;
        entries.push_back(material);
    } else if (group == 2) {
        // Group 2: Textures (sampler + texture view pairs)
        for (u32 i = 0; i < 5; ++i) {
            // Texture view
            WGPUBindGroupLayoutEntry texEntry = {};
            texEntry.binding = i * 2;
            texEntry.visibility = WGPUShaderStage_Fragment;
            texEntry.texture.sampleType = WGPUTextureSampleType_Float;
            texEntry.texture.viewDimension = WGPUTextureViewDimension_2D;
            entries.push_back(texEntry);

            // Sampler
            WGPUBindGroupLayoutEntry samplerEntry = {};
            samplerEntry.binding = i * 2 + 1;
            samplerEntry.visibility = WGPUShaderStage_Fragment;
            samplerEntry.sampler.type = WGPUSamplerBindingType_Filtering;
            entries.push_back(samplerEntry);
        }
    }

    WGPUBindGroupLayoutDescriptor layoutDesc = {};
    layoutDesc.entryCount = static_cast<u32>(entries.size());
    layoutDesc.entries = entries.data();
    return wgpuDeviceCreateBindGroupLayout(m_Device, &layoutDesc);
}

WGPUPipelineLayout WebGPUPipeline::CreatePipelineLayout(const std::vector<WGPUBindGroupLayout>& layouts) {
    WGPUPipelineLayoutDescriptor desc = {};
    desc.bindGroupLayoutCount = static_cast<u32>(layouts.size());
    desc.bindGroupLayouts = layouts.data();
    return wgpuDeviceCreatePipelineLayout(m_Device, &desc);
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
