#include <string>
#include <vector>
#include "Enjin/Renderer/ShaderPaths.h"
#include "Enjin/Renderer/VRS/VariableRateShading.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

VariableRateShading::VariableRateShading(VulkanContext* context)
    : m_Context(context) {}

VariableRateShading::~VariableRateShading() {
    Shutdown();
}

bool VariableRateShading::Initialize(u32 screenWidth, u32 screenHeight) {
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    if (!ProbeHardwareSupport()) {
        ENJIN_LOG_INFO(Renderer, "VRS: Hardware does not support VK_KHR_fragment_shading_rate");
        return false;
    }

    if (!CreateShadingRateImage()) {
        ENJIN_LOG_WARN(Renderer, "VRS: Failed to create shading rate image");
        return false;
    }

    if (!CreateComputePipeline()) {
        ENJIN_LOG_WARN(Renderer, "VRS: Compute pipeline creation failed, using static rates");
    }

    m_Supported = true;
    m_Enabled = true;

    ENJIN_LOG_INFO(Renderer, "VRS initialized: tile size %ux%u, rate image %ux%u, min/max texel %u/%u",
        m_ShadingRateTileWidth, m_ShadingRateTileHeight,
        m_RateImageWidth, m_RateImageHeight,
        m_MinTexelSize, m_MaxTexelSize);
    return true;
}

void VariableRateShading::Shutdown() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    DestroyShadingRateImage();

    if (m_GeneratePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_GeneratePipeline, nullptr);
        m_GeneratePipeline = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    m_Supported = false;
    m_Enabled = false;
}

bool VariableRateShading::ProbeHardwareSupport() {
    VkPhysicalDevice physDevice = m_Context->GetPhysicalDevice();

    // Check if the extension is available
    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extensionCount, extensions.data());

    bool hasExtension = false;
    for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME) == 0) {
            hasExtension = true;
            break;
        }
    }

    if (!hasExtension) return false;

    // Query shading rate properties
    VkPhysicalDeviceFragmentShadingRatePropertiesKHR shadingRateProps{};
    shadingRateProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &shadingRateProps;
    vkGetPhysicalDeviceProperties2(physDevice, &props2);

    m_MinTexelSize = Math::Max(shadingRateProps.minFragmentShadingRateAttachmentTexelSize.width,
                                shadingRateProps.minFragmentShadingRateAttachmentTexelSize.height);
    m_MaxTexelSize = Math::Max(shadingRateProps.maxFragmentShadingRateAttachmentTexelSize.width,
                                shadingRateProps.maxFragmentShadingRateAttachmentTexelSize.height);
    m_ShadingRateTileWidth = shadingRateProps.maxFragmentShadingRateAttachmentTexelSize.width;
    m_ShadingRateTileHeight = shadingRateProps.maxFragmentShadingRateAttachmentTexelSize.height;

    // Ensure sane defaults
    if (m_ShadingRateTileWidth == 0) m_ShadingRateTileWidth = 16;
    if (m_ShadingRateTileHeight == 0) m_ShadingRateTileHeight = 16;

    // Query supported shading rates
    u32 rateCount = 0;
    PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetFragmentShadingRates =
        (PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR)vkGetInstanceProcAddr(
            m_Context->GetInstance(), "vkGetPhysicalDeviceFragmentShadingRatesKHR");
    if (vkGetFragmentShadingRates) {
        vkGetFragmentShadingRates(physDevice, &rateCount, nullptr);
    }

    return rateCount > 0;
}

bool VariableRateShading::CreateShadingRateImage() {
    VkDevice device = m_Context->GetDevice();

    m_RateImageWidth = (m_ScreenWidth + m_ShadingRateTileWidth - 1) / m_ShadingRateTileWidth;
    m_RateImageHeight = (m_ScreenHeight + m_ShadingRateTileHeight - 1) / m_ShadingRateTileHeight;

    // Create image (R8_UINT format for shading rate encoding)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8_UINT;
    imageInfo.extent = { m_RateImageWidth, m_RateImageHeight, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR |
                      VK_IMAGE_USAGE_STORAGE_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_ShadingRateImage) != VK_SUCCESS) {
        return false;
    }

    // Allocate memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_ShadingRateImage, &memReqs);

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

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_ShadingRateImageMemory) != VK_SUCCESS) {
        vkDestroyImage(device, m_ShadingRateImage, nullptr);
        m_ShadingRateImage = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(device, m_ShadingRateImage, m_ShadingRateImageMemory, 0);

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_ShadingRateImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UINT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_ShadingRateImageView) != VK_SUCCESS) {
        vkFreeMemory(device, m_ShadingRateImageMemory, nullptr);
        vkDestroyImage(device, m_ShadingRateImage, nullptr);
        m_ShadingRateImage = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void VariableRateShading::DestroyShadingRateImage() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_ShadingRateImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_ShadingRateImageView, nullptr);
        m_ShadingRateImageView = VK_NULL_HANDLE;
    }
    if (m_ShadingRateImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_ShadingRateImage, nullptr);
        m_ShadingRateImage = VK_NULL_HANDLE;
    }
    if (m_ShadingRateImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_ShadingRateImageMemory, nullptr);
        m_ShadingRateImageMemory = VK_NULL_HANDLE;
    }
}

