#include "Enjin/Renderer/RayTracing/SurfelRadianceCache.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

// Push constants for surfel placement compute shader
struct SurfelPlacementPushConstants {
    f32 cameraPosX;
    f32 cameraPosY;
    f32 cameraPosZ;
    f32 cameraPosW;         // Padding (vec4 alignment)
    f32 cameraRadius;
    f32 surfelRadius;
    f32 maxAge;
    u32 maxSurfels;
    u32 frameCount;
    f32 normalThreshold;
    u32 screenWidth;
    u32 screenHeight;
};

// Push constants for surfel update compute shader
struct SurfelUpdatePushConstants {
    f32 cameraPosX;
    f32 cameraPosY;
    f32 cameraPosZ;
    f32 cameraPosW;         // Padding (vec4 alignment)
    f32 cameraRadius;
    f32 maxAge;
    f32 updateFraction;
    u32 maxSurfels;
    u32 frameCount;
    u32 raysPerSurfel;
    f32 hysteresis;
    u32 padding0;
};

// Push constants for surfel lookup compute shader
struct SurfelLookupPushConstants {
    f32 blendWeight;
    f32 surfelRadius;
    f32 normalThreshold;
    u32 maxSurfels;
    u32 maxSearchCount;
    u32 screenWidth;
    u32 screenHeight;
    u32 padding0;
};

SurfelRadianceCache::SurfelRadianceCache(VulkanContext* context) : m_Context(context) {}

SurfelRadianceCache::~SurfelRadianceCache() { Shutdown(); }

bool SurfelRadianceCache::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;

    CreateResources();
    if (m_SurfelBuffer == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create surfel buffer");
        return false;
    }

    CreateOutputImage();
    if (m_OutputImage == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create output image");
        return false;
    }

    VkDevice device = m_Context->GetDevice();

    // --- Placement pipeline ---
    {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(SurfelPlacementPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &rtDescLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PlacementPipelineLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create placement pipeline layout");
            return false;
        }

        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = sizeof(SURFEL_PLACEMENT_COMP_SPV);
        shaderInfo.pCode = SURFEL_PLACEMENT_COMP_SPV;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create placement shader module");
            return false;
        }

        VkComputePipelineCreateInfo pipeInfo{};
        pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeInfo.stage.module = shaderModule;
        pipeInfo.stage.pName = "main";
        pipeInfo.layout = m_PlacementPipelineLayout;

        VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_PlacementPipeline);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create placement compute pipeline");
            return false;
        }
    }

    // --- Update pipeline ---
    {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(SurfelUpdatePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &rtDescLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_UpdatePipelineLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create update pipeline layout");
            return false;
        }

        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = sizeof(SURFEL_UPDATE_COMP_SPV);
        shaderInfo.pCode = SURFEL_UPDATE_COMP_SPV;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create update shader module");
            return false;
        }

        VkComputePipelineCreateInfo pipeInfo{};
        pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeInfo.stage.module = shaderModule;
        pipeInfo.stage.pName = "main";
        pipeInfo.layout = m_UpdatePipelineLayout;

        VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_UpdatePipeline);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create update compute pipeline");
            return false;
        }
    }

    // --- Lookup pipeline ---
    {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(SurfelLookupPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &rtDescLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_LookupPipelineLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create lookup pipeline layout");
            return false;
        }

        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = sizeof(SURFEL_LOOKUP_COMP_SPV);
        shaderInfo.pCode = SURFEL_LOOKUP_COMP_SPV;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create lookup shader module");
            return false;
        }

        VkComputePipelineCreateInfo pipeInfo{};
        pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeInfo.stage.module = shaderModule;
        pipeInfo.stage.pName = "main";
        pipeInfo.layout = m_LookupPipelineLayout;

        VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_LookupPipeline);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create lookup compute pipeline");
            return false;
        }
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Surfel radiance cache initialized (%u max surfels, %.1f radius, %.1fm camera range)",
                   m_Config.maxSurfels, m_Config.surfelRadius, m_Config.cameraRadius);
    return true;
}

void SurfelRadianceCache::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    // Surfel buffer does not resize (world-space, fixed budget).
    // Only the output image needs to match screen resolution.
    DestroyOutputImage();
    CreateOutputImage();
}

