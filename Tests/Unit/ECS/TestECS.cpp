#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/ECS/StringIntern.h"
#include <atomic>
#include <thread>
#include <vector>

using namespace Enjin;
using namespace Enjin::ECS;

// ===========================================================================
// Entity Lifecycle
// ===========================================================================

ENJIN_TEST(EntityLifecycle, CreateUniqueIDs) {
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    Entity e3 = world.CreateEntity();
    ENJIN_EXPECT_NE(e1, e2);
    ENJIN_EXPECT_NE(e2, e3);
    ENJIN_EXPECT_NE(e1, e3);
    ENJIN_EXPECT_NE(e1, INVALID_ENTITY);
}

ENJIN_TEST(EntityLifecycle, DestroyDeferred) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    world.DestroyEntity(e);
    // Entity should be pending destruction but still technically present until flush
    ENJIN_EXPECT_TRUE(world.IsPendingDestruction(e));
    world.FlushPendingDestructions();
    ENJIN_EXPECT_FALSE(world.IsValid(e));
}

ENJIN_TEST(EntityLifecycle, DestroyImmediate) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    world.DestroyEntityImmediate(e);
    ENJIN_EXPECT_FALSE(world.IsValid(e));
    ENJIN_EXPECT_NULL(world.GetComponent<TransformComponent>(e));
}

ENJIN_TEST(EntityLifecycle, Clear) {
    World world;
    world.CreateEntity();
    world.CreateEntity();
    world.CreateEntity();
    ENJIN_EXPECT_GE(world.GetEntityCount(), (usize)3);
    world.Clear();
    ENJIN_EXPECT_EQ(world.GetEntityCount(), (usize)0);
}

ENJIN_TEST(EntityLifecycle, InvalidEntityIsNotValid) {
    World world;
    ENJIN_EXPECT_FALSE(world.IsValid(INVALID_ENTITY));
    ENJIN_EXPECT_FALSE(world.IsPendingDestruction(INVALID_ENTITY));
}

ENJIN_TEST(EntityLifecycle, DoubleDestroyIsSafe) {
    World world;
    Entity e = world.CreateEntity();
    world.DestroyEntity(e);
    world.DestroyEntity(e); // Should not crash or double-queue
    ENJIN_EXPECT_TRUE(world.IsPendingDestruction(e));
    world.FlushPendingDestructions();
    ENJIN_EXPECT_FALSE(world.IsValid(e));
}

ENJIN_TEST(EntityLifecycle, DestroyInvalidEntityIsSafe) {
    World world;
    world.DestroyEntity(INVALID_ENTITY); // Should not crash
    world.DestroyEntity(9999);           // Non-existent entity
    world.DestroyEntityImmediate(INVALID_ENTITY);
    world.DestroyEntityImmediate(9999);
    ENJIN_EXPECT_EQ(world.GetEntityCount(), (usize)0);
}

ENJIN_TEST(EntityLifecycle, DeferredDestroyNotVisibleToIsValid) {
    World world;
    Entity e = world.CreateEntity();
    world.DestroyEntity(e);
    // IsValid should return false even before flush (pending destruction)
    ENJIN_EXPECT_FALSE(world.IsValid(e));
}

ENJIN_TEST(EntityLifecycle, FlushPendingOnUpdate) {
    World world;
    Entity e = world.CreateEntity();
    world.DestroyEntity(e);
    ENJIN_EXPECT_TRUE(world.IsPendingDestruction(e));
    world.Update(0.016f); // Should flush pending
    ENJIN_EXPECT_FALSE(world.IsPendingDestruction(e));
    ENJIN_EXPECT_FALSE(world.IsValid(e));
}

ENJIN_TEST(EntityLifecycle, ClearFlushesPending) {
    World world;
    Entity e = world.CreateEntity();
    world.DestroyEntity(e);
    world.Clear(); // Should clear pending too
    ENJIN_EXPECT_FALSE(world.IsPendingDestruction(e));
    ENJIN_EXPECT_EQ(world.GetEntityCount(), (usize)0);
}

