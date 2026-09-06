#include "Enjin/Memory/Memory.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <new>       // std::align_val_t, std::bad_alloc

namespace Enjin {

// Default allocator (uses malloc/free for now, can be replaced)
static IAllocator* g_DefaultAllocator = nullptr;

void* Allocate(usize size, usize alignment) {
    if (g_DefaultAllocator) {
        return g_DefaultAllocator->Allocate(size, alignment);
    }
    // Fallback to aligned malloc
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void* ptr = nullptr;
    // POSIX leaves *memptr unspecified when this fails, so the result has to be
    // checked rather than returned blind.
    if (posix_memalign(&ptr, alignment, size) != 0) return nullptr;
    return ptr;
#endif
}

void Deallocate(void* ptr) {
    if (g_DefaultAllocator && ptr) {
        g_DefaultAllocator->Deallocate(ptr);
    } else if (ptr) {
#ifdef _WIN32
        _aligned_free(ptr);
#else
        std::free(ptr);
#endif
    }
}

void* Reallocate(void* ptr, usize oldSize, usize newSize, usize alignment) {
    void* newPtr = Allocate(newSize, alignment);
    if (!newPtr) return nullptr;
    if (ptr) {
        // CF-C1 fix: copy only the smaller of old/new size to prevent buffer overread
        usize copySize = oldSize < newSize ? oldSize : newSize;
        MemoryCopy(newPtr, ptr, copySize);
        Deallocate(ptr);
    }
    return newPtr;
}

usize GetAlignmentOffset(void* ptr, usize alignment) {
    usize address = reinterpret_cast<usize>(ptr);
    usize mask = alignment - 1;
    return (alignment - (address & mask)) & mask;
}

void MemoryCopy(void* dest, const void* src, usize size) {
    std::memcpy(dest, src, size);
}

void MemorySet(void* dest, u8 value, usize size) {
    std::memset(dest, value, size);
}

void MemoryZero(void* dest, usize size) {
    std::memset(dest, 0, size);
}

IAllocator* GetDefaultAllocator() {
    return g_DefaultAllocator;
}

void SetDefaultAllocator(IAllocator* allocator) {
    g_DefaultAllocator = allocator;
}

// StackAllocator implementation
StackAllocator::StackAllocator(usize size) 
    : m_Size(size), m_Offset(0) {
    m_Memory = static_cast<u8*>(std::malloc(size));
    assert(m_Memory && "Failed to allocate memory for StackAllocator");
}

StackAllocator::~StackAllocator() {
    std::free(m_Memory);
}

void* StackAllocator::Allocate(usize size, usize alignment) {
    usize offset = GetAlignmentOffset(m_Memory + m_Offset, alignment);
    usize totalSize = size + offset;
    
    if (m_Offset + totalSize > m_Size) {
        return nullptr; // Out of memory
    }
    
    void* ptr = m_Memory + m_Offset + offset;
    m_Offset += totalSize;
    return ptr;
}

void StackAllocator::Deallocate(void* ptr) {
    // Stack allocator doesn't support individual deallocation
    // Use FreeToMarker instead
    (void)ptr;
}

usize StackAllocator::GetTotalAllocated() const {
    return m_Offset;
}

usize StackAllocator::GetTotalCapacity() const {
    return m_Size;
}

usize StackAllocator::GetMarker() const {
    return m_Offset;
}

void StackAllocator::FreeToMarker(usize marker) {
    assert(marker <= m_Offset);
    m_Offset = marker;
}

void StackAllocator::Reset() {
    m_Offset = 0;
}

// PoolAllocator implementation
PoolAllocator::PoolAllocator(usize objectSize, usize objectCount)
    : m_ObjectSize(objectSize), m_ObjectCount(objectCount) {
    // Ensure object size is at least sizeof(FreeBlock)
    m_ObjectSize = (m_ObjectSize < sizeof(FreeBlock)) ? sizeof(FreeBlock) : m_ObjectSize;
    
    m_Memory = static_cast<u8*>(std::malloc(m_ObjectSize * m_ObjectCount));
    assert(m_Memory && "Failed to allocate memory for PoolAllocator");
    
    // Initialize free list
    m_FreeList = reinterpret_cast<FreeBlock*>(m_Memory);
    FreeBlock* current = m_FreeList;
    for (usize i = 0; i < m_ObjectCount - 1; ++i) {
        current->next = reinterpret_cast<FreeBlock*>(
            m_Memory + (i + 1) * m_ObjectSize);
        current = current->next;
    }
    current->next = nullptr;
}

PoolAllocator::~PoolAllocator() {
    std::free(m_Memory);
}

void* PoolAllocator::Allocate(usize size, usize alignment) {
    (void)alignment; // Pool allocator uses fixed size
    if (size > m_ObjectSize || !m_FreeList) {
        return nullptr;
    }
    
    FreeBlock* block = m_FreeList;
    m_FreeList = m_FreeList->next;
    return block;
}

void PoolAllocator::Deallocate(void* ptr) {
    if (!ptr) return;

    // CF-C2 fix: validate pointer belongs to this pool's memory range
    u8* bytePtr = static_cast<u8*>(ptr);
    u8* poolEnd = m_Memory + m_ObjectSize * m_ObjectCount;
    if (bytePtr < m_Memory || bytePtr >= poolEnd) {
        ENJIN_LOG_ERROR(Core, "PoolAllocator::Deallocate: pointer %p outside pool range [%p, %p)",
                        ptr, static_cast<void*>(m_Memory), static_cast<void*>(poolEnd));
        return;
    }
    usize offset = static_cast<usize>(bytePtr - m_Memory);
    if (offset % m_ObjectSize != 0) {
        ENJIN_LOG_ERROR(Core, "PoolAllocator::Deallocate: pointer %p misaligned (objectSize=%zu)",
                        ptr, m_ObjectSize);
        return;
    }

    FreeBlock* block = static_cast<FreeBlock*>(ptr);
    block->next = m_FreeList;
    m_FreeList = block;
}

usize PoolAllocator::GetTotalAllocated() const {
    usize allocated = 0;
    FreeBlock* current = m_FreeList;
    while (current) {
        ++allocated;
        current = current->next;
    }
    return (m_ObjectCount - allocated) * m_ObjectSize;
}

usize PoolAllocator::GetTotalCapacity() const {
    return m_ObjectSize * m_ObjectCount;
}

// LinearAllocator implementation
LinearAllocator::LinearAllocator(usize size)
    : m_Size(size), m_Offset(0) {
    m_Memory = static_cast<u8*>(std::malloc(size));
    assert(m_Memory && "Failed to allocate memory for LinearAllocator");
}

LinearAllocator::~LinearAllocator() {
    std::free(m_Memory);
}

void* LinearAllocator::Allocate(usize size, usize alignment) {
    usize offset = GetAlignmentOffset(m_Memory + m_Offset, alignment);
    usize totalSize = size + offset;
    
    if (m_Offset + totalSize > m_Size) {
        return nullptr; // Out of memory
    }
    
    void* ptr = m_Memory + m_Offset + offset;
    m_Offset += totalSize;
    return ptr;
}

void LinearAllocator::Deallocate(void* ptr) {
    // Linear allocator doesn't support individual deallocation
    // Use Reset() to free all memory
    (void)ptr;
}

usize LinearAllocator::GetTotalAllocated() const {
    return m_Offset;
}

usize LinearAllocator::GetTotalCapacity() const {
    return m_Size;
}

void LinearAllocator::Reset() {
    m_Offset = 0;
}

} // namespace Enjin

