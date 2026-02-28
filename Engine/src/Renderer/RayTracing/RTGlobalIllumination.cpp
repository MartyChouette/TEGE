#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/RTPipeline.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

static PFN_vkCmdTraceRaysKHR s_vkCmdTraceRaysGI = nullptr;

RTGlobalIllumination::RTGlobalIllumination(VulkanContext* context) : m_Context(context) {}
RTGlobalIllumination::~RTGlobalIllumination() { Shutdown(); }

bool RTGlobalIllumination::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;

    s_vkCmdTraceRaysGI = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_Context->GetDevice(), "vkCmdTraceRaysKHR");
    if (!s_vkCmdTraceRaysGI) return false;

    CreateOutputImage();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    if (vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "RTGlobalIllumination: Failed to create pipeline layout");
        return false;
    }

    m_Pipeline = std::make_unique<RTPipeline>(m_Context);

    std::vector<RTPipeline::ShaderStage> stages = {
        { VK_SHADER_STAGE_RAYGEN_BIT_KHR, RT_GI_RGEN_SPV, sizeof(RT_GI_RGEN_SPV) },
        { VK_SHADER_STAGE_MISS_BIT_KHR, RT_GI_RMISS_SPV, sizeof(RT_GI_RMISS_SPV) },
        { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, RT_GI_RCHIT_SPV, sizeof(RT_GI_RCHIT_SPV) }
    };

    std::vector<RTPipeline::ShaderGroup> groups = {
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR }
    };

    if (!m_Pipeline->Create(stages, groups, rtDescLayout, m_PipelineLayout, 2)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT GI pipeline");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RT GI initialized (%ux%u)", width, height);
    return true;
}

void RTGlobalIllumination::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyOutputImage();
    CreateOutputImage();
}

void RTGlobalIllumination::Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                                     const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                                     const Math::Vector3& cameraPos, u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled || !s_vkCmdTraceRaysGI) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline->GetPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                             m_PipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    const auto& sbt = m_Pipeline->GetSBTRegions();
    s_vkCmdTraceRaysGI(cmd, &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
                        m_Width, m_Height, 1);
}

void RTGlobalIllumination::CreateOutputImage() {
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
        ENJIN_LOG_ERROR(Renderer, "RTGlobalIllumination: Failed to create output image");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_Context->GetDevice(), m_OutputImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_OutputMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "RTGlobalIllumination: Failed to allocate image memory");
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
        ENJIN_LOG_ERROR(Renderer, "RTGlobalIllumination: Failed to create image view");
        return;
    }
}

void RTGlobalIllumination::DestroyOutputImage() {
    if (m_OutputView) { vkDestroyImageView(m_Context->GetDevice(), m_OutputView, nullptr); m_OutputView = VK_NULL_HANDLE; }
    if (m_OutputImage) { vkDestroyImage(m_Context->GetDevice(), m_OutputImage, nullptr); m_OutputImage = VK_NULL_HANDLE; }
    if (m_OutputMemory) { vkFreeMemory(m_Context->GetDevice(), m_OutputMemory, nullptr); m_OutputMemory = VK_NULL_HANDLE; }
}

void RTGlobalIllumination::Shutdown() {
    if (!m_Initialized) return;
    m_Pipeline.reset();
    DestroyOutputImage();
    if (m_PipelineLayout) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    m_Initialized = false;
}

} // namespace Renderer
} // namespace Enjin
