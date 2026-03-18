#include "Enjin/Renderer/RayTracing/RadianceCache.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

// Push constants for the cache update compute shader
struct RadianceCacheUpdatePushConstants {
    u32 tileSize;
    u32 tileCountX;
    u32 tileCountY;
    f32 maxAge;
    f32 depthThreshold;
    f32 normalThreshold;
    f32 hysteresis;
    u32 invalidateAll;
};

// Push constants for the cache read compute shader
struct RadianceCacheReadPushConstants {
    u32 tileSize;
    u32 tileCountX;
    u32 tileCountY;
    f32 depthThreshold;
    f32 normalThreshold;
};

RadianceCache::RadianceCache(VulkanContext* context) : m_Context(context) {}

RadianceCache::~RadianceCache() { Shutdown(); }

bool RadianceCache::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;
    m_TileCountX = (width + m_Config.tileSize - 1) / m_Config.tileSize;
    m_TileCountY = (height + m_Config.tileSize - 1) / m_Config.tileSize;

    CreateResources();
    if (m_TileBuffer == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create tile buffer");
        return false;
    }

    CreateOutputImage();
    if (m_OutputImage == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create output image");
        return false;
    }

    VkDevice device = m_Context->GetDevice();

    // --- Update pipeline ---
    {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(RadianceCacheUpdatePushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &rtDescLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_UpdatePipelineLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create update pipeline layout");
            return false;
        }

        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = sizeof(RADIANCE_CACHE_UPDATE_COMP_SPV);
        shaderInfo.pCode = RADIANCE_CACHE_UPDATE_COMP_SPV;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create update shader module");
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
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create update compute pipeline");
            return false;
        }
    }

    // --- Read pipeline ---
    {
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(RadianceCacheReadPushConstants);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &rtDescLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_ReadPipelineLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create read pipeline layout");
            return false;
        }

        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = sizeof(RADIANCE_CACHE_READ_COMP_SPV);
        shaderInfo.pCode = RADIANCE_CACHE_READ_COMP_SPV;

        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create read shader module");
            return false;
        }

        VkComputePipelineCreateInfo pipeInfo{};
        pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeInfo.stage.module = shaderModule;
        pipeInfo.stage.pName = "main";
        pipeInfo.layout = m_ReadPipelineLayout;

        VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_ReadPipeline);
        vkDestroyShaderModule(device, shaderModule, nullptr);

        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create read compute pipeline");
            return false;
        }
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Radiance cache initialized (%ux%u, %ux%u tiles of %u px)",
                   width, height, m_TileCountX, m_TileCountY, m_Config.tileSize);
    return true;
}

void RadianceCache::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    m_TileCountX = (width + m_Config.tileSize - 1) / m_Config.tileSize;
    m_TileCountY = (height + m_Config.tileSize - 1) / m_Config.tileSize;
    DestroyResources();
    CreateResources();
    DestroyOutputImage();
    CreateOutputImage();
    m_InvalidateRequested = true;
}

