#include "Enjin/Renderer/ClusteredLighting.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/ClusterComputeShaderData.h"  // embedded compute SPIR-V
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

ClusteredLightingSystem::ClusteredLightingSystem(VulkanContext* context)
    : m_Context(context) {}

ClusteredLightingSystem::~ClusteredLightingSystem() {
    Shutdown();
}

bool ClusteredLightingSystem::Initialize(u32 screenWidth, u32 screenHeight) {
    if (m_Initialized) return true;

    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    if (!CreateBuffers()) {
        ENJIN_LOG_ERROR(Renderer, "ClusteredLighting: Failed to create buffers");
        return false;
    }

    if (!CreateDescriptorSets()) {
        ENJIN_LOG_ERROR(Renderer, "ClusteredLighting: Failed to create descriptor sets");
        return false;
    }

    if (!CreateComputePipelines()) {
        ENJIN_LOG_WARN(Renderer, "ClusteredLighting: Compute pipelines not available, using CPU fallback");
        // Continue — system can still provide buffers for shader access
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "ClusteredLighting initialized (%ux%ux%u = %u clusters, screen %ux%u)",
        ClusterGridConfig::TILES_X, ClusterGridConfig::TILES_Y, ClusterGridConfig::TILES_Z,
        ClusterGridConfig::TOTAL_CLUSTERS, screenWidth, screenHeight);
    return true;
}

void ClusteredLightingSystem::Shutdown() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    auto destroyPipeline = [&](VkPipeline& p) { if (p != VK_NULL_HANDLE) { vkDestroyPipeline(device, p, nullptr); p = VK_NULL_HANDLE; } };
    auto destroyLayout = [&](VkPipelineLayout& l) { if (l != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, l, nullptr); l = VK_NULL_HANDLE; } };
    auto destroyDSLayout = [&](VkDescriptorSetLayout& l) { if (l != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, l, nullptr); l = VK_NULL_HANDLE; } };

    destroyPipeline(m_BoundsPipeline);
    destroyPipeline(m_AssignPipeline);
    destroyLayout(m_BoundsPipelineLayout);
    destroyLayout(m_AssignPipelineLayout);
    destroyDSLayout(m_BoundsDescSetLayout);
    destroyDSLayout(m_AssignDescSetLayout);
    destroyDSLayout(m_DescriptorSetLayout);

    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    m_ClusterAABBBuffer.reset();
    m_LightGridBuffer.reset();
    m_LightIndexBuffer.reset();
    m_LightBuffer.reset();
    m_GlobalIndexCountBuffer.reset();
    m_ClusterParamsBuffer.reset();

    m_Initialized = false;
}

bool ClusteredLightingSystem::CreateBuffers() {
    using BC = ClusterGridConfig;

    // Cluster AABB buffer
    m_ClusterAABBBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_ClusterAABBBuffer->Create(BC::TOTAL_CLUSTERS * sizeof(ClusterAABB), BufferUsage::Storage, true)) {
        return false;
    }

    // Light grid buffer (offset + count per cluster)
    m_LightGridBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_LightGridBuffer->Create(BC::TOTAL_CLUSTERS * sizeof(ClusterLightGrid), BufferUsage::Storage, true)) {
        return false;
    }

    // Light index list buffer
    m_LightIndexBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_LightIndexBuffer->Create(BC::MAX_LIGHT_INDICES * sizeof(u32), BufferUsage::Storage, true)) {
        return false;
    }

    // Light data buffer
    m_LightBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_LightBuffer->Create(BC::MAX_LIGHTS * sizeof(ClusterLight), BufferUsage::Storage, true)) {
        return false;
    }

    // Atomic counter for light index allocation
    m_GlobalIndexCountBuffer = std::make_unique<VulkanBuffer>(m_Context);
    VkBufferUsageFlags counterUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (!m_GlobalIndexCountBuffer->Create(sizeof(u32), counterUsage, true)) {
        return false;
    }

    // Cluster params UBO
    struct ClusterParams {
        Math::Matrix4 inverseProjection;
        f32 nearPlane;
        f32 farPlane;
        f32 screenWidth;
        f32 screenHeight;
        u32 tilesX;
        u32 tilesY;
        u32 tilesZ;
        u32 lightCount;
    };
    m_ClusterParamsBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_ClusterParamsBuffer->Create(sizeof(ClusterParams), BufferUsage::Uniform, true)) {
        return false;
    }

    return true;
}

