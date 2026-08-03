#include "Enjin/Renderer/GPUDriven/GPUCulling.h"
#include "Enjin/Renderer/GPUDriven/HiZPyramid.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <algorithm>
#include <cstring>
#include <cfloat>

namespace Enjin {
namespace Renderer {

GPUCullingSystem::GPUCullingSystem(VulkanContext* context)
    : m_Context(context) {
}

GPUCullingSystem::~GPUCullingSystem() {
    Shutdown();
}

bool GPUCullingSystem::Initialize() {
    ENJIN_LOG_INFO(Renderer, "Initializing GPU Culling System...");

    if (!CreateBuffers()) {
        return false;
    }

    if (!CreateComputePipeline()) {
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "GPU Culling System initialized (max objects: %u)", m_MaxObjects);
    return true;
}

void GPUCullingSystem::Shutdown() {
    if (m_CullPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Context->GetDevice(), m_CullPipeline, nullptr);
        m_CullPipeline = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_Context->GetDevice(), m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    // Destroying the pool frees every set allocated from it. The pool was NOT
    // created with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, so calling
    // vkFreeDescriptorSets here is both redundant and illegal (corrupts the pool
    // allocator -- validation VUID-vkFreeDescriptorSets-descriptorPool-00312).
    m_DescriptorSet = VK_NULL_HANDLE;

    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    m_ObjectBuffer.reset();
    m_IndirectDrawBuffer.reset();
    m_FrustumBuffer.reset();
    m_VisibilityBuffer.reset();
    m_DrawCountBuffer.reset();
    m_ObjectDataBuffer.reset();
    m_OcclusionFlagBuffer.reset();

    // Destroy two-phase HiZ pipeline resources
    if (m_CullHiZPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Context->GetDevice(), m_CullHiZPipeline, nullptr);
        m_CullHiZPipeline = VK_NULL_HANDLE;
    }
    if (m_HiZPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_HiZPipelineLayout, nullptr);
        m_HiZPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_HiZDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_Context->GetDevice(), m_HiZDescriptorSetLayout, nullptr);
        m_HiZDescriptorSetLayout = VK_NULL_HANDLE;
    }
}