void RadianceCache::DispatchUpdate(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                                     u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_UpdatePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_UpdatePipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    RadianceCacheUpdatePushConstants pc{};
    pc.tileSize = m_Config.tileSize;
    pc.tileCountX = m_TileCountX;
    pc.tileCountY = m_TileCountY;
    pc.maxAge = m_Config.maxAge;
    pc.depthThreshold = m_Config.depthThreshold;
    pc.normalThreshold = m_Config.normalThreshold;
    pc.hysteresis = m_Config.hysteresis;
    pc.invalidateAll = m_InvalidateRequested ? 1u : 0u;

    vkCmdPushConstants(cmd, m_UpdatePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                        0, sizeof(pc), &pc);

    // Dispatch one thread per tile (8x8 workgroups)
    u32 groupsX = (m_TileCountX + 7) / 8;
    u32 groupsY = (m_TileCountY + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    m_InvalidateRequested = false;

    // Barrier: tile buffer + stale mask must be readable by read pass and GI
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void RadianceCache::DispatchRead(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                                   u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_ReadPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                             m_ReadPipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    RadianceCacheReadPushConstants pc{};
    pc.tileSize = m_Config.tileSize;
    pc.tileCountX = m_TileCountX;
    pc.tileCountY = m_TileCountY;
    pc.depthThreshold = m_Config.depthThreshold;
    pc.normalThreshold = m_Config.normalThreshold;

    vkCmdPushConstants(cmd, m_ReadPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
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

void RadianceCache::InvalidateAll() {
    m_InvalidateRequested = true;
}

void RadianceCache::CreateResources() {
    VkDevice device = m_Context->GetDevice();
    u32 totalTiles = m_TileCountX * m_TileCountY;

    // --- Tile buffer ---
    {
        VkDeviceSize bufferSize = static_cast<VkDeviceSize>(totalTiles) * sizeof(RadianceCacheTile);
        if (bufferSize == 0) return;

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufInfo, nullptr, &m_TileBuffer) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create tile buffer");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_TileBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_TileMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device, m_TileBuffer, nullptr);
            m_TileBuffer = VK_NULL_HANDLE;
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to allocate tile buffer memory");
            return;
        }

        vkBindBufferMemory(device, m_TileBuffer, m_TileMemory, 0);
        m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

        ENJIN_LOG_INFO(Renderer, "RadianceCache: Tile buffer created (%u bytes, %u tiles)",
                       static_cast<u32>(bufferSize), totalTiles);
    }

    // --- Stale mask buffer (1 bit per tile, packed into u32s) ---
    {
        u32 maskU32Count = (totalTiles + 31) / 32;
        VkDeviceSize bufferSize = static_cast<VkDeviceSize>(maskU32Count) * sizeof(u32);

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = bufferSize;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufInfo, nullptr, &m_StaleMaskBuffer) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create stale mask buffer");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_StaleMaskBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_StaleMaskMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device, m_StaleMaskBuffer, nullptr);
            m_StaleMaskBuffer = VK_NULL_HANDLE;
            ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to allocate stale mask memory");
            return;
        }

        vkBindBufferMemory(device, m_StaleMaskBuffer, m_StaleMaskMemory, 0);
        m_Context->TrackAllocation(static_cast<usize>(memReqs.size));
    }

    // Force full invalidation on first frame
    m_InvalidateRequested = true;
}

void RadianceCache::DestroyResources() {
    VkDevice device = m_Context->GetDevice();
    if (m_TileBuffer) { vkDestroyBuffer(device, m_TileBuffer, nullptr); m_TileBuffer = VK_NULL_HANDLE; }
    if (m_TileMemory) { vkFreeMemory(device, m_TileMemory, nullptr); m_TileMemory = VK_NULL_HANDLE; }
    if (m_StaleMaskBuffer) { vkDestroyBuffer(device, m_StaleMaskBuffer, nullptr); m_StaleMaskBuffer = VK_NULL_HANDLE; }
    if (m_StaleMaskMemory) { vkFreeMemory(device, m_StaleMaskMemory, nullptr); m_StaleMaskMemory = VK_NULL_HANDLE; }
}

void RadianceCache::CreateOutputImage() {
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
        ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create output image");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_Context->GetDevice(), m_OutputImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_OutputMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to allocate output image memory");
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
        ENJIN_LOG_ERROR(Renderer, "RadianceCache: Failed to create output image view");
        return;
    }
}

void RadianceCache::DestroyOutputImage() {
    VkDevice device = m_Context->GetDevice();
    if (m_OutputView) { vkDestroyImageView(device, m_OutputView, nullptr); m_OutputView = VK_NULL_HANDLE; }
    if (m_OutputImage) { vkDestroyImage(device, m_OutputImage, nullptr); m_OutputImage = VK_NULL_HANDLE; }
    if (m_OutputMemory) { vkFreeMemory(device, m_OutputMemory, nullptr); m_OutputMemory = VK_NULL_HANDLE; }
}

void RadianceCache::Shutdown() {
    if (!m_Initialized) return;

    VkDevice device = m_Context->GetDevice();

    if (m_UpdatePipeline) { vkDestroyPipeline(device, m_UpdatePipeline, nullptr); m_UpdatePipeline = VK_NULL_HANDLE; }
    if (m_UpdatePipelineLayout) { vkDestroyPipelineLayout(device, m_UpdatePipelineLayout, nullptr); m_UpdatePipelineLayout = VK_NULL_HANDLE; }
    if (m_ReadPipeline) { vkDestroyPipeline(device, m_ReadPipeline, nullptr); m_ReadPipeline = VK_NULL_HANDLE; }
    if (m_ReadPipelineLayout) { vkDestroyPipelineLayout(device, m_ReadPipelineLayout, nullptr); m_ReadPipelineLayout = VK_NULL_HANDLE; }

    DestroyResources();
    DestroyOutputImage();

    m_Initialized = false;
    ENJIN_LOG_INFO(Renderer, "Radiance cache shut down");
}

} // namespace Renderer
} // namespace Enjin
