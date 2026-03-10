#include "Enjin/Renderer/VirtualTexture/PageCache.h"

namespace Enjin {
namespace Renderer {

PageCache::PageCache(u32 maxPages)
    : m_MaxPages(maxPages) {}

bool PageCache::Lookup(const PageID& id, PhysicalPage& outPage) {
    auto it = m_CacheMap.find(id);
    if (it == m_CacheMap.end()) return false;

    // Move to front of LRU (most recently used)
    m_LRUList.splice(m_LRUList.begin(), m_LRUList, it->second.lruIt);
    outPage = it->second.page;
    return true;
}

PhysicalPage PageCache::Insert(const PageID& id, u32 frameIndex) {
    // Check if already present
    auto existing = m_CacheMap.find(id);
    if (existing != m_CacheMap.end()) {
        m_LRUList.splice(m_LRUList.begin(), m_LRUList, existing->second.lruIt);
        existing->second.page.lastUsedFrame = frameIndex;
        return existing->second.page;
    }

    PhysicalPage page{};
    page.occupied = true;
    page.lastUsedFrame = frameIndex;
    page.pageID = id;

    if (m_NextFreeSlot < m_MaxPages) {
        // Use next free slot (first-fill phase)
        SlotToAtlasXY(m_NextFreeSlot, page.atlasX, page.atlasY);
        m_NextFreeSlot++;
    } else {
        // Evict LRU page
        PageID evictID = m_LRUList.back();
        auto evictIt = m_CacheMap.find(evictID);
        if (evictIt != m_CacheMap.end()) {
            page.atlasX = evictIt->second.page.atlasX;
            page.atlasY = evictIt->second.page.atlasY;
            m_LRUList.pop_back();
            m_CacheMap.erase(evictIt);
        } else {
            // Shouldn't happen, but use slot 0 as fallback
            SlotToAtlasXY(0, page.atlasX, page.atlasY);
        }
    }

    // Insert at front of LRU
    m_LRUList.push_front(id);
    CacheEntry entry;
    entry.lruIt = m_LRUList.begin();
    entry.page = page;
    m_CacheMap[id] = entry;

    return page;
}

void PageCache::Touch(const PageID& id, u32 frameIndex) {
    auto it = m_CacheMap.find(id);
    if (it == m_CacheMap.end()) return;
    m_LRUList.splice(m_LRUList.begin(), m_LRUList, it->second.lruIt);
    it->second.page.lastUsedFrame = frameIndex;
}

bool PageCache::GetEvictionCandidate(PageID& outID) const {
    if (m_LRUList.empty()) return false;
    outID = m_LRUList.back();
    return true;
}

} // namespace Renderer
} // namespace Enjin
