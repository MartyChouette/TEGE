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
#include "Enjin/GUI/UICanvas.h"
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
    // Realistic geometry: a grounded capsule (center ~0.8) standing over a low,
    // wide spike (center 0.5), offset horizontally — the case real play hits.
    // The old center-distance check missed this; this test locks in the fix.
    Entity player = MakePlayer(world, Math::Vector3(0.5f, 0.8f, 0.0f), 50.0f, 0.0f);

    // Spike: pure hazard (DamageComponent, no HealthComponent), sitting on the ground.
    Entity spike = world.CreateEntity();
    auto& st = world.AddComponent<TransformComponent>(spike);
    st.position = Math::Vector3(0, 0.5f, 0);
    auto& box = world.AddComponent<BoxColliderComponent>(spike);
    box.size = Math::Vector3(1, 1, 1);
    box.isTrigger = true;
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

ENJIN_TEST(FlagshipCoinRush, PortalLockedUntilAllCoinsCollected) {
    World world;
    // Arrange: player standing ON the portal from the start, but coins remain.
    Entity player = MakePlayer(world, Math::Vector3(0, 1, 5.0f));

    Entity goal = world.CreateEntity();
    auto& gt = world.AddComponent<TransformComponent>(goal);
    gt.position = Math::Vector3(0, 0.6f, 5.0f);
    auto& tz = world.AddComponent<TriggerZoneComponent>(goal);
    tz.shape = TriggerZoneComponent::Shape::Box;
    tz.boxSize = Math::Vector3(2.5f, 3.0f, 2.5f);

    for (int i = 0; i < 2; ++i) {
        Entity coin = world.CreateEntity();
        world.AddComponent<TransformComponent>(coin).position = Math::Vector3(10.0f + i, 1, 0);
        auto& pk = world.AddComponent<PickupComponent>(coin);
        pk.type = PickupComponent::PickupType::Coin;
    }

    Entity rules = world.CreateEntity();
    auto& goc = world.AddComponent<GameOverComponent>(rules);
    goc.victoryOnAllEnemiesDefeated = false;
    goc.victoryTriggerEntity = goal;
    goc.victoryRequiresAllCoins = true;

    // Act 1: on the portal but coins remain -> portal stays locked, no victory.
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);
    ENJIN_EXPECT_TRUE(!world.GetComponent<GameOverComponent>(rules)->triggered);

    // Act 2: collect every coin, then re-check on the portal.
    for (auto pe : world.GetEntitiesWithComponent<PickupComponent>())
        world.GetComponent<PickupComponent>(pe)->isCollected = true;
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);

    // Assert: portal now opens -> victory.
    auto* go = world.GetComponent<GameOverComponent>(rules);
    ENJIN_EXPECT_TRUE(go->triggered);
    ENJIN_EXPECT_TRUE(go->won);
    (void)player;
}

// Cross-controller compatibility: every player controller type must be able to
// collect pickups and trigger win/lose. TopDown3DController was silently excluded
// from every isPlayer check (couldn't collect coins, die, or win) — this locks
// the policy in: gameplay features work for ALL controllers unless inherently
// 2D-only or 3D-only.
ENJIN_TEST(FlagshipCoinRush, TopDown3DPlayerCollectsAndWins) {
    World world;
    // Arrange: a top-down 3D player (isometric-style) with health.
    Entity player = world.CreateEntity();
    world.AddComponent<TransformComponent>(player).position = Math::Vector3(0, 1, 0);
    world.AddComponent<TopDown3DController>(player);
    auto& cap = world.AddComponent<CapsuleColliderComponent>(player);
    cap.radius = 0.3f;
    cap.height = 1.0f;
    auto& hp = world.AddComponent<HealthComponent>(player);
    hp.maxHealth = 100.0f;
    hp.currentHealth = 100.0f;

    Entity coin = world.CreateEntity();
    world.AddComponent<TransformComponent>(coin).position = Math::Vector3(0, 1, 0.5f);
    auto& pk = world.AddComponent<PickupComponent>(coin);
    pk.type = PickupComponent::PickupType::Coin;
    pk.pickupRange = 1.0f;

    Entity goal = world.CreateEntity();
    world.AddComponent<TransformComponent>(goal).position = Math::Vector3(0, 0.6f, 5.0f);
    auto& tz = world.AddComponent<TriggerZoneComponent>(goal);
    tz.boxSize = Math::Vector3(2.5f, 3.0f, 2.5f);

    Entity rules = world.CreateEntity();
    auto& goc = world.AddComponent<GameOverComponent>(rules);
    goc.victoryOnAllEnemiesDefeated = false;
    goc.victoryTriggerEntity = goal;

    // Act 1: coin collection for a top-down player.
    std::vector<Entity> deferred;
    Gameplay::GameplayLoop::CheckPickupOverlaps3D(&world, deferred);

    // Assert 1: the coin was collected.
    ENJIN_EXPECT_TRUE(world.GetComponent<PickupComponent>(coin)->isCollected);

    // Act 2: walk into the goal.
    world.GetComponent<TransformComponent>(player)->position = Math::Vector3(0, 1, 5.0f);
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);

    // Assert 2: victory fires for a TopDown3D player.
    auto* go = world.GetComponent<GameOverComponent>(rules);
    ENJIN_EXPECT_TRUE(go->triggered);
    ENJIN_EXPECT_TRUE(go->won);
}

