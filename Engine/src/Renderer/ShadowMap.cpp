#include "Enjin/Renderer/ShadowMap.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Renderer {

ShadowMap::ShadowMap(VulkanContext* context)
    : m_Context(context) {
    for (u32 i = 0; i < MAX_SHADOW_CASCADES; ++i) {
        m_CascadeViewProj[i] = Math::Matrix4::Identity();
    }
}

ShadowMap::~ShadowMap() {
    Shutdown();
}

bool ShadowMap::Initialize(const ShadowMapConfig& config) {
    if (m_Initialized) {
        return true;
    }

    m_Config = config;
    if (m_Config.cascadeCount > MAX_SHADOW_CASCADES) {
        m_Config.cascadeCount = MAX_SHADOW_CASCADES;
    }

    ENJIN_LOG_INFO(Renderer, "Initializing cascaded shadow map (%ux%u, %u cascades)...",
                   config.resolution, config.resolution, m_Config.cascadeCount);

    if (!CreateDepthResources()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map depth resources");
        return false;
    }

    if (!CreateRenderPass()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map render pass");
        return false;
    }

    if (!CreateFramebuffers()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map framebuffers");
        return false;
    }

    if (!CreateSampler()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow sampler");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Cascaded shadow map initialized");
    return true;
}

void ShadowMap::Shutdown() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    m_Context->WaitForGPU();

    for (u32 i = 0; i < MAX_SHADOW_CASCADES; ++i) {
        if (m_CascadeFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, m_CascadeFramebuffers[i], nullptr);
            m_CascadeFramebuffers[i] = VK_NULL_HANDLE;
        }
    }

    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    if (m_ShadowSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_ShadowSampler, nullptr);
        m_ShadowSampler = VK_NULL_HANDLE;
    }

    for (u32 i = 0; i < MAX_SHADOW_CASCADES; ++i) {
        if (m_CascadeViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_CascadeViews[i], nullptr);
            m_CascadeViews[i] = VK_NULL_HANDLE;
        }
    }

    if (m_DepthArrayView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_DepthArrayView, nullptr);
        m_DepthArrayView = VK_NULL_HANDLE;
    }

    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }

    if (m_DepthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_DepthMemory, nullptr);
        m_DepthMemory = VK_NULL_HANDLE;
    }

    m_Initialized = false;
}

bool ShadowMap::CreateDepthResources() {
    VkDevice device = m_Context->GetDevice();
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    // Create depth image with arrayLayers = cascadeCount
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Config.resolution;
    imageInfo.extent.height = m_Config.resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = m_Config.cascadeCount;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map depth image");
        return false;
    }

    // Allocate memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_DepthImage, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    u32 memoryTypeIndex = UINT32_MAX;
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memoryTypeIndex = i;
            break;
        }
    }

    if (memoryTypeIndex == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "Failed to find suitable memory type for shadow map");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate shadow map memory");
        return false;
    }

    vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0);

    // Create 2D_ARRAY view for shader sampling (all cascades)
    VkImageViewCreateInfo arrayViewInfo{};
    arrayViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    arrayViewInfo.image = m_DepthImage;
    arrayViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    arrayViewInfo.format = VK_FORMAT_D32_SFLOAT;
    arrayViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    arrayViewInfo.subresourceRange.baseMipLevel = 0;
    arrayViewInfo.subresourceRange.levelCount = 1;
    arrayViewInfo.subresourceRange.baseArrayLayer = 0;
    arrayViewInfo.subresourceRange.layerCount = m_Config.cascadeCount;

    if (vkCreateImageView(device, &arrayViewInfo, nullptr, &m_DepthArrayView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map array view");
        return false;
    }

    // Create per-cascade 2D views for framebuffer attachments
    for (u32 i = 0; i < m_Config.cascadeCount; ++i) {
        VkImageViewCreateInfo cascadeViewInfo{};
        cascadeViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        cascadeViewInfo.image = m_DepthImage;
        cascadeViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        cascadeViewInfo.format = VK_FORMAT_D32_SFLOAT;
        cascadeViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        cascadeViewInfo.subresourceRange.baseMipLevel = 0;
        cascadeViewInfo.subresourceRange.levelCount = 1;
        cascadeViewInfo.subresourceRange.baseArrayLayer = i;
        cascadeViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &cascadeViewInfo, nullptr, &m_CascadeViews[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map cascade view %u", i);
            return false;
        }
    }

    return true;
}

bool ShadowMap::CreateRenderPass() {
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthRef;

    // Subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &depthAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = static_cast<u32>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow render pass");
        return false;
    }

    return true;
}

