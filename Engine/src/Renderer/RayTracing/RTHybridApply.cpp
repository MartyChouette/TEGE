#include "Enjin/Renderer/RayTracing/RTHybridApply.h"
#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Logging/Log.h"
#include <array>

namespace Enjin {
namespace Renderer {

struct RTHybridApplyPushConstants {
    u32 mode;              // 0 = multiply (shadow*AO), 1 = additive (reflect+GI)
    f32 shadowStrength;
    f32 aoStrength;
    f32 reflectStrength;
    f32 giStrength;
};

bool RTHybridApply::Initialize(VkRenderPass renderPass, u32 colorAttachmentCount) {
    m_RenderPass = renderPass;
    m_ColorAttachmentCount = (colorAttachmentCount == 0) ? 1 : colorAttachmentCount;
    VkDevice device = m_Context->GetDevice();

    // Descriptor layout: 4 fragment-stage samplers (shadow/AO/reflect/GI)
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    for (u32 i = 0; i < 4; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = 4;
    li.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(device, &li, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "RTHybridApply: failed to create descriptor set layout");
        return false;
    }

    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 };
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &poolSize;
    pi.maxSets = 1;
    if (vkCreateDescriptorPool(device, &pi, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "RTHybridApply: failed to create descriptor pool");
        return false;
    }

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_DescriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &m_DescriptorSetLayout;
    if (vkAllocateDescriptorSets(device, &ai, &m_DescriptorSet) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "RTHybridApply: failed to allocate descriptor set");
        return false;
    }

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(RTHybridApplyPushConstants);
    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &m_DescriptorSetLayout;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(device, &pli, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "RTHybridApply: failed to create pipeline layout");
        return false;
    }

    if (!CreatePipelines()) return false;

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RTHybridApply initialized (player hybrid overlay)");
    return true;
}

