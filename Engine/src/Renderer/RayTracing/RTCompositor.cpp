#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

RTCompositor::RTCompositor(VulkanContext* context) : m_Context(context) {}
RTCompositor::~RTCompositor() { Shutdown(); }

bool RTCompositor::Initialize(VkDescriptorSetLayout rtDescLayout) {
    VkDevice device = m_Context->GetDevice();

    // Pipeline layout with push constants for composite strengths
    // Uses the RT descriptor set layout directly for compatibility
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = 32;  // shadowStr(4), reflStr(4), aoStr(4), giStr(4), enableFlags(4), pad[3](12)

    VkPipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts = &rtDescLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pushRange;
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &m_PipelineLayout);

    // Create compute pipeline
    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = sizeof(RT_COMPOSITE_COMP_SPV);
    moduleInfo.pCode = RT_COMPOSITE_COMP_SPV;

    VkShaderModule module;
    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &module) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT composite shader module");
        return false;
    }

    VkComputePipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeInfo.stage.module = module;
    pipeInfo.stage.pName = "main";
    pipeInfo.layout = m_PipelineLayout;

    VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_Pipeline);
    vkDestroyShaderModule(device, module, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT composite pipeline");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RT Compositor initialized");
    return true;
}

void RTCompositor::Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                             u32 width, u32 height, u32 enableFlags) {
    if (!m_Initialized) return;

    struct CompositePushConstants {
        f32 shadowStrength;
        f32 reflectionStrength;
        f32 aoStrength;
        f32 giStrength;
        i32 enableFlags;
        i32 pad[3];
    };

    CompositePushConstants pc{};
    pc.shadowStrength = m_Config.shadowStrength;
    pc.reflectionStrength = m_Config.reflectionStrength;
    pc.aoStrength = m_Config.aoStrength;
    pc.giStrength = m_Config.giStrength;
    pc.enableFlags = static_cast<i32>(enableFlags);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout,
                             0, 1, &rtDescSet, 0, nullptr);
    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    u32 groupsX = (width + 7) / 8;
    u32 groupsY = (height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);
}

void RTCompositor::Shutdown() {
    if (!m_Initialized) return;
    VkDevice device = m_Context->GetDevice();
    if (m_Pipeline) { vkDestroyPipeline(device, m_Pipeline, nullptr); m_Pipeline = VK_NULL_HANDLE; }
    if (m_PipelineLayout) { vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
    m_Initialized = false;
}

} // namespace Renderer
} // namespace Enjin
