#include "Enjin/Renderer/RayTracing/RTPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <algorithm>

namespace Enjin {
namespace Renderer {

// RT pipeline function pointers
static PFN_vkCreateRayTracingPipelinesKHR s_vkCreateRTPipelines = nullptr;
static PFN_vkGetRayTracingShaderGroupHandlesKHR s_vkGetRTShaderGroupHandles = nullptr;
static PFN_vkGetBufferDeviceAddressKHR s_vkGetBufferDeviceAddr = nullptr;

static void LoadPipelineFunctions(VkDevice device) {
    if (s_vkCreateRTPipelines) return;
    s_vkCreateRTPipelines = (PFN_vkCreateRayTracingPipelinesKHR)
        vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    s_vkGetRTShaderGroupHandles = (PFN_vkGetRayTracingShaderGroupHandlesKHR)
        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    s_vkGetBufferDeviceAddr = (PFN_vkGetBufferDeviceAddressKHR)
        vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
}

RTPipeline::RTPipeline(VulkanContext* context) : m_Context(context) {
    LoadPipelineFunctions(context->GetDevice());
}

RTPipeline::~RTPipeline() {
    Destroy();
}

bool RTPipeline::Create(const std::vector<ShaderStage>& stages,
                        const std::vector<ShaderGroup>& groups,
                        VkDescriptorSetLayout descriptorSetLayout,
                        VkPipelineLayout pipelineLayout,
                        u32 maxRecursionDepth) {
    if (!s_vkCreateRTPipelines) {
        ENJIN_LOG_ERROR(Renderer, "vkCreateRayTracingPipelinesKHR not available");
        return false;
    }

    m_PipelineLayout = pipelineLayout;

    // Create shader modules
    m_ShaderModules.resize(stages.size());
    std::vector<VkPipelineShaderStageCreateInfo> shaderStageInfos(stages.size());

    for (size_t i = 0; i < stages.size(); ++i) {
        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = stages[i].spirvSize;
        moduleInfo.pCode = stages[i].spirvData;

        if (vkCreateShaderModule(m_Context->GetDevice(), &moduleInfo, nullptr, &m_ShaderModules[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT shader module %zu", i);
            return false;
        }

        shaderStageInfos[i] = {};
        shaderStageInfos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfos[i].stage = stages[i].stage;
        shaderStageInfos[i].module = m_ShaderModules[i];
        shaderStageInfos[i].pName = stages[i].entryPoint;
    }

    // Shader groups
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groupInfos(groups.size());
    for (size_t i = 0; i < groups.size(); ++i) {
        groupInfos[i] = {};
        groupInfos[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        groupInfos[i].type = groups[i].type;
        groupInfos[i].generalShader = groups[i].generalShader;
        groupInfos[i].closestHitShader = groups[i].closestHitShader;
        groupInfos[i].anyHitShader = groups[i].anyHitShader;
        groupInfos[i].intersectionShader = groups[i].intersectionShader;
    }

    // Create pipeline
    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = static_cast<u32>(shaderStageInfos.size());
    pipelineInfo.pStages = shaderStageInfos.data();
    pipelineInfo.groupCount = static_cast<u32>(groupInfos.size());
    pipelineInfo.pGroups = groupInfos.data();
    pipelineInfo.maxPipelineRayRecursionDepth = maxRecursionDepth;
    pipelineInfo.layout = pipelineLayout;

    VkResult result = s_vkCreateRTPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE,
                                             1, &pipelineInfo, nullptr, &m_Pipeline);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT pipeline: %d", result);
        return false;
    }

    // Destroy shader modules — no longer needed after pipeline creation
    for (auto& mod : m_ShaderModules) {
        vkDestroyShaderModule(m_Context->GetDevice(), mod, nullptr);
    }
    m_ShaderModules.clear();

    // Build SBT
    if (!BuildSBT(groups)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to build SBT");
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "RT pipeline created: %zu stages, %zu groups, maxRecursion=%u",
        stages.size(), groups.size(), maxRecursionDepth);
    return true;
}

bool RTPipeline::BuildSBT(const std::vector<ShaderGroup>& groups) {
    if (!s_vkGetRTShaderGroupHandles || !s_vkGetBufferDeviceAddr) return false;

    // Query RT pipeline properties for handle size/alignment
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(m_Context->GetPhysicalDevice(), &props2);

    u32 handleSize = rtProps.shaderGroupHandleSize;
    u32 handleAlignment = rtProps.shaderGroupHandleAlignment;
    u32 baseAlignment = rtProps.shaderGroupBaseAlignment;

    // Aligned handle size (stride within each group section)
    u32 handleStride = (handleSize + (handleAlignment - 1)) & ~(handleAlignment - 1);

    // Count groups by type
    u32 raygenCount = 0, missCount = 0, hitCount = 0, callableCount = 0;
    for (const auto& g : groups) {
        switch (g.type) {
            case VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR:
                // General can be raygen, miss, or callable — need to check stage
                if (raygenCount == 0) raygenCount++;  // First general is raygen
                else missCount++;
                break;
            case VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR:
            case VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR:
                hitCount++;
                break;
            default: break;
        }
    }

    // Compute aligned region sizes
    auto alignUp = [](u32 size, u32 align) -> u32 {
        return (size + (align - 1)) & ~(align - 1);
    };

    u32 raygenSize = alignUp(handleStride * 1, baseAlignment);  // One raygen
    u32 missSize = alignUp(handleStride * (missCount > 0 ? missCount : 1), baseAlignment);
    u32 hitSize = alignUp(handleStride * (hitCount > 0 ? hitCount : 1), baseAlignment);
    u32 callableSize = 0;  // Not used for now

    u32 totalSize = raygenSize + missSize + hitSize + callableSize;

    // Get all shader group handles
    u32 groupCount = static_cast<u32>(groups.size());
    std::vector<u8> handleData(static_cast<size_t>(handleSize) * groupCount);
    if (s_vkGetRTShaderGroupHandles(m_Context->GetDevice(), m_Pipeline, 0, groupCount,
                                     handleData.size(), handleData.data()) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to get RT shader group handles");
        return false;
    }

    // Create SBT buffer (host-visible for simplicity)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = totalSize;
    bufInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_Context->GetDevice(), &bufInfo, nullptr, &m_SBTBuffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_Context->GetDevice(), m_SBTBuffer, &memReqs);

    VkMemoryAllocateFlagsInfo allocFlags{};
    allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &allocFlags;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_SBTMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindBufferMemory(m_Context->GetDevice(), m_SBTBuffer, m_SBTMemory, 0);
    m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

    // Write handles into SBT buffer
    void* mapped = nullptr;
    vkMapMemory(m_Context->GetDevice(), m_SBTMemory, 0, totalSize, 0, &mapped);
    std::memset(mapped, 0, totalSize);

    u8* sbtData = static_cast<u8*>(mapped);
    u32 handleIdx = 0;

    // Raygen (group 0)
    std::memcpy(sbtData, handleData.data() + handleIdx * handleSize, handleSize);
    handleIdx++;

    // Miss (groups 1..missCount)
    for (u32 i = 0; i < missCount; ++i) {
        std::memcpy(sbtData + raygenSize + i * handleStride,
                    handleData.data() + handleIdx * handleSize, handleSize);
        handleIdx++;
    }

    // Hit (remaining groups)
    for (u32 i = 0; i < hitCount; ++i) {
        std::memcpy(sbtData + raygenSize + missSize + i * handleStride,
                    handleData.data() + handleIdx * handleSize, handleSize);
        handleIdx++;
    }

    vkUnmapMemory(m_Context->GetDevice(), m_SBTMemory);

    // Get buffer device address
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = m_SBTBuffer;
    VkDeviceAddress sbtAddress = s_vkGetBufferDeviceAddr(m_Context->GetDevice(), &addrInfo);

    // Fill SBT regions
    // Vulkan spec requires raygen.stride == raygen.size (VUID-vkCmdTraceRaysKHR-size-04023)
    m_SBTRegions.raygen.deviceAddress = sbtAddress;
    m_SBTRegions.raygen.stride = raygenSize;
    m_SBTRegions.raygen.size = raygenSize;

    m_SBTRegions.miss.deviceAddress = sbtAddress + raygenSize;
    m_SBTRegions.miss.stride = handleStride;
    m_SBTRegions.miss.size = missSize;

    m_SBTRegions.hit.deviceAddress = sbtAddress + raygenSize + missSize;
    m_SBTRegions.hit.stride = handleStride;
    m_SBTRegions.hit.size = hitSize;

    m_SBTRegions.callable = {};  // Not used

    return true;
}

void RTPipeline::Destroy() {
    // Destroy shader modules if still alive
    for (auto& mod : m_ShaderModules) {
        if (mod != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_Context->GetDevice(), mod, nullptr);
        }
    }
    m_ShaderModules.clear();

    if (m_SBTBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_Context->GetDevice(), m_SBTBuffer, nullptr);
        m_SBTBuffer = VK_NULL_HANDLE;
    }
    if (m_SBTMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_Context->GetDevice(), m_SBTMemory, nullptr);
        m_SBTMemory = VK_NULL_HANDLE;
    }

    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Context->GetDevice(), m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }

    m_SBTRegions = {};
}

} // namespace Renderer
} // namespace Enjin
