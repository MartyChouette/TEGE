#include "Enjin/Renderer/SpotLightShadowMap.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cmath>

namespace Enjin {
namespace Renderer {

SpotLightShadowMap::SpotLightShadowMap(VulkanContext* context)
    : m_Context(context) {
}

SpotLightShadowMap::~SpotLightShadowMap() {
    Shutdown();
}

bool SpotLightShadowMap::Initialize(u32 resolution) {
    if (m_Initialized) return true;

    m_Resolution = resolution;

    ENJIN_LOG_INFO(Renderer, "Initializing spot light shadow map (%ux%u, %u lights)...",
                   resolution, resolution, MAX_LIGHTS);

    if (!CreateDepthResources()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffers()) return false;
    if (!CreateSampler()) return false;

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Spot light shadow map initialized");
    return true;
}

void SpotLightShadowMap::Shutdown() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(device);
    DestroyResources();
    m_Initialized = false;
}

void SpotLightShadowMap::DestroyResources() {
    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    for (u32 i = 0; i < MAX_LIGHTS; ++i) {
        if (m_Framebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, m_Framebuffers[i], nullptr);
            m_Framebuffers[i] = VK_NULL_HANDLE;
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
    for (u32 i = 0; i < MAX_LIGHTS; ++i) {
        if (m_LayerViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_LayerViews[i], nullptr);
            m_LayerViews[i] = VK_NULL_HANDLE;
        }
    }
    if (m_ArrayView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_ArrayView, nullptr);
        m_ArrayView = VK_NULL_HANDLE;
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

bool SpotLightShadowMap::CreateDepthResources() {
    VkDevice device = m_Context->GetDevice();
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Resolution;
    imageInfo.extent.height = m_Resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = MAX_LIGHTS;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create spot light shadow depth image");
        return false;
    }

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
        ENJIN_LOG_ERROR(Renderer, "Failed to find memory type for spot light shadow map");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate spot light shadow map memory");
        return false;
    }
    vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0);

    // Create 2D_ARRAY view for shader sampling
    VkImageViewCreateInfo arrayViewInfo{};
    arrayViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    arrayViewInfo.image = m_DepthImage;
    arrayViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    arrayViewInfo.format = VK_FORMAT_D32_SFLOAT;
    arrayViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    arrayViewInfo.subresourceRange.baseMipLevel = 0;
    arrayViewInfo.subresourceRange.levelCount = 1;
    arrayViewInfo.subresourceRange.baseArrayLayer = 0;
    arrayViewInfo.subresourceRange.layerCount = MAX_LIGHTS;

    if (vkCreateImageView(device, &arrayViewInfo, nullptr, &m_ArrayView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create spot light shadow array view");
        return false;
    }

    // Create per-light 2D views for framebuffer attachments
    for (u32 i = 0; i < MAX_LIGHTS; ++i) {
        VkImageViewCreateInfo layerViewInfo{};
        layerViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        layerViewInfo.image = m_DepthImage;
        layerViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        layerViewInfo.format = VK_FORMAT_D32_SFLOAT;
        layerViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        layerViewInfo.subresourceRange.baseMipLevel = 0;
        layerViewInfo.subresourceRange.levelCount = 1;
        layerViewInfo.subresourceRange.baseArrayLayer = i;
        layerViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &layerViewInfo, nullptr, &m_LayerViews[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create spot light shadow layer view %u", i);
            return false;
        }
    }

    return true;
}

bool SpotLightShadowMap::CreateRenderPass() {
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
        ENJIN_LOG_ERROR(Renderer, "Failed to create spot light shadow render pass");
        return false;
    }
    return true;
}

bool SpotLightShadowMap::CreateFramebuffers() {
    for (u32 i = 0; i < MAX_LIGHTS; ++i) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_RenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &m_LayerViews[i];
        fbInfo.width = m_Resolution;
        fbInfo.height = m_Resolution;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context->GetDevice(), &fbInfo, nullptr, &m_Framebuffers[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create spot light shadow framebuffer %u", i);
            return false;
        }
    }
    return true;
}

bool SpotLightShadowMap::CreateSampler() {
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
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_ShadowSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create spot light shadow sampler");
        return false;
    }
    return true;
}

void SpotLightShadowMap::BeginPass(VkCommandBuffer commandBuffer, u32 spotIndex) {
    if (spotIndex >= MAX_LIGHTS) return;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_Framebuffers[spotIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { m_Resolution, m_Resolution };

    VkClearValue clearValue{};
    clearValue.depthStencil = { 1.0f, 0 };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(m_Resolution);
    viewport.height = static_cast<f32>(m_Resolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { m_Resolution, m_Resolution };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void SpotLightShadowMap::EndPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

Math::Matrix4 SpotLightShadowMap::ComputeViewProj(const Math::Vector3& lightPos,
                                                    const Math::Vector3& lightDir,
                                                    f32 outerConeAngle, f32 range) {
    // Build view matrix looking from light position along its direction
    Math::Vector3 dir = lightDir.Normalized();
    Math::Vector3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(dir.Dot(up)) > 0.99f) {
        up = Math::Vector3(0.0f, 0.0f, 1.0f);
    }
    Math::Matrix4 view = Math::Matrix4::LookAt(lightPos, lightPos + dir, up);

    // Custom Vulkan [0,1] depth perspective projection, no Y-flip
    f32 fov = outerConeAngle * 2.0f; // outerConeAngle is half-angle in degrees
    f32 fovRad = fov * 3.14159265f / 180.0f;
    f32 tanHalfFov = std::tan(fovRad * 0.5f);
    f32 nearP = 0.1f;
    f32 farP = range;

    Math::Matrix4 proj = Math::Matrix4::Identity();
    proj.m[0]  = 1.0f / tanHalfFov;
    proj.m[5]  = 1.0f / tanHalfFov;  // No Y-flip for shadow maps
    proj.m[10] = farP / (nearP - farP);
    proj.m[11] = -1.0f;
    proj.m[14] = -(farP * nearP) / (farP - nearP);
    proj.m[15] = 0.0f;

    return proj * view;
}

} // namespace Renderer
} // namespace Enjin
