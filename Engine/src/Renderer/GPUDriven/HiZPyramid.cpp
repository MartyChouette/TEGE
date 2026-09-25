#include <string>
#include <vector>
#include "Enjin/Renderer/ShaderPaths.h"
#include "Enjin/Renderer/GPUDriven/HiZPyramid.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

HiZPyramid::HiZPyramid(VulkanContext* context)
    : m_Context(context) {
}

HiZPyramid::~HiZPyramid() {
    Shutdown();
}

bool HiZPyramid::Initialize(u32 depthWidth, u32 depthHeight) {
    m_DepthWidth = depthWidth;
    m_DepthHeight = depthHeight;
    m_Width = std::max(1u, depthWidth / 2);
    m_Height = std::max(1u, depthHeight / 2);
    m_MipLevels = static_cast<u32>(std::floor(std::log2(std::max(m_Width, m_Height)))) + 1;
    m_BoundDepthView = VK_NULL_HANDLE;

    if (!CreateImage()) return false;
    if (!CreateSampler()) return false;
    if (!CreateComputePipeline()) return false;
    if (!CreateDescriptorResources()) return false;

    ENJIN_LOG_INFO(Renderer, "HiZ pyramid initialized: %ux%u from %ux%u depth, %u mip levels",
                   m_Width, m_Height, depthWidth, depthHeight, m_MipLevels);
    return true;
}

void HiZPyramid::Shutdown() {
    VkDevice device = m_Context->GetDevice();

    for (auto view : m_MipViews) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device, view, nullptr);
    }
    m_MipViews.clear();

    if (m_FullView != VK_NULL_HANDLE) { vkDestroyImageView(device, m_FullView, nullptr); m_FullView = VK_NULL_HANDLE; }
    if (m_Image != VK_NULL_HANDLE) { vkDestroyImage(device, m_Image, nullptr); m_Image = VK_NULL_HANDLE; }
    if (m_Memory != VK_NULL_HANDLE) { vkFreeMemory(device, m_Memory, nullptr); m_Memory = VK_NULL_HANDLE; }
    if (m_Sampler != VK_NULL_HANDLE) { vkDestroySampler(device, m_Sampler, nullptr); m_Sampler = VK_NULL_HANDLE; }

    if (m_DownsamplePipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, m_DownsamplePipeline, nullptr); m_DownsamplePipeline = VK_NULL_HANDLE; }
    if (m_PipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
    if (m_DescriptorSetLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr); m_DescriptorSetLayout = VK_NULL_HANDLE; }
    if (m_DescriptorPool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr); m_DescriptorPool = VK_NULL_HANDLE; }
    m_DescriptorSets.clear();
}

bool HiZPyramid::CreateImage() {
    VkDevice device = m_Context->GetDevice();

    // Create R32_SFLOAT image with full mip chain
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32_SFLOAT;
    imageInfo.extent = { m_Width, m_Height, 1 };
    imageInfo.mipLevels = m_MipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_Image) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ image");
        return false;
    }

    // Allocate memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_Image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProps);
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_Memory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate HiZ memory");
        return false;
    }
    vkBindImageMemory(device, m_Image, m_Memory, 0);

    // Create full image view (all mips)
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_Image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = m_MipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_FullView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ full image view");
        return false;
    }

    // Create per-mip views for compute shader storage writes
    m_MipViews.resize(m_MipLevels);
    for (u32 mip = 0; mip < m_MipLevels; ++mip) {
        VkImageViewCreateInfo mipViewInfo = viewInfo;
        mipViewInfo.subresourceRange.baseMipLevel = mip;
        mipViewInfo.subresourceRange.levelCount = 1;
        if (vkCreateImageView(device, &mipViewInfo, nullptr, &m_MipViews[mip]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ mip %u view", mip);
            return false;
        }
    }

    return true;
}

bool HiZPyramid::CreateSampler() {
    // Plain nearest. Every read (the downsample and the cull test) is a
    // texelFetch, which ignores filtering, and a combined image sampler still
    // needs a sampler. This used to chain a MAX reduction mode, which is only
    // valid with the samplerFilterMinmax feature enabled; nothing checked it.
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<f32>(m_MipLevels);

    if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ sampler");
        return false;
    }
    return true;
}

