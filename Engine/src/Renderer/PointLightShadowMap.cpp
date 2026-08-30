#include "Enjin/Renderer/PointLightShadowMap.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <array>
#include <cmath>

namespace Enjin {
namespace Renderer {

PointLightShadowMap::PointLightShadowMap(VulkanContext* context)
    : m_Context(context) {
}

PointLightShadowMap::~PointLightShadowMap() {
    Shutdown();
}

bool PointLightShadowMap::Initialize(u32 resolution) {
    if (m_Initialized) return true;

    m_Resolution = resolution;

    ENJIN_LOG_INFO(Renderer, "Initializing point light shadow map (%ux%u, %u lights x %u faces)...",
                   resolution, resolution, MAX_LIGHTS, FACE_COUNT);

    if (!CreateDepthResources()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffers()) return false;
    if (!CreateSampler()) return false;

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Point light shadow map initialized");
    return true;
}

void PointLightShadowMap::Shutdown() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    m_Context->WaitForGPU();
    DestroyResources();
    m_Initialized = false;
}

void PointLightShadowMap::DestroyResources() {
    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    for (u32 i = 0; i < TOTAL_LAYERS; ++i) {
        if (m_FaceFramebuffers[i] != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, m_FaceFramebuffers[i], nullptr);
            m_FaceFramebuffers[i] = VK_NULL_HANDLE;
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
    for (u32 i = 0; i < TOTAL_LAYERS; ++i) {
        if (m_FaceViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_FaceViews[i], nullptr);
            m_FaceViews[i] = VK_NULL_HANDLE;
        }
    }
    if (m_CubeArrayView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_CubeArrayView, nullptr);
        m_CubeArrayView = VK_NULL_HANDLE;
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

bool PointLightShadowMap::CreateDepthResources() {
    VkDevice device = m_Context->GetDevice();
    VkPhysicalDevice physicalDevice = m_Context->GetPhysicalDevice();

    // Create cube-compatible depth image with 24 layers (4 lights * 6 faces)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Resolution;
    imageInfo.extent.height = m_Resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = TOTAL_LAYERS;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // init-time clear to far depth (phantom-shadow fix)
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create point light shadow depth image");
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
        ENJIN_LOG_ERROR(Renderer, "Failed to find memory type for point light shadow map");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate point light shadow map memory");
        return false;
    }
    vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0);

    // Create CUBE_ARRAY view for shader sampling (all 24 layers)
    VkImageViewCreateInfo cubeArrayViewInfo{};
    cubeArrayViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    cubeArrayViewInfo.image = m_DepthImage;
    cubeArrayViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    cubeArrayViewInfo.format = VK_FORMAT_D32_SFLOAT;
    cubeArrayViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    cubeArrayViewInfo.subresourceRange.baseMipLevel = 0;
    cubeArrayViewInfo.subresourceRange.levelCount = 1;
    cubeArrayViewInfo.subresourceRange.baseArrayLayer = 0;
    cubeArrayViewInfo.subresourceRange.layerCount = TOTAL_LAYERS;

    if (vkCreateImageView(device, &cubeArrayViewInfo, nullptr, &m_CubeArrayView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create point light shadow cube array view");
        return false;
    }

    // Create per-face 2D views for framebuffer attachments
    for (u32 i = 0; i < TOTAL_LAYERS; ++i) {
        VkImageViewCreateInfo faceViewInfo{};
        faceViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        faceViewInfo.image = m_DepthImage;
        faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        faceViewInfo.format = VK_FORMAT_D32_SFLOAT;
        faceViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        faceViewInfo.subresourceRange.baseMipLevel = 0;
        faceViewInfo.subresourceRange.levelCount = 1;
        faceViewInfo.subresourceRange.baseArrayLayer = i;
        faceViewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &faceViewInfo, nullptr, &m_FaceViews[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create point light shadow face view %u", i);
            return false;
        }
    }

    return true;
}

bool PointLightShadowMap::CreateRenderPass() {
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
        ENJIN_LOG_ERROR(Renderer, "Failed to create point light shadow render pass");
        return false;
    }
    return true;
}

bool PointLightShadowMap::CreateFramebuffers() {
    for (u32 i = 0; i < TOTAL_LAYERS; ++i) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_RenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &m_FaceViews[i];
        fbInfo.width = m_Resolution;
        fbInfo.height = m_Resolution;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context->GetDevice(), &fbInfo, nullptr, &m_FaceFramebuffers[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create point light shadow framebuffer %u", i);
            return false;
        }
    }
    return true;
}

bool PointLightShadowMap::CreateSampler() {
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
        ENJIN_LOG_ERROR(Renderer, "Failed to create point light shadow sampler");
        return false;
    }
    return true;
}

void PointLightShadowMap::BeginFacePass(VkCommandBuffer commandBuffer, u32 lightIndex, u32 faceIndex) {
    u32 layerIndex = lightIndex * FACE_COUNT + faceIndex;
    if (layerIndex >= TOTAL_LAYERS) return;

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_FaceFramebuffers[layerIndex];
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

void PointLightShadowMap::EndFacePass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

Math::Matrix4 PointLightShadowMap::ComputeFaceViewProj(const Math::Vector3& lightPos, f32 range,
                                                         u32 faceIndex) {
    // Cubemap face directions and up vectors (OpenGL/Vulkan cubemap convention)
    // +X, -X, +Y, -Y, +Z, -Z
    static const Math::Vector3 faceDirs[6] = {
        Math::Vector3( 1.0f,  0.0f,  0.0f),  // +X
        Math::Vector3(-1.0f,  0.0f,  0.0f),  // -X
        Math::Vector3( 0.0f,  1.0f,  0.0f),  // +Y
        Math::Vector3( 0.0f, -1.0f,  0.0f),  // -Y
        Math::Vector3( 0.0f,  0.0f,  1.0f),  // +Z
        Math::Vector3( 0.0f,  0.0f, -1.0f),  // -Z
    };
    static const Math::Vector3 faceUps[6] = {
        Math::Vector3( 0.0f, -1.0f,  0.0f),  // +X
        Math::Vector3( 0.0f, -1.0f,  0.0f),  // -X
        Math::Vector3( 0.0f,  0.0f,  1.0f),  // +Y
        Math::Vector3( 0.0f,  0.0f, -1.0f),  // -Y
        Math::Vector3( 0.0f, -1.0f,  0.0f),  // +Z
        Math::Vector3( 0.0f, -1.0f,  0.0f),  // -Z
    };

    if (faceIndex >= 6) faceIndex = 0;

    Math::Vector3 target = lightPos + faceDirs[faceIndex];
    Math::Matrix4 view = Math::Matrix4::LookAt(lightPos, target, faceUps[faceIndex]);

    // Custom Vulkan [0,1] depth perspective projection, 90 degree FOV, aspect 1:1, no Y-flip
    f32 nearP = 0.1f;
    f32 farP = range;
    Math::Matrix4 proj = Math::Matrix4::Identity();
    // tan(45°) = 1.0, so focal length = 1.0 for 90° FOV
    proj.m[0]  = 1.0f;
    proj.m[5]  = 1.0f;  // No Y-flip for shadow maps
    proj.m[10] = farP / (nearP - farP);
    proj.m[11] = -1.0f;
    proj.m[14] = -(farP * nearP) / (farP - nearP);
    proj.m[15] = 0.0f;

    return proj * view;
}

} // namespace Renderer
} // namespace Enjin
