#include "Enjin/Renderer/RayTracing/ReSTIR.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

// Push constants for the ReSTIR initial candidate compute shader
struct ReSTIRPushConstants {
    u32 totalLightCount;
    u32 initialCandidates;
    f32 distanceBias;
    u32 frameCount;
};

ReSTIR::ReSTIR(VulkanContext* context) : m_Context(context) {}

ReSTIR::~ReSTIR() { Shutdown(); }

bool ReSTIR::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;

    CreateReservoirBuffer();
    if (m_ReservoirBuffer == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create reservoir buffer");
        return false;
    }

    // Create pipeline layout with push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ReSTIRPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create pipeline layout");
        return false;
    }

    // Create compute pipeline from embedded SPIR-V
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = sizeof(RESTIR_INITIAL_COMP_SPV);
    shaderInfo.pCode = RESTIR_INITIAL_COMP_SPV;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_Context->GetDevice(), &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create shader module");
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_PipelineLayout;

    VkResult result = vkCreateComputePipelines(m_Context->GetDevice(), VK_NULL_HANDLE,
                                                1, &pipelineInfo, nullptr, &m_Pipeline);

    vkDestroyShaderModule(m_Context->GetDevice(), shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create compute pipeline");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "ReSTIR initialized (%ux%u, %u reservoirs, %u initial candidates)",
                   width, height, width * height, m_Config.initialCandidates);
    return true;
}

void ReSTIR::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyReservoirBuffer();
    CreateReservoirBuffer();
}

void ReSTIR::Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                       u32 totalLightCount) {
    if (!m_Initialized || !m_Config.enabled || totalLightCount == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_PipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    ReSTIRPushConstants pc{};
    pc.totalLightCount = totalLightCount;
    pc.initialCandidates = m_Config.initialCandidates;
    pc.distanceBias = m_Config.distanceBias;
    pc.frameCount = frameCount;

    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    // Dispatch with 8x8 workgroups
    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Barrier: reservoir buffer must be readable by subsequent RT shaders
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void ReSTIR::CreateReservoirBuffer() {
    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(m_Width) * m_Height * sizeof(Reservoir);
    if (bufferSize == 0) return;

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Context->GetDevice(), &bufInfo, nullptr, &m_ReservoirBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create reservoir buffer");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_ReservoirBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_ReservoirMemory) != VK_SUCCESS) {
        vkDestroyBuffer(m_Context->GetDevice(), m_ReservoirBuffer, nullptr);
        m_ReservoirBuffer = VK_NULL_HANDLE;
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to allocate reservoir memory");
        return;
    }

    vkBindBufferMemory(m_Context->GetDevice(), m_ReservoirBuffer, m_ReservoirMemory, 0);
    m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

    ENJIN_LOG_INFO(Renderer, "ReSTIR: Created reservoir buffer (%u bytes, %u reservoirs)",
                   static_cast<u32>(bufferSize), m_Width * m_Height);
}

void ReSTIR::DestroyReservoirBuffer() {
    if (m_ReservoirBuffer) {
        vkDestroyBuffer(m_Context->GetDevice(), m_ReservoirBuffer, nullptr);
        m_ReservoirBuffer = VK_NULL_HANDLE;
    }
    if (m_ReservoirMemory) {
        vkFreeMemory(m_Context->GetDevice(), m_ReservoirMemory, nullptr);
        m_ReservoirMemory = VK_NULL_HANDLE;
    }
}

void ReSTIR::Shutdown() {
    if (!m_Initialized) return;

    DestroyReservoirBuffer();

    if (m_Pipeline) {
        vkDestroyPipeline(m_Context->GetDevice(), m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    m_Initialized = false;
    ENJIN_LOG_INFO(Renderer, "ReSTIR shut down");
}

} // namespace Renderer
} // namespace Enjin
