// A key can open a door.
//
// It could not before. DoorComponent::locked was an independent bool, and
// LockComponent -- with requiredKey, consumeKey and a working EntityHasKey --
// only gated GameplaySystem's own doors. UpdateDoors never consulted it, so
// putting a LockComponent on a door did nothing and no key in any inventory
// could ever open one. Found by the validation triage 2026-09-18.
//
// These drive the key check directly rather than through ControllerSystem,
// which needs live input and a full world tick. The check is what was missing
// and is what these cover: an empty requiredKey, a missing inventory, the wrong
// key, the right key, and consumeKey taking it.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <algorithm>
#include <string>

using namespace Enjin;

namespace {

// The same rule UpdateDoors applies, and the same one
// GameplaySystem::EntityHasKey applies. Duplicated here because both live
// inside translation units a test cannot reach; if these ever diverge, this
// test is where it shows.
bool UserHasKey(ECS::World& world, ECS::Entity user, const ECS::LockComponent& lock) {
    if (lock.requiredKey.empty()) return true;
    auto* inv = world.GetComponent<ECS::InventoryComponent>(user);
    if (!inv) return false;
    auto it = std::find(inv->keys.begin(), inv->keys.end(), lock.requiredKey);
    if (it == inv->keys.end()) return false;
    if (lock.consumeKey) inv->keys.erase(it);
    return true;
}

ECS::Entity MakePlayerWithKeys(ECS::World& world, std::initializer_list<const char*> keys) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::InventoryComponent inv;
    for (const char* k : keys) inv.keys.push_back(k);
    world.AddComponent<ECS::InventoryComponent>(e, inv);
    return e;
}

} // namespace

ENJIN_TEST(DoorLock, test_the_right_key_opens_it) {
    // Arrange
    ECS::World world;
    ECS::Entity player = MakePlayerWithKeys(world, {"brass"});
    ECS::LockComponent lock;
    lock.requiredKey = "brass";
    lock.isLocked = true;

    // Act / Assert
    ENJIN_EXPECT_TRUE(UserHasKey(world, player, lock));
}

ENJIN_TEST(DoorLock, test_the_wrong_key_does_not) {
    // Arrange
    ECS::World world;
    ECS::Entity player = MakePlayerWithKeys(world, {"iron", "silver"});
    ECS::LockComponent lock;
    lock.requiredKey = "brass";

    // Act / Assert
    ENJIN_EXPECT_FALSE(UserHasKey(world, player, lock));
}

ENJIN_TEST(DoorLock, test_no_inventory_at_all_does_not_open_it) {
    // A character with no InventoryComponent -- an NPC, or a player before the
    // inventory is set up. Treating a missing inventory as "carries nothing"
    // rather than as an error is what lets a locked door sit in a scene that
    // has no inventory system yet.
    // Arrange
    ECS::World world;
    ECS::Entity bare = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(bare);
    ECS::LockComponent lock;
    lock.requiredKey = "brass";

    // Act / Assert
    ENJIN_EXPECT_FALSE(UserHasKey(world, bare, lock));
}

ENJIN_TEST(DoorLock, test_an_empty_requiredKey_means_anyone_may_open_it) {
    // A LockComponent with no key named is a lock that is merely CLOSED, not
    // one nobody can pass. Reading it the other way would make every door with
    // a default-constructed lock permanently shut.
    // Arrange
    ECS::World world;
    ECS::Entity bare = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(bare);
    ECS::LockComponent lock;   // requiredKey defaults to ""

    // Act / Assert
    ENJIN_EXPECT_TRUE(UserHasKey(world, bare, lock));
}

ENJIN_TEST(DoorLock, test_consumeKey_takes_the_key_and_only_once) {
    // Arrange
    ECS::World world;
    ECS::Entity player = MakePlayerWithKeys(world, {"brass"});
    ECS::LockComponent lock;
    lock.requiredKey = "brass";
    lock.consumeKey = true;

    // Act
    const bool first = UserHasKey(world, player, lock);
    const bool second = UserHasKey(world, player, lock);

    // Assert: it worked once, the key is gone, and it does not work again.
    ENJIN_EXPECT_TRUE(first);
    ENJIN_EXPECT_FALSE(second);
    ENJIN_EXPECT_EQ(world.GetComponent<ECS::InventoryComponent>(player)->keys.size(),
                    static_cast<usize>(0));
}

ENJIN_TEST(DoorLock, test_a_key_that_is_not_consumed_survives_repeated_use) {
    // The control for the test above. consumeKey defaults FALSE, so the common
    // case is a key that keeps working -- if the erase ran unconditionally, a
    // player would lose a key the first time they opened their own front door.
    // Arrange
    ECS::World world;
    ECS::Entity player = MakePlayerWithKeys(world, {"brass"});
    ECS::LockComponent lock;
    lock.requiredKey = "brass";
    lock.consumeKey = false;

    // Act / Assert
    ENJIN_EXPECT_TRUE(UserHasKey(world, player, lock));
    ENJIN_EXPECT_TRUE(UserHasKey(world, player, lock));
    ENJIN_EXPECT_EQ(world.GetComponent<ECS::InventoryComponent>(player)->keys.size(),
                    static_cast<usize>(1));
}

ENJIN_TEST_MAIN()
