#include "EnjinTest.h"
#include "Enjin/Memory/Memory.h"
#include "Enjin/Memory/FrameAllocator.h"
#include <cstdint>

using namespace Enjin;

// ===========================================================================
// LinearAllocator
// ===========================================================================

ENJIN_TEST(LinearAllocator, BasicAllocate) {
    LinearAllocator alloc(1024);
    void* ptr = alloc.Allocate(64);
    ENJIN_EXPECT_NOT_NULL(ptr);
    ENJIN_EXPECT_GT(alloc.GetTotalAllocated(), (usize)0);
}

ENJIN_TEST(LinearAllocator, Reset) {
    LinearAllocator alloc(1024);
    alloc.Allocate(64);
    alloc.Allocate(128);
    ENJIN_EXPECT_GT(alloc.GetTotalAllocated(), (usize)0);
    alloc.Reset();
    ENJIN_EXPECT_EQ(alloc.GetTotalAllocated(), (usize)0);
}

ENJIN_TEST(LinearAllocator, OverflowReturnsNull) {
    LinearAllocator alloc(64);
    void* p1 = alloc.Allocate(32);
    ENJIN_EXPECT_NOT_NULL(p1);
    // Try to allocate more than remaining
    void* p2 = alloc.Allocate(64);
    ENJIN_EXPECT_NULL(p2);
}

ENJIN_TEST(LinearAllocator, CapacityMatchesConstructor) {
    LinearAllocator alloc(2048);
    ENJIN_EXPECT_EQ(alloc.GetTotalCapacity(), (usize)2048);
}

// ===========================================================================
// StackAllocator
// ===========================================================================

ENJIN_TEST(StackAllocator, AllocateAndFreeToMarker) {
    StackAllocator alloc(1024);
    usize marker = alloc.GetMarker();
    void* p1 = alloc.Allocate(64);
    ENJIN_EXPECT_NOT_NULL(p1);
    ENJIN_EXPECT_GT(alloc.GetTotalAllocated(), (usize)0);
    alloc.FreeToMarker(marker);
    ENJIN_EXPECT_EQ(alloc.GetTotalAllocated(), marker);
}

ENJIN_TEST(StackAllocator, MultipleAllocsPartialFree) {
    StackAllocator alloc(1024);
    alloc.Allocate(32);
    usize mid = alloc.GetMarker();
    alloc.Allocate(64);
    alloc.Allocate(128);
    ENJIN_EXPECT_GT(alloc.GetTotalAllocated(), mid);
    alloc.FreeToMarker(mid);
    ENJIN_EXPECT_EQ(alloc.GetTotalAllocated(), mid);
}

ENJIN_TEST(StackAllocator, Reset) {
    StackAllocator alloc(1024);
    alloc.Allocate(64);
    alloc.Allocate(128);
    alloc.Reset();
    ENJIN_EXPECT_EQ(alloc.GetTotalAllocated(), (usize)0);
}

ENJIN_TEST(StackAllocator, CapacityMatchesConstructor) {
    StackAllocator alloc(4096);
    ENJIN_EXPECT_EQ(alloc.GetTotalCapacity(), (usize)4096);
}

// ===========================================================================
// PoolAllocator
// ===========================================================================

ENJIN_TEST(PoolAllocator, AllocateDeallocate) {
    PoolAllocator alloc(64, 8);
    void* p = alloc.Allocate(64);
    ENJIN_EXPECT_NOT_NULL(p);
    alloc.Deallocate(p);
}

ENJIN_TEST(PoolAllocator, ExhaustPoolReturnsNull) {
    PoolAllocator alloc(64, 4);
    void* ptrs[4];
    for (int i = 0; i < 4; i++) {
        ptrs[i] = alloc.Allocate(64);
        ENJIN_EXPECT_NOT_NULL(ptrs[i]);
    }
    void* overflow = alloc.Allocate(64);
    ENJIN_EXPECT_NULL(overflow);
    // Clean up
    for (int i = 0; i < 4; i++)
        alloc.Deallocate(ptrs[i]);
}

