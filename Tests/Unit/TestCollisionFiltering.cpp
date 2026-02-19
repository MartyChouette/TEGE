#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Scene/SceneSerializer.h"

using namespace Enjin;
using namespace Enjin::ECS;

// ===========================================================================
// Helper: bilateral collision check (matches engine logic)
// ===========================================================================

static bool ShouldCollide(u32 aCatBits, u32 aColMask, u32 bCatBits, u32 bColMask) {
    return (aCatBits & bColMask) != 0 && (bCatBits & aColMask) != 0;
}

// ===========================================================================
// Bilateral Bitmask Rules
// ===========================================================================

ENJIN_TEST(BilateralRules, DefaultsCollide) {
    // Both defaults: category=1, mask=0xFFFFFFFF
    ENJIN_EXPECT_TRUE(ShouldCollide(1, 0xFFFFFFFF, 1, 0xFFFFFFFF));
}

ENJIN_TEST(BilateralRules, SameGroupCollides) {
    // Both in group 4 (bit 2), both accept group 4
    ENJIN_EXPECT_TRUE(ShouldCollide(4, 4, 4, 4));
}

ENJIN_TEST(BilateralRules, MutualExclusion) {
    // A is in group 1, B is in group 2 — each only accepts its own group
    ENJIN_EXPECT_FALSE(ShouldCollide(1, 1, 2, 2));
}

ENJIN_TEST(BilateralRules, OneWayBlockIsNotEnough) {
    // A accepts B's group, but B doesn't accept A's group → no collision
    ENJIN_EXPECT_FALSE(ShouldCollide(1, 0xFFFFFFFF, 2, 2));
}

ENJIN_TEST(BilateralRules, BothWaysRequired) {
    // A in group 1 accepts group 2, B in group 2 accepts group 1 → collision
    ENJIN_EXPECT_TRUE(ShouldCollide(1, 2, 2, 1));
}

ENJIN_TEST(BilateralRules, ZeroCategoryNeverCollides) {
    // Entity with no category bits collides with nothing
    ENJIN_EXPECT_FALSE(ShouldCollide(0, 0xFFFFFFFF, 1, 0xFFFFFFFF));
}

ENJIN_TEST(BilateralRules, ZeroMaskNeverCollides) {
    // Entity that accepts nothing collides with nothing
    ENJIN_EXPECT_FALSE(ShouldCollide(1, 0, 1, 0xFFFFFFFF));
}

ENJIN_TEST(BilateralRules, MultiGroupMembership) {
    // A is in groups 1+2 (bits 0+1 = 3), B is in group 2 (bit 1 = 2)
    // A accepts group 2, B accepts group 1+2
    ENJIN_EXPECT_TRUE(ShouldCollide(3, 2, 2, 3));
}

ENJIN_TEST(BilateralRules, HighBitGroups) {
    // Using group 31 (bit 31 = 0x80000000)
    u32 group31 = 1u << 31;
    ENJIN_EXPECT_TRUE(ShouldCollide(group31, group31, group31, group31));
    ENJIN_EXPECT_FALSE(ShouldCollide(group31, group31, 1, 1));
}

ENJIN_TEST(BilateralRules, AllGroupsVsNone) {
    ENJIN_EXPECT_FALSE(ShouldCollide(0xFFFFFFFF, 0, 0xFFFFFFFF, 0));
}

// ===========================================================================
// Group Isolation Scenarios
// ===========================================================================

ENJIN_TEST(GroupIsolation, PlayerVsEnvironmentOnly) {
    // Player in group 1 (0x1), Environment in group 2 (0x2), Projectile in group 4 (0x4)
    // Player accepts environment + projectile
    u32 playerCat  = 0x1, playerMask  = 0x2 | 0x4;
    u32 envCat     = 0x2, envMask     = 0x1;  // env accepts player only
    u32 projCat    = 0x4, projMask    = 0x1;  // proj accepts player only

    ENJIN_EXPECT_TRUE(ShouldCollide(playerCat, playerMask, envCat, envMask));
    ENJIN_EXPECT_TRUE(ShouldCollide(playerCat, playerMask, projCat, projMask));
    // Env vs Projectile should NOT collide
    ENJIN_EXPECT_FALSE(ShouldCollide(envCat, envMask, projCat, projMask));
}

