#include "Enjin/Platform/Platform.h"
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Effects/SplatRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace Enjin {
namespace Effects {

void SplatRenderer::Initialize(Renderer::VulkanContext* context) {
    m_Context = context;
}

void SplatRenderer::Shutdown() {
    m_Current = nullptr;
    m_Pipelines.clear();
    m_VS.reset();
    m_FS.reset();
    m_InstanceBuffer.reset();
    m_Cpu.clear();
    m_Sorted.clear();
    m_Order.clear();
    m_Count = 0;
}

void SplatRenderer::LoadSplats(Assets::SplatData&& data) {
    m_Cpu = std::move(data.splats);
    m_Count = static_cast<u32>(m_Cpu.size());
    m_InstanceBuffer.reset();
    m_NeedInitialSort = true;
    if (m_Count == 0) return;

    // Host-visible: the sort re-uploads the whole buffer (throttled), so a
    // persistent staging round trip per sort would only add copies.
    m_InstanceBuffer = std::make_unique<Renderer::VulkanBuffer>(m_Context);
    if (!m_InstanceBuffer->Create(m_Count * sizeof(Assets::SplatInstance),
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true)) {
        ENJIN_LOG_ERROR(Renderer, "SplatRenderer: instance buffer creation failed (%u splats)", m_Count);
        m_InstanceBuffer.reset();
        m_Count = 0;
        m_Cpu.clear();
        return;
    }
    m_InstanceBuffer->UploadData(m_Cpu.data(), m_Count * sizeof(Assets::SplatInstance));
    ENJIN_LOG_INFO(Renderer, "SplatRenderer: %u splats resident (%.1f MB)",
                   m_Count, m_Count * sizeof(Assets::SplatInstance) / (1024.0f * 1024.0f));
}

void SplatRenderer::Clear() {
    m_InstanceBuffer.reset();
    m_Cpu.clear();
    m_Sorted.clear();
    m_Order.clear();
    m_Count = 0;
}

void SplatRenderer::SortIfNeeded(const Math::Matrix4& view, const Math::Matrix4& model) {
    if (m_Count == 0 || !m_InstanceBuffer) return;

    // Camera basis from the view matrix (row 2 of view = -forward in world)
    Math::Vector3 camFwd(-view.m[2], -view.m[6], -view.m[10]);
    // Camera position: -(R^T * t)
    Math::Vector3 t(view.m[12], view.m[13], view.m[14]);
    Math::Vector3 camPos(
        -(view.m[0] * t.x + view.m[1] * t.y + view.m[2] * t.z),
        -(view.m[4] * t.x + view.m[5] * t.y + view.m[6] * t.z),
        -(view.m[8] * t.x + view.m[9] * t.y + view.m[10] * t.z));

    if (!m_NeedInitialSort) {
        f32 moved = (camPos - m_LastSortPos).Length();
        f32 turned = 1.0f - (camFwd.x * m_LastSortFwd.x + camFwd.y * m_LastSortFwd.y +
                             camFwd.z * m_LastSortFwd.z);
        if (moved < 0.25f && turned < 0.001f) return;   // ~2.5 degrees
    }
    m_NeedInitialSort = false;
    m_LastSortPos = camPos;
    m_LastSortFwd = camFwd;

    // Depth key per splat: distance along the view direction of the MODEL-space
    // position transformed to world. Back-to-front for alpha compositing.
    if (m_Order.size() != m_Count) {
        m_Order.resize(m_Count);
        for (u32 i = 0; i < m_Count; ++i) m_Order[i] = i;
    }
    std::vector<f32> depth(m_Count);
    for (u32 i = 0; i < m_Count; ++i) {
        const auto& s = m_Cpu[i];
        f32 wx = model.m[0] * s.px + model.m[4] * s.py + model.m[8] * s.pz + model.m[12];
        f32 wy = model.m[1] * s.px + model.m[5] * s.py + model.m[9] * s.pz + model.m[13];
        f32 wz = model.m[2] * s.px + model.m[6] * s.py + model.m[10] * s.pz + model.m[14];
        depth[i] = (wx - camPos.x) * camFwd.x + (wy - camPos.y) * camFwd.y + (wz - camPos.z) * camFwd.z;
    }
    std::sort(m_Order.begin(), m_Order.end(),
              [&depth](u32 a, u32 b) { return depth[a] > depth[b]; });

    m_Sorted.resize(m_Count);
    for (u32 i = 0; i < m_Count; ++i) m_Sorted[i] = m_Cpu[m_Order[i]];
    m_InstanceBuffer->UploadData(m_Sorted.data(), m_Count * sizeof(Assets::SplatInstance));
}

void SplatRenderer::EnsureDrawPipeline(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout,
                                       u32 colorAttachmentCount) {
    if (!m_Context || renderPass == VK_NULL_HANDLE) return;
    for (auto& e : m_Pipelines) {
        if (e.pass == renderPass) { m_Current = e.pipeline.get(); return; }
    }

    if (!m_VS) {
        m_VS = std::make_unique<Renderer::VulkanShader>(m_Context);
        if (!m_VS->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::SplatVertexShaderData),
                Renderer::ShaderData::SplatVertexShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "SplatRenderer: vertex shader load failed");
            m_VS.reset();
            return;
        }
    }
    if (!m_FS) {
        m_FS = std::make_unique<Renderer::VulkanShader>(m_Context);
        if (!m_FS->LoadFromSPIRV(
                reinterpret_cast<const u8*>(Renderer::ShaderData::SplatFragmentShaderData),
                Renderer::ShaderData::SplatFragmentShaderDataSize)) {
            ENJIN_LOG_ERROR(Renderer, "SplatRenderer: fragment shader load failed");
            m_FS.reset();
            return;
        }
    }

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Assets::SplatInstance);
    binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 4> attrs{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0};    // pos + opacity
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16};   // color
    attrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 32};   // scale
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 48};   // rotation

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    Renderer::PipelineConfig config;
    config.renderPass = renderPass;
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = false;   // blended splats occlude via sorting, not depth
    config.cullMode = VK_CULL_MODE_NONE;
    config.alphaBlend = true;
    config.colorAttachmentCount = colorAttachmentCount;   // MRT rule (VUID-07609)
    config.customVertexInput = &vertexInput;

    auto pipeline = std::make_unique<Renderer::VulkanPipeline>(m_Context);
    if (!pipeline->CreateWithLayout(config, m_VS.get(), m_FS.get(), sharedLayout)) {
        ENJIN_LOG_ERROR(Renderer, "SplatRenderer: pipeline creation failed");
        return;
    }
    PipelineEntry entry;
    entry.pass = renderPass;
    entry.pipeline = std::move(pipeline);
    m_Current = entry.pipeline.get();
    m_Pipelines.push_back(std::move(entry));
    ENJIN_LOG_INFO(Renderer, "SplatRenderer: pipeline created (%u attachment(s), %zu cached)",
                   colorAttachmentCount, m_Pipelines.size());
}

void SplatRenderer::RecreateDrawPipeline() {
    m_Current = nullptr;
    m_Pipelines.clear();
    m_VS.reset();
    m_FS.reset();
}

void SplatRenderer::Render(VkCommandBuffer cmd, VkDescriptorSet sharedSet,
                           const Math::Matrix4& model, f32 viewportW, f32 viewportH,
                           f32 opacityScale, f32 splatScale) {
    if (m_Count == 0 || !m_Current || !m_InstanceBuffer) return;

    m_Current->Bind(cmd);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_Current->GetLayout(), 0, 1, &sharedSet, 0, nullptr);

    struct SplatPush {
        Math::Matrix4 model;
        f32 params[4];
    } push;
    push.model = model;
    push.params[0] = viewportW;
    push.params[1] = viewportH;
    push.params[2] = opacityScale;
    push.params[3] = splatScale;
    vkCmdPushConstants(cmd, m_Current->GetLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(push), &push);

    VkBuffer vb = m_InstanceBuffer->GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
    vkCmdDraw(cmd, 6, m_Count, 0, 0);
}

} // namespace Effects
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
