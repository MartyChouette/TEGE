#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTPipeline.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

static PFN_vkCmdTraceRaysKHR s_vkCmdTraceRays = nullptr;

RTShadows::RTShadows(VulkanContext* context) : m_Context(context) {}

RTShadows::~RTShadows() { Shutdown(); }

// Helper to create an image for RT output
static bool CreateRTImage(VulkanContext* ctx, u32 w, u32 h, VkFormat format,
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
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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
        return false;
    }
    vkBindImageMemory(ctx->GetDevice(), image, memory, 0);
    ctx->TrackAllocation(static_cast<usize>(memReqs.size));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(ctx->GetDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
        vkFreeMemory(ctx->GetDevice(), memory, nullptr);
        vkDestroyImage(ctx->GetDevice(), image, nullptr);
        return false;
    }

    return true;
}

static void DestroyRTImage(VulkanContext* ctx, VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
    if (view) { vkDestroyImageView(ctx->GetDevice(), view, nullptr); view = VK_NULL_HANDLE; }
    if (image) { vkDestroyImage(ctx->GetDevice(), image, nullptr); image = VK_NULL_HANDLE; }
    if (memory) { vkFreeMemory(ctx->GetDevice(), memory, nullptr); memory = VK_NULL_HANDLE; }
}

bool RTShadows::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;

    s_vkCmdTraceRays = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_Context->GetDevice(), "vkCmdTraceRaysKHR");
    if (!s_vkCmdTraceRays) {
        ENJIN_LOG_ERROR(Renderer, "vkCmdTraceRaysKHR not available");
        return false;
    }

    CreateOutputImage();

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout);

    // Create RT pipeline
    m_Pipeline = std::make_unique<RTPipeline>(m_Context);

    std::vector<RTPipeline::ShaderStage> stages = {
        { VK_SHADER_STAGE_RAYGEN_BIT_KHR, RT_SHADOW_RGEN_SPV, sizeof(RT_SHADOW_RGEN_SPV) },
        { VK_SHADER_STAGE_MISS_BIT_KHR, RT_SHADOW_RMISS_SPV, sizeof(RT_SHADOW_RMISS_SPV) },
        { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, RT_SHADOW_RCHIT_SPV, sizeof(RT_SHADOW_RCHIT_SPV) }
    };

    std::vector<RTPipeline::ShaderGroup> groups = {
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR }
    };

    if (!m_Pipeline->Create(stages, groups, rtDescLayout, m_PipelineLayout, 1)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT shadow pipeline");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RT Shadows initialized (%ux%u)", width, height);
    return true;
}

void RTShadows::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyOutputImage();
    CreateOutputImage();
}

void RTShadows::Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                          const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                          f32 lightIntensity, u32 frameCount) {
    if (!m_Initialized || !m_Config.enabled || !s_vkCmdTraceRays) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline->GetPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                             m_PipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    const auto& sbt = m_Pipeline->GetSBTRegions();
    s_vkCmdTraceRays(cmd, &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
                      m_Width, m_Height, 1);
}

void RTShadows::CreateOutputImage() {
    CreateRTImage(m_Context, m_Width, m_Height, VK_FORMAT_R16_SFLOAT,
                  m_OutputImage, m_OutputMemory, m_OutputView);
}

void RTShadows::DestroyOutputImage() {
    DestroyRTImage(m_Context, m_OutputImage, m_OutputMemory, m_OutputView);
}

void RTShadows::Shutdown() {
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