// Global new/delete operators
void* operator new(std::size_t size) {
    void* ptr = Enjin::Allocate(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void* operator new[](std::size_t size) {
    void* ptr = Enjin::Allocate(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept {
    Enjin::Deallocate(ptr);
}

void operator delete[](void* ptr) noexcept {
    Enjin::Deallocate(ptr);
}

// SIZED deallocation (C++14).
//
// These were missing, and the compiler does not fall back to the unsized form
// above: it emits a call to the DEFAULT sized operator delete, which frees with
// libc free(). So every std::vector, std::string and most container teardown in
// the engine allocated through Enjin::Allocate and freed through an allocator
// that knew nothing about it.
//
// Today that survives because the fallback path is posix_memalign/_aligned_malloc,
// and posix_memalign memory is legal to free(). It stops surviving the moment
// anyone calls SetDefaultAllocator: Allocate then hands out memory from a custom
// arena and the default sized delete hands it to libc free(), which is heap
// corruption. The API is public (Memory.h) and exists to be used.
//
// AddressSanitizer reports it today as alloc-dealloc-mismatch (malloc vs
// operator delete), which is how this was found -- the first CI run that ever
// executed a sanitizer flagged it on TestMemory's static initialisation.
void operator delete(void* ptr, std::size_t) noexcept {
    Enjin::Deallocate(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    Enjin::Deallocate(ptr);
}

// NOTE: the ALIGNED new/delete overloads are deliberately NOT overridden.
//
// They were added alongside the sized deletes above and then backed out: the Linux
// lavapipe render smoke started failing with "LLVM ERROR: out of memory" in the
// exact push that introduced them, and stayed red for four consecutive runs.
// The sized deletes are what AddressSanitizer actually required -- they fix the
// alloc/dealloc mismatch without changing WHICH allocator serves memory. The
// aligned new was speculative, and it did change that: over-aligned types went
// from glibc's aligned_alloc to posix_memalign via Enjin::Allocate, and glibc's
// memalign path carries more per-allocation overhead. lavapipe allocates
// heavily and over-aligned, on a runner with little headroom.
//
// Leaving them to the default implementation means an over-aligned type does
// not route through g_DefaultAllocator. That is a real (currently theoretical)
// gap: nothing sets a custom allocator today. Re-adding them needs the OOM
// understood first, not another guess.

