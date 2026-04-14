#include "Enjin/Renderer/RayTracing/AdaptiveRayBudget.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

AdaptiveRayBudget::AdaptiveRayBudget(VulkanContext* context) : m_Context(context) {}
AdaptiveRayBudget::~AdaptiveRayBudget() { Shutdown(); }

bool AdaptiveRayBudget::Initialize(u32 width, u32 height) {
    m_Width = width;
    m_Height = height;
    CreateImages();
    if (m_VarianceImage == VK_NULL_HANDLE || m_BudgetImage == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Renderer, "AdaptiveRayBudget: Failed to create images");
        return false;
    }
    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "AdaptiveRayBudget initialized (%ux%u)", width, height);
    return true;
}

void AdaptiveRayBudget::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    DestroyImages();
    m_Width = width;
    m_Height = height;
    CreateImages();
}

void AdaptiveRayBudget::DispatchVarianceEstimate(VkCommandBuffer cmd, VkImageView currentShadowView,
                                                  VkImageView prevShadowView) {
    if (!m_Initialized || !m_Config.enabled) return;
    // Compute pipeline will be wired when shaders are compiled and embedded into RTShaderData.h.
    // The variance map defaults to zero (uniform minimum ray budget) until then.
    (void)cmd; (void)currentShadowView; (void)prevShadowView;
}

void AdaptiveRayBudget::DispatchBudgetAllocation(VkCommandBuffer cmd, VkImageView depthView,
                                                  VkImageView normalView, VkImageView staleMaskView) {
    if (!m_Initialized || !m_Config.enabled) return;
    (void)cmd; (void)depthView; (void)normalView; (void)staleMaskView;
}

void AdaptiveRayBudget::CreateImages() {
    VkDevice device = m_Context->GetDevice();

    auto createImage = [&](VkFormat format, VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = format;
        imgInfo.extent = { m_Width, m_Height, 1 };
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &imgInfo, nullptr, &image) != VK_SUCCESS) return;

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyImage(device, image, nullptr);
            image = VK_NULL_HANDLE;
            return;
        }
        vkBindImageMemory(device, image, memory, 0);
        m_Context->TrackAllocation(static_cast<usize>(memReqs.size));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
            vkFreeMemory(device, memory, nullptr);
            vkDestroyImage(device, image, nullptr);
            image = VK_NULL_HANDLE;
            memory = VK_NULL_HANDLE;
        }
    };

    createImage(VK_FORMAT_R16_SFLOAT, m_VarianceImage, m_VarianceMemory, m_VarianceView);
    createImage(VK_FORMAT_R8_UINT, m_BudgetImage, m_BudgetMemory, m_BudgetView);
}

void AdaptiveRayBudget::DestroyImages() {
    VkDevice device = m_Context->GetDevice();
    auto destroy = [&](VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
        if (view) { vkDestroyImageView(device, view, nullptr); view = VK_NULL_HANDLE; }
        if (img) { vkDestroyImage(device, img, nullptr); img = VK_NULL_HANDLE; }
        if (mem) { vkFreeMemory(device, mem, nullptr); mem = VK_NULL_HANDLE; }
    };
    destroy(m_VarianceImage, m_VarianceMemory, m_VarianceView);
    destroy(m_BudgetImage, m_BudgetMemory, m_BudgetView);
}

void AdaptiveRayBudget::Shutdown() {
    if (!m_Initialized) return;
    DestroyImages();
    if (m_Sampler) {
        vkDestroySampler(m_Context->GetDevice(), m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
    m_Initialized = false;
    ENJIN_LOG_INFO(Renderer, "AdaptiveRayBudget shut down");
}

} // namespace Renderer
} // namespace Enjin