ENJIN_TEST(EntityLifecycle, EntityRecycling) {
    World world;
    Entity e1 = world.CreateEntity();
    world.DestroyEntityImmediate(e1);
    Entity e2 = world.CreateEntity();
    // The slot index is recycled, but the generation bumps on destroy so the
    // stale handle (e1) can never alias the new entity
    ENJIN_EXPECT_EQ(EntityIndex(e1), EntityIndex(e2));
    ENJIN_EXPECT_TRUE(e1 != e2);
    ENJIN_EXPECT_FALSE(world.IsValid(e1));
    ENJIN_EXPECT_TRUE(world.IsValid(e2));
}

ENJIN_TEST(EntityLifecycle, AddComponentToRecycledEntity) {
    // Regression for the raw-handle indexing class (c8471de fixed it in
    // RenderSystem; ComponentStorage's sparse array had the same disease).
    // A recycled entity carries generation >= 1 in the high 32 bits, so any
    // container sized by the raw u64 handle attempts a ~2^32-element resize.
    World world;
    Entity e1 = world.CreateEntity();
    world.AddComponent<TransformComponent>(e1).position = Math::Vector3(1, 1, 1);
    world.DestroyEntityImmediate(e1);

    Entity e2 = world.CreateEntity();  // recycled slot, generation bumped
    ENJIN_ASSERT_TRUE(EntityIndex(e1) == EntityIndex(e2));
    ENJIN_ASSERT_TRUE(e1 != e2);

    // Must not OOM, and must land on the new entity.
    auto& t = world.AddComponent<TransformComponent>(e2);
    t.position = Math::Vector3(2, 2, 2);

    ENJIN_EXPECT_TRUE(world.HasComponent<TransformComponent>(e2));
    ENJIN_EXPECT_FLOAT_EQ(world.GetComponent<TransformComponent>(e2)->position.x, 2.0f);
    // The stale handle must not alias the recycled slot's new component.
    ENJIN_EXPECT_TRUE(world.GetComponent<TransformComponent>(e1) == nullptr);
    ENJIN_EXPECT_FALSE(world.HasComponent<TransformComponent>(e1));
}

ENJIN_TEST(EntityLifecycle, MassCreateDestroy) {
    World world;
    std::vector<Entity> entities;
    for (int i = 0; i < 1000; i++) {
        entities.push_back(world.CreateEntity());
    }
    ENJIN_EXPECT_EQ(world.GetEntityCount(), (usize)1000);
    for (Entity e : entities) {
        world.DestroyEntity(e);
    }
    world.FlushPendingDestructions();
    ENJIN_EXPECT_EQ(world.GetEntityCount(), (usize)0);
}

// ===========================================================================
// Component CRUD
// ===========================================================================

