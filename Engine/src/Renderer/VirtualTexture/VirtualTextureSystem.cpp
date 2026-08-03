#include "Enjin/Renderer/VirtualTexture/VirtualTextureSystem.h"
#include "Enjin/Renderer/VirtualTexture/PageCache.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <cstring>
#include <algorithm>
#include <cmath>

// stb_image for page loading — declarations only. The implementation lives in
// VulkanImage.cpp's TU with EXTERNAL linkage (no STB_IMAGE_STATIC there), so we
// link against those symbols. Defining STB_IMAGE_IMPLEMENTATION here too emitted
// duplicate stbi_* symbols across both objects (LNK4006).
#include "stb_image.h"

namespace Enjin {
namespace Renderer {

VirtualTextureSystem::VirtualTextureSystem(VulkanContext* context)
    : m_Context(context) {}

VirtualTextureSystem::~VirtualTextureSystem() {
    Shutdown();
}

bool VirtualTextureSystem::Initialize(u32 screenWidth, u32 screenHeight) {
    if (m_Initialized) return true;

    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    if (!CreatePhysicalAtlas()) {
        ENJIN_LOG_ERROR(Renderer, "VirtualTexture: Failed to create physical atlas");
        return false;
    }

    if (!CreateIndirectionTexture()) {
        ENJIN_LOG_ERROR(Renderer, "VirtualTexture: Failed to create indirection texture");
        return false;
    }

    if (!CreateFeedbackResources()) {
        ENJIN_LOG_WARN(Renderer, "VirtualTexture: Feedback resources failed, basic mode only");
    }

    if (!CreateStagingBuffer()) {
        ENJIN_LOG_WARN(Renderer, "VirtualTexture: Staging buffer creation failed");
    }

    m_PageCache = std::make_unique<PageCache>(VTConfig::ATLAS_PAGES);

    // Launch background streaming thread
    m_StreamingActive = true;
    m_StreamingThread = std::thread(&VirtualTextureSystem::StreamingThreadFunc, this);

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "VirtualTexture initialized: %ux%u atlas, %u page slots, %ux%u pages",
        VTConfig::ATLAS_SIZE, VTConfig::ATLAS_SIZE, VTConfig::ATLAS_PAGES,
        VTConfig::PAGE_SIZE, VTConfig::PAGE_SIZE);
    return true;
}

void VirtualTextureSystem::Shutdown() {
    // Signal streaming thread to stop and wake it
    m_StreamingActive = false;
    m_StreamingCV.notify_all();
    if (m_StreamingThread.joinable()) {
        m_StreamingThread.join();
    }

    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    auto destroyImage = [&](VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
        if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); view = VK_NULL_HANDLE; }
        if (img != VK_NULL_HANDLE) { vkDestroyImage(device, img, nullptr); img = VK_NULL_HANDLE; }
        if (mem != VK_NULL_HANDLE) { vkFreeMemory(device, mem, nullptr); mem = VK_NULL_HANDLE; }
    };

    destroyImage(m_Atlas, m_AtlasMemory, m_AtlasView);
    destroyImage(m_IndirectionTexture, m_IndirectionMemory, m_IndirectionView);
    destroyImage(m_FeedbackImage, m_FeedbackMemory, m_FeedbackView);

    if (m_AtlasSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_AtlasSampler, nullptr);
        m_AtlasSampler = VK_NULL_HANDLE;
    }

    m_StagingBuffer.reset();
    m_FeedbackReadbackBuffer.reset();
    m_PageCache.reset();
    m_TextureRegistry.clear();
    m_Initialized = false;
}

bool VirtualTextureSystem::CreatePhysicalAtlas() {
    VkDevice device = m_Context->GetDevice();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = { VTConfig::ATLAS_SIZE, VTConfig::ATLAS_SIZE, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_Atlas) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_Atlas, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProps);
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_AtlasMemory) != VK_SUCCESS) {
        vkDestroyImage(device, m_Atlas, nullptr);
        m_Atlas = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(device, m_Atlas, m_AtlasMemory, 0);

    // Create view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_Atlas;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_AtlasView) != VK_SUCCESS) {
        return false;
    }

    // Create sampler with trilinear filtering
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_AtlasSampler) != VK_SUCCESS) {
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "VirtualTexture: Physical atlas created (%u MB)",
        static_cast<u32>(memReqs.size / (1024 * 1024)));
    return true;
}

