#include "Enjin/Renderer/RayTracing/RTTemporalReuse.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <array>

namespace Enjin {
namespace Renderer {

// Helper to create a 2D image + memory + view (same pattern as SVGFDenoiser)
static bool CreateImage2D(VulkanContext* ctx, u32 w, u32 h, VkFormat format,
                           VkImageUsageFlags usage,
                           VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
    VkDevice device = ctx->GetDevice();

    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent = { w, h, 1 };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = usage;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imgInfo, nullptr, &image) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(device, image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(device, image, memory, 0);
    ctx->TrackAllocation(static_cast<usize>(memReqs.size));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        vkFreeMemory(device, memory, nullptr);
        vkDestroyImage(device, image, nullptr);
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

static void DestroyImage2D(VulkanContext* ctx, VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
    VkDevice device = ctx->GetDevice();
    if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); view = VK_NULL_HANDLE; }
    if (image != VK_NULL_HANDLE) { vkDestroyImage(device, image, nullptr); image = VK_NULL_HANDLE; }
    if (memory != VK_NULL_HANDLE) { vkFreeMemory(device, memory, nullptr); memory = VK_NULL_HANDLE; }
}

RTTemporalReuse::RTTemporalReuse(VulkanContext* context) : m_Context(context) {}

RTTemporalReuse::~RTTemporalReuse() {
    Shutdown();
}

bool RTTemporalReuse::Initialize(u32 width, u32 height) {
    if (m_Initialized) return true;

    m_Width = width;
    m_Height = height;

    CreateDescriptorResources();
    CreateComputePipeline();
    CreateHistoryImages();

    ResetHistory();
    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RT Temporal Reuse initialized (%ux%u)", width, height);
    return true;
}

void RTTemporalReuse::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyHistoryImages();
    CreateHistoryImages();
    ResetHistory();
}

void RTTemporalReuse::ResetHistory() {
    m_HasShadowHistory = false;
    m_HasAOHistory = false;
    m_HasReflectionHistory = false;
    m_HasGIHistory = false;
}

void RTTemporalReuse::CreateDescriptorResources() {
    VkDevice device = m_Context->GetDevice();

    // Descriptor set layout: 7 bindings
    // 0-3: combined image samplers (current input, depth, normals, motion)
    // 4-5: storage images (history, output)
    // 6: storage image (confidence)
    std::array<VkDescriptorSetLayoutBinding, 7> bindings{};
    for (u32 i = 0; i <= 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    for (u32 i = 4; i <= 6; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout);

    // Descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 3;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSet);

    // Create sampler for input textures (linear filter, clamp-to-edge)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler);
}

void RTTemporalReuse::DestroyDescriptorResources() {
    VkDevice device = m_Context->GetDevice();
    if (m_Sampler) { vkDestroySampler(device, m_Sampler, nullptr); m_Sampler = VK_NULL_HANDLE; }
    if (m_DescriptorPool) { vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr); m_DescriptorPool = VK_NULL_HANDLE; }
    if (m_DescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr); m_DescriptorSetLayout = VK_NULL_HANDLE; }
    m_DescriptorSet = VK_NULL_HANDLE;
}

void RTTemporalReuse::CreateComputePipeline() {
    VkDevice device = m_Context->GetDevice();

    // Push constants: historyLength(4), disocclusionThreshold(4), normalThreshold(4), flags(4)
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = 16;

    VkPipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pushRange;
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &m_PipelineLayout);

    // Create compute pipeline from embedded SPIR-V
    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = sizeof(RT_TEMPORAL_REUSE_COMP_SPV);
    moduleInfo.pCode = RT_TEMPORAL_REUSE_COMP_SPV;

    VkShaderModule module;
    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &module) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT temporal reuse shader module");
        return;
    }

    VkComputePipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeInfo.stage.module = module;
    pipeInfo.stage.pName = "main";
    pipeInfo.layout = m_PipelineLayout;

    VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &m_Pipeline);
    vkDestroyShaderModule(device, module, nullptr);

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT temporal reuse compute pipeline");
    }
}

void RTTemporalReuse::CreateHistoryImages() {
    VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // Shadow history (R16F — stored in RGBA16F for uniform shader access)
    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16G16B16A16_SFLOAT, usage,
                  m_ShadowHistoryImage, m_ShadowHistoryMemory, m_ShadowHistoryView);

    // AO history (R16F — stored in RGBA16F)
    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16G16B16A16_SFLOAT, usage,
                  m_AOHistoryImage, m_AOHistoryMemory, m_AOHistoryView);

    // Reflection history (RGBA16F)
    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16G16B16A16_SFLOAT, usage,
                  m_ReflectionHistoryImage, m_ReflectionHistoryMemory, m_ReflectionHistoryView);

    // GI history (RGBA16F)
    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16G16B16A16_SFLOAT, usage,
                  m_GIHistoryImage, m_GIHistoryMemory, m_GIHistoryView);

    // Confidence map (R8 UNORM)
    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R8_UNORM, usage,
                  m_ConfidenceImage, m_ConfidenceMemory, m_ConfidenceView);
}