ENJIN_TEST(ComponentCRUD, AddAndGet) {
    World world;
    Entity e = world.CreateEntity();
    TransformComponent tc;
    tc.position = Math::Vector3(1.0f, 2.0f, 3.0f);
    world.AddComponent<TransformComponent>(e, tc);

    auto* got = world.GetComponent<TransformComponent>(e);
    ENJIN_ASSERT_NOT_NULL(got);
    ENJIN_EXPECT_FLOAT_EQ(got->position.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(got->position.y, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(got->position.z, 3.0f);
}

ENJIN_TEST(ComponentCRUD, HasComponent) {
    World world;
    Entity e = world.CreateEntity();
    ENJIN_EXPECT_FALSE(world.HasComponent<TransformComponent>(e));
    world.AddComponent<TransformComponent>(e);
    ENJIN_EXPECT_TRUE(world.HasComponent<TransformComponent>(e));
}

ENJIN_TEST(ComponentCRUD, RemoveComponent) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    ENJIN_EXPECT_TRUE(world.HasComponent<TransformComponent>(e));
    world.RemoveComponent<TransformComponent>(e);
    ENJIN_EXPECT_FALSE(world.HasComponent<TransformComponent>(e));
    ENJIN_EXPECT_NULL(world.GetComponent<TransformComponent>(e));
}

ENJIN_TEST(ComponentCRUD, OverwriteComponent) {
    World world;
    Entity e = world.CreateEntity();
    TransformComponent tc1;
    tc1.position = Math::Vector3(1.0f, 0.0f, 0.0f);
    world.AddComponent<TransformComponent>(e, tc1);

    TransformComponent tc2;
    tc2.position = Math::Vector3(99.0f, 0.0f, 0.0f);
    world.AddComponent<TransformComponent>(e, tc2);

    auto* got = world.GetComponent<TransformComponent>(e);
    ENJIN_ASSERT_NOT_NULL(got);
    ENJIN_EXPECT_FLOAT_EQ(got->position.x, 99.0f);
}

ENJIN_TEST(ComponentCRUD, GetMissingReturnsNull) {
    World world;
    Entity e = world.CreateEntity();
    ENJIN_EXPECT_NULL(world.GetComponent<TransformComponent>(e));
}

ENJIN_TEST(ComponentCRUD, GetNeverUsedTypeReturnsNull) {
    // GetComponent for a type that was never AddComponent'd should return null
    // without allocating storage (audit finding #3)
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    // NameComponent was never used — should return null, not create storage
    ENJIN_EXPECT_NULL(world.GetComponent<NameComponent>(e));
    ENJIN_EXPECT_FALSE(world.HasComponent<NameComponent>(e));
}

ENJIN_TEST(ComponentCRUD, RemoveMissingComponentIsSafe) {
    World world;
    Entity e = world.CreateEntity();
    // Removing a component that was never added should not crash
    world.RemoveComponent<TransformComponent>(e);
    ENJIN_EXPECT_FALSE(world.HasComponent<TransformComponent>(e));
}

ENJIN_TEST(ComponentCRUD, ComponentSurvivesOtherEntityDestroy) {
    // Swap-and-pop removal: ensure surviving entities' components remain valid
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    Entity e3 = world.CreateEntity();
    TransformComponent tc1; tc1.position = Math::Vector3(1.0f, 0.0f, 0.0f);
    TransformComponent tc2; tc2.position = Math::Vector3(2.0f, 0.0f, 0.0f);
    TransformComponent tc3; tc3.position = Math::Vector3(3.0f, 0.0f, 0.0f);
    world.AddComponent<TransformComponent>(e1, tc1);
    world.AddComponent<TransformComponent>(e2, tc2);
    world.AddComponent<TransformComponent>(e3, tc3);

    // Destroy middle entity
    world.DestroyEntityImmediate(e2);

    // e1 and e3 should still have correct components
    auto* g1 = world.GetComponent<TransformComponent>(e1);
    auto* g3 = world.GetComponent<TransformComponent>(e3);
    ENJIN_ASSERT_NOT_NULL(g1);
    ENJIN_ASSERT_NOT_NULL(g3);
    ENJIN_EXPECT_FLOAT_EQ(g1->position.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(g3->position.x, 3.0f);
}

ENJIN_TEST(ComponentCRUD, DestroyEntityClearsAllComponents) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    world.AddComponent<NameComponent>(e, NameComponent("Test"));
    world.DestroyEntityImmediate(e);
    ENJIN_EXPECT_NULL(world.GetComponent<TransformComponent>(e));
    ENJIN_EXPECT_NULL(world.GetComponent<NameComponent>(e));
}

// ===========================================================================
// Entity Queries
// ===========================================================================

ENJIN_TEST(EntityQuery, GetEntitiesWithComponent) {
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    Entity e3 = world.CreateEntity();
    world.AddComponent<TransformComponent>(e1);
    world.AddComponent<TransformComponent>(e2);
    // e3 has no TransformComponent

    auto& entities = world.GetEntitiesWithComponent<TransformComponent>();
    ENJIN_EXPECT_EQ(entities.size(), (size_t)2);
}

ENJIN_TEST(EntityQuery, GetEntitiesWithTwoComponents) {
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    Entity e3 = world.CreateEntity();
    world.AddComponent<TransformComponent>(e1);
    world.AddComponent<NameComponent>(e1, NameComponent("A"));
    world.AddComponent<TransformComponent>(e2);
    // e2 has Transform but no Name
    world.AddComponent<NameComponent>(e3, NameComponent("C"));
    // e3 has Name but no Transform

    auto entities = world.GetEntitiesWithComponents<TransformComponent, NameComponent>();
    ENJIN_EXPECT_EQ(entities.size(), (size_t)1);
}

ENJIN_TEST(EntityQuery, FindEntityByName) {
    World world;
    Entity e1 = world.CreateEntity();
    world.AddComponent<NameComponent>(e1, NameComponent("Player"));

    Entity found = world.FindEntityByName("Player");
    ENJIN_EXPECT_EQ(found, e1);

    Entity notFound = world.FindEntityByName("NonExistent");
    ENJIN_EXPECT_EQ(notFound, INVALID_ENTITY);
}

ENJIN_TEST(EntityQuery, FindEntityByNameAfterDestroy) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<NameComponent>(e, NameComponent("Temp"));

    ENJIN_EXPECT_EQ(world.FindEntityByName("Temp"), e);

    world.DestroyEntityImmediate(e);
    world.InvalidateNameCache();

    ENJIN_EXPECT_EQ(world.FindEntityByName("Temp"), INVALID_ENTITY);
}

