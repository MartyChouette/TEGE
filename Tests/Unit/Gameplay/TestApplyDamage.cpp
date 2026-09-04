// Damage must be applied by one rule, and that rule must be able to kill.
//
// The sequence -- i-frames block, shield absorbs, health drops, knockback
// pushes, zero health sets isDead -- existed five times: three copies inside
// GameplayLoop.cpp and the AngelScript Health_Damage binding. The script copy
// had diverged in two ways that these tests pin down:
//
//   1. It never set isDead. It wrote onDeathNotify -- a HealthComponent field
//      typed ECS::Entity, meant to hold the id of an entity to notify --
//      assigning it `true`. Nothing in the engine read that field, and both
//      UpdateHealthSystems and UpdateGameOverState key on isDead, never on
//      currentHealth <= 0. So a game dealing damage from script dropped the
//      player to 0 HP with no death, no respawn and no game over.
//
//   2. It absorbed shield BEFORE checking invulnerability, so an invulnerable
//      player still bled shield on every contact.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Gameplay/GameplayLoop.h"

#include <vector>

using namespace Enjin;
using namespace Enjin::Gameplay::GameplayLoop;

namespace {

// A target with health, and nothing else unless a test adds it.
ECS::Entity MakeTarget(ECS::World& w, f32 health, f32 shield = 0.0f) {
    const ECS::Entity e = w.CreateEntity();
    ECS::HealthComponent hp;
    hp.maxHealth = health;
    hp.currentHealth = health;
    hp.maxShield = shield;
    hp.currentShield = shield;
    w.AddComponent<ECS::HealthComponent>(e, hp);
    return e;
}

} // namespace

ENJIN_TEST(ApplyDamage, LethalDamageSetsIsDead) {
    // Arrange
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 30.0f);

    // Act
    const bool landed = ApplyDamage(&w, target, 30.0f);

    // Assert: this is the assertion the script binding failed. isDead is what
    // every death, respawn and game-over path actually reads.
    const auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    ENJIN_ASSERT_TRUE(hp != nullptr);
    ENJIN_EXPECT_TRUE(landed);
    ENJIN_EXPECT_TRUE(hp->isDead);
    ENJIN_EXPECT_TRUE(hp->currentHealth == 0.0f);
}

ENJIN_TEST(ApplyDamage, OverkillClampsToZeroAndStillDies) {
    // Arrange
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 10.0f);

    // Act
    ApplyDamage(&w, target, 999.0f);

    // Assert
    const auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    ENJIN_EXPECT_TRUE(hp->isDead);
    ENJIN_EXPECT_TRUE(hp->currentHealth == 0.0f);
}

ENJIN_TEST(ApplyDamage, InvulnerabilityBlocksTheHitBeforeTheShieldIsTouched) {
    // Arrange: an invulnerable target holding shield.
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 100.0f, /*shield*/ 50.0f);
    w.GetComponent<ECS::HealthComponent>(target)->isInvulnerable = true;

    // Act
    const bool landed = ApplyDamage(&w, target, 20.0f);

    // Assert: the ordering bug drained shield here while reporting nothing hit.
    const auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    ENJIN_EXPECT_TRUE(!landed);
    ENJIN_EXPECT_TRUE(hp->currentShield == 50.0f);
    ENJIN_EXPECT_TRUE(hp->currentHealth == 100.0f);
}

ENJIN_TEST(ApplyDamage, IFrameTimerAlsoBlocksTheHitBeforeTheShield) {
    // Arrange: a live i-frame window rather than the held flag.
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 100.0f, /*shield*/ 50.0f);
    w.GetComponent<ECS::HealthComponent>(target)->invulnerabilityTimer = 0.4f;

    // Act
    const bool landed = ApplyDamage(&w, target, 20.0f);

    // Assert
    const auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    ENJIN_EXPECT_TRUE(!landed);
    ENJIN_EXPECT_TRUE(hp->currentShield == 50.0f);
}

ENJIN_TEST(ApplyDamage, ShieldAbsorbsBeforeHealth) {
    // Arrange
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 100.0f, /*shield*/ 30.0f);

    // Act: more than the shield holds, so the remainder must reach health.
    ApplyDamage(&w, target, 50.0f);

    // Assert
    const auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    ENJIN_EXPECT_TRUE(hp->currentShield == 0.0f);
    ENJIN_EXPECT_TRUE(hp->currentHealth == 80.0f);
    ENJIN_EXPECT_TRUE(!hp->isDead);
}

