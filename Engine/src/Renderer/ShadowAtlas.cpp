#include "Enjin/Renderer/ShadowAtlas.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <array>

#if !ENJIN_RENDERER_WEBGPU

namespace Enjin {
namespace Renderer {

// --- ShadowTile UV helpers ---

Math::Vector2 ShadowTile::uvOffset() const {
    // Note: atlas size comes from the atlas, but we compute relative UV here
    // Caller must divide by atlas size. We store pixel coords.
    return Math::Vector2(static_cast<f32>(x), static_cast<f32>(y));
}

Math::Vector2 ShadowTile::uvScale() const {
    return Math::Vector2(static_cast<f32>(size), static_cast<f32>(size));
}

// --- ShadowAtlas ---

ShadowAtlas::ShadowAtlas(VulkanContext* context)
    : m_Context(context) {}

ShadowAtlas::~ShadowAtlas() {
    Shutdown();
}

bool ShadowAtlas::Initialize(const ShadowAtlasConfig& config) {
    if (m_Initialized) return true;
    m_Config = config;

    if (!CreateRenderPass()) return false;
    if (!CreateDepthImage()) return false;
    if (!CreateSampler()) return false;

    RebuildSlotGrid();
    m_Initialized = true;

    ENJIN_LOG_INFO(Renderer, "ShadowAtlas initialized: %ux%u, max %u tiles (min %u, max %u px)",
                   m_Config.atlasSize, m_Config.atlasSize, m_Config.maxTiles,
                   m_Config.minTileSize, m_Config.maxTileSize);
    return true;
}

void ShadowAtlas::Shutdown() {
    if (!m_Initialized) return;
    DestroyResources();
    m_Initialized = false;
}

// --- Tile allocation ---

void ShadowAtlas::ResetAllocations() {
    for (auto& slot : m_Slots) slot.used = false;
    m_AllocatedCount = 0;
}

ShadowTile ShadowAtlas::AllocateTile(u32 requestedSize) {
    ShadowTile result;

    // Clamp to valid range and round up to power of two
    u32 size = std::max(requestedSize, m_Config.minTileSize);
    size = std::min(size, m_Config.maxTileSize);
    // Round up to nearest power of 2
    u32 pot = 1;
    while (pot < size) pot <<= 1;
    size = std::min(pot, m_Config.maxTileSize);

    // Find first unused slot that fits
    for (auto& slot : m_Slots) {
        if (!slot.used && slot.size >= size) {
            slot.used = true;
            result.x = slot.x;
            result.y = slot.y;
            result.size = size; // Use requested (clamped) size, not slot size
            result.allocated = true;
            m_AllocatedCount++;
            return result;
        }
    }

    return result; // allocated = false
}

void ShadowAtlas::FreeTile(const ShadowTile& tile) {
    for (auto& slot : m_Slots) {
        if (slot.x == tile.x && slot.y == tile.y && slot.used) {
            slot.used = false;
            m_AllocatedCount--;
            return;
        }
    }
}

// --- Per-tile rendering ---

void ShadowAtlas::BeginTilePass(VkCommandBuffer cmd, const ShadowTile& tile) {
    VkRenderPassBeginInfo rpBI{};
    rpBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBI.renderPass = m_RenderPass;
    rpBI.framebuffer = m_Framebuffer;
    rpBI.renderArea.offset = { static_cast<i32>(tile.x), static_cast<i32>(tile.y) };
    rpBI.renderArea.extent = { tile.size, tile.size };

    VkClearValue clearValue{};
    clearValue.depthStencil = { 1.0f, 0 };
    rpBI.clearValueCount = 1;
    rpBI.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor to this tile's region
    VkViewport viewport{};
    viewport.x = static_cast<f32>(tile.x);
    viewport.y = static_cast<f32>(tile.y);
    viewport.width = static_cast<f32>(tile.size);
    viewport.height = static_cast<f32>(tile.size);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { static_cast<i32>(tile.x), static_cast<i32>(tile.y) };
    scissor.extent = { tile.size, tile.size };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void ShadowAtlas::EndTilePass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

// --- Resize ---

void ShadowAtlas::SetAtlasSize(u32 size) {
    if (size == m_Config.atlasSize) return;
    m_Config.atlasSize = size;
    if (m_Initialized) {
        DestroyResources();
        CreateDepthImage();
        CreateSampler();
        RebuildSlotGrid();
    }
}

// --- Vulkan resource creation ---

bool ShadowAtlas::CreateDepthImage() {
    VkDevice device = m_Context->GetDevice();

    // Single 2D depth image (not layered)
    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.extent = { m_Config.atlasSize, m_Config.atlasSize, 1 };
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.format = VK_FORMAT_D32_SFLOAT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageCI, nullptr, &m_DepthImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ShadowAtlas: failed to create depth image");
        return false;
    }

    // Allocate device-local memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_DepthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ShadowAtlas: failed to allocate depth memory");
        return false;
    }
    vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0);