ENJIN_TEST(EntityQuery, GetEntitiesEmptyWorld) {
    World world;
    auto& entities = world.GetEntitiesWithComponent<TransformComponent>();
    ENJIN_EXPECT_EQ(entities.size(), (size_t)0);
    ENJIN_EXPECT_EQ(world.GetAllEntities().size(), (size_t)0);
}

ENJIN_TEST(EntityQuery, GetEntitiesAfterClear) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    world.Clear();
    auto& entities = world.GetEntitiesWithComponent<TransformComponent>();
    ENJIN_EXPECT_EQ(entities.size(), (size_t)0);
}

ENJIN_TEST(EntityQuery, DuplicateNameReturnsLast) {
    // Name cache stores last entity with a given name
    World world;
    Entity e1 = world.CreateEntity();
    Entity e2 = world.CreateEntity();
    world.AddComponent<NameComponent>(e1, NameComponent("Same"));
    world.AddComponent<NameComponent>(e2, NameComponent("Same"));
    world.InvalidateNameCache();
    Entity found = world.FindEntityByName("Same");
    // Should return one of them (implementation-defined which, but not INVALID)
    ENJIN_EXPECT_NE(found, INVALID_ENTITY);
}

// ===========================================================================
// Hierarchy (parent/child cleanup on destroy)
// ===========================================================================

ENJIN_TEST(Hierarchy, DestroyParentReparentsChildren) {
    World world;
    Entity parent = world.CreateEntity();
    Entity child = world.CreateEntity();
    world.AddComponent<ChildrenComponent>(parent).children = {child};
    world.AddComponent<ParentComponent>(child).parent = parent;

    world.DestroyEntityImmediate(parent);

    // Child should survive but lose its parent
    ENJIN_EXPECT_TRUE(world.IsValid(child));
    ENJIN_EXPECT_FALSE(world.HasComponent<ParentComponent>(child));
}

ENJIN_TEST(Hierarchy, DestroyChildUpdatesParent) {
    World world;
    Entity parent = world.CreateEntity();
    Entity child1 = world.CreateEntity();
    Entity child2 = world.CreateEntity();
    world.AddComponent<ChildrenComponent>(parent).children = {child1, child2};
    world.AddComponent<ParentComponent>(child1).parent = parent;
    world.AddComponent<ParentComponent>(child2).parent = parent;

    world.DestroyEntityImmediate(child1);

    auto* cc = world.GetComponent<ChildrenComponent>(parent);
    ENJIN_ASSERT_NOT_NULL(cc);
    ENJIN_EXPECT_EQ(cc->children.size(), (size_t)1);
    ENJIN_EXPECT_EQ(cc->children[0], child2);
}

// ===========================================================================
// Lock-free reads (adr-0004): fork-join concurrency
// ===========================================================================