void SurfelRadianceCache::DispatchPlacement(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                                             const Math::Vector3& cameraPos, u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled) return;

    // Only run placement every placementInterval frames
    if (frameCount % m_Config.placementInterval != 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PlacementPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_PlacementPipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    SurfelPlacementPushConstants pc{};
    pc.cameraPosX = cameraPos.x;
    pc.cameraPosY = cameraPos.y;
    pc.cameraPosZ = cameraPos.z;
    pc.cameraPosW = 0.0f;
    pc.cameraRadius = m_Config.cameraRadius;
    pc.surfelRadius = m_Config.surfelRadius;
    pc.maxAge = m_Config.maxAge;
    pc.maxSurfels = m_Config.maxSurfels;
    pc.frameCount = frameCount;
    pc.normalThreshold = m_Config.normalThreshold;
    pc.screenWidth = m_Width;
    pc.screenHeight = m_Height;

    vkCmdPushConstants(cmd, m_PlacementPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    // Dispatch one thread per 16x16 pixel tile
    u32 tilesX = (m_Width + 127) / 128;   // 8 threads * 16 pixels = 128
    u32 tilesY = (m_Height + 127) / 128;
    vkCmdDispatch(cmd, tilesX, tilesY, 1);

    // Barrier: surfel buffer must be readable by update pass
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void SurfelRadianceCache::DispatchUpdate(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                                          const Math::Vector3& cameraPos, u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_UpdatePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_UpdatePipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    SurfelUpdatePushConstants pc{};
    pc.cameraPosX = cameraPos.x;
    pc.cameraPosY = cameraPos.y;
    pc.cameraPosZ = cameraPos.z;
    pc.cameraPosW = 0.0f;
    pc.cameraRadius = m_Config.cameraRadius;
    pc.maxAge = m_Config.maxAge;
    pc.updateFraction = m_Config.updateFraction;
    pc.maxSurfels = m_Config.maxSurfels;
    pc.frameCount = frameCount;
    pc.raysPerSurfel = m_Config.raysPerSurfel;
    pc.hysteresis = 0.9f;  // Exponential moving average factor
    pc.padding0 = 0;

    vkCmdPushConstants(cmd, m_UpdatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    // Dispatch enough threads to cover the fraction of surfels being updated.
    // Each thread updates one surfel; updateFraction determines which subset.
    u32 surfelsToUpdate = static_cast<u32>(static_cast<f32>(m_Config.maxSurfels) * m_Config.updateFraction);
    u32 groups = (surfelsToUpdate + 63) / 64;  // 64 threads per workgroup
    vkCmdDispatch(cmd, groups, 1, 1);

    // Barrier: surfel buffer must be readable by lookup pass
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void SurfelRadianceCache::DispatchLookup(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                                          u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_LookupPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_LookupPipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    SurfelLookupPushConstants pc{};
    pc.blendWeight = m_Config.blendWeight;
    pc.surfelRadius = m_Config.surfelRadius;
    pc.normalThreshold = m_Config.normalThreshold;
    pc.maxSurfels = m_Config.maxSurfels;
    pc.maxSearchCount = 512;  // Cap per-pixel search cost
    pc.screenWidth = m_Width;
    pc.screenHeight = m_Height;
    pc.padding0 = 0;

    vkCmdPushConstants(cmd, m_LookupPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    // Dispatch one thread per pixel (8x8 workgroups)
    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Barrier: output image must be readable by compositor
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void SurfelRadianceCache::InvalidateAll() {
    m_InvalidateRequested = true;
    m_ActiveSurfelCount = 0;
}

void SurfelRadianceCache::CreateResources() {
    VkDevice device = m_Context->GetDevice();

    // --- Surfel buffer ---
    {
        VkDeviceSize bufferSize = static_cast<VkDeviceSize>(m_Config.maxSurfels) * sizeof(Surfel);
        if (bufferSize == 0) return;

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufInfo, nullptr, &m_SurfelBuffer) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create surfel buffer");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_SurfelBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_SurfelMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device, m_SurfelBuffer, nullptr);
            m_SurfelBuffer = VK_NULL_HANDLE;
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to allocate surfel buffer memory");
            return;
        }

        vkBindBufferMemory(device, m_SurfelBuffer, m_SurfelMemory, 0);
        m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

        ENJIN_LOG_INFO(Renderer, "SurfelRadianceCache: Surfel buffer created (%llu bytes, %u max surfels)",
                       static_cast<unsigned long long>(bufferSize), m_Config.maxSurfels);
    }

    // --- Counter/metadata buffer (3 u32s) ---
    {
        VkDeviceSize bufferSize = sizeof(u32) * 4;  // 4 u32s (aligned to 16 bytes)

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufInfo, nullptr, &m_SurfelCounterBuffer) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create counter buffer");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_SurfelCounterBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_SurfelCounterMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device, m_SurfelCounterBuffer, nullptr);
            m_SurfelCounterBuffer = VK_NULL_HANDLE;
            ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to allocate counter buffer memory");
            return;
        }

        vkBindBufferMemory(device, m_SurfelCounterBuffer, m_SurfelCounterMemory, 0);
        m_Context->TrackAllocation(static_cast<usize>(memReqs.size));
    }

    m_InvalidateRequested = true;
}

void SurfelRadianceCache::DestroyResources() {
    VkDevice device = m_Context->GetDevice();
    if (m_SurfelBuffer) { vkDestroyBuffer(device, m_SurfelBuffer, nullptr); m_SurfelBuffer = VK_NULL_HANDLE; }
    if (m_SurfelMemory) { vkFreeMemory(device, m_SurfelMemory, nullptr); m_SurfelMemory = VK_NULL_HANDLE; }
    if (m_SurfelCounterBuffer) { vkDestroyBuffer(device, m_SurfelCounterBuffer, nullptr); m_SurfelCounterBuffer = VK_NULL_HANDLE; }
    if (m_SurfelCounterMemory) { vkFreeMemory(device, m_SurfelCounterMemory, nullptr); m_SurfelCounterMemory = VK_NULL_HANDLE; }
}

void SurfelRadianceCache::CreateOutputImage() {
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imgInfo.extent = { m_Width, m_Height, 1 };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(m_Context->GetDevice(), &imgInfo, nullptr, &m_OutputImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create output image");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_Context->GetDevice(), m_OutputImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_OutputMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to allocate output image memory");
        return;
    }
    vkBindImageMemory(m_Context->GetDevice(), m_OutputImage, m_OutputMemory, 0);
    m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_OutputImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_OutputView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "SurfelRadianceCache: Failed to create output image view");
        return;
    }
}

