#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <cstdlib>
#include <new>

namespace Enjin {

// Forward declarations
class IAllocator;

// Memory alignment
constexpr usize DEFAULT_ALIGNMENT = 16;
constexpr usize CACHE_LINE_SIZE = 64;

// Memory allocation functions
ENJIN_API void* Allocate(usize size, usize alignment = DEFAULT_ALIGNMENT);
ENJIN_API void  Deallocate(void* ptr);
ENJIN_API void* Reallocate(void* ptr, usize oldSize, usize newSize, usize alignment = DEFAULT_ALIGNMENT);

// Memory utilities
ENJIN_API usize GetAlignmentOffset(void* ptr, usize alignment);
ENJIN_API void  MemoryCopy(void* dest, const void* src, usize size);
ENJIN_API void  MemorySet(void* dest, u8 value, usize size);
ENJIN_API void  MemoryZero(void* dest, usize size);

// Allocator interface
class ENJIN_API IAllocator {
public:
    virtual ~IAllocator() = default;
    virtual void* Allocate(usize size, usize alignment = DEFAULT_ALIGNMENT) = 0;
    virtual void  Deallocate(void* ptr) = 0;
    virtual usize GetTotalAllocated() const = 0;
    virtual usize GetTotalCapacity() const = 0;
};

// Stack allocator - Fast, but requires deallocation in reverse order
class ENJIN_API StackAllocator : public IAllocator {
public:
    StackAllocator(usize size);
    ~StackAllocator();

    void* Allocate(usize size, usize alignment = DEFAULT_ALIGNMENT) override;
    void  Deallocate(void* ptr) override;
    usize GetTotalAllocated() const override;
    usize GetTotalCapacity() const override;

    void Reset(); // Reset to beginning (only if all allocations are freed)
    usize GetMarker() const; // Get current position
    void  FreeToMarker(usize marker); // Free all allocations after marker

private:
    u8* m_Memory;
    usize m_Size;
    usize m_Offset;
};

// Pool allocator - Fast allocation/deallocation for fixed-size objects
class ENJIN_API PoolAllocator : public IAllocator {
public:
    PoolAllocator(usize objectSize, usize objectCount);
    ~PoolAllocator();

    void* Allocate(usize size, usize alignment = DEFAULT_ALIGNMENT) override;
    void  Deallocate(void* ptr) override;
    usize GetTotalAllocated() const override;
    usize GetTotalCapacity() const override;

private:
    struct FreeBlock {
        FreeBlock* next;
    };

    u8* m_Memory;
    usize m_ObjectSize;
    usize m_ObjectCount;
    FreeBlock* m_FreeList;
};

// Linear allocator - Very fast, but can only reset all at once
class ENJIN_API LinearAllocator : public IAllocator {
public:
    LinearAllocator(usize size);
    ~LinearAllocator();

    void* Allocate(usize size, usize alignment = DEFAULT_ALIGNMENT) override;
    void  Deallocate(void* ptr) override;
    usize GetTotalAllocated() const override;
    usize GetTotalCapacity() const override;

    void Reset(); // Reset to beginning

private:
    u8* m_Memory;
    usize m_Size;
    usize m_Offset;
};

// Global default allocator
ENJIN_API IAllocator* GetDefaultAllocator();
ENJIN_API void        SetDefaultAllocator(IAllocator* allocator);

} // namespace Enjin

// Override global new/delete to use custom allocator
void* operator new(std::size_t size);
void* operator new[](std::size_t size);
void  operator delete(void* ptr) noexcept;
void  operator delete[](void* ptr) noexcept;