VkBuffer GPUCullingSystem::GetIndirectDrawBuffer() const {
    return m_IndirectDrawBuffer ? m_IndirectDrawBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUCullingSystem::GetDrawCountBuffer() const {
    return m_DrawCountBuffer ? m_DrawCountBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUCullingSystem::GetObjectDataBuffer() const {
    return m_ObjectDataBuffer ? m_ObjectDataBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUCullingSystem::GetObjectBuffer() const {
    return m_ObjectBuffer ? m_ObjectBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUCullingSystem::GetVisibilityBuffer() const {
    return m_VisibilityBuffer ? m_VisibilityBuffer->GetBuffer() : VK_NULL_HANDLE;
}

VkBuffer GPUCullingSystem::GetOcclusionFlagBuffer() const {
    return m_OcclusionFlagBuffer ? m_OcclusionFlagBuffer->GetBuffer() : VK_NULL_HANDLE;
}

bool GPUCullingSystem::UploadObjectData(const void* data, usize sizeBytes) {
    if (!m_ObjectDataBuffer || !data || sizeBytes == 0) return false;
    usize maxSize = static_cast<usize>(m_MaxObjects) * OBJECT_DATA_GPU_SIZE;
    if (sizeBytes > maxSize) {
        ENJIN_LOG_WARN(Renderer, "ObjectData upload truncated: %zu > %zu bytes", sizeBytes, maxSize);
        sizeBytes = maxSize;
    }
    return m_ObjectDataBuffer->UploadData(data, sizeBytes);
}

void GPUCullingSystem::SubmitObjects(const std::vector<CullableObject>& objects) {
    if (objects.size() > m_MaxObjects) {
        ENJIN_LOG_WARN(Renderer, "Too many objects (%zu), truncating to %u", objects.size(), m_MaxObjects);
    }

    usize count = std::min(objects.size(), static_cast<usize>(m_MaxObjects));
    usize bufferSize = count * sizeof(CullableObject);

    if (!m_ObjectBuffer->UploadData(objects.data(), bufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload object data for culling");
        return;
    }

    m_Stats.totalObjects = static_cast<u32>(count);
    m_ObjectCount = static_cast<u32>(count);
}

bool GPUCullingSystem::ExecuteCulling(
    const Math::Matrix4& viewMatrix,
    const Math::Matrix4& projectionMatrix,
    VkCommandBuffer commandBuffer,
    VkBuffer& outIndirectDrawBuffer,
    u32& outDrawCount
) {
    if (commandBuffer == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "Invalid command buffer");
        return false;
    }

    if (m_ObjectCount == 0) {
        outDrawCount = 0;
        return true;
    }

    // CPU fallback when compute pipeline isn't available
    if (m_CullPipeline == VK_NULL_HANDLE) {
        // Mark all objects as visible (no culling)
        m_CachedVisibility.resize(m_ObjectCount);
        std::fill(m_CachedVisibility.begin(), m_CachedVisibility.end(), 1u);
        outDrawCount = m_ObjectCount;
        m_Stats.totalObjects = m_ObjectCount;
        m_Stats.visibleObjects = m_ObjectCount;
        m_Stats.culledObjects = 0;
        outIndirectDrawBuffer = m_IndirectDrawBuffer ? m_IndirectDrawBuffer->GetBuffer() : VK_NULL_HANDLE;
        return true;
    }

    // Update frustum planes
    Math::Matrix4 viewProj = projectionMatrix * viewMatrix;
    
    struct FrustumPlanes {
        Math::Vector4 planes[6]; // left, right, bottom, top, near, far
    } frustum;
    
    // Extract frustum planes from view-projection matrix
    // Left plane
    frustum.planes[0] = Math::Vector4(
        viewProj.m[3] + viewProj.m[0],
        viewProj.m[7] + viewProj.m[4],
        viewProj.m[11] + viewProj.m[8],
        viewProj.m[15] + viewProj.m[12]
    );
    
    // Right plane
    frustum.planes[1] = Math::Vector4(
        viewProj.m[3] - viewProj.m[0],
        viewProj.m[7] - viewProj.m[4],
        viewProj.m[11] - viewProj.m[8],
        viewProj.m[15] - viewProj.m[12]
    );
    
    // Bottom plane
    frustum.planes[2] = Math::Vector4(
        viewProj.m[3] + viewProj.m[1],
        viewProj.m[7] + viewProj.m[5],
        viewProj.m[11] + viewProj.m[9],
        viewProj.m[15] + viewProj.m[13]
    );
    
    // Top plane
    frustum.planes[3] = Math::Vector4(
        viewProj.m[3] - viewProj.m[1],
        viewProj.m[7] - viewProj.m[5],
        viewProj.m[11] - viewProj.m[9],
        viewProj.m[15] - viewProj.m[13]
    );
    
    // Near plane
    frustum.planes[4] = Math::Vector4(
        viewProj.m[3] + viewProj.m[2],
        viewProj.m[7] + viewProj.m[6],
        viewProj.m[11] + viewProj.m[10],
        viewProj.m[15] + viewProj.m[14]
    );
    
    // Far plane
    frustum.planes[5] = Math::Vector4(
        viewProj.m[3] - viewProj.m[2],
        viewProj.m[7] - viewProj.m[6],
        viewProj.m[11] - viewProj.m[10],
        viewProj.m[15] - viewProj.m[14]
    );

    // Normalize planes
    for (u32 i = 0; i < 6; ++i) {
        f32 length = Math::Sqrt(
            frustum.planes[i].x * frustum.planes[i].x +
            frustum.planes[i].y * frustum.planes[i].y +
            frustum.planes[i].z * frustum.planes[i].z
        );
        if (length > Math::EPSILON) {
            frustum.planes[i] = frustum.planes[i] / length;
        }
    }

    m_FrustumBuffer->UploadData(&frustum, sizeof(frustum));

    // Update descriptor set
    UpdateDescriptorSet(commandBuffer);

    // Reset atomic draw count to 0 BEFORE dispatch
    if (m_DrawCountBuffer) {
        vkCmdFillBuffer(commandBuffer, m_DrawCountBuffer->GetBuffer(), 0, sizeof(u32), 0);

        VkMemoryBarrier fillBarrier{};
        fillBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        fillBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        fillBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &fillBarrier, 0, nullptr, 0, nullptr);
    }

    // Bind compute pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_CullPipeline);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

    // Dispatch compute shader
    // Workgroup size is 64, so we need (objectCount + 63) / 64 workgroups
    u32 workgroupCount = (m_ObjectCount + 63) / 64;
    vkCmdDispatch(commandBuffer, workgroupCount, 1, 1);

    // Memory barrier - ensure compute shader completes before using indirect buffer
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    outIndirectDrawBuffer = m_IndirectDrawBuffer->GetBuffer();

    // GPU-driven: no CPU readback. Draw count is stored in m_DrawCountBuffer
    // for vkCmdDrawIndexedIndirectCount. Visibility buffer still written for
    // CPU-side fallback queries (IsVisible), read lazily from previous frame.
    m_CachedVisibility.resize(m_ObjectCount);
    void* mapped = m_VisibilityBuffer->Map();
    if (mapped) {
        const u32* visibility = static_cast<const u32*>(mapped);
        u32 visibleCount = 0;
        for (u32 i = 0; i < m_ObjectCount; ++i) {
            m_CachedVisibility[i] = visibility[i];
            if (visibility[i] != 0) visibleCount++;
        }
        m_VisibilityBuffer->Unmap();
        m_Stats.visibleObjects = visibleCount;
        m_Stats.culledObjects = m_ObjectCount - visibleCount;
    } else {
        std::fill(m_CachedVisibility.begin(), m_CachedVisibility.end(), 1u);
        m_Stats.visibleObjects = m_ObjectCount;
        m_Stats.culledObjects = 0;
    }

    outDrawCount = m_ObjectCount;
    m_Stats.totalObjects = m_ObjectCount;

    return true;
}

bool GPUCullingSystem::CreateBuffers() {
    // Object buffer (input)
    usize objectBufferSize = m_MaxObjects * sizeof(CullableObject);
    m_ObjectBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_ObjectBuffer->Create(objectBufferSize, BufferUsage::Storage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create object buffer");
        return false;
    }

    // Indirect draw buffer (output)
    usize indirectBufferSize = m_MaxObjects * sizeof(VkDrawIndexedIndirectCommand);
    m_IndirectDrawBuffer = std::make_unique<VulkanBuffer>(m_Context);
    // Need both storage (for compute shader write) and indirect draw (for vkCmdDrawIndexedIndirect)
    VkBufferUsageFlags indirectUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (!m_IndirectDrawBuffer->Create(indirectBufferSize, indirectUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create indirect draw buffer");
        return false;
    }

    // Frustum buffer
    struct FrustumPlanes {
        Math::Vector4 planes[6];
    };
    m_FrustumBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_FrustumBuffer->Create(sizeof(FrustumPlanes), BufferUsage::Uniform, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create frustum buffer");
        return false;
    }

    // Visibility buffer (per-object visibility flags)
    usize visibilityBufferSize = m_MaxObjects * sizeof(u32);
    m_VisibilityBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_VisibilityBuffer->Create(visibilityBufferSize, BufferUsage::Storage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create visibility buffer");
        return false;
    }

    // Draw count buffer (atomic counter, 4 bytes)
    m_DrawCountBuffer = std::make_unique<VulkanBuffer>(m_Context);
    VkBufferUsageFlags drawCountUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (!m_DrawCountBuffer->Create(sizeof(u32), drawCountUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create draw count buffer");
        return false;
    }

    // Object data SSBO (per-object material/transform for indirect rendering)
    // Matches ObjectDataGPU struct (192 bytes: model + prevModel + material fields + teleported flag)
    usize objectDataSize = m_MaxObjects * OBJECT_DATA_GPU_SIZE;
    m_ObjectDataBuffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!m_ObjectDataBuffer->Create(objectDataSize, BufferUsage::Storage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create object data buffer");
        return false;
    }

    // Occlusion flag buffer (per-object flags for two-phase culling)
    usize occlusionFlagSize = m_MaxObjects * sizeof(u32);
    m_OcclusionFlagBuffer = std::make_unique<VulkanBuffer>(m_Context);
    VkBufferUsageFlags occFlagUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (!m_OcclusionFlagBuffer->Create(occlusionFlagSize, occFlagUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create occlusion flag buffer");
        return false;
    }

    return true;
}

bool GPUCullingSystem::CreateComputePipeline() {
    // Create descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> bindings(5);

    // Object buffer (binding 0)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Indirect draw buffer (binding 1)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Frustum buffer (binding 2)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Visibility buffer (binding 3)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Draw count buffer (binding 4) — atomic counter
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(
        m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create descriptor set layout: %d", result);
        return false;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    result = vkCreatePipelineLayout(
        m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create pipeline layout: %d", result);
        return false;
    }

    // Create descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[0].descriptorCount = 4; // Object, indirect, visibility, draw count buffers
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1; // Frustum buffer

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    result = vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create descriptor pool: %d", result);
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;

    result = vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &m_DescriptorSet);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate descriptor set: %d", result);
        return false;
    }

    // Load compute shader - try multiple paths
    VulkanShader computeShader(m_Context);

    const char* shaderPaths[] = {
        "shaders/cull.comp.spv",
        "Engine/shaders/cull.comp.spv",
        "../Engine/shaders/cull.comp.spv",
        "../../Engine/shaders/cull.comp.spv",
        "../../../Engine/shaders/cull.comp.spv",
        "bin/shaders/cull.comp.spv"
    };
    
    bool shaderLoaded = false;
    for (const char* path : shaderPaths) {
        // logMissing=false: these are candidate probes, most will not exist —
        // the loop logs the real outcome (loaded / CPU fallback) below.
        if (computeShader.LoadFromFile(path, false)) {
            if (computeShader.GetModule() != VK_NULL_HANDLE) {
                shaderLoaded = true;
                ENJIN_LOG_INFO(Renderer, "Loaded compute shader from: %s", path);
                break;
            }
        }
    }

    if (!shaderLoaded) {
        ENJIN_LOG_WARN(Renderer, "Compute shader not found at any path, GPU culling will use CPU fallback");
        ENJIN_LOG_INFO(Renderer, "To enable GPU culling, compile: glslc Engine/shaders/cull.comp -o Engine/shaders/cull.comp.spv");
        // Continue without GPU pipeline - CPU fallback will be used
        return true; // Return true so system can still be used with CPU fallback
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_PipelineLayout;
    
    VkPipelineShaderStageCreateInfo shaderStage{};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.module = computeShader.GetModule();
    shaderStage.pName = "main";
    pipelineInfo.stage = shaderStage;

    result = vkCreateComputePipelines(
        m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_CullPipeline);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create compute pipeline: %d", result);
        ENJIN_LOG_WARN(Renderer, "GPU Culling will use CPU fallback");
        return true; // Allow CPU fallback
    }

    ENJIN_LOG_INFO(Renderer, "GPU Culling compute pipeline created successfully");

    return true;
}

void GPUCullingSystem::UpdateDescriptorSet(VkCommandBuffer commandBuffer) {
    (void)commandBuffer; // Not needed for descriptor set update

    std::vector<VkWriteDescriptorSet> writes(5);
    std::vector<VkDescriptorBufferInfo> bufferInfos(5);

    // Object buffer
    bufferInfos[0].buffer = m_ObjectBuffer->GetBuffer();
    bufferInfos[0].offset = 0;
    bufferInfos[0].range = VK_WHOLE_SIZE;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_DescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &bufferInfos[0];

    // Indirect draw buffer
    bufferInfos[1].buffer = m_IndirectDrawBuffer->GetBuffer();
    bufferInfos[1].offset = 0;
    bufferInfos[1].range = VK_WHOLE_SIZE;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_DescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &bufferInfos[1];

    // Frustum buffer
    bufferInfos[2].buffer = m_FrustumBuffer->GetBuffer();
    bufferInfos[2].offset = 0;
    bufferInfos[2].range = VK_WHOLE_SIZE;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_DescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].descriptorCount = 1;
    writes[2].pBufferInfo = &bufferInfos[2];

    // Visibility buffer
    bufferInfos[3].buffer = m_VisibilityBuffer->GetBuffer();
    bufferInfos[3].offset = 0;
    bufferInfos[3].range = VK_WHOLE_SIZE;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_DescriptorSet;
    writes[3].dstBinding = 3;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].descriptorCount = 1;
    writes[3].pBufferInfo = &bufferInfos[3];

    // Draw count buffer (atomic counter)
    bufferInfos[4].buffer = m_DrawCountBuffer->GetBuffer();
    bufferInfos[4].offset = 0;
    bufferInfos[4].range = VK_WHOLE_SIZE;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = m_DescriptorSet;
    writes[4].dstBinding = 4;
    writes[4].dstArrayElement = 0;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[4].descriptorCount = 1;
    writes[4].pBufferInfo = &bufferInfos[4];

    vkUpdateDescriptorSets(m_Context->GetDevice(),
        static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

void GPUCullingSystem::UpdateFrustumPlanes(const Math::Matrix4& viewProj) {
    // Frustum planes are updated in ExecuteCulling
    (void)viewProj;
}

bool GPUCullingSystem::CreateHiZComputePipeline() {
    if (m_HiZPipelineCreated) return m_CullHiZPipeline != VK_NULL_HANDLE;

    m_HiZPipelineCreated = true;

    // Descriptor set layout: 7 bindings (same as base + HiZ sampler + occlusion flags)
    std::vector<VkDescriptorSetLayoutBinding> bindings(7);

    // Bindings 0-4 same as base pipeline
    for (u32 i = 0; i < 5; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = (i == 2) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    // Binding 5: Hi-Z pyramid sampler
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 6: Occlusion flag buffer
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_HiZDescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ descriptor set layout");
        return false;
    }

    // Push constant for phase selection
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(u32);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_HiZDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_HiZPipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ pipeline layout");
        return false;
    }

    // Descriptor pool for HiZ set
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    VkDescriptorPool hizPool;
    if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &hizPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ descriptor pool");
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = hizPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_HiZDescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &m_HiZDescriptorSet) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate HiZ descriptor set");
        return false;
    }

    // Load cull_hiz.comp shader
    VulkanShader computeShader(m_Context);
    const char* shaderPaths[] = {
        "shaders/cull_hiz.comp.spv",
        "Engine/shaders/cull_hiz.comp.spv",
        "../Engine/shaders/cull_hiz.comp.spv",
        "../../Engine/shaders/cull_hiz.comp.spv",
        "bin/shaders/cull_hiz.comp.spv"
    };

    bool loaded = false;
    for (const char* path : shaderPaths) {
        if (computeShader.LoadFromFile(path, false) && computeShader.GetModule() != VK_NULL_HANDLE) {
            loaded = true;
            ENJIN_LOG_INFO(Renderer, "Loaded HiZ cull shader from: %s", path);
            break;
        }
    }

    if (!loaded) {
        ENJIN_LOG_WARN(Renderer, "HiZ cull shader not found, two-phase culling unavailable");
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_HiZPipelineLayout;

    VkPipelineShaderStageCreateInfo shaderStage{};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.module = computeShader.GetModule();
    shaderStage.pName = "main";
    pipelineInfo.stage = shaderStage;

    if (vkCreateComputePipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_CullHiZPipeline) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ compute pipeline");
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Two-phase HiZ culling pipeline created");
    return true;
}

void GPUCullingSystem::UpdateDescriptorSetTwoPhase(VkCommandBuffer commandBuffer) {
    (void)commandBuffer;
    if (!m_HiZPyramid || m_HiZDescriptorSet == VK_NULL_HANDLE) return;

    std::vector<VkWriteDescriptorSet> writes(7);
    std::vector<VkDescriptorBufferInfo> bufferInfos(6);

    // Buffers 0-4: same as base
    VkBuffer buffers[] = {
        m_ObjectBuffer->GetBuffer(),
        m_IndirectDrawBuffer->GetBuffer(),
        m_FrustumBuffer->GetBuffer(),
        m_VisibilityBuffer->GetBuffer(),
        m_DrawCountBuffer->GetBuffer()
    };
    VkDescriptorType types[] = {
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    };
    for (u32 i = 0; i < 5; ++i) {
        bufferInfos[i].buffer = buffers[i];
        bufferInfos[i].offset = 0;
        bufferInfos[i].range = VK_WHOLE_SIZE;

        writes[i] = {};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_HiZDescriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorType = types[i];
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &bufferInfos[i];
    }

    // Binding 5: Hi-Z pyramid
    VkDescriptorImageInfo hizImageInfo{};
    hizImageInfo.sampler = m_HiZPyramid->GetSampler();
    hizImageInfo.imageView = m_HiZPyramid->GetView();
    hizImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[5] = {};
    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = m_HiZDescriptorSet;
    writes[5].dstBinding = 5;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].descriptorCount = 1;
    writes[5].pImageInfo = &hizImageInfo;

    // Binding 6: Occlusion flag buffer
    bufferInfos[5].buffer = m_OcclusionFlagBuffer->GetBuffer();
    bufferInfos[5].offset = 0;
    bufferInfos[5].range = VK_WHOLE_SIZE;

    writes[6] = {};
    writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[6].dstSet = m_HiZDescriptorSet;
    writes[6].dstBinding = 6;
    writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[6].descriptorCount = 1;
    writes[6].pBufferInfo = &bufferInfos[5];

    vkUpdateDescriptorSets(m_Context->GetDevice(), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
}

bool GPUCullingSystem::ExecuteTwoPhase(
    const Math::Matrix4& viewMatrix,
    const Math::Matrix4& projectionMatrix,
    VkCommandBuffer commandBuffer,
    VkBuffer& outIndirectDrawBuffer,
    u32& outDrawCount
) {
    if (!m_HiZPyramid || commandBuffer == VK_NULL_HANDLE || m_ObjectCount == 0) {
        // Fall back to single-phase
        return ExecuteCulling(viewMatrix, projectionMatrix, commandBuffer, outIndirectDrawBuffer, outDrawCount);
    }

    // Lazily create the HiZ pipeline on first use
    if (!m_HiZPipelineCreated) {
        if (!CreateHiZComputePipeline()) {
            return ExecuteCulling(viewMatrix, projectionMatrix, commandBuffer, outIndirectDrawBuffer, outDrawCount);
        }
    }

    if (m_CullHiZPipeline == VK_NULL_HANDLE) {
        return ExecuteCulling(viewMatrix, projectionMatrix, commandBuffer, outIndirectDrawBuffer, outDrawCount);
    }

    // Upload frustum planes (same as single-phase)
    Math::Matrix4 viewProj = projectionMatrix * viewMatrix;

    struct FrustumData {
        Math::Vector4 planes[6];
        Math::Matrix4 viewProj;
        f32 screenWidth;
        f32 screenHeight;
        f32 nearPlane;
        f32 _pad;
    } frustumData;

    // Extract and normalize frustum planes
    auto extractPlane = [&](int idx, float s0, float s1, float s2, float s3) {
        frustumData.planes[idx] = Math::Vector4(s0, s1, s2, s3);
        f32 len = Math::Sqrt(s0*s0 + s1*s1 + s2*s2);
        if (len > Math::EPSILON) {
            frustumData.planes[idx] = frustumData.planes[idx] / len;
        }
    };
    extractPlane(0, viewProj.m[3]+viewProj.m[0], viewProj.m[7]+viewProj.m[4], viewProj.m[11]+viewProj.m[8], viewProj.m[15]+viewProj.m[12]);
    extractPlane(1, viewProj.m[3]-viewProj.m[0], viewProj.m[7]-viewProj.m[4], viewProj.m[11]-viewProj.m[8], viewProj.m[15]-viewProj.m[12]);
    extractPlane(2, viewProj.m[3]+viewProj.m[1], viewProj.m[7]+viewProj.m[5], viewProj.m[11]+viewProj.m[9], viewProj.m[15]+viewProj.m[13]);
    extractPlane(3, viewProj.m[3]-viewProj.m[1], viewProj.m[7]-viewProj.m[5], viewProj.m[11]-viewProj.m[9], viewProj.m[15]-viewProj.m[13]);
    extractPlane(4, viewProj.m[3]+viewProj.m[2], viewProj.m[7]+viewProj.m[6], viewProj.m[11]+viewProj.m[10], viewProj.m[15]+viewProj.m[14]);
    extractPlane(5, viewProj.m[3]-viewProj.m[2], viewProj.m[7]-viewProj.m[6], viewProj.m[11]-viewProj.m[10], viewProj.m[15]-viewProj.m[14]);

    frustumData.viewProj = viewProj;
    frustumData.screenWidth = static_cast<f32>(m_HiZPyramid->GetWidth());
    frustumData.screenHeight = static_cast<f32>(m_HiZPyramid->GetHeight());
    frustumData.nearPlane = 0.01f;
    frustumData._pad = 0.0f;

    m_FrustumBuffer->UploadData(&frustumData, sizeof(frustumData));

    // Update descriptors
    UpdateDescriptorSetTwoPhase(commandBuffer);

    u32 workgroupCount = (m_ObjectCount + 63) / 64;

    // --- Phase 0: Frustum + previous-frame HiZ cull ---

    // Clear draw count and occlusion flags
    vkCmdFillBuffer(commandBuffer, m_DrawCountBuffer->GetBuffer(), 0, sizeof(u32), 0);
    vkCmdFillBuffer(commandBuffer, m_OcclusionFlagBuffer->GetBuffer(), 0,
                    m_ObjectCount * sizeof(u32), 0);

    VkMemoryBarrier fillBarrier{};
    fillBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    fillBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    fillBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &fillBarrier, 0, nullptr, 0, nullptr);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_CullHiZPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_HiZPipelineLayout, 0, 1, &m_HiZDescriptorSet, 0, nullptr);

    // Phase 0 push constant
    u32 phase = 0;
    vkCmdPushConstants(commandBuffer, m_HiZPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &phase);
    vkCmdDispatch(commandBuffer, workgroupCount, 1, 1);

    // Barrier: phase 0 compute → HiZ regeneration (read visibility + indirect draw)
    VkMemoryBarrier phase0Barrier{};
    phase0Barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    phase0Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    phase0Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0, 1, &phase0Barrier, 0, nullptr, 0, nullptr);

    // Between phases: the caller should regenerate the HiZ pyramid from the depth buffer
    // produced by rendering phase 0 visible objects. This is handled by RenderSystem.
    // For now, we proceed to phase 1 using the same HiZ (still beneficial as phase 0
    // removes the majority of occluded objects, and phase 1 recovers false negatives).

    // --- Phase 1: Re-test occluded objects against (potentially updated) HiZ ---
    phase = 1;
    vkCmdPushConstants(commandBuffer, m_HiZPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &phase);
    vkCmdDispatch(commandBuffer, workgroupCount, 1, 1);

    // Final barrier: phase 1 compute → draw
    VkMemoryBarrier finalBarrier{};
    finalBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 1, &finalBarrier, 0, nullptr, 0, nullptr);

    outIndirectDrawBuffer = m_IndirectDrawBuffer->GetBuffer();

    // Read back visibility (lazy, from previous frame — same pattern as single-phase)
    m_CachedVisibility.resize(m_ObjectCount);
    void* mapped = m_VisibilityBuffer->Map();
    if (mapped) {
        const u32* vis = static_cast<const u32*>(mapped);
        u32 visCount = 0;
        for (u32 i = 0; i < m_ObjectCount; ++i) {
            m_CachedVisibility[i] = vis[i];
            if (vis[i] != 0) visCount++;
        }
        m_VisibilityBuffer->Unmap();
        m_Stats.visibleObjects = visCount;
        m_Stats.culledObjects = m_ObjectCount - visCount;
    } else {
        std::fill(m_CachedVisibility.begin(), m_CachedVisibility.end(), 1u);
        m_Stats.visibleObjects = m_ObjectCount;
        m_Stats.culledObjects = 0;
    }

    outDrawCount = m_ObjectCount;
    m_Stats.totalObjects = m_ObjectCount;
    return true;
}

} // namespace Renderer
} // namespace Enjin
