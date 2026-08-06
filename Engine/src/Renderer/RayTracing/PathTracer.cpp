#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/RTPipeline.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

static PFN_vkCmdTraceRaysKHR s_vkCmdTraceRaysPT = nullptr;

PathTracer::PathTracer(VulkanContext* context) : m_Context(context) {}
PathTracer::~PathTracer() { Shutdown(); }

bool PathTracer::Initialize(u32 width, u32 height, VkDescriptorSetLayout rtDescLayout) {
    m_Width = width;
    m_Height = height;

    s_vkCmdTraceRaysPT = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_Context->GetDevice(), "vkCmdTraceRaysKHR");
    if (!s_vkCmdTraceRaysPT) return false;

    CreateAccumulationImage();

    // Linear clamp sampler for the display path (PP fullscreen draw samples the
    // accumulation image; also handles the swapchain-to-view downscale)
    if (m_OutputSampler == VK_NULL_HANDLE) {
        VkSamplerCreateInfo sampInfo{};
        sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampInfo.magFilter = VK_FILTER_LINEAR;
        sampInfo.minFilter = VK_FILTER_LINEAR;
        sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        if (vkCreateSampler(m_Context->GetDevice(), &sampInfo, nullptr, &m_OutputSampler) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "PathTracer: Failed to create output sampler");
            return false;
        }
    }

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescLayout;
    if (vkCreatePipelineLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "PathTracer: Failed to create pipeline layout");
        return false;
    }

    m_Pipeline = std::make_unique<RTPipeline>(m_Context);

    std::vector<RTPipeline::ShaderStage> stages = {
        { VK_SHADER_STAGE_RAYGEN_BIT_KHR, RT_PATHTRACE_RGEN_SPV, sizeof(RT_PATHTRACE_RGEN_SPV) },
        { VK_SHADER_STAGE_MISS_BIT_KHR, RT_PATHTRACE_RMISS_SPV, sizeof(RT_PATHTRACE_RMISS_SPV) },
        { VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, RT_PATHTRACE_RCHIT_SPV, sizeof(RT_PATHTRACE_RCHIT_SPV) }
    };

    std::vector<RTPipeline::ShaderGroup> groups = {
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR },
        { VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR }
    };

    u32 maxRecursion = m_Config.maxBounces + 1;
    if (maxRecursion > m_Context->GetRTCapabilities().maxRayRecursionDepth) {
        maxRecursion = m_Context->GetRTCapabilities().maxRayRecursionDepth;
    }

    if (!m_Pipeline->Create(stages, groups, rtDescLayout, m_PipelineLayout, maxRecursion)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create path tracer pipeline");
        return false;
    }

    m_AccumulatedSamples = 0;
    std::memset(&m_LastViewProj, 0, sizeof(m_LastViewProj));

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Path tracer initialized (%ux%u, maxBounces=%u, targetSPP=%u)",
        width, height, m_Config.maxBounces, m_Config.targetSPP);
    return true;
}

void PathTracer::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    m_Width = width;
    m_Height = height;
    DestroyAccumulationImage();
    CreateAccumulationImage();
    ResetAccumulation();
}

void PathTracer::Dispatch(VkCommandBuffer cmd, VkDescriptorSet rtDescSet,
                           const Math::Matrix4& invViewProj, const Math::Vector3& cameraPos,
                           const Math::Vector3& lightDir, u32 frameCount) {
    if (!m_Initialized || !s_vkCmdTraceRaysPT) return;
    if (IsConverged()) return;  // Already converged

    // Check if camera moved — reset accumulation
    if (std::memcmp(&invViewProj, &m_LastViewProj, sizeof(Math::Matrix4)) != 0) {
        if (m_AccumulatedSamples > 0) {
            ResetAccumulation();
        }
        m_LastViewProj = invViewProj;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline->GetPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                             m_PipelineLayout, 0, 1, &rtDescSet, 0, nullptr);

    const auto& sbt = m_Pipeline->GetSBTRegions();
    s_vkCmdTraceRaysPT(cmd, &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
                        m_Width, m_Height, 1);

    m_AccumulatedSamples++;
}

void PathTracer::ResetAccumulation() {
    m_AccumulatedSamples = 0;
}

void PathTracer::CreateAccumulationImage() {
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

    if (vkCreateImage(m_Context->GetDevice(), &imgInfo, nullptr, &m_AccumulationImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "PathTracer: Failed to create accumulation image");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_Context->GetDevice(), m_AccumulationImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_Context->GetDevice(), &allocInfo, nullptr, &m_AccumulationMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "PathTracer: Failed to allocate image memory");
        return;
    }
    vkBindImageMemory(m_Context->GetDevice(), m_AccumulationImage, m_AccumulationMemory, 0);
    m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_AccumulationImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(m_Context->GetDevice(), &viewInfo, nullptr, &m_AccumulationView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "PathTracer: Failed to create image view");
        return;
    }
}

void PathTracer::DestroyAccumulationImage() {
    if (m_AccumulationView) { vkDestroyImageView(m_Context->GetDevice(), m_AccumulationView, nullptr); m_AccumulationView = VK_NULL_HANDLE; }
    if (m_AccumulationImage) { vkDestroyImage(m_Context->GetDevice(), m_AccumulationImage, nullptr); m_AccumulationImage = VK_NULL_HANDLE; }
    if (m_AccumulationMemory) { vkFreeMemory(m_Context->GetDevice(), m_AccumulationMemory, nullptr); m_AccumulationMemory = VK_NULL_HANDLE; }
}

void PathTracer::Shutdown() {
    if (!m_Initialized) return;
    m_Pipeline.reset();
    DestroyAccumulationImage();
    if (m_OutputSampler) {
        vkDestroySampler(m_Context->GetDevice(), m_OutputSampler, nullptr);
        m_OutputSampler = VK_NULL_HANDLE;
    }
    if (m_PipelineLayout) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    m_Initialized = false;
}

} // namespace Renderer
} // namespace Enjin
