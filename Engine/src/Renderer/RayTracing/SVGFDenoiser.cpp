#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <array>

namespace Enjin {
namespace Renderer {

SVGFDenoiser::SVGFDenoiser(VulkanContext* context) : m_Context(context) {}

SVGFDenoiser::~SVGFDenoiser() {
    Shutdown();
}

// Helper to create a 2D image + memory + view
static bool CreateImage2D(VulkanContext* ctx, u32 w, u32 h, VkFormat format,
                           VkImageUsageFlags usage,
                           VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
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

    if (vkCreateImage(ctx->GetDevice(), &imgInfo, nullptr, &image) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(ctx->GetDevice(), image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(ctx->GetDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyImage(ctx->GetDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(ctx->GetDevice(), image, memory, 0);
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

    if (vkCreateImageView(ctx->GetDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
        vkFreeMemory(ctx->GetDevice(), memory, nullptr);
        vkDestroyImage(ctx->GetDevice(), image, nullptr);
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

static void DestroyImage2D(VulkanContext* ctx, VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
    if (view != VK_NULL_HANDLE) { vkDestroyImageView(ctx->GetDevice(), view, nullptr); view = VK_NULL_HANDLE; }
    if (image != VK_NULL_HANDLE) { vkDestroyImage(ctx->GetDevice(), image, nullptr); image = VK_NULL_HANDLE; }
    if (memory != VK_NULL_HANDLE) { vkFreeMemory(ctx->GetDevice(), memory, nullptr); memory = VK_NULL_HANDLE; }
}

bool SVGFDenoiser::Initialize(u32 width, u32 height) {
    if (m_Initialized) return true;

    m_Width = width;
    m_Height = height;

    CreateComputePipelines();
    CreateIntermediateImages();
    CreateDescriptorSets();

    m_HasHistory = false;
    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "SVGF denoiser initialized (%ux%u)", width, height);
    return true;
}

void SVGFDenoiser::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyIntermediateImages();
    CreateIntermediateImages();
    m_HasHistory = false;
}

void SVGFDenoiser::CreateComputePipelines() {
    VkDevice device = m_Context->GetDevice();

    // Descriptor set layout for SVGF passes
    // Bindings: 0=noisy input, 1=depth, 2=normal, 3=motion, 4=history, 5=moments,
    //           6=variance, 7=pingpong[0], 8=pingpong[1], 9=output
    std::array<VkDescriptorSetLayoutBinding, 10> bindings{};
    for (u32 i = 0; i < 10; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = (i <= 3) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                               : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout);

    // Push constant for pass-specific parameters
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = 32;  // stepSize(4), temporalAlpha(4), sigma[3](12), iteration(4), flags(4), pad(4)

    VkPipelineLayoutCreateInfo pipeLayoutInfo{};
    pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeLayoutInfo.setLayoutCount = 1;
    pipeLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;
    pipeLayoutInfo.pushConstantRangeCount = 1;
    pipeLayoutInfo.pPushConstantRanges = &pushRange;
    vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &m_PipelineLayout);

    // Create compute pipelines from embedded SPIR-V
    auto createComputePipeline = [&](const u32* spirv, usize spirvSize) -> VkPipeline {
        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = spirvSize;
        moduleInfo.pCode = spirv;

        VkShaderModule module;
        if (vkCreateShaderModule(device, &moduleInfo, nullptr, &module) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        VkComputePipelineCreateInfo pipeInfo{};
        pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeInfo.stage.module = module;
        pipeInfo.stage.pName = "main";
        pipeInfo.layout = m_PipelineLayout;

        VkPipeline pipeline;
        VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline);
        vkDestroyShaderModule(device, module, nullptr);
        return (result == VK_SUCCESS) ? pipeline : VK_NULL_HANDLE;
    };

    m_TemporalPipeline = createComputePipeline(
        RT_SVGF_TEMPORAL_COMP_SPV, sizeof(RT_SVGF_TEMPORAL_COMP_SPV));
    m_VariancePipeline = createComputePipeline(
        RT_SVGF_VARIANCE_COMP_SPV, sizeof(RT_SVGF_VARIANCE_COMP_SPV));
    m_AtrousPipeline = createComputePipeline(
        RT_SVGF_ATROUS_COMP_SPV, sizeof(RT_SVGF_ATROUS_COMP_SPV));

    if (!m_TemporalPipeline || !m_VariancePipeline || !m_AtrousPipeline) {
        ENJIN_LOG_WARN(Renderer, "Some SVGF compute pipelines failed to create");
    }
}

void SVGFDenoiser::CreateIntermediateImages() {
    VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16G16B16A16_SFLOAT, usage,
                  m_HistoryImage, m_HistoryMemory, m_HistoryView);
    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16G16_SFLOAT, usage,
                  m_MomentsImage, m_MomentsMemory, m_MomentsView);
    CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16_SFLOAT, usage,
                  m_VarianceImage, m_VarianceMemory, m_VarianceView);

    for (int i = 0; i < 2; ++i) {
        CreateImage2D(m_Context, m_Width, m_Height, VK_FORMAT_R16G16B16A16_SFLOAT, usage,
                      m_PingPongImages[i], m_PingPongMemory[i], m_PingPongViews[i]);
    }
}

void SVGFDenoiser::DestroyIntermediateImages() {
    DestroyImage2D(m_Context, m_HistoryImage, m_HistoryMemory, m_HistoryView);
    DestroyImage2D(m_Context, m_MomentsImage, m_MomentsMemory, m_MomentsView);
    DestroyImage2D(m_Context, m_VarianceImage, m_VarianceMemory, m_VarianceView);
    for (int i = 0; i < 2; ++i) {
        DestroyImage2D(m_Context, m_PingPongImages[i], m_PingPongMemory[i], m_PingPongViews[i]);
    }
}

void SVGFDenoiser::CreateDescriptorSets() {
    VkDevice device = m_Context->GetDevice();

    // Descriptor pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 4;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[1].descriptorCount = 6;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;
    vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSet);
}

void SVGFDenoiser::RunDenoiserPasses(VkCommandBuffer cmd, VkImageView noisyInput,
                                      VkImageView depthView, VkImageView normalView,
                                      VkImageView motionView, VkImageView output,
                                      bool singleChannel) {
    if (!m_TemporalPipeline || !m_VariancePipeline || !m_AtrousPipeline) return;

    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;

    struct SVGFPushConstants {
        i32 stepSize;
        f32 temporalAlpha;
        f32 sigmaDepth;
        f32 sigmaNormal;
        f32 sigmaLuminance;
        i32 iteration;
        i32 flags;  // bit 0: single channel, bit 1: has history
        i32 pad;
    };

    SVGFPushConstants pc{};
    pc.temporalAlpha = m_Config.temporalAlpha;
    pc.sigmaDepth = m_Config.sigmaDepth;
    pc.sigmaNormal = m_Config.sigmaNormal;
    pc.sigmaLuminance = m_Config.sigmaLuminance;
    pc.flags = (singleChannel ? 1 : 0) | (m_HasHistory ? 2 : 0);

    // Pass 1: Temporal accumulation
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_TemporalPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_PipelineLayout,
                             0, 1, &m_DescriptorSet, 0, nullptr);
    pc.stepSize = 0;
    pc.iteration = 0;
    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    // Barrier after temporal pass
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Pass 2: Variance estimation
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_VariancePipeline);
    vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
    vkCmdDispatch(cmd, groupsX, groupsY, 1);

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Pass 3: A-trous wavelet filter (iterated)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_AtrousPipeline);
    for (u32 i = 0; i < m_Config.atrousIterations; ++i) {
        pc.stepSize = 1 << i;  // 1, 2, 4, 8, 16
        pc.iteration = static_cast<i32>(i);
        vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
        vkCmdDispatch(cmd, groupsX, groupsY, 1);

        if (i < m_Config.atrousIterations - 1) {
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  0, 1, &barrier, 0, nullptr, 0, nullptr);
        }
    }

    m_HasHistory = true;
}

