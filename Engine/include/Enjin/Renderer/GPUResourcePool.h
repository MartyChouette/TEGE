#pragma once

#include "Enjin/Platform/Types.h"
#include <vector>
#include <cassert>

namespace Enjin::Renderer {

// Generic slotmap for GPU resource handle → native resource mapping.
// Handles encode (generation << 32 | index). Generation prevents use-after-free.
// Used by buffer/texture/pipeline/bind group managers on all backends.
template<typename T>
class GPUResourcePool {
public:
    struct Slot {
        T resource{};
        u32 generation = 1;  // Start at 1 so PackHandle(0,1) != 0 (handle 0 = invalid sentinel)
        bool alive = false;
    };

    // Allocate a slot and return a handle. The resource is default-constructed.
    u64 Allocate() {
        u32 index;
        if (!m_FreeList.empty()) {
            index = m_FreeList.back();
            m_FreeList.pop_back();
        } else {
            index = static_cast<u32>(m_Slots.size());
            m_Slots.push_back({});
        }
        auto& slot = m_Slots[index];
        slot.alive = true;
        // Generation was already bumped on free (or starts at 0)
        return PackHandle(index, slot.generation);
    }

    // Free a slot. Bumps generation so old handles become invalid.
    void Free(u64 handle) {
        u32 index = GetIndex(handle);
        u32 gen = GetGeneration(handle);
        if (index >= m_Slots.size()) return;
        auto& slot = m_Slots[index];
        if (!slot.alive || slot.generation != gen) return;
        slot.alive = false;
        slot.generation++;
        slot.resource = T{};
        m_FreeList.push_back(index);
    }

    // Get a mutable pointer to the resource. Returns nullptr if handle is invalid.
    T* Get(u64 handle) {
        u32 index = GetIndex(handle);
        u32 gen = GetGeneration(handle);
        if (index >= m_Slots.size()) return nullptr;
        auto& slot = m_Slots[index];
        if (!slot.alive || slot.generation != gen) return nullptr;
        return &slot.resource;
    }

    // Get a const pointer to the resource.
    const T* Get(u64 handle) const {
        u32 index = GetIndex(handle);
        u32 gen = GetGeneration(handle);
        if (index >= m_Slots.size()) return nullptr;
        auto& slot = m_Slots[index];
        if (!slot.alive || slot.generation != gen) return nullptr;
        return &slot.resource;
    }

    bool IsValid(u64 handle) const {
        u32 index = GetIndex(handle);
        u32 gen = GetGeneration(handle);
        if (index >= m_Slots.size()) return false;
        auto& slot = m_Slots[index];
        return slot.alive && slot.generation == gen;
    }

    // Iterate all alive slots (for cleanup on shutdown).
    template<typename Fn>
    void ForEach(Fn&& fn) {
        for (auto& slot : m_Slots) {
            if (slot.alive) fn(slot.resource);
        }
    }

    void Clear() {
        m_Slots.clear();
        m_FreeList.clear();
    }

    u32 AliveCount() const {
        u32 count = 0;
        for (auto& s : m_Slots) { if (s.alive) count++; }
        return count;
    }

private:
    static u64 PackHandle(u32 index, u32 generation) {
        return (static_cast<u64>(generation) << 32) | static_cast<u64>(index);
    }
    static u32 GetIndex(u64 handle) { return static_cast<u32>(handle & 0xFFFFFFFF); }
    static u32 GetGeneration(u64 handle) { return static_cast<u32>(handle >> 32); }

    std::vector<Slot> m_Slots;
    std::vector<u32> m_FreeList;
};

} // namespace Enjin::Renderer