bool RTHybridApply::CreatePipelines() {
    VkDevice device = m_Context->GetDevice();

    VkShaderModuleCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vi.codeSize = ShaderData::FullscreenVertexShaderDataSize;
    vi.pCode = reinterpret_cast<const u32*>(ShaderData::FullscreenVertexShaderData);
    VkShaderModule vert = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &vi, nullptr, &vert) != VK_SUCCESS) return false;

    VkShaderModuleCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fi.codeSize = ShaderData::RTHybridApplyFragmentShaderDataSize;
    fi.pCode = reinterpret_cast<const u32*>(ShaderData::RTHybridApplyFragmentShaderData);
    VkShaderModule frag = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &fi, nullptr, &frag) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert, nullptr);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo dss{};
    dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    dss.depthTestEnable = VK_FALSE;
    dss.depthWriteEnable = VK_FALSE;

    // Blend attachment 0 differs per pipeline; attachment 1 (velocity, if the MRT
    // pass has one) is write-masked off so the overlay leaves it untouched.
    auto makePipeline = [&](bool additive, VkPipeline& out) -> bool {
        VkPipelineColorBlendAttachmentState blend[2]{};
        blend[0].blendEnable = VK_TRUE;
        blend[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (additive) {
            // scene += src
            blend[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        } else {
            // scene *= src
            blend[0].srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
            blend[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        }
        blend[0].colorBlendOp = VK_BLEND_OP_ADD;
        blend[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend[0].alphaBlendOp = VK_BLEND_OP_ADD;
        blend[1].colorWriteMask = 0;  // velocity untouched

        u32 blendCount = (m_ColorAttachmentCount <= 2) ? m_ColorAttachmentCount : 2;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = blendCount;
        cb.pAttachments = blend;

        VkGraphicsPipelineCreateInfo gp{};
        gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        gp.stageCount = 2;
        gp.pStages = stages;
        gp.pVertexInputState = &vin;
        gp.pInputAssemblyState = &ia;
        gp.pViewportState = &vp;
        gp.pRasterizationState = &rs;
        gp.pMultisampleState = &ms;
        gp.pDepthStencilState = &dss;
        gp.pColorBlendState = &cb;
        gp.pDynamicState = &ds;
        gp.layout = m_PipelineLayout;
        gp.renderPass = m_RenderPass;
        gp.subpass = 0;
        return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp, nullptr, &out) == VK_SUCCESS;
    };

    bool ok = makePipeline(false, m_MultiplyPipeline) && makePipeline(true, m_AdditivePipeline);

    vkDestroyShaderModule(device, vert, nullptr);
    vkDestroyShaderModule(device, frag, nullptr);
    if (!ok) ENJIN_LOG_ERROR(Renderer, "RTHybridApply: failed to create blend pipelines");
    return ok;
}

void RTHybridApply::DestroyPipelines() {
    VkDevice device = m_Context->GetDevice();
    if (m_MultiplyPipeline) { vkDestroyPipeline(device, m_MultiplyPipeline, nullptr); m_MultiplyPipeline = VK_NULL_HANDLE; }
    if (m_AdditivePipeline) { vkDestroyPipeline(device, m_AdditivePipeline, nullptr); m_AdditivePipeline = VK_NULL_HANDLE; }
}

void RTHybridApply::UpdateRenderPass(VkRenderPass renderPass, u32 colorAttachmentCount) {
    if (!m_Initialized) return;
    if (renderPass == m_RenderPass && colorAttachmentCount == m_ColorAttachmentCount) return;
    m_RenderPass = renderPass;
    m_ColorAttachmentCount = (colorAttachmentCount == 0) ? 1 : colorAttachmentCount;
    DestroyPipelines();
    CreatePipelines();
}

void RTHybridApply::SetInputs(VkImageView shadow, VkImageView ao, VkImageView reflect,
                              VkImageView gi, VkSampler sampler) {
    if (!m_Initialized) return;
    if (shadow == VK_NULL_HANDLE || ao == VK_NULL_HANDLE || reflect == VK_NULL_HANDLE ||
        gi == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) return;
    if (shadow == m_ShadowView && ao == m_AOView && reflect == m_ReflectView &&
        gi == m_GIView && sampler == m_Sampler && m_DescriptorsWritten) return;

    m_ShadowView = shadow; m_AOView = ao; m_ReflectView = reflect; m_GIView = gi; m_Sampler = sampler;

    VkImageView views[4] = { shadow, ao, reflect, gi };
    std::array<VkDescriptorImageInfo, 4> infos{};
    std::array<VkWriteDescriptorSet, 4> writes{};
    for (u32 i = 0; i < 4; ++i) {
        infos[i].imageView = views[i];
        infos[i].sampler = sampler;
        infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_DescriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &infos[i];
    }
    vkUpdateDescriptorSets(m_Context->GetDevice(), 4, writes.data(), 0, nullptr);
    m_DescriptorsWritten = true;
}

void RTHybridApply::Apply(VkCommandBuffer cmd, u32 width, u32 height,
                          f32 shadowStrength, f32 aoStrength, f32 reflectStrength, f32 giStrength) {
    if (!m_Initialized || !m_DescriptorsWritten || m_MultiplyPipeline == VK_NULL_HANDLE) return;

    VkViewport vp{ 0.0f, 0.0f, static_cast<f32>(width), static_cast<f32>(height), 0.0f, 1.0f };
    VkRect2D sc{ {0, 0}, { width, height } };
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

    RTHybridApplyPushConstants pc{};
    pc.shadowStrength = shadowStrength;
    pc.aoStrength = aoStrength;
    pc.reflectStrength = reflectStrength;
    pc.giStrength = giStrength;

    // Multiply pass: scene *= shadow*AO
    pc.mode = 0;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MultiplyPipeline);
    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // Additive pass: scene += reflect+GI
    pc.mode = 1;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_AdditivePipeline);
    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void RTHybridApply::Shutdown() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();
    DestroyPipelines();
    if (m_PipelineLayout) { vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
    if (m_DescriptorPool) { vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr); m_DescriptorPool = VK_NULL_HANDLE; }
    if (m_DescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr); m_DescriptorSetLayout = VK_NULL_HANDLE; }
    m_Initialized = false;
    m_DescriptorsWritten = false;
}

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
