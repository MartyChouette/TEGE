// A destructible drops the pickups it was authored to drop, and difficulty
// scales how many.
//
// It dropped none. DestructibleComponent::spawnPickup, pickupId and
// pickupCount were serialized, shown in the inspector, and read by NOTHING, so
// a crate authored to drop three coins broke open and dropped nothing on every
// platform. The only hits for pickupCount in the whole engine were the two
// lines of SceneSerializer that write and read it.
//
// That also left DynamicDifficultyComponent::resourceDropMultiplier with
// nothing to multiply: the system computed it every frame, the inspector
// displayed it, a script could read it, and no drop existed for it to scale.
// Both halves are wired now, which is why they are tested together.
//
// These drive DestructibleSystem rather than the serializer, because the gap
// was between "the fields are stored" and "something acts on them".
#include "EnjinTest.h"
#include "Enjin/Effects/Destructible.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/DynamicDifficulty.h"

#include <string>

using namespace Enjin;

namespace {

// A destructible that dies in one hit and is authored to drop `count` of `id`.
ECS::Entity MakeCrate(ECS::World& world, const char* id, i32 count) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent t;
    t.position = Math::Vector3(5.0f, 0.0f, -3.0f);
    world.AddComponent<ECS::TransformComponent>(e, t);

    ECS::DestructibleComponent dc;
    dc.health = 1.0f;
    dc.destroyOnHit = true;
    dc.spawnPickup = true;
    dc.pickupId = id;
    dc.pickupCount = count;
    world.AddComponent<ECS::DestructibleComponent>(e, dc);
    return e;
}

// Difficulty is read off the first ENABLED component, the same rule the
// difficulty system itself uses.
void SetDropMultiplier(ECS::World& world, f32 mult, bool enabled = true) {
    ECS::Entity e = world.CreateEntity();
    ECS::DynamicDifficultyComponent dd;
    dd.enabled = enabled;
    dd.resourceDropMultiplier = mult;
    world.AddComponent<ECS::DynamicDifficultyComponent>(e, dd);
}

usize CountPickups(ECS::World& world) {
    return world.GetEntitiesWithComponent<ECS::PickupComponent>().size();
}

// Destroy the crate and let the system process its queue.
void BreakIt(Effects::DestructibleSystem& sys, ECS::Entity crate) {
    sys.ApplyDamage(crate, 10.0f);
    sys.Update(0.016f);
}

} // namespace

ENJIN_TEST(DestructibleDrops, test_a_crate_drops_the_pickups_it_was_authored_with) {
    // THE regression. This was zero.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    ECS::Entity crate = MakeCrate(world, "coin", 3);

    // Act
    BreakIt(sys, crate);

    // Assert
    ENJIN_EXPECT_EQ(CountPickups(world), static_cast<usize>(3));
}

ENJIN_TEST(DestructibleDrops, test_the_drop_carries_the_authored_id) {
    // A drop nobody can identify is the same as no drop: whatever collects it
    // needs to know which item it was.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    ECS::Entity crate = MakeCrate(world, "brass_key", 1);

    // Act
    BreakIt(sys, crate);

    // Assert
    const auto pickups = world.GetEntitiesWithComponent<ECS::PickupComponent>();
    ENJIN_ASSERT_TRUE(pickups.size() == 1);
    auto* p = world.GetComponent<ECS::PickupComponent>(pickups[0]);
    ENJIN_ASSERT_TRUE(p != nullptr);
    ENJIN_EXPECT_TRUE(p->customId == std::string("brass_key"));
}

ENJIN_TEST(DestructibleDrops, test_a_crate_with_spawnPickup_off_drops_nothing) {
    // The flag has to still mean something.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    ECS::Entity crate = MakeCrate(world, "coin", 3);
    world.GetComponent<ECS::DestructibleComponent>(crate)->spawnPickup = false;

    // Act
    BreakIt(sys, crate);

    // Assert
    ENJIN_EXPECT_EQ(CountPickups(world), static_cast<usize>(0));
}

ENJIN_TEST(DestructibleDrops, test_the_difficulty_multiplier_scales_the_count) {
    // The other half: resourceDropMultiplier had no drop to scale.
    // 2 authored x 2.0 = 4.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    SetDropMultiplier(world, 2.0f);
    ECS::Entity crate = MakeCrate(world, "coin", 2);

    // Act
    BreakIt(sys, crate);

    // Assert
    ENJIN_EXPECT_EQ(CountPickups(world), static_cast<usize>(4));
}

ENJIN_TEST(DestructibleDrops, test_the_multiplier_rounds_rather_than_truncates) {
    // 1 coin at 1.5x is 2, not 1. Truncating would make every multiplier below
    // 2.0 invisible on a single-item drop, which is most of them.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    SetDropMultiplier(world, 1.5f);
    ECS::Entity crate = MakeCrate(world, "coin", 1);

    // Act
    BreakIt(sys, crate);

    // Assert
    ENJIN_EXPECT_EQ(CountPickups(world), static_cast<usize>(2));
}

ENJIN_TEST(DestructibleDrops, test_a_disabled_difficulty_component_does_not_scale) {
    // Difficulty that is switched off must not quietly halve a game's loot.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    SetDropMultiplier(world, 0.5f, /*enabled*/ false);
    ECS::Entity crate = MakeCrate(world, "coin", 4);

    // Act
    BreakIt(sys, crate);

    // Assert
    ENJIN_EXPECT_EQ(CountPickups(world), static_cast<usize>(4));
}

ENJIN_TEST(DestructibleDrops, test_the_drops_do_not_all_stack_on_one_point) {
    // Pickups have a magnet range and a pickup range, so three coins at
    // identical coordinates read as one coin to a player and to the collect
    // logic. They are scattered on a ring.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    ECS::Entity crate = MakeCrate(world, "coin", 3);

    // Act
    BreakIt(sys, crate);

    // Assert
    const auto pickups = world.GetEntitiesWithComponent<ECS::PickupComponent>();
    ENJIN_ASSERT_TRUE(pickups.size() == 3);
    const auto* a = world.GetComponent<ECS::TransformComponent>(pickups[0]);
    const auto* b = world.GetComponent<ECS::TransformComponent>(pickups[1]);
    ENJIN_ASSERT_TRUE(a && b);
    const f32 dx = a->position.x - b->position.x;
    const f32 dz = a->position.z - b->position.z;
    ENJIN_EXPECT_TRUE((dx * dx + dz * dz) > 0.0001f);
}

ENJIN_TEST(DestructibleDrops, test_drops_land_at_the_crate_not_at_the_origin) {
    // The crate is at (5, 0, -3). Dropping at the world origin would look like
    // loot vanishing, and is the kind of thing a scattered ring can hide.
    // Arrange
    ECS::World world;
    Effects::DestructibleSystem sys;
    sys.Initialize(&world);
    ECS::Entity crate = MakeCrate(world, "coin", 1);

    // Act
    BreakIt(sys, crate);

    // Assert
    const auto pickups = world.GetEntitiesWithComponent<ECS::PickupComponent>();
    ENJIN_ASSERT_TRUE(pickups.size() == 1);
    const auto* t = world.GetComponent<ECS::TransformComponent>(pickups[0]);
    ENJIN_ASSERT_TRUE(t != nullptr);
    ENJIN_EXPECT_TRUE(std::abs(t->position.x - 5.0f) < 1.0f);
    ENJIN_EXPECT_TRUE(std::abs(t->position.z + 3.0f) < 1.0f);
}

ENJIN_TEST_MAIN()
