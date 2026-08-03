#include "Enjin/Renderer/GPUDriven/HiZPyramid.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
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

bool HiZPyramid::Initialize(u32 width, u32 height) {
    m_Width = width;
    m_Height = height;
    m_MipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1;

    if (!CreateImage()) return false;
    if (!CreateSampler()) return false;
    if (!CreateComputePipeline()) return false;
    if (!CreateDescriptorResources()) return false;

    ENJIN_LOG_INFO(Renderer, "HiZ pyramid initialized: %ux%u, %u mip levels", width, height, m_MipLevels);
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

bool HiZPyramid::Resize(u32 width, u32 height) {
    Shutdown();
    return Initialize(width, height);
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
    // Use reduction mode sampler for conservative reads (MIN = closest occluder)
    VkSamplerReductionModeCreateInfo reductionInfo{};
    reductionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
    reductionInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX; // MAX depth = furthest (conservative for occlusion)

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext = &reductionInfo;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
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

    // Load compute shader
    VulkanShader computeShader(m_Context);
    const char* shaderPaths[] = {
        "shaders/hiz_generate.comp.spv",
        "Engine/shaders/hiz_generate.comp.spv",
        "../Engine/shaders/hiz_generate.comp.spv",
        "../../Engine/shaders/hiz_generate.comp.spv",
        "../../../Engine/shaders/hiz_generate.comp.spv"
    };
    bool loaded = false;
    for (const char* path : shaderPaths) {
        if (computeShader.LoadFromFile(path, false)) { loaded = true; break; }
    }
    if (!loaded) {
        ENJIN_LOG_WARN(Renderer, "HiZ downsample shader not found — Hi-Z occlusion disabled");
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
    u32 setCount = m_MipLevels > 0 ? m_MipLevels - 1 : 0; // One set per mip transition
    if (setCount == 0) return true;

    // Descriptor pool
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

    // Allocate descriptor sets
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

    // Write descriptor sets: each reads from mip N, writes to mip N+1
    // Note: descriptor sets for mip 0 source are updated dynamically in Generate()
    // since the initial depth image view comes from outside
    for (u32 i = 0; i < setCount; ++i) {
        VkDescriptorImageInfo srcInfo{};
        srcInfo.sampler = m_Sampler;
        srcInfo.imageView = m_MipViews[i]; // Read from mip i
        srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo dstInfo{};
        dstInfo.imageView = m_MipViews[i + 1]; // Write to mip i+1
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

        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    return true;
}

void HiZPyramid::Generate(VkCommandBuffer commandBuffer, VkImageView depthImageView) {
    if (m_DownsamplePipeline == VK_NULL_HANDLE || m_MipLevels <= 1) return;

    // Transition entire HiZ image to GENERAL for compute writes
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = m_Image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = m_MipLevels;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_DownsamplePipeline);

    // Generate mip chain: each level reads previous, writes next
    u32 srcWidth = m_Width;
    u32 srcHeight = m_Height;

    for (u32 mip = 0; mip < m_MipLevels - 1; ++mip) {
        u32 dstWidth = std::max(1u, srcWidth / 2);
        u32 dstHeight = std::max(1u, srcHeight / 2);

        // Transition source mip to SHADER_READ_ONLY
        if (mip > 0) {
            VkImageMemoryBarrier readBarrier{};
            readBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            readBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            readBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            readBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            readBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            readBarrier.image = m_Image;
            readBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            readBarrier.subresourceRange.baseMipLevel = mip;
            readBarrier.subresourceRange.levelCount = 1;
            readBarrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &readBarrier);
        }

        // Bind descriptor set for this mip transition
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
            m_PipelineLayout, 0, 1, &m_DescriptorSets[mip], 0, nullptr);

        // Push source dimensions
        i32 srcDims[2] = { static_cast<i32>(srcWidth), static_cast<i32>(srcHeight) };
        vkCmdPushConstants(commandBuffer, m_PipelineLayout,
            VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(srcDims), srcDims);

        // Dispatch
        u32 groupsX = (dstWidth + 7) / 8;
        u32 groupsY = (dstHeight + 7) / 8;
        vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);

        srcWidth = dstWidth;
        srcHeight = dstHeight;
    }

    // Final barrier: transition to SHADER_READ_ONLY for sampling in culling shader
    VkImageMemoryBarrier finalBarrier{};
    finalBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    finalBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    finalBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    finalBarrier.image = m_Image;
    finalBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    finalBarrier.subresourceRange.baseMipLevel = 0;
    finalBarrier.subresourceRange.levelCount = m_MipLevels;
    finalBarrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &finalBarrier);
}

} // namespace Renderer
} // namespace Enjin
