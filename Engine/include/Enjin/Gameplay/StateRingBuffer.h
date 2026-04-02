#pragma once

#include "Enjin/Platform/Platform.h"
#include <vector>
#include <cassert>

namespace Enjin::Gameplay {

// Fixed-capacity ring buffer for frame history storage.
// O(1) push, O(1) random access. Overwrites oldest when full.
template<typename T>
class StateRingBuffer {
public:
    StateRingBuffer() = default;
    explicit StateRingBuffer(u32 capacity) { Reserve(capacity); }

    void Reserve(u32 capacity) {
        m_Buffer.resize(capacity);
        m_Capacity = capacity;
        m_Head = 0;
        m_Count = 0;
    }

    void Push(const T& item) {
        if (m_Capacity == 0) return;
        m_Buffer[m_Head] = item;
        m_Head = (m_Head + 1) % m_Capacity;
        if (m_Count < m_Capacity) ++m_Count;
    }

    void Push(T&& item) {
        if (m_Capacity == 0) return;
        m_Buffer[m_Head] = std::move(item);
        m_Head = (m_Head + 1) % m_Capacity;
        if (m_Count < m_Capacity) ++m_Count;
    }

    // Index 0 = oldest, Count()-1 = newest
    const T& At(u32 index) const {
        assert(index < m_Count);
        u32 start = (m_Head + m_Capacity - m_Count) % m_Capacity;
        return m_Buffer[(start + index) % m_Capacity];
    }

    T& At(u32 index) {
        assert(index < m_Count);
        u32 start = (m_Head + m_Capacity - m_Count) % m_Capacity;
        return m_Buffer[(start + index) % m_Capacity];
    }

    // Most recent entry
    const T& Back() const { return At(m_Count - 1); }
    T& Back() { return At(m_Count - 1); }

    // Oldest entry
    const T& Front() const { return At(0); }

    // Trim entries from the end (newest first) — used when rewinding consumes frames
    void PopBack(u32 n = 1) {
        if (n >= m_Count) { m_Count = 0; return; }
        m_Count -= n;
        m_Head = (m_Head + m_Capacity - n) % m_Capacity;
    }

    u32 Count() const { return m_Count; }
    u32 Capacity() const { return m_Capacity; }
    bool Empty() const { return m_Count == 0; }

    void Clear() { m_Count = 0; m_Head = 0; }

    // Approximate memory usage in bytes
    usize MemoryUsage() const { return m_Buffer.capacity() * sizeof(T); }

private:
    std::vector<T> m_Buffer;
    u32 m_Capacity = 0;
    u32 m_Head = 0;     // Next write position
    u32 m_Count = 0;    // Number of valid entries
};

} // namespace Enjin::Gameplay
