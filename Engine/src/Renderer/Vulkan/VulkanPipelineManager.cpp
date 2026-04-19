#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Vulkan/VulkanPipelineManager.h"
#include "Enjin/Renderer/Vulkan/VulkanShaderManager.h"
#include "Enjin/Logging/Log.h"

namespace Enjin::Renderer {

VulkanPipelineManager::VulkanPipelineManager(VulkanContext* context, VulkanShaderManager* shaderMgr)
    : m_Context(context), m_ShaderMgr(shaderMgr) {}

VulkanPipelineManager::~VulkanPipelineManager() {
    Shutdown();
}

void VulkanPipelineManager::Shutdown() {
    m_Pool.ForEach([](VulkanPipelineSlot& slot) {
        if (slot.pipeline) slot.pipeline->Destroy();
    });
    m_Pool.Clear();
}

GPUPipelineHandle VulkanPipelineManager::CreateRenderPipeline(const GPURenderPipelineDesc& desc) {
    // Resolve shader handles to native VulkanShader pointers
    VulkanShader* vs = m_ShaderMgr->GetNativeShader(desc.vertexShader);
    VulkanShader* fs = m_ShaderMgr->GetNativeShader(desc.fragmentShader);
    if (!vs || !fs) {
        ENJIN_LOG_ERROR(Core, "VulkanPipelineManager: Invalid shader handles");
        return {};
    }

    // Translate GPURenderPipelineDesc → PipelineConfig
    PipelineConfig config;
    config.renderPass = m_RenderPass;
    config.msaaSamples = m_MSAASamples;

    switch (desc.topology) {
        case GPUPrimitiveTopology::TriangleList:  config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
        case GPUPrimitiveTopology::TriangleStrip: config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
        case GPUPrimitiveTopology::LineList:      config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
        case GPUPrimitiveTopology::LineStrip:     config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
        case GPUPrimitiveTopology::PointList:     config.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
    }

    switch (desc.cullMode) {
        case GPUCullMode::None:  config.cullMode = VK_CULL_MODE_NONE; break;
        case GPUCullMode::Front: config.cullMode = VK_CULL_MODE_FRONT_BIT; break;
        case GPUCullMode::Back:  config.cullMode = VK_CULL_MODE_BACK_BIT; break;
    }

    config.frontFace = (desc.frontFace == GPUFrontFace::CCW) ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;

    switch (desc.polygonMode) {
        case GPUPolygonMode::Fill:  config.polygonMode = VK_POLYGON_MODE_FILL; break;
        case GPUPolygonMode::Line:  config.polygonMode = VK_POLYGON_MODE_LINE; break;
        case GPUPolygonMode::Point: config.polygonMode = VK_POLYGON_MODE_POINT; break;
    }

    config.depthTest = desc.depthTest;
    config.depthWrite = desc.depthWrite;
    config.depthBiasEnable = desc.depthBiasEnable;
    config.depthBiasConstant = desc.depthBiasConstant;
    config.depthBiasSlope = desc.depthBiasSlope;
    config.hasColorAttachment = desc.hasColorAttachment;
    config.alphaBlend = desc.alphaBlend;
    config.colorAttachmentCount = desc.colorAttachmentCount;

    auto pipeline = std::make_unique<VulkanPipeline>(m_Context);
    if (!pipeline->Create(config, vs, fs)) {
        ENJIN_LOG_ERROR(Core, "VulkanPipelineManager: Pipeline creation failed%s%s",
                        desc.label ? ": " : "", desc.label ? desc.label : "");
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->pipeline = std::move(pipeline);
    return GPUPipelineHandle{handle};
}

void VulkanPipelineManager::DestroyPipeline(GPUPipelineHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    if (slot->pipeline) slot->pipeline->Destroy();
    m_Pool.Free(handle.id);
}

bool VulkanPipelineManager::IsValid(GPUPipelineHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

VulkanPipeline* VulkanPipelineManager::GetNativePipeline(GPUPipelineHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->pipeline.get() : nullptr;
}

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU
