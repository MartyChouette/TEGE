#include "Enjin/Renderer/RayTracing/ReSTIR.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cstring>

namespace Enjin {
namespace Renderer {

// Push constants for the ReSTIR initial candidate compute shader
struct ReSTIRInitialPushConstants {
    u32 totalLightCount;
    u32 initialCandidates;
    f32 distanceBias;
    u32 frameCount;
};

// Push constants for the ReSTIR temporal reuse compute shader
struct ReSTIRTemporalPushConstants {
    u32 totalLightCount;
    u32 temporalMaxHistory;
    f32 depthThreshold;
    f32 normalThreshold;
    u32 frameCount;
};

// Push constants for the ReSTIR spatial reuse compute shader
struct ReSTIRSpatialPushConstants {
    u32 totalLightCount;
    u32 spatialNeighbors;
    f32 spatialRadius;
    f32 depthThreshold;
    f32 normalThreshold;
    u32 frameCount;
};

ReSTIR::ReSTIR(VulkanContext* context) : m_Context(context) {}

ReSTIR::~ReSTIR() { Shutdown(); }

bool ReSTIR::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;

    CreateReservoirBuffers();
    if (m_ReservoirBuffers[0] == VK_NULL_HANDLE || m_ReservoirBuffers[1] == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create reservoir buffers");
        return false;
    }