bool ShadowMap::CreateFramebuffers() {
    for (u32 i = 0; i < m_Config.cascadeCount; ++i) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_RenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &m_CascadeViews[i];
        fbInfo.width = m_Config.resolution;
        fbInfo.height = m_Config.resolution;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context->GetDevice(), &fbInfo, nullptr, &m_CascadeFramebuffers[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create shadow framebuffer for cascade %u", i);
            return false;
        }
    }

    return true;
}

bool ShadowMap::CreateSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    // Enable comparison for shadow sampling (PCF)
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_ShadowSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow sampler");
        return false;
    }

    return true;
}

void ShadowMap::BeginCascadePass(VkCommandBuffer commandBuffer, u32 cascadeIndex) {
    if (cascadeIndex >= m_Config.cascadeCount) return;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_CascadeFramebuffers[cascadeIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { m_Config.resolution, m_Config.resolution };

    VkClearValue clearValue{};
    clearValue.depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(m_Config.resolution);
    viewport.height = static_cast<f32>(m_Config.resolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { m_Config.resolution, m_Config.resolution };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void ShadowMap::EndCascadePass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

void ShadowMap::DestroyDepthResources() {
    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    m_Context->WaitForGPU();

    for (u32 i = 0; i < MAX_SHADOW_CASCADES; ++i) {
        if (m_CascadeFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, m_CascadeFramebuffers[i], nullptr);
            m_CascadeFramebuffers[i] = VK_NULL_HANDLE;
        }
    }
    for (u32 i = 0; i < MAX_SHADOW_CASCADES; ++i) {
        if (m_CascadeViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_CascadeViews[i], nullptr);
            m_CascadeViews[i] = VK_NULL_HANDLE;
        }
    }
    if (m_DepthArrayView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_DepthArrayView, nullptr);
        m_DepthArrayView = VK_NULL_HANDLE;
    }
    if (m_DepthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_DepthImage, nullptr);
        m_DepthImage = VK_NULL_HANDLE;
    }
    if (m_DepthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_DepthMemory, nullptr);
        m_DepthMemory = VK_NULL_HANDLE;
    }
}

void ShadowMap::SetResolution(u32 size) {
    if (size == m_Config.resolution || !m_Initialized) return;

    // Clamp to valid values
    if (size < 512) size = 512;
    if (size > 4096) size = 4096;

    ENJIN_LOG_INFO(Renderer, "Changing shadow map resolution from %u to %u", m_Config.resolution, size);
    m_Config.resolution = size;

    // Recreate depth resources and framebuffers at new size
    DestroyDepthResources();
    if (!CreateDepthResources()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate shadow map at resolution %u", size);
        return;
    }
    if (!CreateFramebuffers()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate shadow framebuffers at resolution %u", size);
    }
}

