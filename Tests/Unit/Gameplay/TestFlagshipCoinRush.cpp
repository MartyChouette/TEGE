// TestFlagshipCoinRush.cpp
// Behavioral proof that the Coin Rush flagship loop actually works: coins
// collect, spikes drain health to defeat, and reaching the goal trigger wins.
// These drive the shared GameplayLoop the editor PlayMode, desktop Player, and
// web Player all run, so passing here means the loop works on every platform.

#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include <vector>

using namespace Enjin;
using namespace Enjin::ECS;

// Build a minimal 3D player: ThirdPersonController + Transform + capsule + health.
static Entity MakePlayer(World& world, const Math::Vector3& pos, f32 maxHp = 100.0f, f32 iframes = 0.0f) {
    Entity p = world.CreateEntity();
    auto& t = world.AddComponent<TransformComponent>(p);
    t.position = pos;
    world.AddComponent<ThirdPersonController>(p);
    auto& cap = world.AddComponent<CapsuleColliderComponent>(p);
    cap.radius = 0.3f;
    cap.height = 1.0f;
    auto& hp = world.AddComponent<HealthComponent>(p);
    hp.maxHealth = maxHp;
    hp.currentHealth = maxHp;
    hp.invulnerabilityTime = iframes;
    return p;
}

ENJIN_TEST(FlagshipCoinRush, PickupCollectsCoin) {
    World world;
    Entity player = MakePlayer(world, Math::Vector3(0, 1, 0));

    Entity coin = world.CreateEntity();
    auto& ct = world.AddComponent<TransformComponent>(coin);
    ct.position = Math::Vector3(0, 1, 0.5f);  // within pickupRange + player radius
    auto& pk = world.AddComponent<PickupComponent>(coin);
    pk.type = PickupComponent::PickupType::Coin;
    pk.pickupRange = 1.0f;

    std::vector<Entity> deferred;
    Gameplay::GameplayLoop::CheckPickupOverlaps3D(&world, deferred);

    auto* pkAfter = world.GetComponent<PickupComponent>(coin);
    ENJIN_ASSERT_TRUE(pkAfter != nullptr);
    ENJIN_EXPECT_TRUE(pkAfter->isCollected);  // coin was collected on overlap

    (void)player;
}

ENJIN_TEST(FlagshipCoinRush, CoinOutOfRangeNotCollected) {
    World world;
    MakePlayer(world, Math::Vector3(0, 1, 0));

    Entity coin = world.CreateEntity();
    auto& ct = world.AddComponent<TransformComponent>(coin);
    ct.position = Math::Vector3(0, 1, 8.0f);  // far away
    auto& pk = world.AddComponent<PickupComponent>(coin);
    pk.pickupRange = 1.0f;

    std::vector<Entity> deferred;
    Gameplay::GameplayLoop::CheckPickupOverlaps3D(&world, deferred);

    ENJIN_EXPECT_TRUE(!world.GetComponent<PickupComponent>(coin)->isCollected);
}

ENJIN_TEST(FlagshipCoinRush, HazardDrainsHealthToDefeat) {
    World world;
    Entity player = MakePlayer(world, Math::Vector3(0, 1, 0), 50.0f, 0.0f);

    // Spike: pure hazard (DamageComponent, no HealthComponent), overlapping the player.
    Entity spike = world.CreateEntity();
    auto& st = world.AddComponent<TransformComponent>(spike);
    st.position = Math::Vector3(0, 1, 0);
    auto& box = world.AddComponent<BoxColliderComponent>(spike);
    box.size = Math::Vector3(1, 1, 1);
    auto& dmg = world.AddComponent<DamageComponent>(spike);
    dmg.damage = 25.0f;
    dmg.damageOnce = false;
    dmg.destroyOnHit = false;

    // Game rules: defeat on player death.
    Entity rules = world.CreateEntity();
    auto& goc = world.AddComponent<GameOverComponent>(rules);
    goc.victoryOnAllEnemiesDefeated = false;

    std::vector<Entity> deferred;

    // First touch: 50 -> 25.
    Gameplay::GameplayLoop::CheckHazardOverlaps3D(&world, deferred);
    ENJIN_EXPECT_TRUE(world.GetComponent<HealthComponent>(player)->currentHealth == 25.0f);
    ENJIN_EXPECT_TRUE(!world.GetComponent<HealthComponent>(player)->isDead);

    // Second touch: 25 -> 0, dead.
    Gameplay::GameplayLoop::CheckHazardOverlaps3D(&world, deferred);
    ENJIN_EXPECT_TRUE(world.GetComponent<HealthComponent>(player)->isDead);

    // Game over resolves to DEFEAT (triggered, not won).
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);
    auto* go = world.GetComponent<GameOverComponent>(rules);
    ENJIN_EXPECT_TRUE(go->triggered);
    ENJIN_EXPECT_TRUE(!go->won);
}

ENJIN_TEST(FlagshipCoinRush, ReachingGoalTriggersVictory) {
    World world;
    Entity player = MakePlayer(world, Math::Vector3(0, 1, 20.0f));  // start far from goal

    // Goal portal with a trigger zone.
    Entity goal = world.CreateEntity();
    auto& gt = world.AddComponent<TransformComponent>(goal);
    gt.position = Math::Vector3(0, 0.6f, 5.0f);
    auto& tz = world.AddComponent<TriggerZoneComponent>(goal);
    tz.shape = TriggerZoneComponent::Shape::Box;
    tz.boxSize = Math::Vector3(2.5f, 3.0f, 2.5f);
    tz.triggerOnce = true;

    Entity rules = world.CreateEntity();
    auto& goc = world.AddComponent<GameOverComponent>(rules);
    goc.victoryOnAllEnemiesDefeated = false;
    goc.victoryTriggerEntity = goal;

    // Far away: trigger empty, no victory.
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    ENJIN_EXPECT_TRUE(world.GetComponent<TriggerZoneComponent>(goal)->entitiesInside.empty());
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);
    ENJIN_EXPECT_TRUE(!world.GetComponent<GameOverComponent>(rules)->triggered);

    // Walk into the goal.
    world.GetComponent<TransformComponent>(player)->position = Math::Vector3(0, 1, 5.0f);
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    ENJIN_EXPECT_TRUE(!world.GetComponent<TriggerZoneComponent>(goal)->entitiesInside.empty());

    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);
    auto* go = world.GetComponent<GameOverComponent>(rules);
    ENJIN_EXPECT_TRUE(go->triggered);
    ENJIN_EXPECT_TRUE(go->won);  // VICTORY on reaching the portal
}

ENJIN_TEST_MAIN()