// UI unification Phase 2: when a GameOverComponent's screen becomes visible, the
// shared GameplayLoop spawns ONE UICanvas game-over screen (UITemplates) rendered
// identically on desktop, editor play, and web. This proves the spawn.
ENJIN_TEST(FlagshipCoinRush, GameOverSpawnsUnifiedCanvasScreen) {
    World world;
    // Arrange: dead player, defeat expected.
    Entity player = world.CreateEntity();
    world.AddComponent<TransformComponent>(player).position = Math::Vector3(0, 1, 0);
    world.AddComponent<ThirdPersonController>(player);
    auto& hp = world.AddComponent<HealthComponent>(player);
    hp.maxHealth = 100.0f;
    hp.currentHealth = 0.0f;
    hp.isDead = true;

    Entity rules = world.CreateEntity();
    auto& goc = world.AddComponent<GameOverComponent>(rules);
    goc.victoryOnAllEnemiesDefeated = false;
    goc.delay = 0.5f;
    goc.defeatMessage = "The spikes got you!";

    // Act: trigger defeat, then tick past the delay so the screen becomes visible.
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);   // triggers
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 1.0f);     // delay elapses -> spawn

    // Assert: exactly one GameOverScreen canvas exists with the defeat message.
    int canvasCount = 0;
    bool foundMessage = false;
    for (auto e : world.GetEntitiesWithComponent<GUI::UICanvasComponent>()) {
        auto* canvas = world.GetComponent<GUI::UICanvasComponent>(e);
        if (canvas && canvas->canvasName == "GameOverScreen") {
            ++canvasCount;
            for (const auto& el : canvas->elements) {
                if (el.data.text == "The spikes got you!") foundMessage = true;
            }
        }
    }
    ENJIN_EXPECT_TRUE(canvasCount == 1);
    ENJIN_EXPECT_TRUE(foundMessage);

    // Act 2: further ticks must NOT spawn duplicates.
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);
    int secondCount = 0;
    for (auto e : world.GetEntitiesWithComponent<GUI::UICanvasComponent>()) {
        auto* canvas = world.GetComponent<GUI::UICanvasComponent>(e);
        if (canvas && canvas->canvasName == "GameOverScreen") ++secondCount;
    }
    ENJIN_EXPECT_TRUE(secondCount == 1);
}

ENJIN_TEST(FlagshipCoinRush, TopDown3DPlayerDeathTriggersDefeat) {
    World world;
    // Arrange: top-down player already dead (spike drained them elsewhere).
    Entity player = world.CreateEntity();
    world.AddComponent<TransformComponent>(player).position = Math::Vector3(0, 1, 0);
    world.AddComponent<TopDown3DController>(player);
    auto& hp = world.AddComponent<HealthComponent>(player);
    hp.maxHealth = 100.0f;
    hp.currentHealth = 0.0f;
    hp.isDead = true;

    Entity rules = world.CreateEntity();
    auto& goc = world.AddComponent<GameOverComponent>(rules);
    goc.victoryOnAllEnemiesDefeated = false;

    // Act
    Gameplay::GameplayLoop::UpdateGameOverState(&world, 0.016f);

    // Assert: defeat fires for a TopDown3D player.
    auto* go = world.GetComponent<GameOverComponent>(rules);
    ENJIN_EXPECT_TRUE(go->triggered);
    ENJIN_EXPECT_TRUE(!go->won);
}

ENJIN_TEST_MAIN()
