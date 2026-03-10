#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <condition_variable>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// Virtual texturing page configuration
struct VTConfig {
    static constexpr u32 PAGE_SIZE = 128;          // Pixels per page tile (128x128)
    static constexpr u32 ATLAS_SIZE = 8192;        // Physical atlas size (8K x 8K)
    static constexpr u32 ATLAS_PAGES = (ATLAS_SIZE / PAGE_SIZE) * (ATLAS_SIZE / PAGE_SIZE); // 4096 pages
    static constexpr u32 MAX_VIRTUAL_SIZE = 65536; // Max virtual texture size (64K)
    static constexpr u32 MAX_MIP_LEVELS = 10;      // log2(65536/128) + 1
    static constexpr u32 FEEDBACK_SCALE = 8;       // Feedback buffer is 1/8 screen resolution
    static constexpr u32 MAX_UPLOADS_PER_FRAME = 32; // Max page uploads per frame
};

// Page identifier (virtual texture ID + mip level + page x/y)
struct PageID {
    u32 textureID;
    u16 mipLevel;
    u16 pageX;
    u16 pageY;
    u16 _pad;

    bool operator==(const PageID& o) const {
        return textureID == o.textureID && mipLevel == o.mipLevel && pageX == o.pageX && pageY == o.pageY;
    }
};

// Physical page location in the atlas
struct PhysicalPage {
    u16 atlasX;    // X position in atlas (in pages, not pixels)
    u16 atlasY;    // Y position in atlas
    bool occupied;
    u32 lastUsedFrame;
    PageID pageID;
};

// Page request from feedback analysis
struct PageRequest {
    PageID pageID;
    u32 priority;  // Lower = higher priority (distance-based)
};

// Indirection table entry (maps virtual page to physical atlas location)
struct IndirectionEntry {
    f32 scaleX;    // UV scale to atlas page
    f32 scaleY;
    f32 offsetX;   // UV offset within atlas
    f32 offsetY;
};

// Registered virtual texture metadata
struct VirtualTextureInfo {
    std::string path;       // Source file path on disk
    u32 width;              // Full texture width in pixels
    u32 height;             // Full texture height in pixels
    u32 mipLevels;          // Number of mip levels
    u32 pagesX;             // Page count in X at mip 0
    u32 pagesY;             // Page count in Y at mip 0
};

// Page data loaded by the streaming thread, ready for GPU upload
struct LoadedPage {
    PageID pageID;
    std::vector<u8> pixels; // RGBA8 pixel data (PAGE_SIZE * PAGE_SIZE * 4 bytes)
};

} // namespace Renderer
} // namespace Enjin

// Hash for PageID
namespace std {
    template<> struct hash<Enjin::Renderer::PageID> {
        size_t operator()(const Enjin::Renderer::PageID& p) const {
            return hash<uint64_t>()(
                (static_cast<uint64_t>(p.textureID) << 32) |
                (static_cast<uint64_t>(p.mipLevel) << 16) |
                (static_cast<uint64_t>(p.pageX) << 8) |
                p.pageY
            );
        }
    };
}

namespace Enjin {
namespace Renderer {

// Forward declare page cache
class PageCache;
class FeedbackBuffer;

// Virtual Texture System — streams texture pages on demand from disk.
// Architecture:
// 1. Feedback pass: low-res render writes page IDs needed by visible fragments
// 2. Feedback readback: CPU reads feedback, generates page request list
// 3. Streaming: background thread loads requested pages from disk
// 4. Upload: newly loaded pages copied to physical atlas
// 5. Indirection update: indirection texture updated to point virtual→physical
class ENJIN_API VirtualTextureSystem {
public:
    VirtualTextureSystem(VulkanContext* context);
    ~VirtualTextureSystem();

    bool Initialize(u32 screenWidth, u32 screenHeight);
    void Shutdown();

    // Per-frame update: process feedback, stream pages, upload to atlas
    void Update(VkCommandBuffer commandBuffer, u32 frameIndex);

    // Render the feedback pass (low-res pass that writes page IDs)
    void RenderFeedback(VkCommandBuffer commandBuffer, const Math::Matrix4& viewProj);

    // Process feedback readback and generate streaming requests
    void ProcessFeedback();

    // Get resources for fragment shader binding
    VkImageView GetIndirectionTextureView() const { return m_IndirectionView; }
    VkImageView GetPhysicalAtlasView() const { return m_AtlasView; }
    VkSampler GetAtlasSampler() const { return m_AtlasSampler; }

    // Register a virtual texture (returns virtual texture ID)
    u32 RegisterTexture(const std::string& path, u32 width, u32 height);

    void Resize(u32 screenWidth, u32 screenHeight);

    // Statistics
    u32 GetResidentPageCount() const { return m_ResidentPageCount; }
    u32 GetPendingRequestCount() const { return m_PendingRequestCount; }

private:
    bool CreatePhysicalAtlas();
    bool CreateIndirectionTexture();
    bool CreateFeedbackResources();
    bool CreateStagingBuffer();
    void UploadPages(VkCommandBuffer commandBuffer);
    void UpdateIndirection(VkCommandBuffer commandBuffer, const PhysicalPage& page);
    void StreamingThreadFunc();

    VulkanContext* m_Context = nullptr;

    // Physical atlas texture (8K x 8K)
    VkImage m_Atlas = VK_NULL_HANDLE;
    VkDeviceMemory m_AtlasMemory = VK_NULL_HANDLE;
    VkImageView m_AtlasView = VK_NULL_HANDLE;
    VkSampler m_AtlasSampler = VK_NULL_HANDLE;

    // Indirection texture (maps virtual UV → physical atlas location)
    VkImage m_IndirectionTexture = VK_NULL_HANDLE;
    VkDeviceMemory m_IndirectionMemory = VK_NULL_HANDLE;
    VkImageView m_IndirectionView = VK_NULL_HANDLE;

    // Feedback buffer (low-res render target for page ID capture)
    VkImage m_FeedbackImage = VK_NULL_HANDLE;
    VkDeviceMemory m_FeedbackMemory = VK_NULL_HANDLE;
    VkImageView m_FeedbackView = VK_NULL_HANDLE;
    std::unique_ptr<VulkanBuffer> m_FeedbackReadbackBuffer;

    // Page staging buffer (host-visible, used to upload loaded pages to atlas)
    std::unique_ptr<VulkanBuffer> m_StagingBuffer;

    // Page cache (LRU eviction)
    std::unique_ptr<PageCache> m_PageCache;

    // Registered virtual textures (ID -> metadata)
    std::unordered_map<u32, VirtualTextureInfo> m_TextureRegistry;
    u32 m_NextTextureID = 1;

    // Streaming state
    std::vector<PageRequest> m_PendingRequests;
    std::mutex m_RequestMutex;
    std::atomic<bool> m_StreamingActive{false};
    std::thread m_StreamingThread;
    std::condition_variable m_StreamingCV;

    // Pages loaded by streaming thread, ready for GPU upload
    std::vector<LoadedPage> m_LoadedPages;
    std::mutex m_LoadedMutex;

    // Dedup set for in-flight requests (pages currently being loaded)
    std::unordered_set<PageID> m_InFlightPages;

    u32 m_ScreenWidth = 0;
    u32 m_ScreenHeight = 0;
    u32 m_ResidentPageCount = 0;
    u32 m_PendingRequestCount = 0;
    u32 m_FrameIndex = 0;
    bool m_Initialized = false;
    bool m_AtlasNeedsTransition = true; // First frame needs UNDEFINED -> TRANSFER_DST
};

} // namespace Renderer
} // namespace Enjin