    // Create initial candidate pipeline layout with push constants
    VkPushConstantRange initialPCRange{};
    initialPCRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    initialPCRange.offset = 0;
    initialPCRange.size = sizeof(ReSTIRInitialPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &initialPCRange;

    if (vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_InitialPipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create initial pipeline layout");
        return false;
    }

    // Create initial candidate compute pipeline from embedded SPIR-V
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = sizeof(RESTIR_INITIAL_COMP_SPV);
    shaderInfo.pCode = RESTIR_INITIAL_COMP_SPV;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_Context->GetDevice(), &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create initial shader module");
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_InitialPipelineLayout;

    VkResult result = vkCreateComputePipelines(m_Context->GetDevice(), VK_NULL_HANDLE,
                                                1, &pipelineInfo, nullptr, &m_InitialPipeline);

    vkDestroyShaderModule(m_Context->GetDevice(), shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create initial compute pipeline");
        return false;
    }

    // Create temporal reuse pipeline
    if (!CreateTemporalPipeline(rtDescLayout)) {
        ENJIN_LOG_WARN(Renderer, "ReSTIR: Temporal reuse pipeline creation failed — temporal reuse disabled");
    }

    // Create spatial reuse pipeline
    if (!CreateSpatialPipeline(rtDescLayout)) {
        ENJIN_LOG_WARN(Renderer, "ReSTIR: Spatial reuse pipeline creation failed — spatial reuse disabled");
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "ReSTIR initialized (%ux%u, %u reservoirs, %u initial candidates, temporal=%s, spatial=%s)",
                   width, height, width * height, m_Config.initialCandidates,
                   m_TemporalPipeline ? "available" : "unavailable",
                   m_SpatialPipeline ? "available" : "unavailable");
    return true;
}

bool ReSTIR::CreateTemporalPipeline(VkDescriptorSetLayout rtDescLayout) {
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(ReSTIRTemporalPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;

    if (vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_TemporalPipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create temporal pipeline layout");
        return false;
    }

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = sizeof(RESTIR_TEMPORAL_COMP_SPV);
    shaderInfo.pCode = RESTIR_TEMPORAL_COMP_SPV;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_Context->GetDevice(), &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create temporal shader module");
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_TemporalPipelineLayout;

    VkResult result = vkCreateComputePipelines(m_Context->GetDevice(), VK_NULL_HANDLE,
                                                1, &pipelineInfo, nullptr, &m_TemporalPipeline);

    vkDestroyShaderModule(m_Context->GetDevice(), shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create temporal compute pipeline");
        return false;
    }

    return true;
}

bool ReSTIR::CreateSpatialPipeline(VkDescriptorSetLayout rtDescLayout) {
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(ReSTIRSpatialPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;

    if (vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_SpatialPipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create spatial pipeline layout");
        return false;
    }

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = sizeof(RESTIR_SPATIAL_COMP_SPV);
    shaderInfo.pCode = RESTIR_SPATIAL_COMP_SPV;

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_Context->GetDevice(), &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create spatial shader module");
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_SpatialPipelineLayout;

    VkResult result = vkCreateComputePipelines(m_Context->GetDevice(), VK_NULL_HANDLE,
                                                1, &pipelineInfo, nullptr, &m_SpatialPipeline);

    vkDestroyShaderModule(m_Context->GetDevice(), shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create spatial compute pipeline");
        return false;
    }

    return true;
}

void ReSTIR::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyReservoirBuffers();
    CreateReservoirBuffers();
}

void ReSTIR::Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                       u32 totalLightCount) {
    if (!m_Initialized || !m_Config.enabled || totalLightCount == 0) return;

    // Swap ping-pong buffers -- previous frame's output becomes this frame's prev input
    m_CurrentBuffer = 1 - m_CurrentBuffer;

    // Update descriptor set bindings 19/20 to reflect the swapped buffers.
    // Binding 19 = current frame's reservoir (read-write by all passes)
    // Binding 20 = previous frame's reservoir (read-only by temporal pass)
    {
        VkDescriptorBufferInfo curBufInfo{};
        curBufInfo.buffer = m_ReservoirBuffers[m_CurrentBuffer];
        curBufInfo.offset = 0;
        curBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo prevBufInfo{};
        prevBufInfo.buffer = m_ReservoirBuffers[1 - m_CurrentBuffer];
        prevBufInfo.offset = 0;
        prevBufInfo.range = VK_WHOLE_SIZE;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = rtDescSet;
        writes[0].dstBinding = 19;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].pBufferInfo = &curBufInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = rtDescSet;
        writes[1].dstBinding = 20;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &prevBufInfo;

        vkUpdateDescriptorSets(m_Context->GetDevice(),
                               static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    // Pass 1: Initial candidate generation (writes to current buffer at binding 19)
    DispatchInitial(cmd, rtDescSet, frameCount, totalLightCount);

    // Pass 2: Temporal reuse (reads prev buffer at binding 20, modifies current at binding 19)
    if (m_Config.temporalReuse && m_TemporalPipeline) {
        DispatchTemporal(cmd, rtDescSet, frameCount, totalLightCount);
    }

    // Pass 3: Spatial reuse (modifies current buffer at binding 19 in-place)
    if (m_Config.spatialReuse && m_SpatialPipeline) {
        DispatchSpatial(cmd, rtDescSet, frameCount, totalLightCount);
    }

    // Final barrier: ensure reservoir data is fully written and visible to RT shaders.
    // The per-pass barriers only cover compute->compute transitions. If the last
    // dispatched pass was initial (no temporal/spatial), its barrier only targets
    // COMPUTE_SHADER_BIT. We need an additional barrier for RAY_TRACING_SHADER_BIT_KHR.
    if (!m_Config.spatialReuse || !m_SpatialPipeline) {
        VkMemoryBarrier finalBarrier{};
        finalBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              0, 1, &finalBarrier, 0, nullptr, 0, nullptr);
    }
}

void ReSTIR::DispatchInitial(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                              u32 totalLightCount) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_InitialPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_InitialPipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    ReSTIRInitialPushConstants pc{};
    pc.totalLightCount = totalLightCount;
    pc.initialCandidates = m_Config.initialCandidates;
    pc.distanceBias = m_Config.distanceBias;
    pc.frameCount = frameCount;

