#pragma once

#include "Enjin/Memory/Memory.h"
#include "Enjin/Platform/Types.h"
#include <cassert>
#include <cstring>
#include <new>
#include <type_traits>

namespace Enjin {

// Per-frame linear allocator wrapper.
// Designed to be Reset() once per frame, giving O(1) "allocation" for hot-path
// arrays that are rebuilt every frame (render lists, shadow casters, etc.).
class FrameAllocator {
public:
    explicit FrameAllocator(usize capacityBytes)
        : m_Allocator(capacityBytes) {}

    void Reset() { m_Allocator.Reset(); }

    void* Allocate(usize size, usize alignment = DEFAULT_ALIGNMENT) {
        return m_Allocator.Allocate(size, alignment);
    }

    usize GetTotalAllocated() const { return m_Allocator.GetTotalAllocated(); }
    usize GetTotalCapacity() const { return m_Allocator.GetTotalCapacity(); }

private:
    LinearAllocator m_Allocator;
};

// Lightweight array backed by a FrameAllocator.
// Replaces std::vector in hot paths where the pattern is:
//   clear() → reserve() → push_back() N times → iterate → discard
//
// POD types only (no constructors/destructors called). Supports trivially
// copyable/movable types like Entity, CullableObject, ObjectDataGPU, etc.
template<typename T>
class FrameArray {
public:
    FrameArray() = default;

    // Allocate backing storage from a FrameAllocator.
    // Call once per frame after FrameAllocator::Reset().
    // maxCount: maximum number of elements this array can hold.
    void Init(FrameAllocator& allocator, usize maxCount) {
        m_Data = static_cast<T*>(allocator.Allocate(maxCount * sizeof(T), alignof(T)));
        m_Capacity = m_Data ? maxCount : 0;
        m_Size = 0;
    }

    void push_back(const T& value) {
        assert(m_Size < m_Capacity && "FrameArray overflow");
        if (m_Size < m_Capacity) {
            m_Data[m_Size++] = value;
        }
    }

    T& emplace_back() {
        assert(m_Size < m_Capacity && "FrameArray overflow");
        return m_Data[m_Size++];
    }

    void resize(usize newSize) {
        assert(newSize <= m_Capacity && "FrameArray resize beyond capacity");
        m_Size = (newSize <= m_Capacity) ? newSize : m_Capacity;
    }

    void clear() { m_Size = 0; }

    T& operator[](usize index) { return m_Data[index]; }
    const T& operator[](usize index) const { return m_Data[index]; }

    T* data() { return m_Data; }
    const T* data() const { return m_Data; }

    usize size() const { return m_Size; }
    usize capacity() const { return m_Capacity; }
    bool empty() const { return m_Size == 0; }

    T* begin() { return m_Data; }
    T* end() { return m_Data + m_Size; }
    const T* begin() const { return m_Data; }
    const T* end() const { return m_Data + m_Size; }

    T& front() { return m_Data[0]; }
    const T& front() const { return m_Data[0]; }
    T& back() { return m_Data[m_Size - 1]; }
    const T& back() const { return m_Data[m_Size - 1]; }

private:
    T* m_Data = nullptr;
    usize m_Size = 0;
    usize m_Capacity = 0;
};

} // namespace Enjin