bool VirtualTextureSystem::CreateIndirectionTexture() {
    VkDevice device = m_Context->GetDevice();

    // Indirection texture: small (e.g., 512x512), maps virtual page -> physical atlas UV
    u32 indirectionSize = VTConfig::MAX_VIRTUAL_SIZE / VTConfig::PAGE_SIZE; // 512

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT; // (scaleX, scaleY, offsetX, offsetY)
    imageInfo.extent = { indirectionSize, indirectionSize, 1 };
    imageInfo.mipLevels = VTConfig::MAX_MIP_LEVELS;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_IndirectionTexture) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_IndirectionTexture, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProps);
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_IndirectionMemory) != VK_SUCCESS) {
        vkDestroyImage(device, m_IndirectionTexture, nullptr);
        m_IndirectionTexture = VK_NULL_HANDLE;
        return false;
    }
    vkBindImageMemory(device, m_IndirectionTexture, m_IndirectionMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_IndirectionTexture;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VTConfig::MAX_MIP_LEVELS, 0, 1 };
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_IndirectionView) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool VirtualTextureSystem::CreateFeedbackResources() {
    VkDevice device = m_Context->GetDevice();
    u32 fbWidth = m_ScreenWidth / VTConfig::FEEDBACK_SCALE;
    u32 fbHeight = m_ScreenHeight / VTConfig::FEEDBACK_SCALE;
    if (fbWidth == 0) fbWidth = 1;
    if (fbHeight == 0) fbHeight = 1;

    // Feedback image: R32G32_UINT (textureID | mip+pageX+pageY packed)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R32G32_UINT;
    imageInfo.extent = { fbWidth, fbHeight, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_FeedbackImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_FeedbackImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProps);
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_FeedbackMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(device, m_FeedbackImage, m_FeedbackMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_FeedbackImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32_UINT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_FeedbackView) != VK_SUCCESS) {
        return false;
    }

    // Readback buffer
    usize readbackSize = fbWidth * fbHeight * 8; // R32G32_UINT = 8 bytes/pixel
    m_FeedbackReadbackBuffer = std::make_unique<VulkanBuffer>(m_Context);
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (!m_FeedbackReadbackBuffer->Create(readbackSize, usage, true)) {
        return false;
    }

    return true;
}

bool VirtualTextureSystem::CreateStagingBuffer() {
    // Staging buffer large enough for MAX_UPLOADS_PER_FRAME pages plus indirection entries.
    // Each page is PAGE_SIZE * PAGE_SIZE * 4 bytes (RGBA8).
    // Reserve extra space at the end for indirection texture updates (16 bytes each).
    usize pageBytes = static_cast<usize>(VTConfig::PAGE_SIZE) * VTConfig::PAGE_SIZE * 4;
    usize indirectionReserve = static_cast<usize>(VTConfig::MAX_UPLOADS_PER_FRAME) * sizeof(IndirectionEntry);
    usize stagingSize = pageBytes * VTConfig::MAX_UPLOADS_PER_FRAME + indirectionReserve;

    m_StagingBuffer = std::make_unique<VulkanBuffer>(m_Context);
    // TRANSFER_SRC for copying page data to atlas images.
    // TRANSFER_DST for vkCmdUpdateBuffer when writing indirection entries.
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (!m_StagingBuffer->Create(stagingSize, usage, true)) {
        ENJIN_LOG_ERROR(Renderer, "VirtualTexture: Failed to create staging buffer (%u KB)",
            static_cast<u32>(stagingSize / 1024));
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "VirtualTexture: Staging buffer created (%u KB)",
        static_cast<u32>(stagingSize / 1024));
    return true;
}

// ---------------------------------------------------------------------------
// RegisterTexture
// ---------------------------------------------------------------------------