void SVGFDenoiser::DenoiseSingleChannel(VkCommandBuffer cmd, VkImageView noisyInput,
                                          VkImageView depthView, VkImageView normalView,
                                          VkImageView motionView, VkImageView output) {
    RunDenoiserPasses(cmd, noisyInput, depthView, normalView, motionView, output, true);
}

void SVGFDenoiser::DenoiseColor(VkCommandBuffer cmd, VkImageView noisyInput,
                                  VkImageView depthView, VkImageView normalView,
                                  VkImageView motionView, VkImageView output) {
    RunDenoiserPasses(cmd, noisyInput, depthView, normalView, motionView, output, false);
}

void SVGFDenoiser::ResetHistory() {
    m_HasHistory = false;
}

void SVGFDenoiser::Shutdown() {
    if (!m_Initialized) return;

    VkDevice device = m_Context->GetDevice();

    DestroyIntermediateImages();

    if (m_TemporalPipeline) { vkDestroyPipeline(device, m_TemporalPipeline, nullptr); m_TemporalPipeline = VK_NULL_HANDLE; }
    if (m_VariancePipeline) { vkDestroyPipeline(device, m_VariancePipeline, nullptr); m_VariancePipeline = VK_NULL_HANDLE; }
    if (m_AtrousPipeline) { vkDestroyPipeline(device, m_AtrousPipeline, nullptr); m_AtrousPipeline = VK_NULL_HANDLE; }
    if (m_PipelineLayout) { vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
    if (m_DescriptorPool) { vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr); m_DescriptorPool = VK_NULL_HANDLE; }
    if (m_DescriptorSetLayout) { vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr); m_DescriptorSetLayout = VK_NULL_HANDLE; }

    m_Initialized = false;
}

} // namespace Renderer
} // namespace Enjin
