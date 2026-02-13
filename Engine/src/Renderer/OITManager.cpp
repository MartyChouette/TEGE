#include "Enjin/Renderer/OITManager.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

OITManager::~OITManager() {
    if (m_Initialized) {
        Shutdown();
    }
}

bool OITManager::Initialize(VulkanContext* context, u32 width, u32 height) {
    if (!context) return false;
    m_Context = context;
    m_Width = width;
    m_Height = height;

    if (!CreateTextures(width, height)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create OIT textures");
        return false;
    }

    m_Initialized = true;
    return true;
}

void OITManager::Shutdown() {
    DestroyTextures();
    m_Initialized = false;
    m_Context = nullptr;
}

void OITManager::Resize(u32 width, u32 height) {
    if (!m_Initialized) return;
    if (width == m_Width && height == m_Height) return;

    DestroyTextures();
    CreateTextures(width, height);
    m_Width = width;
    m_Height = height;
}

void OITManager::BeginTransparentPass(VkCommandBuffer /*cmd*/) {
    // Stub: Would transition accumulation + revealage to COLOR_ATTACHMENT_OPTIMAL,
    // clear them, and begin a render pass with additive blend states.
    // Requires compiled composite shader to be useful.
}

void OITManager::EndTransparentPass(VkCommandBuffer /*cmd*/) {
    // Stub: Would transition images to SHADER_READ_ONLY_OPTIMAL for composite pass.
}

void OITManager::CompositePass(VkCommandBuffer /*cmd*/) {
    // Stub: Would bind composite shader, sample accumulation + revealage,
    // and blend over the opaque framebuffer using:
    //   color = accumulation.rgb / max(accumulation.a, 1e-5)
    //   alpha = 1.0 - revealage
    // Requires compiled SPIR-V composite shader.
}

static VkImageSubresourceRange MakeSubresourceRange() {
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    return range;
}

bool OITManager::CreateTextures(u32 width, u32 height) {
    if (!m_Context || width == 0 || height == 0) return false;

    VkDevice device = m_Context->GetDevice();

    // --- Accumulation texture (RGBA16F) ---
    VkImageCreateInfo accumImageInfo{};
    accumImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    accumImageInfo.imageType = VK_IMAGE_TYPE_2D;
    accumImageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    accumImageInfo.extent = { width, height, 1 };
    accumImageInfo.mipLevels = 1;
    accumImageInfo.arrayLayers = 1;
    accumImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    accumImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    accumImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    accumImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &accumImageInfo, nullptr, &m_AccumulationImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements accumMemReqs;
    vkGetImageMemoryRequirements(device, m_AccumulationImage, &accumMemReqs);

    VkMemoryAllocateInfo accumAllocInfo{};
    accumAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    accumAllocInfo.allocationSize = accumMemReqs.size;
    accumAllocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        accumMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &accumAllocInfo, nullptr, &m_AccumulationMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(device, m_AccumulationImage, m_AccumulationMemory, 0);

    VkImageViewCreateInfo accumViewInfo{};
    accumViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    accumViewInfo.image = m_AccumulationImage;
    accumViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    accumViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    accumViewInfo.subresourceRange = MakeSubresourceRange();

    if (vkCreateImageView(device, &accumViewInfo, nullptr, &m_AccumulationView) != VK_SUCCESS) {
        return false;
    }

    // --- Revealage texture (R8) ---
    VkImageCreateInfo revealImageInfo{};
    revealImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    revealImageInfo.imageType = VK_IMAGE_TYPE_2D;
    revealImageInfo.format = VK_FORMAT_R8_UNORM;
    revealImageInfo.extent = { width, height, 1 };
    revealImageInfo.mipLevels = 1;
    revealImageInfo.arrayLayers = 1;
    revealImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    revealImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    revealImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    revealImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &revealImageInfo, nullptr, &m_RevealageImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements revealMemReqs;
    vkGetImageMemoryRequirements(device, m_RevealageImage, &revealMemReqs);

    VkMemoryAllocateInfo revealAllocInfo{};
    revealAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    revealAllocInfo.allocationSize = revealMemReqs.size;
    revealAllocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        revealMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &revealAllocInfo, nullptr, &m_RevealageMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(device, m_RevealageImage, m_RevealageMemory, 0);

    VkImageViewCreateInfo revealViewInfo{};
    revealViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    revealViewInfo.image = m_RevealageImage;
    revealViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    revealViewInfo.format = VK_FORMAT_R8_UNORM;
    revealViewInfo.subresourceRange = MakeSubresourceRange();

    if (vkCreateImageView(device, &revealViewInfo, nullptr, &m_RevealageView) != VK_SUCCESS) {
        return false;
    }

    return true;
}

void OITManager::DestroyTextures() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_AccumulationView) { vkDestroyImageView(device, m_AccumulationView, nullptr); m_AccumulationView = VK_NULL_HANDLE; }
    if (m_AccumulationImage) { vkDestroyImage(device, m_AccumulationImage, nullptr); m_AccumulationImage = VK_NULL_HANDLE; }
    if (m_AccumulationMemory) { vkFreeMemory(device, m_AccumulationMemory, nullptr); m_AccumulationMemory = VK_NULL_HANDLE; }

    if (m_RevealageView) { vkDestroyImageView(device, m_RevealageView, nullptr); m_RevealageView = VK_NULL_HANDLE; }
    if (m_RevealageImage) { vkDestroyImage(device, m_RevealageImage, nullptr); m_RevealageImage = VK_NULL_HANDLE; }
    if (m_RevealageMemory) { vkFreeMemory(device, m_RevealageMemory, nullptr); m_RevealageMemory = VK_NULL_HANDLE; }
}

} // namespace Renderer
} // namespace Enjin