u32 VirtualTextureSystem::RegisterTexture(const std::string& path, u32 width, u32 height) {
    if (width == 0 || height == 0) {
        ENJIN_LOG_WARN(Renderer, "VirtualTexture: Cannot register texture with zero dimensions: %s", path.c_str());
        return 0;
    }

    u32 id = m_NextTextureID++;

    VirtualTextureInfo info;
    info.path = path;
    info.width = width;
    info.height = height;

    // Compute mip levels: number of times we can halve until page-sized
    u32 maxDim = std::max(width, height);
    info.mipLevels = 1;
    while ((maxDim >> info.mipLevels) >= VTConfig::PAGE_SIZE && info.mipLevels < VTConfig::MAX_MIP_LEVELS) {
        info.mipLevels++;
    }

    // Page counts at mip 0
    info.pagesX = (width + VTConfig::PAGE_SIZE - 1) / VTConfig::PAGE_SIZE;
    info.pagesY = (height + VTConfig::PAGE_SIZE - 1) / VTConfig::PAGE_SIZE;

    u32 mipLevels = info.mipLevels;
    u32 pagesX = info.pagesX;
    u32 pagesY = info.pagesY;
    m_TextureRegistry[id] = std::move(info);

    ENJIN_LOG_INFO(Renderer, "VirtualTexture: Registered texture %u: %s (%ux%u, %u mips, %ux%u pages)",
        id, path.c_str(), width, height, mipLevels, pagesX, pagesY);

    return id;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void VirtualTextureSystem::Update(VkCommandBuffer commandBuffer, u32 frameIndex) {
    if (!m_Initialized) return;
    m_FrameIndex = frameIndex;

    // Process pending page requests from feedback readback
    ProcessFeedback();

    // Upload ready pages to atlas
    UploadPages(commandBuffer);
}

// ---------------------------------------------------------------------------
// RenderFeedback - Transition feedback image for rendering, then copy to
// readback buffer after the pass so ProcessFeedback can read it next frame.
// The actual draw calls into the feedback render pass are issued by the
// caller (RenderSystem) using the feedback pipeline. This method handles
// the image transitions and readback copy.
// ---------------------------------------------------------------------------

void VirtualTextureSystem::RenderFeedback(VkCommandBuffer commandBuffer, const Math::Matrix4& viewProj) {
    if (!m_Initialized || m_FeedbackImage == VK_NULL_HANDLE) return;
    (void)viewProj; // viewProj is used by the caller's feedback pipeline push constants

    u32 fbWidth = m_ScreenWidth / VTConfig::FEEDBACK_SCALE;
    u32 fbHeight = m_ScreenHeight / VTConfig::FEEDBACK_SCALE;
    if (fbWidth == 0 || fbHeight == 0) return;

    // Transition feedback image: COLOR_ATTACHMENT -> TRANSFER_SRC for readback
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_FeedbackImage;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy feedback image to readback buffer
    if (m_FeedbackReadbackBuffer) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;   // Tightly packed
        region.bufferImageHeight = 0; // Tightly packed
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { fbWidth, fbHeight, 1 };

        vkCmdCopyImageToBuffer(commandBuffer,
            m_FeedbackImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_FeedbackReadbackBuffer->GetBuffer(), 1, &region);

        // Buffer barrier so the CPU can read the data
        VkBufferMemoryBarrier bufBarrier{};
        bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufBarrier.buffer = m_FeedbackReadbackBuffer->GetBuffer();
        bufBarrier.offset = 0;
        bufBarrier.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 0, nullptr, 1, &bufBarrier, 0, nullptr);
    }

    // Transition feedback image back to COLOR_ATTACHMENT for next frame
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ---------------------------------------------------------------------------
// ProcessFeedback - Read feedback buffer, deduplicate, sort by priority,
// and queue streaming requests for pages not already resident or in-flight.
// ---------------------------------------------------------------------------