ENJIN_TEST(GroupIsolation, TeamBasedFiltering) {
    // Team A = group 1, Team B = group 2
    // Team A collides with Team B, not self
    u32 teamACat = 1, teamAMask = 2;
    u32 teamBCat = 2, teamBMask = 1;

    ENJIN_EXPECT_TRUE(ShouldCollide(teamACat, teamAMask, teamBCat, teamBMask));
    ENJIN_EXPECT_FALSE(ShouldCollide(teamACat, teamAMask, teamACat, teamAMask)); // self
    ENJIN_EXPECT_FALSE(ShouldCollide(teamBCat, teamBMask, teamBCat, teamBMask)); // self
}

// ===========================================================================
// Component Defaults
// ===========================================================================

ENJIN_TEST(ComponentDefaults, BoxColliderDefaults) {
    BoxColliderComponent box;
    ENJIN_EXPECT_EQ(box.categoryBits, 1u);
    ENJIN_EXPECT_EQ(box.collisionMask, 0xFFFFFFFFu);
    ENJIN_EXPECT_FALSE(box.isTrigger);
    ENJIN_EXPECT_FLOAT_EQ(box.friction, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(box.bounciness, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(box.size.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(box.size.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(box.size.z, 1.0f);
}

ENJIN_TEST(ComponentDefaults, SphereColliderDefaults) {
    SphereColliderComponent sphere;
    ENJIN_EXPECT_EQ(sphere.categoryBits, 1u);
    ENJIN_EXPECT_EQ(sphere.collisionMask, 0xFFFFFFFFu);
    ENJIN_EXPECT_FALSE(sphere.isTrigger);
    ENJIN_EXPECT_FLOAT_EQ(sphere.radius, 0.5f);
}

ENJIN_TEST(ComponentDefaults, CapsuleColliderDefaults) {
    CapsuleColliderComponent capsule;
    ENJIN_EXPECT_EQ(capsule.categoryBits, 1u);
    ENJIN_EXPECT_EQ(capsule.collisionMask, 0xFFFFFFFFu);
    ENJIN_EXPECT_FALSE(capsule.isTrigger);
    ENJIN_EXPECT_FLOAT_EQ(capsule.radius, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(capsule.height, 2.0f);
    ENJIN_EXPECT_EQ((int)capsule.direction, (int)CapsuleColliderComponent::Direction::Y);
}

// ===========================================================================
// ECS Wiring
// ===========================================================================

ENJIN_TEST(ECSWiring, AddAndRetrieveBoxCollider) {
    World world;
    Entity e = world.CreateEntity();
    auto& box = world.AddComponent<BoxColliderComponent>(e);
    box.categoryBits = 0x4;
    box.collisionMask = 0x8;
    box.isTrigger = true;

    auto* retrieved = world.GetComponent<BoxColliderComponent>(e);
    ENJIN_ASSERT_NOT_NULL(retrieved);
    ENJIN_EXPECT_EQ(retrieved->categoryBits, 0x4u);
    ENJIN_EXPECT_EQ(retrieved->collisionMask, 0x8u);
    ENJIN_EXPECT_TRUE(retrieved->isTrigger);
}

ENJIN_TEST(ECSWiring, MultipleColliderTypes) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<BoxColliderComponent>(e).categoryBits = 0x1;
    world.AddComponent<SphereColliderComponent>(e).categoryBits = 0x2;
    world.AddComponent<CapsuleColliderComponent>(e).categoryBits = 0x4;

    ENJIN_EXPECT_EQ(world.GetComponent<BoxColliderComponent>(e)->categoryBits, 0x1u);
    ENJIN_EXPECT_EQ(world.GetComponent<SphereColliderComponent>(e)->categoryBits, 0x2u);
    ENJIN_EXPECT_EQ(world.GetComponent<CapsuleColliderComponent>(e)->categoryBits, 0x4u);
}

// ===========================================================================
// Serialization Round-Trip
// ===========================================================================

ENJIN_TEST(Serialization, BoxColliderRoundTrip) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    auto& box = world.AddComponent<BoxColliderComponent>(e);
    box.categoryBits = 0xDEAD;
    box.collisionMask = 0xBEEF;
    box.isTrigger = true;
    box.friction = 0.75f;
    box.bounciness = 0.3f;
    box.size = Math::Vector3(2.0f, 3.0f, 4.0f);
    box.center = Math::Vector3(0.5f, 1.0f, -0.5f);

    Scene::SceneSerializer serializer(&world);
    std::string json = serializer.SaveToString();
    ENJIN_EXPECT_GT(json.size(), (size_t)0);

    World world2;
    Scene::SceneSerializer deserializer(&world2);
    auto result = deserializer.LoadFromString(json);
    ENJIN_ASSERT_TRUE(result.success);
    ENJIN_ASSERT_TRUE(result.entities.size() > 0);

    auto* box2 = world2.GetComponent<BoxColliderComponent>(result.entities[0]);
    ENJIN_ASSERT_NOT_NULL(box2);
    ENJIN_EXPECT_EQ(box2->categoryBits, 0xDEADu);
    ENJIN_EXPECT_EQ(box2->collisionMask, 0xBEEFu);
    ENJIN_EXPECT_TRUE(box2->isTrigger);
    ENJIN_EXPECT_FLOAT_EQ(box2->friction, 0.75f);
    ENJIN_EXPECT_FLOAT_EQ(box2->bounciness, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(box2->size.x, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(box2->size.y, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(box2->size.z, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(box2->center.x, 0.5f);
}

ENJIN_TEST(Serialization, SphereColliderRoundTrip) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    auto& sphere = world.AddComponent<SphereColliderComponent>(e);
    sphere.categoryBits = 0x10;
    sphere.collisionMask = 0x20;
    sphere.radius = 2.5f;
    sphere.isTrigger = true;

    Scene::SceneSerializer serializer(&world);
    std::string json = serializer.SaveToString();

    World world2;
    Scene::SceneSerializer deserializer(&world2);
    auto result = deserializer.LoadFromString(json);
    ENJIN_ASSERT_TRUE(result.success);

    auto* s2 = world2.GetComponent<SphereColliderComponent>(result.entities[0]);
    ENJIN_ASSERT_NOT_NULL(s2);
    ENJIN_EXPECT_EQ(s2->categoryBits, 0x10u);
    ENJIN_EXPECT_EQ(s2->collisionMask, 0x20u);
    ENJIN_EXPECT_FLOAT_EQ(s2->radius, 2.5f);
    ENJIN_EXPECT_TRUE(s2->isTrigger);
}

ENJIN_TEST(Serialization, CapsuleColliderRoundTrip) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    auto& cap = world.AddComponent<CapsuleColliderComponent>(e);
    cap.categoryBits = 0xFF;
    cap.collisionMask = 0x00;
    cap.radius = 1.5f;
    cap.height = 4.0f;
    cap.direction = CapsuleColliderComponent::Direction::Z;

    Scene::SceneSerializer serializer(&world);
    std::string json = serializer.SaveToString();

    World world2;
    Scene::SceneSerializer deserializer(&world2);
    auto result = deserializer.LoadFromString(json);
    ENJIN_ASSERT_TRUE(result.success);

    auto* c2 = world2.GetComponent<CapsuleColliderComponent>(result.entities[0]);
    ENJIN_ASSERT_NOT_NULL(c2);
    ENJIN_EXPECT_EQ(c2->categoryBits, 0xFFu);
    ENJIN_EXPECT_EQ(c2->collisionMask, 0x00u);
    ENJIN_EXPECT_FLOAT_EQ(c2->radius, 1.5f);
    ENJIN_EXPECT_FLOAT_EQ(c2->height, 4.0f);
    ENJIN_EXPECT_EQ((int)c2->direction, (int)CapsuleColliderComponent::Direction::Z);
}

// ===========================================================================
// Trigger Semantics
// ===========================================================================

ENJIN_TEST(TriggerSemantics, TriggerStillUsesCollisionMask) {
    // Triggers participate in collision filtering but don't generate contact forces.
    // The bitmask should still be checked.
    BoxColliderComponent triggerA;
    triggerA.isTrigger = true;
    triggerA.categoryBits = 0x1;
    triggerA.collisionMask = 0x2;

    BoxColliderComponent solidB;
    solidB.categoryBits = 0x2;
    solidB.collisionMask = 0x1;

    ENJIN_EXPECT_TRUE(ShouldCollide(triggerA.categoryBits, triggerA.collisionMask,
                                     solidB.categoryBits, solidB.collisionMask));

    // Flip B's mask to exclude A
    solidB.collisionMask = 0x4;
    ENJIN_EXPECT_FALSE(ShouldCollide(triggerA.categoryBits, triggerA.collisionMask,
                                      solidB.categoryBits, solidB.collisionMask));
}

ENJIN_TEST_MAIN()