    // Create 2D view for shader sampling
    VkImageViewCreateInfo viewCI{};
    viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image = m_DepthImage;
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = VK_FORMAT_D32_SFLOAT;
    viewCI.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(device, &viewCI, nullptr, &m_DepthView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ShadowAtlas: failed to create depth view");
        return false;
    }

    // Create single framebuffer covering entire atlas
    VkFramebufferCreateInfo fbCI{};
    fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCI.renderPass = m_RenderPass;
    fbCI.attachmentCount = 1;
    fbCI.pAttachments = &m_DepthView;
    fbCI.width = m_Config.atlasSize;
    fbCI.height = m_Config.atlasSize;
    fbCI.layers = 1;

    if (vkCreateFramebuffer(device, &fbCI, nullptr, &m_Framebuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ShadowAtlas: failed to create framebuffer");
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "ShadowAtlas: created %ux%u depth image (%.1f MB)",
                   m_Config.atlasSize, m_Config.atlasSize,
                   static_cast<f32>(memReqs.size) / (1024.0f * 1024.0f));
    return true;
}

bool ShadowAtlas::CreateRenderPass() {
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

    std::array<VkSubpassDependency, 2> deps{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpCI{};
    rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.attachmentCount = 1;
    rpCI.pAttachments = &depthAttachment;
    rpCI.subpassCount = 1;
    rpCI.pSubpasses = &subpass;
    rpCI.dependencyCount = static_cast<u32>(deps.size());
    rpCI.pDependencies = deps.data();

    if (vkCreateRenderPass(m_Context->GetDevice(), &rpCI, nullptr, &m_RenderPass) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ShadowAtlas: failed to create render pass");
        return false;
    }
    return true;
}

bool ShadowAtlas::CreateSampler() {
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerCI.mipLodBias = 0.0f;
    samplerCI.maxAnisotropy = 1.0f;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerCI.compareEnable = VK_TRUE;
    samplerCI.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    if (vkCreateSampler(m_Context->GetDevice(), &samplerCI, nullptr, &m_ShadowSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "ShadowAtlas: failed to create sampler");
        return false;
    }
    return true;
}

void ShadowAtlas::DestroyResources() {
    VkDevice device = m_Context->GetDevice();
    if (m_Framebuffer) { vkDestroyFramebuffer(device, m_Framebuffer, nullptr); m_Framebuffer = VK_NULL_HANDLE; }
    if (m_DepthView) { vkDestroyImageView(device, m_DepthView, nullptr); m_DepthView = VK_NULL_HANDLE; }
    if (m_DepthImage) { vkDestroyImage(device, m_DepthImage, nullptr); m_DepthImage = VK_NULL_HANDLE; }
    if (m_DepthMemory) { vkFreeMemory(device, m_DepthMemory, nullptr); m_DepthMemory = VK_NULL_HANDLE; }
    if (m_ShadowSampler) { vkDestroySampler(device, m_ShadowSampler, nullptr); m_ShadowSampler = VK_NULL_HANDLE; }
    if (m_RenderPass) { vkDestroyRenderPass(device, m_RenderPass, nullptr); m_RenderPass = VK_NULL_HANDLE; }
}

// --- Slot grid ---

void ShadowAtlas::RebuildSlotGrid() {
    m_Slots.clear();
    m_AllocatedCount = 0;

    // Simple uniform grid: divide atlas into tiles of maxTileSize.
    // For a 4096 atlas with 1024 max tiles: 4x4 grid = 16 slots.
    // For a 8192 atlas with 2048 max tiles: 4x4 grid = 16 slots.
    u32 tileSize = m_Config.maxTileSize;
    u32 tilesPerRow = m_Config.atlasSize / tileSize;
    if (tilesPerRow == 0) tilesPerRow = 1;

    u32 totalSlots = tilesPerRow * tilesPerRow;
    if (totalSlots > m_Config.maxTiles) totalSlots = m_Config.maxTiles;

    for (u32 i = 0; i < totalSlots; ++i) {
        u32 col = i % tilesPerRow;
        u32 row = i / tilesPerRow;
        if (row >= tilesPerRow) break;

        TileSlot slot;
        slot.x = col * tileSize;
        slot.y = row * tileSize;
        slot.size = tileSize;
        slot.used = false;
        m_Slots.push_back(slot);
    }

    ENJIN_LOG_INFO(Renderer, "ShadowAtlas: %zu tile slots (%ux%u grid, %u px each)",
                   m_Slots.size(), tilesPerRow, tilesPerRow, tileSize);
}

} // namespace Renderer
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