void RTTemporalReuse::DestroyHistoryImages() {
    DestroyImage2D(m_Context, m_ShadowHistoryImage, m_ShadowHistoryMemory, m_ShadowHistoryView);
    DestroyImage2D(m_Context, m_AOHistoryImage, m_AOHistoryMemory, m_AOHistoryView);
    DestroyImage2D(m_Context, m_ReflectionHistoryImage, m_ReflectionHistoryMemory, m_ReflectionHistoryView);
    DestroyImage2D(m_Context, m_GIHistoryImage, m_GIHistoryMemory, m_GIHistoryView);
    DestroyImage2D(m_Context, m_ConfidenceImage, m_ConfidenceMemory, m_ConfidenceView);
}

void RTTemporalReuse::DispatchReuse(VkCommandBuffer cmd, VkImageView currentView,
                                     VkImageView depthView, VkImageView normalView,
                                     VkImageView motionView, VkImageView outputView,
                                     VkImage historyImage, VkImageView historyView,
                                     bool singleChannel, bool& hasHistory) {
    if (!m_Pipeline || !m_Initialized) return;

    VkDevice device = m_Context->GetDevice();

    // Transition history image to GENERAL for read+write in compute shader
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = hasHistory ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.image = historyImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Update descriptor set bindings for this dispatch
    std::array<VkWriteDescriptorSet, 7> writes{};
    VkDescriptorImageInfo inputInfo{m_Sampler, currentView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo depthInfo{m_Sampler, depthView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo normalInfo{m_Sampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo motionInfo{m_Sampler, motionView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo historyInfo{{}, historyView, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo outputInfo{{}, outputView, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo confInfo{{}, m_ConfidenceView, VK_IMAGE_LAYOUT_GENERAL};

    for (u32 i = 0; i < 7; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_DescriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
    }

    // Bindings 0-3: combined image samplers
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &inputInfo;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &depthInfo;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &normalInfo;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].pImageInfo = &motionInfo;

    // Bindings 4-6: storage images
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[4].pImageInfo = &historyInfo;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[5].pImageInfo = &outputInfo;
    writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[6].pImageInfo = &confInfo;

    vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

    // Push constants
    struct ReusePushConstants {
        f32 historyLength;
        f32 disocclusionThreshold;
        f32 normalThreshold;
        i32 flags;
    };

    ReusePushConstants pc{};
    pc.historyLength = m_Config.historyLength;
    pc.disocclusionThreshold = m_Config.disocclusionThreshold;
    pc.normalThreshold = m_Config.normalThreshold;
    pc.flags = (singleChannel ? 1 : 0) | (hasHistory ? 2 : 0);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout,
                             0, 1, &m_DescriptorSet, 0, nullptr);
    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Barrier after compute for downstream reads
    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &memBarrier, 0, nullptr, 0, nullptr);

    hasHistory = true;
}

void RTTemporalReuse::ReuseShadows(VkCommandBuffer cmd, VkImageView currentShadowView,
                                    VkImageView depthView, VkImageView normalView,
                                    VkImageView motionView, VkImageView outputView) {
    if (!m_Config.enabled || !m_Config.reuseShadows) return;
    DispatchReuse(cmd, currentShadowView, depthView, normalView, motionView, outputView,
                  m_ShadowHistoryImage, m_ShadowHistoryView, true, m_HasShadowHistory);
}

void RTTemporalReuse::ReuseAO(VkCommandBuffer cmd, VkImageView currentAOView,
                                VkImageView depthView, VkImageView normalView,
                                VkImageView motionView, VkImageView outputView) {
    if (!m_Config.enabled || !m_Config.reuseAO) return;
    DispatchReuse(cmd, currentAOView, depthView, normalView, motionView, outputView,
                  m_AOHistoryImage, m_AOHistoryView, true, m_HasAOHistory);
}

void RTTemporalReuse::ReuseReflections(VkCommandBuffer cmd, VkImageView currentReflectView,
                                        VkImageView depthView, VkImageView normalView,
                                        VkImageView motionView, VkImageView outputView) {
    if (!m_Config.enabled || !m_Config.reuseReflections) return;
    DispatchReuse(cmd, currentReflectView, depthView, normalView, motionView, outputView,
                  m_ReflectionHistoryImage, m_ReflectionHistoryView, false, m_HasReflectionHistory);
}

void RTTemporalReuse::ReuseGI(VkCommandBuffer cmd, VkImageView currentGIView,
                                VkImageView depthView, VkImageView normalView,
                                VkImageView motionView, VkImageView outputView) {
    if (!m_Config.enabled || !m_Config.reuseGI) return;
    DispatchReuse(cmd, currentGIView, depthView, normalView, motionView, outputView,
                  m_GIHistoryImage, m_GIHistoryView, false, m_HasGIHistory);
}

void RTTemporalReuse::Shutdown() {
    if (!m_Initialized) return;

    VkDevice device = m_Context->GetDevice();

    DestroyHistoryImages();
    DestroyDescriptorResources();

    if (m_Pipeline) { vkDestroyPipeline(device, m_Pipeline, nullptr); m_Pipeline = VK_NULL_HANDLE; }
    if (m_PipelineLayout) { vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }

    m_Initialized = false;
}

} // namespace Renderer
} // namespace Enjin