ENJIN_TEST(ApplyDamage, AHitOpensTheInvulnerabilityWindow) {
    // Arrange
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 100.0f);
    w.GetComponent<ECS::HealthComponent>(target)->invulnerabilityTime = 0.75f;

    // Act
    ApplyDamage(&w, target, 10.0f);

    // Assert: without this a hazard re-damages on every frame of contact.
    const auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    ENJIN_EXPECT_TRUE(hp->invulnerabilityTimer == 0.75f);
    ENJIN_EXPECT_TRUE(hp->timeSinceLastDamage == 0.0f);
}

ENJIN_TEST(ApplyDamage, AlreadyDeadTargetsTakeNoFurtherDamage) {
    // Arrange
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 100.0f);
    auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    hp->isDead = true;
    hp->currentHealth = 0.0f;

    // Act
    const bool landed = ApplyDamage(&w, target, 10.0f);

    // Assert
    ENJIN_EXPECT_TRUE(!landed);
    ENJIN_EXPECT_TRUE(w.GetComponent<ECS::HealthComponent>(target)->currentHealth == 0.0f);
}

ENJIN_TEST(ApplyDamage, KnockbackLiftComesFromTheComponentNotAConstant) {
    // Arrange: a 2D character to the RIGHT of its damager. The lift used to be
    // the literal 0.5f in three separate copies, so a designer could author the
    // horizontal push and not the vertical.
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 100.0f);
    ECS::TransformComponent tt;
    tt.position = Math::Vector3(5.0f, 0.0f, 0.0f);
    w.AddComponent<ECS::TransformComponent>(target, tt);
    w.AddComponent<ECS::Platformer2DController>(target, ECS::Platformer2DController{});

    const ECS::Entity damager = w.CreateEntity();
    ECS::TransformComponent dt;
    dt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    w.AddComponent<ECS::TransformComponent>(damager, dt);
    ECS::DamageComponent dmg;
    dmg.damage = 5.0f;
    dmg.knockbackForce = 10.0f;
    dmg.knockbackUpScale = 0.25f;   // deliberately not the old 0.5f literal
    w.AddComponent<ECS::DamageComponent>(damager, dmg);

    // Act
    ApplyDamage(&w, target, dmg.damage, damager,
                w.GetComponent<ECS::DamageComponent>(damager));

    // Assert: pushed away (+X, since the target is to the right of the damager)
    // and lifted by the authored fraction, not by a hardcoded half.
    const auto* ctrl = w.GetComponent<ECS::Platformer2DController>(target);
    ENJIN_ASSERT_TRUE(ctrl != nullptr);
    ENJIN_EXPECT_TRUE(ctrl->velocity.x == 10.0f);
    ENJIN_EXPECT_TRUE(ctrl->velocity.y == 2.5f);
    ENJIN_EXPECT_TRUE(!ctrl->isGrounded);
}

ENJIN_TEST(ApplyDamage, ContactDamageGoesThroughTheSameRule) {
    // Arrange: the highest-level entry point, to prove the copies were actually
    // replaced rather than the shared function merely added beside them.
    ECS::World w;
    const ECS::Entity target = MakeTarget(w, 12.0f);
    ECS::TransformComponent tt;
    tt.position = Math::Vector3(1.0f, 0.0f, 0.0f);
    w.AddComponent<ECS::TransformComponent>(target, tt);

    const ECS::Entity hazard = w.CreateEntity();
    ECS::TransformComponent dt;
    dt.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    w.AddComponent<ECS::TransformComponent>(hazard, dt);
    ECS::DamageComponent dmg;
    dmg.damage = 12.0f;
    dmg.destroyOnHit = false;
    w.AddComponent<ECS::DamageComponent>(hazard, dmg);

    // Act
    std::vector<ECS::Entity> deferred;
    ProcessContactDamage(&w, hazard, target, deferred);

    // Assert
    const auto* hp = w.GetComponent<ECS::HealthComponent>(target);
    ENJIN_EXPECT_TRUE(hp->isDead);
    ENJIN_EXPECT_TRUE(hp->currentHealth == 0.0f);
}

ENJIN_TEST_MAIN()
