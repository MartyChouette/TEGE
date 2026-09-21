// ComputeWorldMatrix memoises, so it is a WRITE, and adr-0004 forbids writing
// component data from a worker thread.
//
// It reads like a getter. On a cache miss it stores `cachedWorldMatrix` and
// clears `worldMatrixDirty` for the entity AND every ancestor it walks, so two
// workers holding sibling entities under one parent both recompute and both
// write that parent's 64-byte matrix at the same time: a torn matrix, for one
// frame, non-reproducibly.
//
// `World::AssertOwnerThread` cannot see it -- that guard is for STRUCTURAL
// mutation (Add/Remove/Create/Destroy/Clear), and this is a data write. The
// parallel shadow pass avoided it by pre-warming every caster on the main
// thread, which works and is a convention the next parallel region can forget.
//
// So off the owner thread the function computes the same answer and does not
// store it. These tests pin both halves: the answer is still right, and the
// cache is left alone.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"

#include <thread>
#include <vector>
#include <atomic>

using namespace Enjin;

namespace {

// A parent at a known offset with `childCount` children hanging off it. Every
// child's world matrix walks through the parent, which is what makes the parent
// the contended write in the real bug.
ECS::Entity MakeFamily(ECS::World& world, std::vector<ECS::Entity>& children,
                       int childCount) {
    ECS::Entity parent = world.CreateEntity();
    ECS::TransformComponent pxf;
    pxf.position = Math::Vector3(10.0f, 0.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(parent, pxf);

    for (int i = 0; i < childCount; ++i) {
        ECS::Entity child = world.CreateEntity();
        ECS::TransformComponent cxf;
        cxf.position = Math::Vector3(0.0f, static_cast<f32>(i + 1), 0.0f);
        world.AddComponent<ECS::TransformComponent>(child, cxf);
        ECS::ParentComponent pc;
        pc.parent = parent;
        world.AddComponent<ECS::ParentComponent>(child, pc);
        children.push_back(child);
    }
    return parent;
}

// Mark every transform dirty, the way RenderSystem::BeginFrameTransformCaches
// does at the top of each frame.
void DirtyAll(ECS::World& world, const std::vector<ECS::Entity>& all) {
    for (ECS::Entity e : all) {
        if (auto* xf = world.GetComponent<ECS::TransformComponent>(e))
            xf->worldMatrixDirty = true;
    }
}

}  // namespace

ENJIN_TEST(WorldMatrixThreadSafety, test_a_worker_thread_gets_the_correct_matrix) {
    // Arrange — one child under a parent, cache cold.
    ECS::World world;
    std::vector<ECS::Entity> children;
    ECS::Entity parent = MakeFamily(world, children, 1);
    DirtyAll(world, {parent, children[0]});

    // Act — compute it from a worker, not the owner thread.
    Math::Matrix4 fromWorker;
    std::thread t([&] { fromWorker = ECS::ComputeWorldMatrix(&world, children[0]); });
    t.join();

    // Assert — parent at x=10, child at y=1, so the world position is (10,1,0).
    // Refusing to cache must not mean refusing to answer.
    ENJIN_EXPECT_FLOAT_EQ(fromWorker.m[12], 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(fromWorker.m[13], 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(fromWorker.m[14], 0.0f);
}

ENJIN_TEST(WorldMatrixThreadSafety, test_a_worker_thread_does_not_write_the_cache) {
    // Arrange
    ECS::World world;
    std::vector<ECS::Entity> children;
    ECS::Entity parent = MakeFamily(world, children, 1);
    DirtyAll(world, {parent, children[0]});

    // Act
    std::thread t([&] { (void)ECS::ComputeWorldMatrix(&world, children[0]); });
    t.join();

    // Assert — both the child AND the parent it walked through must still be
    // dirty. The parent is the one that matters: it is the shared write.
    ENJIN_EXPECT_TRUE(world.GetComponent<ECS::TransformComponent>(children[0])->worldMatrixDirty);
    ENJIN_EXPECT_TRUE(world.GetComponent<ECS::TransformComponent>(parent)->worldMatrixDirty);
}

ENJIN_TEST(WorldMatrixThreadSafety, test_the_owner_thread_still_caches) {
    // Arrange — the memoisation is the whole reason this function exists, so
    // refusing to cache off-thread must not have disabled it on-thread.
    ECS::World world;
    std::vector<ECS::Entity> children;
    ECS::Entity parent = MakeFamily(world, children, 1);
    DirtyAll(world, {parent, children[0]});

    // Act
    (void)ECS::ComputeWorldMatrix(&world, children[0]);

    // Assert — the walk cleans the child and every ancestor it passed through.
    ENJIN_EXPECT_TRUE(!world.GetComponent<ECS::TransformComponent>(children[0])->worldMatrixDirty);
    ENJIN_EXPECT_TRUE(!world.GetComponent<ECS::TransformComponent>(parent)->worldMatrixDirty);
}

ENJIN_TEST(WorldMatrixThreadSafety, test_many_workers_sharing_one_parent_agree) {
    // Arrange — the shape of the real bug: sibling entities under one parent,
    // fanned across threads, cache cold. Before the fix every one of these
    // threads wrote the parent's matrix concurrently.
    ECS::World world;
    std::vector<ECS::Entity> children;
    ECS::Entity parent = MakeFamily(world, children, 32);
    std::vector<ECS::Entity> all = children;
    all.push_back(parent);

    std::vector<Math::Matrix4> results(children.size());
    std::atomic<int> ready{0};

    // Act — run it repeatedly, re-dirtying between rounds, because a race that
    // needs two threads in the same window will not show on a single attempt.
    bool allCorrect = true;
    for (int round = 0; round < 40 && allCorrect; ++round) {
        DirtyAll(world, all);
        ready = 0;
        std::vector<std::thread> threads;
        for (usize i = 0; i < children.size(); ++i) {
            threads.emplace_back([&, i] {
                ++ready;
                while (ready.load() < static_cast<int>(children.size())) {}   // start together
                results[i] = ECS::ComputeWorldMatrix(&world, children[i]);
            });
        }
        for (auto& th : threads) th.join();

        for (usize i = 0; i < children.size(); ++i) {
            if (results[i].m[12] != 10.0f ||
                results[i].m[13] != static_cast<f32>(i + 1) ||
                results[i].m[14] != 0.0f) {
                allCorrect = false;
                break;
            }
        }
    }

    // Assert
    ENJIN_EXPECT_TRUE(allCorrect);
}

ENJIN_TEST_MAIN()
