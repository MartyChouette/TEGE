// The GPU cull dispatch reads more object slots than were submitted.
//
// cull.comp and cull_hiz.comp run in workgroups of 64 and bound themselves by
// the object buffer's length, so a dispatch for N objects processes every slot
// up to the end of its last workgroup. SubmitObjects therefore uploads a blank
// tail over that range. Without it, a scene with fewer objects than the one
// before kept drawing the old ones: New Scene after a 12-mesh scene, add one
// ground plane, and all twelve old meshes were back on screen.
//
// The upload itself needs a GPU. What can be pinned here is the range it has
// to cover, and that a blank object is one the shaders will not draw.
#include "EnjinTest.h"
#include "Enjin/Renderer/GPUDriven/GPUCulling.h"

using namespace Enjin;
using Renderer::GPUCullingSystem;

ENJIN_TEST(GPUCullSubmit, TheBlankTailReachesTheEndOfTheLastWorkgroup) {
    const usize maxObjects = 100000;
    const usize group = GPUCullingSystem::kCullWorkgroupSize;

    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(0, maxObjects), usize(0));
    // The case that was reported: one object, sixty-three stale slots beside it
    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(1, maxObjects), group);
    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(group - 1, maxObjects), group);
    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(group, maxObjects), group);
    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(group + 1, maxObjects), group * 2);

    // Every count is covered, and never by more than one workgroup of slack
    for (usize n = 0; n < group * 5; ++n) {
        const usize slots = GPUCullingSystem::DispatchedObjectSlots(n, maxObjects);
        ENJIN_EXPECT_TRUE(slots >= n);
        ENJIN_EXPECT_TRUE(slots < n + group);
        ENJIN_EXPECT_EQ(slots % group, usize(0));
    }
}

ENJIN_TEST(GPUCullSubmit, TheTailNeverRunsPastTheBuffer) {
    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(100, 100), usize(100));
    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(65, 100), usize(100));
    ENJIN_EXPECT_EQ(GPUCullingSystem::DispatchedObjectSlots(1, 10), usize(10));
}

// The tail is filled with default objects. The shaders emit a draw only for
// an object with indirectEligible set, so a default one has to leave it clear.
ENJIN_TEST(GPUCullSubmit, ADefaultObjectIsNotDrawn) {
    const Renderer::CullableObject blank{};
    ENJIN_EXPECT_EQ(blank.indirectEligible, 0u);
    ENJIN_EXPECT_EQ(blank.indexCount, 0u);
}

ENJIN_TEST_MAIN()