void VirtualTextureSystem::ProcessFeedback() {
    if (!m_FeedbackReadbackBuffer) return;

    // Read back feedback data from previous frame
    void* mapped = m_FeedbackReadbackBuffer->Map();
    if (!mapped) return;

    u32 fbWidth = m_ScreenWidth / VTConfig::FEEDBACK_SCALE;
    u32 fbHeight = m_ScreenHeight / VTConfig::FEEDBACK_SCALE;
    if (fbWidth == 0 || fbHeight == 0) {
        m_FeedbackReadbackBuffer->Unmap();
        return;
    }

    const u32* data = static_cast<const u32*>(mapped);

    // Use a set for deduplication during feedback scan
    std::unordered_set<PageID> requestedPages;
    std::vector<PageRequest> newRequests;

    // Feedback format: R32G32_UINT per pixel
    // .r = textureID, .g = (mip << 24) | (pageY << 12) | pageX
    for (u32 y = 0; y < fbHeight; ++y) {
        for (u32 x = 0; x < fbWidth; ++x) {
            u32 idx = (y * fbWidth + x) * 2;
            u32 texID = data[idx];
            u32 packed = data[idx + 1];

            if (texID == 0 && packed == 0) continue;

            PageID pid;
            pid.textureID = texID;
            pid.mipLevel = static_cast<u16>(packed >> 24);
            pid.pageY = static_cast<u16>((packed >> 12) & 0xFFF);
            pid.pageX = static_cast<u16>(packed & 0xFFF);
            pid._pad = 0;

            // Validate texture ID exists in registry
            if (m_TextureRegistry.find(pid.textureID) == m_TextureRegistry.end()) continue;

            // Check if already resident in cache
            PhysicalPage existing;
            if (m_PageCache->Lookup(pid, existing)) {
                m_PageCache->Touch(pid, m_FrameIndex);
                continue;
            }

            // Deduplicate within this feedback frame
            if (requestedPages.count(pid) > 0) continue;
            requestedPages.insert(pid);

            // Skip pages already being streamed
            if (m_InFlightPages.count(pid) > 0) continue;

            PageRequest req;
            req.pageID = pid;
            req.priority = pid.mipLevel; // Lower mip = higher priority
            newRequests.push_back(req);
        }
    }

    m_FeedbackReadbackBuffer->Unmap();

    // Sort by priority (lower value = higher priority = process first)
    std::sort(newRequests.begin(), newRequests.end(),
        [](const PageRequest& a, const PageRequest& b) { return a.priority < b.priority; });

    // Merge into pending requests and wake streaming thread
    {
        std::lock_guard<std::mutex> lock(m_RequestMutex);
        for (auto& req : newRequests) {
            m_PendingRequests.push_back(req);
            m_InFlightPages.insert(req.pageID);
        }
        m_PendingRequestCount = static_cast<u32>(m_PendingRequests.size());
    }

    if (!newRequests.empty()) {
        m_StreamingCV.notify_one();
    }
}

// ---------------------------------------------------------------------------
// UploadPages - Take pages loaded by the streaming thread and copy them
// into the physical atlas via staging buffer. Update the indirection texture
// to point virtual page coordinates to the new physical atlas location.
// ---------------------------------------------------------------------------