ENJIN_TEST(PoolAllocator, FreeAndReuse) {
    PoolAllocator alloc(64, 2);
    void* p1 = alloc.Allocate(64);
    void* p2 = alloc.Allocate(64);
    ENJIN_EXPECT_NOT_NULL(p1);
    ENJIN_EXPECT_NOT_NULL(p2);
    // Pool full
    ENJIN_EXPECT_NULL(alloc.Allocate(64));
    // Free one and re-allocate
    alloc.Deallocate(p1);
    void* p3 = alloc.Allocate(64);
    ENJIN_EXPECT_NOT_NULL(p3);
    alloc.Deallocate(p2);
    alloc.Deallocate(p3);
}

ENJIN_TEST(PoolAllocator, CapacityReasonable) {
    PoolAllocator alloc(128, 16);
    ENJIN_EXPECT_GE(alloc.GetTotalCapacity(), (usize)16);
}

// Regression: a 0-count pool walked into `i < m_ObjectCount - 1`, where usize
// wraps to SIZE_MAX and the free-list loop writes through every address in the
// process. An empty pool is legal to construct; it just hands out nothing.
ENJIN_TEST(PoolAllocator, ZeroCountPoolIsEmpty) {
    PoolAllocator alloc(64, 0);
    ENJIN_EXPECT_NULL(alloc.Allocate(64));
    ENJIN_EXPECT_EQ(alloc.GetTotalCapacity(), (usize)0);
}

// Regression: freeing the same block twice pushed it onto the free list twice,
// so the list cycled and two later Allocate calls handed the same memory to two
// owners. The second free is now refused, leaving one free slot, not two.
ENJIN_TEST(PoolAllocator, DoubleFreeIsRejected) {
    PoolAllocator alloc(64, 2);
    void* p1 = alloc.Allocate(64);
    void* p2 = alloc.Allocate(64);
    ENJIN_ASSERT_NOT_NULL(p1);
    ENJIN_ASSERT_NOT_NULL(p2);
    alloc.Deallocate(p1);
    alloc.Deallocate(p1);  // logs an error and does nothing
    void* a = alloc.Allocate(64);
    void* b = alloc.Allocate(64);
    ENJIN_EXPECT_NOT_NULL(a);
    ENJIN_EXPECT_NULL(b);  // p2 is still out; only one slot was ever free
    alloc.Deallocate(a);
    alloc.Deallocate(p2);
}

// ===========================================================================
// FrameArray
// ===========================================================================

// maxCount * sizeof(T) wrapping would take a small block and then report
// capacity for maxCount elements.
ENJIN_TEST(FrameArray, InitRejectsOverflowingCount) {
    FrameAllocator alloc(1024);
    FrameArray<u64> arr;
    arr.Init(alloc, SIZE_MAX / 4);
    ENJIN_EXPECT_EQ(arr.capacity(), (usize)0);
    ENJIN_EXPECT_EQ(arr.size(), (usize)0);
}

ENJIN_TEST(FrameArray, InitFailureLeavesZeroCapacity) {
    FrameAllocator alloc(64);
    FrameArray<u64> arr;
    arr.Init(alloc, 1000);  // more than the allocator holds
    ENJIN_EXPECT_EQ(arr.capacity(), (usize)0);
}

#ifdef NDEBUG
// Regression: emplace_back only asserted, so a Release build wrote past the end
// of the buffer and kept incrementing m_Size. Debug still aborts on the assert,
// which is why this case is Release-only.
ENJIN_TEST(FrameArray, EmplaceBackStopsAtCapacity) {
    FrameAllocator alloc(1024);
    FrameArray<u32> arr;
    arr.Init(alloc, 2);
    arr.emplace_back() = 1;
    arr.emplace_back() = 2;
    arr.emplace_back() = 3;  // refused; overwrites the caller's own last slot
    ENJIN_EXPECT_EQ(arr.size(), (usize)2);
    ENJIN_EXPECT_EQ(arr[0], (u32)1);
    ENJIN_EXPECT_EQ(arr[1], (u32)3);
}
#endif

ENJIN_TEST_MAIN()