    vkCmdPushConstants(cmd, m_InitialPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Barrier: initial pass output must be readable by temporal/spatial passes
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void ReSTIR::DispatchTemporal(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                               u32 totalLightCount) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_TemporalPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_TemporalPipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    ReSTIRTemporalPushConstants pc{};
    pc.totalLightCount = totalLightCount;
    pc.temporalMaxHistory = m_Config.temporalMaxHistory;
    pc.depthThreshold = m_Config.temporalDepthThreshold;
    pc.normalThreshold = m_Config.temporalNormalThreshold;
    pc.frameCount = frameCount;

    vkCmdPushConstants(cmd, m_TemporalPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Barrier: temporal pass output must be readable by spatial pass or RT shaders
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void ReSTIR::DispatchSpatial(VkCommandBuffer cmd, VkDescriptorSet rtDescSet, u32 frameCount,
                              u32 totalLightCount) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_SpatialPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_SpatialPipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    ReSTIRSpatialPushConstants pc{};
    pc.totalLightCount = totalLightCount;
    pc.spatialNeighbors = m_Config.spatialNeighbors;
    pc.spatialRadius = m_Config.spatialRadius;
    pc.depthThreshold = m_Config.spatialDepthThreshold;
    pc.normalThreshold = m_Config.spatialNormalThreshold;
    pc.frameCount = frameCount;

    vkCmdPushConstants(cmd, m_SpatialPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Final barrier: reservoir buffer must be readable by subsequent RT shaders
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void ReSTIR::CreateReservoirBuffers() {
    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(m_Width) * m_Height * sizeof(Reservoir);
    if (bufferSize == 0) return;

    for (u32 i = 0; i < 2; ++i) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_Context->GetDevice(), &bufInfo, nullptr, &m_ReservoirBuffers[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to create reservoir buffer %u", i);
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_ReservoirBuffers[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_ReservoirMemory[i]) != VK_SUCCESS) {
            vkDestroyBuffer(m_Context->GetDevice(), m_ReservoirBuffers[i], nullptr);
            m_ReservoirBuffers[i] = VK_NULL_HANDLE;
            ENJIN_LOG_ERROR(Renderer, "ReSTIR: Failed to allocate reservoir memory %u", i);
            return;
        }

        vkBindBufferMemory(m_Context->GetDevice(), m_ReservoirBuffers[i], m_ReservoirMemory[i], 0);
        m_Context->TrackAllocation(static_cast<usize>(memReqs.size));
    }

    ENJIN_LOG_INFO(Renderer, "ReSTIR: Created ping-pong reservoir buffers (%u bytes each, %u reservoirs)",
                   static_cast<u32>(bufferSize), m_Width * m_Height);
}

void ReSTIR::DestroyReservoirBuffers() {
    for (u32 i = 0; i < 2; ++i) {
        if (m_ReservoirBuffers[i]) {
            vkDestroyBuffer(m_Context->GetDevice(), m_ReservoirBuffers[i], nullptr);
            m_ReservoirBuffers[i] = VK_NULL_HANDLE;
        }
        if (m_ReservoirMemory[i]) {
            vkFreeMemory(m_Context->GetDevice(), m_ReservoirMemory[i], nullptr);
            m_ReservoirMemory[i] = VK_NULL_HANDLE;
        }
    }
}

void ReSTIR::Shutdown() {
    if (!m_Initialized) return;

    DestroyReservoirBuffers();

    if (m_InitialPipeline) {
        vkDestroyPipeline(m_Context->GetDevice(), m_InitialPipeline, nullptr);
        m_InitialPipeline = VK_NULL_HANDLE;
    }
    if (m_InitialPipelineLayout) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_InitialPipelineLayout, nullptr);
        m_InitialPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_TemporalPipeline) {
        vkDestroyPipeline(m_Context->GetDevice(), m_TemporalPipeline, nullptr);
        m_TemporalPipeline = VK_NULL_HANDLE;
    }
    if (m_TemporalPipelineLayout) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_TemporalPipelineLayout, nullptr);
        m_TemporalPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_SpatialPipeline) {
        vkDestroyPipeline(m_Context->GetDevice(), m_SpatialPipeline, nullptr);
        m_SpatialPipeline = VK_NULL_HANDLE;
    }
    if (m_SpatialPipelineLayout) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_SpatialPipelineLayout, nullptr);
        m_SpatialPipelineLayout = VK_NULL_HANDLE;
    }

    m_Initialized = false;
    ENJIN_LOG_INFO(Renderer, "ReSTIR shut down");
}

} // namespace Renderer
} // namespace Enjin
