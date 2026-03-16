#include "EnjinTest.h"
#include "Enjin/Memory/Memory.h"

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

ENJIN_TEST_MAIN()