void ShadowMap::UpdateCascades(const Math::Matrix4& cameraView, const Math::Matrix4& cameraProj,
                                f32 cameraNear, f32 cameraFar, const Math::Vector3& lightDir) {
    // Clamp camera far to shadow distance
    f32 effectiveFar = std::min(cameraFar, m_Config.shadowDistance);

    // Compute split distances using practical split scheme
    // (blend between logarithmic and linear split)
    f32 lambda = m_Config.splitLambda;
    f32 ratio = effectiveFar / cameraNear;
    f32 range = effectiveFar - cameraNear;

    for (u32 i = 0; i < m_Config.cascadeCount; ++i) {
        f32 p = static_cast<f32>(i + 1) / static_cast<f32>(m_Config.cascadeCount);
        f32 logSplit = cameraNear * std::pow(ratio, p);
        f32 linearSplit = cameraNear + range * p;
        m_CascadeSplits[i] = lambda * logSplit + (1.0f - lambda) * linearSplit;
    }

    // Compute inverse view-projection to get frustum corners in world space
    Math::Matrix4 invViewProj = (cameraProj * cameraView).Inverse();

    // Unproject the 4 frustum rays at the actual near (z_ndc=0) and far (z_ndc=1) planes.
    // We then interpolate linearly in world space for each cascade's split distances,
    // which is correct because depth varies linearly along each camera ray.
    // (Using linear depth fractions as NDC z is WRONG — perspective maps depth non-linearly.)
    Math::Vector3 nearCorners[4], farCorners[4];
    {
        int idx = 0;
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                Math::Vector4 nearNdc(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 0.0f, 1.0f);
                Math::Vector4 farNdc(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 1.0f, 1.0f);
                Math::Vector4 nw = invViewProj * nearNdc;
                Math::Vector4 fw = invViewProj * farNdc;
                nearCorners[idx] = Math::Vector3(nw.x / nw.w, nw.y / nw.w, nw.z / nw.w);
                farCorners[idx]  = Math::Vector3(fw.x / fw.w, fw.y / fw.w, fw.z / fw.w);
                idx++;
            }
        }
    }

    for (u32 cascade = 0; cascade < m_Config.cascadeCount; ++cascade) {
        f32 splitNear = (cascade == 0) ? cameraNear : m_CascadeSplits[cascade - 1];
        f32 splitFar = m_CascadeSplits[cascade];

        // Linear interpolation fractions along the camera rays (world space)
        f32 nearT = (splitNear - cameraNear) / (cameraFar - cameraNear);
        f32 farT  = (splitFar  - cameraNear) / (cameraFar - cameraNear);

        // 8 world-space corners of the cascade sub-frustum
        Math::Vector3 corners[8];
        for (int i = 0; i < 4; ++i) {
            Math::Vector3 ray = farCorners[i] - nearCorners[i];
            corners[i]     = nearCorners[i] + ray * nearT;
            corners[i + 4] = nearCorners[i] + ray * farT;
        }

        // Compute frustum center
        Math::Vector3 center(0.0f);
        for (int i = 0; i < 8; ++i) {
            center = center + corners[i];
        }
        center = center * (1.0f / 8.0f);

        // Build light view matrix
        Math::Vector3 lightDirN = lightDir.Normalized();
        Math::Vector3 lightUp(0.0f, 1.0f, 0.0f);
        if (std::abs(lightDirN.Dot(lightUp)) > 0.99f) {
            lightUp = Math::Vector3(0.0f, 0.0f, 1.0f);
        }
        Math::Matrix4 lightView = Math::Matrix4::LookAt(center - lightDirN * 50.0f, center, lightUp);

        // Transform frustum corners to light space and compute AABB
        f32 minX = 1e9f, maxX = -1e9f;
        f32 minY = 1e9f, maxY = -1e9f;
        f32 minZ = 1e9f, maxZ = -1e9f;

        for (int i = 0; i < 8; ++i) {
            Math::Vector4 lsCorner = lightView * Math::Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f);
            minX = std::min(minX, lsCorner.x);
            maxX = std::max(maxX, lsCorner.x);
            minY = std::min(minY, lsCorner.y);
            maxY = std::max(maxY, lsCorner.y);
            minZ = std::min(minZ, lsCorner.z);
            maxZ = std::max(maxZ, lsCorner.z);
        }

        // Add Z padding so shadow casters behind the frustum are captured
        f32 zPad = 50.0f;
        minZ -= zPad;
        maxZ += zPad;

        // Texel-size snapping to prevent shadow swimming when camera moves
        f32 worldUnitsPerTexel = std::max(maxX - minX, maxY - minY) / static_cast<f32>(m_Config.resolution);
        if (worldUnitsPerTexel > 0.0f) {
            minX = std::floor(minX / worldUnitsPerTexel) * worldUnitsPerTexel;
            maxX = std::floor(maxX / worldUnitsPerTexel) * worldUnitsPerTexel;
            minY = std::floor(minY / worldUnitsPerTexel) * worldUnitsPerTexel;
            maxY = std::floor(maxY / worldUnitsPerTexel) * worldUnitsPerTexel;
        }

        // Build orthographic projection directly from light-space AABB bounds.
        // We bypass Math::Matrix4::Orthographic() because it applies a Vulkan Y-flip
        // (m[5] negated) without adjusting the m[13] translation, which clips geometry
        // for non-centered AABBs. Shadow maps are offscreen and don't need Y-flip.
        // Depth maps to Vulkan [0,1]: z_view=maxZ (closest to light) → 0, z_view=minZ → 1.
        Math::Matrix4 lightProj = Math::Matrix4::Identity();
        lightProj.m[0]  =  2.0f / (maxX - minX);
        lightProj.m[5]  =  2.0f / (maxY - minY);
        lightProj.m[10] = -1.0f / (maxZ - minZ);
        lightProj.m[12] = -(maxX + minX) / (maxX - minX);
        lightProj.m[13] = -(maxY + minY) / (maxY - minY);
        lightProj.m[14] =  maxZ / (maxZ - minZ);

        m_CascadeViewProj[cascade] = lightProj * lightView;
    }
}

const Math::Matrix4& ShadowMap::GetCascadeViewProj(u32 index) const {
    if (index >= MAX_SHADOW_CASCADES) {
        static Math::Matrix4 identity = Math::Matrix4::Identity();
        return identity;
    }
    return m_CascadeViewProj[index];
}

f32 ShadowMap::GetCascadeSplit(u32 index) const {
    if (index >= MAX_SHADOW_CASCADES) return 0.0f;
    return m_CascadeSplits[index];
}

} // namespace Renderer
} // namespace Enjin