bool HiZPyramid::CreateComputePipeline() {
    VkDevice device = m_Context->GetDevice();

    // Descriptor set layout: binding 0 = source sampler, binding 1 = dest storage image
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ descriptor set layout");
        return false;
    }

    // Push constant for source mip dimensions
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(i32) * 2; // ivec2 srcSize

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ pipeline layout");
        return false;
    }

    // Embedded, like cull_hiz: an exported game ships no .spv files, so a
    // file-loaded shader here silently disabled occlusion in every built game.
    VulkanShader computeShader(m_Context);
    if (!computeShader.LoadFromSPIRV(ShaderData::HiZGenerateComputeShaderData,
                                     ShaderData::HiZGenerateComputeShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "HiZ downsample shader failed to load - occlusion culling disabled");
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = computeShader.GetModule();
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_PipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_DownsamplePipeline) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ downsample pipeline");
        return false;
    }

    return true;
}

bool HiZPyramid::CreateDescriptorResources() {
    VkDevice device = m_Context->GetDevice();
    const u32 setCount = m_MipLevels;   // depth -> mip 0, then mip i-1 -> mip i

    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = setCount;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = setCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create HiZ descriptor pool");
        return false;
    }

    std::vector<VkDescriptorSetLayout> layouts(setCount, m_DescriptorSetLayout);
    m_DescriptorSets.resize(setCount);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = setCount;
    allocInfo.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(device, &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate HiZ descriptor sets");
        return false;
    }

    // Sets 1.. are fixed. Set 0's SOURCE is the depth buffer, bound in Generate.
    for (u32 i = 0; i < setCount; ++i) {
        VkDescriptorImageInfo srcInfo{};
        srcInfo.sampler = m_Sampler;
        srcInfo.imageView = (i > 0) ? m_MipViews[i - 1] : VK_NULL_HANDLE;
        srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo dstInfo{};
        dstInfo.imageView = m_MipViews[i];
        dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet writes[2]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_DescriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &srcInfo;
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_DescriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &dstInfo;

        // Set 0 gets only its destination now.
        if (i == 0) vkUpdateDescriptorSets(device, 1, &writes[1], 0, nullptr);
        else        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    return true;
}

void HiZPyramid::Generate(VkCommandBuffer commandBuffer, VkImageView depthImageView) {
    if (m_DownsamplePipeline == VK_NULL_HANDLE || m_DescriptorSets.empty() ||
        depthImageView == VK_NULL_HANDLE) {
        return;
    }
    VkDevice device = m_Context->GetDevice();

    // Point set 0 at the depth buffer. Only when the view CHANGES: the owner
    // rebuilds this pyramid when the depth buffer is recreated, so in practice
    // this is written once, before any frame has used the set.
    if (depthImageView != m_BoundDepthView) {
        VkDescriptorImageInfo depthInfo{};
        depthInfo.sampler = m_Sampler;
        depthInfo.imageView = depthImageView;
        depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = m_DescriptorSets[0];
        w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.descriptorCount = 1;
        w.pImageInfo = &depthInfo;
        vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);
        m_BoundDepthView = depthImageView;
    }

    auto levelBarrier = [&](u32 baseMip, u32 count, VkImageLayout oldLayout, VkImageLayout newLayout,
                            VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask = srcAccess;
        b.dstAccessMask = dstAccess;
        b.oldLayout = oldLayout;
        b.newLayout = newLayout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = m_Image;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.baseMipLevel = baseMip;
        b.subresourceRange.levelCount = count;
        b.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    };

    // Every level to GENERAL for writing. UNDEFINED discards last frame's
    // pyramid, which is rebuilt in full below; what this waits on is the
    // previous cull pass reading it.
    levelBarrier(0, m_MipLevels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                 VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_DownsamplePipeline);

    u32 srcW = m_DepthWidth, srcH = m_DepthHeight;
    u32 dstW = m_Width,      dstH = m_Height;
    for (u32 level = 0; level < m_MipLevels; ++level) {
        if (level > 0) {
            // The level just written becomes this dispatch's source.
            levelBarrier(level - 1, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_PipelineLayout, 0, 1, &m_DescriptorSets[level], 0, nullptr);
        i32 srcDims[2] = { static_cast<i32>(srcW), static_cast<i32>(srcH) };
        vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(srcDims), srcDims);
        vkCmdDispatch(commandBuffer, (dstW + 7) / 8, (dstH + 7) / 8, 1);

        srcW = dstW;
        srcH = dstH;
        dstW = std::max(1u, dstW / 2);
        dstH = std::max(1u, dstH / 2);
    }

    // Only the LAST level is still GENERAL; the rest moved as they were read.
    levelBarrier(m_MipLevels - 1, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
}

} // namespace Renderer
} // namespace Enjin