bool ClusteredLightingSystem::CreateDescriptorSets() {
    VkDevice device = m_Context->GetDevice();

    // Fragment shader descriptor set layout (bindings 14-15 for LightGrid + LightIndex)
    VkDescriptorSetLayoutBinding fragBindings[3] = {};
    // Binding 14: LightGrid SSBO
    fragBindings[0].binding = 0;
    fragBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    fragBindings[0].descriptorCount = 1;
    fragBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // Binding 15: LightIndex SSBO
    fragBindings[1].binding = 1;
    fragBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    fragBindings[1].descriptorCount = 1;
    fragBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // Binding 16: Light data SSBO (for reading actual light properties)
    fragBindings[2].binding = 2;
    fragBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    fragBindings[2].descriptorCount = 1;
    fragBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo fragLayoutInfo{};
    fragLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    fragLayoutInfo.bindingCount = 3;
    fragLayoutInfo.pBindings = fragBindings;
    if (vkCreateDescriptorSetLayout(device, &fragLayoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        return false;
    }

    // Compute bounds descriptor set layout
    VkDescriptorSetLayoutBinding boundsBindings[2] = {};
    boundsBindings[0].binding = 0;
    boundsBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    boundsBindings[0].descriptorCount = 1;
    boundsBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    boundsBindings[1].binding = 1;
    boundsBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    boundsBindings[1].descriptorCount = 1;
    boundsBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo boundsLayoutInfo{};
    boundsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    boundsLayoutInfo.bindingCount = 2;
    boundsLayoutInfo.pBindings = boundsBindings;
    if (vkCreateDescriptorSetLayout(device, &boundsLayoutInfo, nullptr, &m_BoundsDescSetLayout) != VK_SUCCESS) {
        return false;
    }

    // Compute assign descriptor set layout (all buffers)
    VkDescriptorSetLayoutBinding assignBindings[6] = {};
    for (u32 i = 0; i < 6; ++i) {
        assignBindings[i].binding = i;
        assignBindings[i].descriptorType = (i == 5) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        assignBindings[i].descriptorCount = 1;
        assignBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo assignLayoutInfo{};
    assignLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    assignLayoutInfo.bindingCount = 6;
    assignLayoutInfo.pBindings = assignBindings;
    if (vkCreateDescriptorSetLayout(device, &assignLayoutInfo, nullptr, &m_AssignDescSetLayout) != VK_SUCCESS) {
        return false;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 12 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 }
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 3;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        return false;
    }

    // Allocate descriptor sets
    VkDescriptorSetLayout layouts[] = { m_DescriptorSetLayout, m_BoundsDescSetLayout, m_AssignDescSetLayout };
    VkDescriptorSet sets[3];
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 3;
    allocInfo.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(device, &allocInfo, sets) != VK_SUCCESS) {
        return false;
    }
    m_DescriptorSet = sets[0];
    m_BoundsDescSet = sets[1];
    m_AssignDescSet = sets[2];

    // Write fragment descriptor set
    VkDescriptorBufferInfo gridInfo{ m_LightGridBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo indexInfo{ m_LightIndexBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo lightInfo{ m_LightBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };

    VkWriteDescriptorSet fragWrites[3] = {};
    for (u32 i = 0; i < 3; ++i) {
        fragWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        fragWrites[i].dstSet = m_DescriptorSet;
        fragWrites[i].dstBinding = i;
        fragWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        fragWrites[i].descriptorCount = 1;
    }
    fragWrites[0].pBufferInfo = &gridInfo;
    fragWrites[1].pBufferInfo = &indexInfo;
    fragWrites[2].pBufferInfo = &lightInfo;
    vkUpdateDescriptorSets(device, 3, fragWrites, 0, nullptr);

    // Write bounds descriptor set
    VkDescriptorBufferInfo aabbInfo{ m_ClusterAABBBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo paramsInfo{ m_ClusterParamsBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    VkWriteDescriptorSet boundsWrites[2] = {};
    boundsWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    boundsWrites[0].dstSet = m_BoundsDescSet;
    boundsWrites[0].dstBinding = 0;
    boundsWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    boundsWrites[0].descriptorCount = 1;
    boundsWrites[0].pBufferInfo = &aabbInfo;
    boundsWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    boundsWrites[1].dstSet = m_BoundsDescSet;
    boundsWrites[1].dstBinding = 1;
    boundsWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    boundsWrites[1].descriptorCount = 1;
    boundsWrites[1].pBufferInfo = &paramsInfo;
    vkUpdateDescriptorSets(device, 2, boundsWrites, 0, nullptr);

    // Write assign descriptor set
    VkDescriptorBufferInfo assignBufferInfos[6];
    assignBufferInfos[0] = { m_ClusterAABBBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    assignBufferInfos[1] = { m_LightBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    assignBufferInfos[2] = { m_LightGridBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    assignBufferInfos[3] = { m_LightIndexBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    assignBufferInfos[4] = { m_GlobalIndexCountBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    assignBufferInfos[5] = { m_ClusterParamsBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };

    VkWriteDescriptorSet assignWrites[6] = {};
    for (u32 i = 0; i < 6; ++i) {
        assignWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        assignWrites[i].dstSet = m_AssignDescSet;
        assignWrites[i].dstBinding = i;
        assignWrites[i].descriptorType = (i == 5) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        assignWrites[i].descriptorCount = 1;
        assignWrites[i].pBufferInfo = &assignBufferInfos[i];
    }
    vkUpdateDescriptorSets(device, 6, assignWrites, 0, nullptr);

    return true;
}

bool ClusteredLightingSystem::CreateComputePipelines() {
    VkDevice device = m_Context->GetDevice();

    // --- Bounds pipeline ---
    VkPipelineLayoutCreateInfo boundsLayoutInfo{};
    boundsLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    boundsLayoutInfo.setLayoutCount = 1;
    boundsLayoutInfo.pSetLayouts = &m_BoundsDescSetLayout;
    if (vkCreatePipelineLayout(device, &boundsLayoutInfo, nullptr, &m_BoundsPipelineLayout) != VK_SUCCESS) {
        return false;
    }

    VulkanShader boundsShader(m_Context);
    // Prefer embedded SPIR-V so clustered lighting works in shipped Player builds
    // (no loose .spv on disk); fall back to disk for dev/hot-reload workflows.
    bool boundsLoaded = boundsShader.LoadFromSPIRV(LightClusterBoundsComputeShaderData,
                                                   LightClusterBoundsComputeShaderDataSize)
                        && boundsShader.GetModule() != VK_NULL_HANDLE;
    if (!boundsLoaded) {
        const char* boundsPaths[] = {
            "shaders/light_cluster_bounds.comp.spv",
            "Engine/shaders/light_cluster_bounds.comp.spv",
            "../Engine/shaders/light_cluster_bounds.comp.spv",
            "../../Engine/shaders/light_cluster_bounds.comp.spv"
        };
        for (const char* path : boundsPaths) {
            if (boundsShader.LoadFromFile(path) && boundsShader.GetModule() != VK_NULL_HANDLE) {
                boundsLoaded = true;
                break;
            }
        }
    }

    if (boundsLoaded) {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_BoundsPipelineLayout;
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = boundsShader.GetModule();
        stage.pName = "main";
        pipelineInfo.stage = stage;
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_BoundsPipeline);
    }

    // --- Assign pipeline ---
    VkPipelineLayoutCreateInfo assignLayoutInfo{};
    assignLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    assignLayoutInfo.setLayoutCount = 1;
    assignLayoutInfo.pSetLayouts = &m_AssignDescSetLayout;
    if (vkCreatePipelineLayout(device, &assignLayoutInfo, nullptr, &m_AssignPipelineLayout) != VK_SUCCESS) {
        return false;
    }

    VulkanShader assignShader(m_Context);
    bool assignLoaded = assignShader.LoadFromSPIRV(LightClusterAssignComputeShaderData,
                                                   LightClusterAssignComputeShaderDataSize)
                        && assignShader.GetModule() != VK_NULL_HANDLE;
    if (!assignLoaded) {
        const char* assignPaths[] = {
            "shaders/light_cluster_assign.comp.spv",
            "Engine/shaders/light_cluster_assign.comp.spv",
            "../Engine/shaders/light_cluster_assign.comp.spv",
            "../../Engine/shaders/light_cluster_assign.comp.spv"
        };
        for (const char* path : assignPaths) {
            if (assignShader.LoadFromFile(path) && assignShader.GetModule() != VK_NULL_HANDLE) {
                assignLoaded = true;
                break;
            }
        }
    }

    if (assignLoaded) {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = m_AssignPipelineLayout;
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = assignShader.GetModule();
        stage.pName = "main";
        pipelineInfo.stage = stage;
        vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_AssignPipeline);
    }

    if (!boundsLoaded || !assignLoaded) {
        // Should not happen now that the SPIR-V is embedded, but if it does,
        // report failure so RenderSystem resets the system and the per-frame
        // clustered path is skipped instead of using null pipelines.
        ENJIN_LOG_WARN(Renderer, "ClusteredLighting: compute shaders unavailable — disabling clustered path");
        return false;
    }

    return true;
}

void ClusteredLightingSystem::UpdateClusterBounds(
    const Math::Matrix4& inverseProjection, f32 nearPlane, f32 farPlane
) {
    if (!m_Initialized) return;

    // Upload cluster params
    struct ClusterParams {
        Math::Matrix4 inverseProjection;
        f32 nearPlane;
        f32 farPlane;
        f32 screenWidth;
        f32 screenHeight;
        u32 tilesX;
        u32 tilesY;
        u32 tilesZ;
        u32 lightCount;
    } params;

    params.inverseProjection = inverseProjection;
    params.nearPlane = nearPlane;
    params.farPlane = farPlane;
    params.screenWidth = static_cast<f32>(m_ScreenWidth);
    params.screenHeight = static_cast<f32>(m_ScreenHeight);
    params.tilesX = ClusterGridConfig::TILES_X;
    params.tilesY = ClusterGridConfig::TILES_Y;
    params.tilesZ = ClusterGridConfig::TILES_Z;
    params.lightCount = 0;

    m_ClusterParamsBuffer->UploadData(&params, sizeof(params));
    m_BoundsNeedUpdate = false;
}

void ClusteredLightingSystem::AssignLights(
    VkCommandBuffer commandBuffer,
    const ClusterLight* lights,
    u32 lightCount,
    const Math::Matrix4& viewMatrix
) {
    if (!m_Initialized || lightCount == 0) return;

    // Upload light data to GPU
    u32 uploadCount = Math::Min(lightCount, ClusterGridConfig::MAX_LIGHTS);
    m_LightBuffer->UploadData(lights, uploadCount * sizeof(ClusterLight));

    // Update light count in params
    struct ClusterParams {
        Math::Matrix4 inverseProjection;
        f32 nearPlane;
        f32 farPlane;
        f32 screenWidth;
        f32 screenHeight;
        u32 tilesX;
        u32 tilesY;
        u32 tilesZ;
        u32 lightCount;
    };
    // Patch just the light count (offset 92 = 64 + 4*4 + 3*4)
    // Instead, re-read and re-upload the whole params to be safe
    void* mapped = m_ClusterParamsBuffer->Map();
    if (mapped) {
        auto* params = static_cast<ClusterParams*>(mapped);
        params->lightCount = uploadCount;
        m_ClusterParamsBuffer->Unmap();
    }

    if (m_AssignPipeline == VK_NULL_HANDLE) return;

    // Reset atomic counter
    vkCmdFillBuffer(commandBuffer, m_GlobalIndexCountBuffer->GetBuffer(), 0, sizeof(u32), 0);

    // Clear light grid
    vkCmdFillBuffer(commandBuffer, m_LightGridBuffer->GetBuffer(), 0,
        ClusterGridConfig::TOTAL_CLUSTERS * sizeof(ClusterLightGrid), 0);

    VkMemoryBarrier clearBarrier{};
    clearBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    clearBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    clearBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &clearBarrier, 0, nullptr, 0, nullptr);

    // Dispatch bounds computation (if needed)
    if (m_BoundsNeedUpdate && m_BoundsPipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_BoundsPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
            m_BoundsPipelineLayout, 0, 1, &m_BoundsDescSet, 0, nullptr);

        u32 boundsWorkgroups = (ClusterGridConfig::TOTAL_CLUSTERS + 63) / 64;
        vkCmdDispatch(commandBuffer, boundsWorkgroups, 1, 1);

        VkMemoryBarrier boundsBarrier{};
        boundsBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        boundsBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        boundsBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &boundsBarrier, 0, nullptr, 0, nullptr);

        m_BoundsNeedUpdate = false;
    }

    // Dispatch light assignment
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_AssignPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_AssignPipelineLayout, 0, 1, &m_AssignDescSet, 0, nullptr);

    u32 assignWorkgroups = (ClusterGridConfig::TOTAL_CLUSTERS + 63) / 64;
    vkCmdDispatch(commandBuffer, assignWorkgroups, 1, 1);

    // Barrier: compute → fragment shader read
    VkMemoryBarrier assignBarrier{};
    assignBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    assignBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    assignBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 1, &assignBarrier, 0, nullptr, 0, nullptr);
}

VkBuffer ClusteredLightingSystem::GetLightGridBuffer() const {
    return m_LightGridBuffer ? m_LightGridBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer ClusteredLightingSystem::GetLightIndexBuffer() const {
    return m_LightIndexBuffer ? m_LightIndexBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer ClusteredLightingSystem::GetLightBuffer() const {
    return m_LightBuffer ? m_LightBuffer->GetBuffer() : VK_NULL_HANDLE;
}

usize ClusteredLightingSystem::GetLightBufferSize() const {
    return m_LightBuffer ? m_LightBuffer->GetSize() : 0;
}

usize ClusteredLightingSystem::GetLightGridBufferSize() const {
    return m_LightGridBuffer ? m_LightGridBuffer->GetSize() : 0;
}

usize ClusteredLightingSystem::GetLightIndexBufferSize() const {
    return m_LightIndexBuffer ? m_LightIndexBuffer->GetSize() : 0;
}

void ClusteredLightingSystem::Resize(u32 screenWidth, u32 screenHeight) {
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;
    m_BoundsNeedUpdate = true;
}

} // namespace Renderer
} // namespace Enjin
