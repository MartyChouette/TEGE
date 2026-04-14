#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/VirtualTexture/VirtualTextureSystem.h"
#include <unordered_map>
#include <list>
#include <vector>

namespace Enjin {
namespace Renderer {

// LRU page cache for virtual texturing.
// Manages physical atlas page slots with least-recently-used eviction.
// Each page is a 128x128 tile in the physical atlas.
class PageCache {
public:
    explicit PageCache(u32 maxPages);

    // Try to find a page in the cache. Returns physical page info if found.
    // Promotes the page to most-recently-used on hit.
    bool Lookup(const PageID& id, PhysicalPage& outPage);

    // Insert a page into the cache. Returns the physical location assigned.
    // If cache is full, evicts the least-recently-used page.
    PhysicalPage Insert(const PageID& id, u32 frameIndex);

    // Mark a page as used this frame (prevents immediate eviction)
    void Touch(const PageID& id, u32 frameIndex);

    // Get eviction candidate (LRU page)
    bool GetEvictionCandidate(PageID& outID) const;

    u32 GetOccupiedCount() const { return static_cast<u32>(m_LRUList.size()); }
    u32 GetCapacity() const { return m_MaxPages; }

private:
    u32 m_MaxPages;
    u32 m_NextFreeSlot = 0;  // Linear allocator for first-fill

    // LRU list: front = most recently used, back = least recently used
    using LRUList = std::list<PageID>;
    LRUList m_LRUList;

    // Map from PageID to (LRU iterator + physical page info)
    struct CacheEntry {
        LRUList::iterator lruIt;
        PhysicalPage page;
    };
    std::unordered_map<PageID, CacheEntry> m_CacheMap;

    // Convert linear slot to atlas x/y
    void SlotToAtlasXY(u32 slot, u16& x, u16& y) const {
        u32 pagesPerRow = VTConfig::ATLAS_SIZE / VTConfig::PAGE_SIZE;
        x = static_cast<u16>(slot % pagesPerRow);
        y = static_cast<u16>(slot / pagesPerRow);
    }
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