void VirtualTextureSystem::UploadPages(VkCommandBuffer commandBuffer) {
    if (!m_StagingBuffer || !m_PageCache) {
        m_ResidentPageCount = m_PageCache ? m_PageCache->GetOccupiedCount() : 0;
        return;
    }

    // Grab loaded pages from the streaming thread
    std::vector<LoadedPage> pages;
    {
        std::lock_guard<std::mutex> lock(m_LoadedMutex);
        if (m_LoadedPages.empty()) {
            m_ResidentPageCount = m_PageCache->GetOccupiedCount();
            return;
        }
        // Take up to MAX_UPLOADS_PER_FRAME pages
        u32 count = std::min(static_cast<u32>(m_LoadedPages.size()), VTConfig::MAX_UPLOADS_PER_FRAME);
        pages.assign(
            std::make_move_iterator(m_LoadedPages.begin()),
            std::make_move_iterator(m_LoadedPages.begin() + count));
        m_LoadedPages.erase(m_LoadedPages.begin(), m_LoadedPages.begin() + count);
    }

    if (pages.empty()) {
        m_ResidentPageCount = m_PageCache->GetOccupiedCount();
        return;
    }

    // On first upload, transition atlas from UNDEFINED to TRANSFER_DST
    if (m_AtlasNeedsTransition) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Atlas;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        m_AtlasNeedsTransition = false;
    } else {
        // Transition atlas from SHADER_READ to TRANSFER_DST for writing
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Atlas;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Map staging buffer and copy all pages into it, then issue copy commands
    void* stagingMapped = m_StagingBuffer->Map();
    if (!stagingMapped) {
        ENJIN_LOG_ERROR(Renderer, "VirtualTexture: Failed to map staging buffer for upload");
        m_ResidentPageCount = m_PageCache->GetOccupiedCount();
        return;
    }

    usize pageBytes = static_cast<usize>(VTConfig::PAGE_SIZE) * VTConfig::PAGE_SIZE * 4;

    for (u32 i = 0; i < static_cast<u32>(pages.size()); ++i) {
        const LoadedPage& lp = pages[i];

        // Copy pixel data into staging buffer at offset for this page
        usize offset = static_cast<usize>(i) * pageBytes;
        std::memcpy(static_cast<u8*>(stagingMapped) + offset, lp.pixels.data(), pageBytes);

        // Insert into page cache (may evict LRU)
        PhysicalPage physical = m_PageCache->Insert(lp.pageID, m_FrameIndex);

        // Issue copy from staging buffer to atlas at the physical page location
        VkBufferImageCopy region{};
        region.bufferOffset = static_cast<VkDeviceSize>(offset);
        region.bufferRowLength = 0;   // Tightly packed
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {
            static_cast<i32>(physical.atlasX * VTConfig::PAGE_SIZE),
            static_cast<i32>(physical.atlasY * VTConfig::PAGE_SIZE),
            0
        };
        region.imageExtent = { VTConfig::PAGE_SIZE, VTConfig::PAGE_SIZE, 1 };

        vkCmdCopyBufferToImage(commandBuffer,
            m_StagingBuffer->GetBuffer(), m_Atlas,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Update indirection texture for this page
        UpdateIndirection(commandBuffer, physical);

        // Remove from in-flight set
        {
            std::lock_guard<std::mutex> lock(m_RequestMutex);
            m_InFlightPages.erase(lp.pageID);
        }
    }

    m_StagingBuffer->Unmap();

    // Transition atlas back to SHADER_READ_ONLY for sampling
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_Atlas;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_ResidentPageCount = m_PageCache->GetOccupiedCount();

    ENJIN_LOG_INFO(Renderer, "VirtualTexture: Uploaded %u pages (resident: %u/%u)",
        static_cast<u32>(pages.size()), m_ResidentPageCount, VTConfig::ATLAS_PAGES);
}

// ---------------------------------------------------------------------------
// UpdateIndirection - Write a single indirection entry for a physical page.
// The indirection texture maps virtual page coordinates to physical atlas UVs.
// Each pixel in the indirection texture at (pageX, pageY) for the given mip
// stores (scaleX, scaleY, offsetX, offsetY) pointing into the physical atlas.
// ---------------------------------------------------------------------------

void VirtualTextureSystem::UpdateIndirection(VkCommandBuffer commandBuffer, const PhysicalPage& page) {
    if (m_IndirectionTexture == VK_NULL_HANDLE) return;

    // Compute indirection entry: maps this virtual page to its physical atlas location
    f32 atlasScale = static_cast<f32>(VTConfig::PAGE_SIZE) / static_cast<f32>(VTConfig::ATLAS_SIZE);
    IndirectionEntry entry;
    entry.scaleX = atlasScale;
    entry.scaleY = atlasScale;
    entry.offsetX = static_cast<f32>(page.atlasX * VTConfig::PAGE_SIZE) / static_cast<f32>(VTConfig::ATLAS_SIZE);
    entry.offsetY = static_cast<f32>(page.atlasY * VTConfig::PAGE_SIZE) / static_cast<f32>(VTConfig::ATLAS_SIZE);

    // We need to update a single texel in the indirection texture at (pageX, pageY, mipLevel).
    // Use vkCmdUpdateBuffer to write the entry to the staging buffer, then copy to image.
    // For simplicity, use vkCmdCopyBufferToImage targeting the specific texel region.

    // Write the indirection entry into the staging buffer at a known offset
    // (after all page data, use the tail of the staging buffer)
    usize pageBytes = static_cast<usize>(VTConfig::PAGE_SIZE) * VTConfig::PAGE_SIZE * 4;
    usize indirectionOffset = pageBytes * VTConfig::MAX_UPLOADS_PER_FRAME; // Past all page upload slots

    // Verify staging buffer is large enough (it should be, but clamp to buffer size)
    usize entrySize = sizeof(IndirectionEntry); // 16 bytes (4 floats)
    if (indirectionOffset + entrySize > m_StagingBuffer->GetSize()) {
        // Staging buffer not large enough for indirection updates; skip
        return;
    }

    // Use vkCmdUpdateBuffer to write the entry directly into the staging buffer
    // (vkCmdUpdateBuffer writes inline data, limited to 65536 bytes, fine for 16 bytes)
    vkCmdUpdateBuffer(commandBuffer, m_StagingBuffer->GetBuffer(),
        static_cast<VkDeviceSize>(indirectionOffset), entrySize, &entry);

    // Barrier: staging buffer write -> transfer read
    VkBufferMemoryBarrier bufBarrier{};
    bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufBarrier.buffer = m_StagingBuffer->GetBuffer();
    bufBarrier.offset = static_cast<VkDeviceSize>(indirectionOffset);
    bufBarrier.size = entrySize;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 1, &bufBarrier, 0, nullptr);

    // Transition indirection texture mip level for transfer write
    VkImageMemoryBarrier imgBarrier{};
    imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // May not have been written before
    imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgBarrier.srcAccessMask = 0;
    imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imgBarrier.image = m_IndirectionTexture;
    imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgBarrier.subresourceRange.baseMipLevel = page.pageID.mipLevel;
    imgBarrier.subresourceRange.levelCount = 1;
    imgBarrier.subresourceRange.baseArrayLayer = 0;
    imgBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

    // Copy single texel from staging buffer to indirection texture
    VkBufferImageCopy region{};
    region.bufferOffset = static_cast<VkDeviceSize>(indirectionOffset);
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = page.pageID.mipLevel;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {
        static_cast<i32>(page.pageID.pageX),
        static_cast<i32>(page.pageID.pageY),
        0
    };
    region.imageExtent = { 1, 1, 1 };

    vkCmdCopyBufferToImage(commandBuffer,
        m_StagingBuffer->GetBuffer(), m_IndirectionTexture,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition indirection texture back to shader-readable
    imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
}

// ---------------------------------------------------------------------------
// StreamingThreadFunc - Background thread that picks page requests from the
// queue and loads the corresponding pixel data from disk. Loaded pages are
// placed in m_LoadedPages for the main thread to upload to the GPU.
// ---------------------------------------------------------------------------

void VirtualTextureSystem::StreamingThreadFunc() {
    while (m_StreamingActive) {
        // Wait until there are pending requests or shutdown is signaled
        PageRequest request;
        bool hasRequest = false;

        {
            std::unique_lock<std::mutex> lock(m_RequestMutex);
            m_StreamingCV.wait(lock, [this] {
                return !m_PendingRequests.empty() || !m_StreamingActive;
            });

            if (!m_StreamingActive) break;

            if (!m_PendingRequests.empty()) {
                request = m_PendingRequests.front();
                m_PendingRequests.erase(m_PendingRequests.begin());
                m_PendingRequestCount = static_cast<u32>(m_PendingRequests.size());
                hasRequest = true;
            }
        }

        if (!hasRequest) continue;

        // Look up texture metadata
        auto regIt = m_TextureRegistry.find(request.pageID.textureID);
        if (regIt == m_TextureRegistry.end()) {
            // Unknown texture, remove from in-flight
            std::lock_guard<std::mutex> lock(m_RequestMutex);
            m_InFlightPages.erase(request.pageID);
            continue;
        }

        const VirtualTextureInfo& texInfo = regIt->second;
        const PageID& pid = request.pageID;

        // Compute the pixel region this page covers in the source texture at mip 0.
        // Mip downsampling is done iteratively below.
        u32 srcX = static_cast<u32>(pid.pageX) * VTConfig::PAGE_SIZE;
        u32 srcY = static_cast<u32>(pid.pageY) * VTConfig::PAGE_SIZE;

        // Load the full texture from disk using stb_image
        // In a production system, you'd use a tiled file format (e.g., OIIO tiled TIFF
        // or a custom .vtex format) to avoid loading the entire texture. For now, we load
        // the full image and extract the page region.
        int imgW = 0, imgH = 0, imgC = 0;
        stbi_uc* pixels = stbi_load(texInfo.path.c_str(), &imgW, &imgH, &imgC, STBI_rgb_alpha);
        if (!pixels) {
            ENJIN_LOG_WARN(Renderer, "VirtualTexture: Failed to load %s for page (%u,%u) mip %u",
                texInfo.path.c_str(), pid.pageX, pid.pageY, pid.mipLevel);
            std::lock_guard<std::mutex> lock(m_RequestMutex);
            m_InFlightPages.erase(pid);
            continue;
        }

        // For mip levels > 0, we need to downsample. Simple box filter approach:
        // Allocate a mip-sized buffer by averaging 2x2 blocks iteratively.
        std::vector<u8> mipData;
        const u8* srcData = pixels;
        u32 srcW = static_cast<u32>(imgW);
        u32 srcH = static_cast<u32>(imgH);

        if (pid.mipLevel > 0) {
            // Iteratively downsample to target mip level
            mipData.resize(static_cast<usize>(srcW) * srcH * 4);
            std::memcpy(mipData.data(), pixels, mipData.size());

            for (u16 m = 0; m < pid.mipLevel; ++m) {
                u32 newW = std::max(1u, srcW / 2);
                u32 newH = std::max(1u, srcH / 2);
                std::vector<u8> downsampled(static_cast<usize>(newW) * newH * 4);

                for (u32 dy = 0; dy < newH; ++dy) {
                    for (u32 dx = 0; dx < newW; ++dx) {
                        u32 sx = dx * 2;
                        u32 sy = dy * 2;
                        // Average 2x2 block
                        u32 r = 0, g = 0, b = 0, a = 0;
                        u32 count = 0;
                        for (u32 ky = 0; ky < 2 && (sy + ky) < srcH; ++ky) {
                            for (u32 kx = 0; kx < 2 && (sx + kx) < srcW; ++kx) {
                                usize idx = (static_cast<usize>(sy + ky) * srcW + (sx + kx)) * 4;
                                r += mipData[idx];
                                g += mipData[idx + 1];
                                b += mipData[idx + 2];
                                a += mipData[idx + 3];
                                count++;
                            }
                        }
                        usize dIdx = (static_cast<usize>(dy) * newW + dx) * 4;
                        downsampled[dIdx]     = static_cast<u8>(r / count);
                        downsampled[dIdx + 1] = static_cast<u8>(g / count);
                        downsampled[dIdx + 2] = static_cast<u8>(b / count);
                        downsampled[dIdx + 3] = static_cast<u8>(a / count);
                    }
                }

                mipData = std::move(downsampled);
                srcW = newW;
                srcH = newH;
            }

            srcData = mipData.data();
        }

        // Extract the page region from the (possibly mipped) source data
        usize pagePixelCount = static_cast<usize>(VTConfig::PAGE_SIZE) * VTConfig::PAGE_SIZE;
        LoadedPage loadedPage;
        loadedPage.pageID = pid;
        loadedPage.pixels.resize(pagePixelCount * 4, 0); // Zero-fill for out-of-bounds

        for (u32 py = 0; py < VTConfig::PAGE_SIZE; ++py) {
            u32 readY = srcY + py;
            if (readY >= srcH) break;

            for (u32 px = 0; px < VTConfig::PAGE_SIZE; ++px) {
                u32 readX = srcX + px;
                if (readX >= srcW) break;

                usize srcIdx = (static_cast<usize>(readY) * srcW + readX) * 4;
                usize dstIdx = (static_cast<usize>(py) * VTConfig::PAGE_SIZE + px) * 4;
                loadedPage.pixels[dstIdx]     = srcData[srcIdx];
                loadedPage.pixels[dstIdx + 1] = srcData[srcIdx + 1];
                loadedPage.pixels[dstIdx + 2] = srcData[srcIdx + 2];
                loadedPage.pixels[dstIdx + 3] = srcData[srcIdx + 3];
            }
        }

        stbi_image_free(pixels);

        // Push to loaded queue for main thread upload
        {
            std::lock_guard<std::mutex> lock(m_LoadedMutex);
            m_LoadedPages.push_back(std::move(loadedPage));
        }
    }
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

void VirtualTextureSystem::Resize(u32 screenWidth, u32 screenHeight) {
    if (screenWidth == m_ScreenWidth && screenHeight == m_ScreenHeight) return;

    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    // Recreate feedback resources at new resolution
    if (m_Context && m_Initialized) {
        VkDevice device = m_Context->GetDevice();

        // Destroy old feedback resources
        if (m_FeedbackView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_FeedbackView, nullptr);
            m_FeedbackView = VK_NULL_HANDLE;
        }
        if (m_FeedbackImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, m_FeedbackImage, nullptr);
            m_FeedbackImage = VK_NULL_HANDLE;
        }
        if (m_FeedbackMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device, m_FeedbackMemory, nullptr);
            m_FeedbackMemory = VK_NULL_HANDLE;
        }
        m_FeedbackReadbackBuffer.reset();

        if (!CreateFeedbackResources()) {
            ENJIN_LOG_WARN(Renderer, "VirtualTexture: Failed to recreate feedback resources on resize");
        }
    }
}

} // namespace Renderer
} // namespace Enjin
