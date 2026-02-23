#include "Enjin/Renderer/RayTracing/RTTranslucency.h"
#include "Enjin/Renderer/RayTracing/RTPipeline.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

static PFN_vkCmdTraceRaysKHR s_vkCmdTraceRaysTrans = nullptr;

// Same RT image helpers as RTShadows (linked from RTShadows.cpp)
extern bool CreateRTImage(VulkanContext* ctx, u32 w, u32 h, VkFormat format,
                           VkImage& image, VkDeviceMemory& memory, VkImageView& view);
extern void DestroyRTImage(VulkanContext* ctx, VkImage& image, VkDeviceMemory& memory, VkImageView& view);

RTTranslucency::RTTranslucency(VulkanContext* context) : m_Context(context) {}
RTTranslucency::~RTTranslucency() { Shutdown(); }

bool RTTranslucency::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;

    s_vkCmdTraceRaysTrans = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_Context->GetDevice(), "vkCmdTraceRaysKHR");
    if (!s_vkCmdTraceRaysTrans) return false;

    CreateOutputImage();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout);

    m_Pipeline = std::make_unique<RTPipeline>(m_Context);

    std::vector<RTPipeline::ShaderStage> stages = {
        { VK_SHADER_STAGE_RAYGEN_BIT_KHR, RT_TRANSLUCENCY_RGEN_SPV, sizeof(RT_TRANSLUCENCY_RGEN_SPV) },
        { VK_SHADER_STAGE_MISS_BIT_KHR, RT_TRANSLUCENCY_RMISS_SPV, sizeof(RT_TRANSLUCENCY_RMISS_SPV) },
        { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, RT_TRANSLUCENCY_RCHIT_SPV, sizeof(RT_TRANSLUCENCY_RCHIT_SPV) }
    };

    std::vector<RTPipeline::ShaderGroup> groups = {
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR }
    };

    if (!m_Pipeline->Create(stages, groups, rtDescLayout, m_PipelineLayout, 2)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT translucency pipeline");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RT Translucency initialized (%ux%u)", width, height);
    return true;
}

void RTTranslucency::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyOutputImage();
    CreateOutputImage();
}

void RTTranslucency::Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                                const Math::Matrix4& invViewProj, const Math::Vector3& cameraPos,
                                u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled || !s_vkCmdTraceRaysTrans) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline->GetPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                             m_PipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    const auto& sbt = m_Pipeline->GetSBTRegions();
    s_vkCmdTraceRaysTrans(cmd, &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
                            m_Width, m_Height, 1);
}

void RTTranslucency::CreateOutputImage() {
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

    vkCreateImage(m_Context->GetDevice(), &imgInfo, nullptr, &m_OutputImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_Context->GetDevice(), m_OutputImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_OutputMemory);
    vkBindImageMemory(m_Context->GetDevice(), m_OutputImage, m_OutputMemory, 0);
    m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_OutputImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_OutputView);
}

void RTTranslucency::DestroyOutputImage() {
    if (m_OutputView) { vkDestroyImageView(m_Context->GetDevice(), m_OutputView, nullptr); m_OutputView = VK_NULL_HANDLE; }
    if (m_OutputImage) { vkDestroyImage(m_Context->GetDevice(), m_OutputImage, nullptr); m_OutputImage = VK_NULL_HANDLE; }
    if (m_OutputMemory) { vkFreeMemory(m_Context->GetDevice(), m_OutputMemory, nullptr); m_OutputMemory = VK_NULL_HANDLE; }
}

void RTTranslucency::Shutdown() {
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