ENJIN_TEST(ComponentThreadSafety, ConcurrentReadsAreConsistentAndRaceFree) {
    // adr-0004: GetComponent/HasComponent are lock-free. This mirrors the engine's
    // fork-join pattern — the owner (this) thread fully populates the world, then
    // parks at the join while N worker threads ONLY read. No structural mutation
    // happens during the parallel region, so every read must observe stable, correct
    // data across multiple component storages with no crash and no torn value.
    // Under a thread sanitizer this also proves the read path is race-free for the
    // intended usage; if a lock were still required for correctness, the removed
    // guard would surface here.

    // Arrange: populate two storages so the concurrent map lookups traverse >1 entry.
    World world;
    constexpr int kEntities = 2000;
    std::vector<Entity> entities;
    entities.reserve(kEntities);
    for (int i = 0; i < kEntities; ++i) {
        Entity e = world.CreateEntity();
        TransformComponent tc;
        tc.position = Math::Vector3((f32)i, (f32)(i * 2), (f32)(i * 3));
        world.AddComponent<TransformComponent>(e, tc);
        if (i % 2 == 0) {
            world.AddComponent<NameComponent>(e, NameComponent("even"));
        }
        entities.push_back(e);
    }

    // Act: fan lock-free reads across worker threads while the owner is parked here.
    constexpr int kThreads = 8;
    constexpr int kPasses = 50;
    std::atomic<int> missing{0};
    std::atomic<int> mismatches{0};
    std::atomic<int> nameMismatch{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&]() {
            for (int pass = 0; pass < kPasses; ++pass) {
                for (int i = 0; i < kEntities; ++i) {
                    Entity e = entities[i];
                    if (!world.HasComponent<TransformComponent>(e)) { missing++; continue; }
                    const auto* tc = world.GetComponent<TransformComponent>(e);
                    if (!tc) { missing++; continue; }
                    if (tc->position.x != (f32)i) mismatches++;
                    // Odd entities never got a NameComponent — the miss path (null
                    // storage / sparse gap) must also be safe under concurrency.
                    const bool hasName = world.HasComponent<NameComponent>(e);
                    if (hasName != (i % 2 == 0)) nameMismatch++;
                }
            }
        });
    }
    for (auto& w : workers) w.join();

    // Assert: every read landed on correct, stable data.
    ENJIN_EXPECT_EQ(missing.load(), 0);
    ENJIN_EXPECT_EQ(mismatches.load(), 0);
    ENJIN_EXPECT_EQ(nameMismatch.load(), 0);

    // The owner thread resumes mutation after the join — the world is not wedged.
    Entity extra = world.CreateEntity();
    world.AddComponent<TransformComponent>(extra);
    ENJIN_EXPECT_TRUE(world.HasComponent<TransformComponent>(extra));
}

// ===========================================================================
// Entity destroy observer (PlayMode incremental capture mechanism)
// ===========================================================================

ENJIN_TEST(EntityDestroyObserver, FiresWithIntactDataBeforeComponentRemoval) {
    // The observer must fire BEFORE components are removed, so a listener (PlayMode)
    // can serialize the dying entity while its data is still intact. This is the hook
    // that lets Stop recreate exactly the entities destroyed during play.
    World world;
    Entity e = world.CreateEntity();
    TransformComponent tc; tc.position = Math::Vector3(7.0f, 0.0f, 0.0f);
    world.AddComponent<TransformComponent>(e, tc);
    world.AddComponent<NameComponent>(e, NameComponent("victim"));

    int fireCount = 0;
    bool sawIntactData = false;
    world.SetEntityDestroyObserver([&](Entity observed) {
        ++fireCount;
        auto* t = world.GetComponent<TransformComponent>(observed);
        auto* n = world.GetComponent<NameComponent>(observed);
        sawIntactData = (t && t->position.x == 7.0f && n && n->name == "victim");
    });

    world.DestroyEntityImmediate(e);

    ENJIN_EXPECT_EQ(fireCount, 1);
    ENJIN_EXPECT_TRUE(sawIntactData);
    ENJIN_EXPECT_NULL(world.GetComponent<TransformComponent>(e));  // gone after destroy
}

ENJIN_TEST(EntityDestroyObserver, FiresOnDeferredFlushNotOnQueue) {
    // Deferred DestroyEntity must not fire the observer until the flush actually
    // destroys the entity — that is where the data is still live.
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    int fireCount = 0;
    world.SetEntityDestroyObserver([&](Entity) { ++fireCount; });

    world.DestroyEntity(e);            // queued only
    ENJIN_EXPECT_EQ(fireCount, 0);
    world.FlushPendingDestructions();  // now the entity actually dies
    ENJIN_EXPECT_EQ(fireCount, 1);
}