void SurfelRadianceCache::DestroyOutputImage() {
    VkDevice device = m_Context->GetDevice();
    if (m_OutputView) { vkDestroyImageView(device, m_OutputView, nullptr); m_OutputView = VK_NULL_HANDLE; }
    if (m_OutputImage) { vkDestroyImage(device, m_OutputImage, nullptr); m_OutputImage = VK_NULL_HANDLE; }
    if (m_OutputMemory) { vkFreeMemory(device, m_OutputMemory, nullptr); m_OutputMemory = VK_NULL_HANDLE; }
}

void SurfelRadianceCache::Shutdown() {
    if (!m_Initialized) return;

    VkDevice device = m_Context->GetDevice();

    if (m_PlacementPipeline) { vkDestroyPipeline(device, m_PlacementPipeline, nullptr); m_PlacementPipeline = VK_NULL_HANDLE; }
    if (m_PlacementPipelineLayout) { vkDestroyPipelineLayout(device, m_PlacementPipelineLayout, nullptr); m_PlacementPipelineLayout = VK_NULL_HANDLE; }
    if (m_UpdatePipeline) { vkDestroyPipeline(device, m_UpdatePipeline, nullptr); m_UpdatePipeline = VK_NULL_HANDLE; }
    if (m_UpdatePipelineLayout) { vkDestroyPipelineLayout(device, m_UpdatePipelineLayout, nullptr); m_UpdatePipelineLayout = VK_NULL_HANDLE; }
    if (m_LookupPipeline) { vkDestroyPipeline(device, m_LookupPipeline, nullptr); m_LookupPipeline = VK_NULL_HANDLE; }
    if (m_LookupPipelineLayout) { vkDestroyPipelineLayout(device, m_LookupPipelineLayout, nullptr); m_LookupPipelineLayout = VK_NULL_HANDLE; }

    DestroyResources();
    DestroyOutputImage();

    m_Initialized = false;
    ENJIN_LOG_INFO(Renderer, "Surfel radiance cache shut down");
}

} // namespace Renderer
} // namespace Enjin
