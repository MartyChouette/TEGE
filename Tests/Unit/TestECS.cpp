#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"

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

ENJIN_TEST_MAIN()