ENJIN_TEST(EntityDestroyObserver, ClearStopsObservation) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    int fireCount = 0;
    world.SetEntityDestroyObserver([&](Entity) { ++fireCount; });
    world.ClearEntityDestroyObserver();
    world.DestroyEntityImmediate(e);
    ENJIN_EXPECT_EQ(fireCount, 0);
}

// ===========================================================================
// EntityEventBus
// ===========================================================================

ENJIN_TEST(EventBus, SendReceive) {
    EntityEventBus bus;
    int count = 0;
    bus.Listen("test", [&](const EntityEvent&) { count++; });
    EntityEvent evt; evt.name = "test";
    bus.Send("test", evt);
    ENJIN_EXPECT_EQ(count, 1);
}

ENJIN_TEST(EventBus, RemoveListener) {
    EntityEventBus bus;
    int count = 0;
    u32 id = bus.Listen("test", [&](const EntityEvent&) { count++; });
    bus.RemoveListener(id);
    EntityEvent evt; evt.name = "test";
    bus.Send("test", evt);
    ENJIN_EXPECT_EQ(count, 0);
}

ENJIN_TEST(EventBus, RemoveAllForEntity) {
    EntityEventBus bus;
    int count = 0;
    Entity e = 42;
    bus.Listen("test", [&](const EntityEvent&) { count++; }, e);
    bus.Listen("other", [&](const EntityEvent&) { count++; }, e);
    bus.RemoveAllForEntity(e);
    EntityEvent evt;
    bus.Send("test", evt);
    bus.Send("other", evt);
    ENJIN_EXPECT_EQ(count, 0);
}

ENJIN_TEST(EventBus, DeferredEvents) {
    EntityEventBus bus;
    int count = 0;
    bus.Listen("deferred", [&](const EntityEvent&) { count++; });
    EntityEvent evt; evt.name = "deferred";
    bus.SendDeferred("deferred", evt);
    ENJIN_EXPECT_EQ(count, 0); // Not delivered yet
    bus.ProcessDeferred();
    ENJIN_EXPECT_EQ(count, 1);
}

ENJIN_TEST(EventBus, ClearRemovesAll) {
    EntityEventBus bus;
    int count = 0;
    bus.Listen("test", [&](const EntityEvent&) { count++; });
    bus.SendDeferred("test", EntityEvent{});
    bus.Clear();
    bus.ProcessDeferred();
    EntityEvent evt;
    bus.Send("test", evt);
    ENJIN_EXPECT_EQ(count, 0);
}

ENJIN_TEST(EventBus, CallbackCanModifyListeners) {
    // Callbacks that add/remove listeners should not crash (iterator safety)
    EntityEventBus bus;
    int count = 0;
    bus.Listen("test", [&](const EntityEvent&) {
        count++;
        bus.Listen("test", [&](const EntityEvent&) { count += 10; });
    });
    EntityEvent evt;
    bus.Send("test", evt);
    ENJIN_EXPECT_EQ(count, 1); // Only original listener fires this round
}

// ===========================================================================
// StringIntern
// ===========================================================================

ENJIN_TEST(StringIntern, InternAndRetrieve) {
    auto& si = StringIntern::Instance();
    si.Clear();
    u32 id = si.Intern("hello");
    ENJIN_EXPECT_NE(id, (u32)0);
    ENJIN_EXPECT_EQ(si.GetString(id), std::string("hello"));
}

ENJIN_TEST(StringIntern, DuplicateReturnsSameId) {
    auto& si = StringIntern::Instance();
    si.Clear();
    u32 id1 = si.Intern("world");
    u32 id2 = si.Intern("world");
    ENJIN_EXPECT_EQ(id1, id2);
}

ENJIN_TEST(StringIntern, OutOfRangeReturnsEmpty) {
    auto& si = StringIntern::Instance();
    si.Clear();
    ENJIN_EXPECT_EQ(si.GetString(9999), std::string(""));
}

ENJIN_TEST(StringIntern, FindUnknownReturnsZero) {
    auto& si = StringIntern::Instance();
    si.Clear();
    ENJIN_EXPECT_EQ(si.Find("nonexistent"), (u32)0);
}

ENJIN_TEST_MAIN()
