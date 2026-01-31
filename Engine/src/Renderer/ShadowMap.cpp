#include "Enjin/Renderer/ShadowMap.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <array>

namespace Enjin {
namespace Renderer {

ShadowMap::ShadowMap(VulkanContext* context)
    : m_Context(context) {
}

ShadowMap::~ShadowMap() {
    Shutdown();
}

bool ShadowMap::Initialize(const ShadowMapConfig& config) {
    if (m_Initialized) {
        return true;
    }

    m_Config = config;

    ENJIN_LOG_INFO(Renderer, "Initializing shadow map (%ux%u)...", config.resolution, config.resolution);

    if (!CreateDepthResources()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map depth resources");
        return false;
    }

    if (!CreateRenderPass()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map render pass");
        return false;
    }

    if (!CreateFramebuffer()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map framebuffer");
        return false;
    }

    if (!CreateSampler()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow sampler");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "Shadow map initialized");
    return true;
}

void ShadowMap::Shutdown() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(device);

    if (m_Framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
        m_Framebuffer = VK_NULL_HANDLE;
    }

    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    if (m_ShadowSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_ShadowSampler, nullptr);
        m_ShadowSampler = VK_NULL_HANDLE;
    }

    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
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

    // Create depth image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_Config.resolution;
    imageInfo.extent.height = m_Config.resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
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

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_DepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow map image view");
        return false;
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

bool ShadowMap::CreateFramebuffer() {
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_RenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &m_DepthImageView;
    fbInfo.width = m_Config.resolution;
    fbInfo.height = m_Config.resolution;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(m_Context->GetDevice(), &fbInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow framebuffer");
        return false;
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

void ShadowMap::BeginShadowPass(VkCommandBuffer commandBuffer) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_Framebuffer;
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

void ShadowMap::EndShadowPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

void ShadowMap::DestroyDepthResources() {
    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(device);

    if (m_Framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
        m_Framebuffer = VK_NULL_HANDLE;
    }
    if (m_DepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
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

    // Recreate depth resources and framebuffer at new size
    DestroyDepthResources();
    if (!CreateDepthResources()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate shadow map at resolution %u", size);
        return;
    }
    if (!CreateFramebuffer()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate shadow framebuffer at resolution %u", size);
    }
}

void ShadowMap::UpdateFrustum(const Math::Matrix4& viewProj, const Math::Vector3& lightDir) {
    // Compute inverse view-projection to get camera frustum corners in world space
    Math::Matrix4 invViewProj = viewProj.Inverse();

    // 8 NDC corners of the camera frustum
    Math::Vector3 corners[8];
    int idx = 0;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                Math::Vector4 ndc(
                    x * 2.0f - 1.0f,
                    y * 2.0f - 1.0f,
                    z * 1.0f,  // Vulkan depth range [0, 1]
                    1.0f
                );
                Math::Vector4 world = invViewProj * ndc;
                corners[idx] = Math::Vector3(world.x / world.w, world.y / world.w, world.z / world.w);
                idx++;
            }
        }
    }

    // Compute frustum center
    Math::Vector3 center(0.0f);
    for (int i = 0; i < 8; ++i) {
        center = center + corners[i];
    }
    center = center * (1.0f / 8.0f);

    // Build light view matrix looking from center along light direction
    Math::Vector3 lightUp(0.0f, 1.0f, 0.0f);
    if (std::abs(lightDir.Dot(lightUp)) > 0.99f) {
        lightUp = Math::Vector3(0.0f, 0.0f, 1.0f);
    }
    Math::Matrix4 lightView = Math::Matrix4::LookAt(center + lightDir * 50.0f, center, lightUp);

    // Transform frustum corners into light space and compute AABB
    f32 minX = 1e9f, maxX = -1e9f;
    f32 minY = 1e9f, maxY = -1e9f;
    f32 minZ = 1e9f, maxZ = -1e9f;

    for (int i = 0; i < 8; ++i) {
        Math::Vector4 lightSpaceCorner = lightView * Math::Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f);
        if (lightSpaceCorner.x < minX) minX = lightSpaceCorner.x;
        if (lightSpaceCorner.x > maxX) maxX = lightSpaceCorner.x;
        if (lightSpaceCorner.y < minY) minY = lightSpaceCorner.y;
        if (lightSpaceCorner.y > maxY) maxY = lightSpaceCorner.y;
        if (lightSpaceCorner.z < minZ) minZ = lightSpaceCorner.z;
        if (lightSpaceCorner.z > maxZ) maxZ = lightSpaceCorner.z;
    }

    // Add some padding to avoid shadow popping at edges
    f32 padding = 5.0f;
    minX -= padding; maxX += padding;
    minY -= padding; maxY += padding;
    minZ -= padding; maxZ += padding;

    // Compute ortho size as half the max dimension
    f32 sizeX = (maxX - minX) * 0.5f;
    f32 sizeY = (maxY - minY) * 0.5f;
    m_FittedOrthoSize = std::max(sizeX, sizeY);
    m_FittedCenter = center;

    // Update light position based on fitted frustum
    m_LightDirection = lightDir.Normalized();
    m_LightPosition = center + m_LightDirection * 50.0f;

    // Update near/far for the fitted frustum
    m_Config.nearPlane = 0.1f;
    m_Config.farPlane = maxZ - minZ + 100.0f;
}

void ShadowMap::SetLightDirection(const Math::Vector3& direction) {
    m_LightDirection = direction.Normalized();
}

void ShadowMap::SetLightPosition(const Math::Vector3& position) {
    m_LightPosition = position;
}

Math::Matrix4 ShadowMap::GetLightViewMatrix() const {
    // Look from position in the direction of light
    Math::Vector3 target = m_LightPosition - m_LightDirection * 10.0f;

    // Calculate up vector (avoid parallel with direction)
    Math::Vector3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(m_LightDirection.Dot(up)) > 0.99f) {
        up = Math::Vector3(0.0f, 0.0f, 1.0f);
    }

    return Math::Matrix4::LookAt(m_LightPosition, target, up);
}

Math::Matrix4 ShadowMap::GetLightProjectionMatrix() const {
    // Use fitted ortho size if auto-fit is enabled, otherwise use config value
    f32 size = m_AutoFit ? m_FittedOrthoSize : m_Config.orthoSize;
    if (size < 1.0f) size = m_Config.orthoSize; // Fallback if not yet fitted
    return Math::Matrix4::Orthographic(-size, size, -size, size, m_Config.nearPlane, m_Config.farPlane);
}

Math::Matrix4 ShadowMap::GetLightSpaceMatrix() const {
    return GetLightProjectionMatrix() * GetLightViewMatrix();
}

} // namespace Renderer
} // namespace Enjin