bool VariableRateShading::CreateComputePipeline() {
    VkDevice device = m_Context->GetDevice();

    // Descriptor set layout: output shading rate image (storage image)
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS) {
        return false;
    }

    // Push constants for mode, screen dims, focus point
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(u32) * 4; // mode, width, height, pad

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        return false;
    }

    // Descriptor pool and set
    VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSet) != VK_SUCCESS) {
        return false;
    }

    // Write descriptor
    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.imageView = m_ShadingRateImageView;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_DescriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &imageDescInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    // Load compute shader
    VulkanShader shader(m_Context);
    const std::vector<std::string> shaderPaths = Renderer::ShaderSearchPaths("vrs_generate.comp.spv");
    bool loaded = false;
    for (const std::string& path : shaderPaths) {
        if (shader.LoadFromFile(path, false) && shader.GetModule() != VK_NULL_HANDLE) {
            loaded = true;
            break;
        }
    }
    if (!loaded) return false;

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_PipelineLayout;
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader.GetModule();
    stage.pName = "main";
    pipelineInfo.stage = stage;

    return vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GeneratePipeline) == VK_SUCCESS;
}

void VariableRateShading::GenerateShadingRateImage(
    VkCommandBuffer commandBuffer,
    VkImageView motionBufferView,
    VkImageView colorBufferView
) {
    if (!m_Enabled || !m_Supported || m_GeneratePipeline == VK_NULL_HANDLE) return;

    (void)motionBufferView;
    (void)colorBufferView;

    // Transition shading rate image to GENERAL for compute write
    VkImageMemoryBarrier toGeneral{};
    toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.srcAccessMask = 0;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toGeneral.image = m_ShadingRateImage;
    toGeneral.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toGeneral);

    // Dispatch compute shader
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_GeneratePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);

    struct PushData { u32 mode; u32 width; u32 height; u32 pad; } push;
    push.mode = static_cast<u32>(m_Mode);
    push.width = m_RateImageWidth;
    push.height = m_RateImageHeight;
    push.pad = 0;
    vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

    u32 groupsX = (m_RateImageWidth + 7) / 8;
    u32 groupsY = (m_RateImageHeight + 7) / 8;
    vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);

    // Transition to shading rate attachment layout
    VkImageMemoryBarrier toShadingRate{};
    toShadingRate.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toShadingRate.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toShadingRate.newLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
    toShadingRate.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toShadingRate.dstAccessMask = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
    toShadingRate.image = m_ShadingRateImage;
    toShadingRate.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
        0, 0, nullptr, 0, nullptr, 1, &toShadingRate);
}

VkRenderingFragmentShadingRateAttachmentInfoKHR VariableRateShading::GetAttachmentInfo() const {
    VkRenderingFragmentShadingRateAttachmentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
    info.imageView = m_ShadingRateImageView;
    info.imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
    info.shadingRateAttachmentTexelSize = { m_ShadingRateTileWidth, m_ShadingRateTileHeight };
    return info;
}

void VariableRateShading::Resize(u32 screenWidth, u32 screenHeight) {
    if (screenWidth == m_ScreenWidth && screenHeight == m_ScreenHeight) return;
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    if (m_Supported) {
        DestroyShadingRateImage();
        CreateShadingRateImage();

        // Re-write descriptor
        if (m_DescriptorSet != VK_NULL_HANDLE) {
            VkDescriptorImageInfo imageDescInfo{};
            imageDescInfo.imageView = m_ShadingRateImageView;
            imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_DescriptorSet;
            write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            write.descriptorCount = 1;
            write.pImageInfo = &imageDescInfo;
            vkUpdateDescriptorSets(m_Context->GetDevice(), 1, &write, 0, nullptr);
        }
    }
}

} // namespace Renderer
} // namespace Enjin
