// DamageResistanceComponent, which the central damage path never read.
//
// Six per-type multipliers, an inspector, and a GetMultiplier helper -- and
// GameplayLoop::ApplyDamage, the one place contact damage, hazards, stomps and
// every script damage call flow through, ignored all of it. The only consumer
// in the engine was AISystem's melee path, which hardcodes the PHYSICAL
// multiplier. So five of the six types did nothing at all, and the sixth worked
// for exactly one kind of attacker.
//
// Found by the feature-audit swarm, verified by reading ApplyDamage.
#include "EnjinTest.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using DamageType = ECS::DamageComponent::DamageType;

namespace {

ECS::Entity MakeTarget(ECS::World& world, f32 health, f32 shield = 0.0f) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::HealthComponent hp;
    hp.maxHealth = health;
    hp.currentHealth = health;
    hp.currentShield = shield;
    hp.invulnerabilityTime = 0.0f;   // off, so repeated hits in a test land
    world.AddComponent<ECS::HealthComponent>(e, hp);
    return e;
}

// The attacker carries the damage TYPE, which is what the resistance is keyed
// on. A source-less call is treated as Physical.
ECS::Entity MakeAttacker(ECS::World& world, DamageType type, f32 amount) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::DamageComponent dmg;
    dmg.type = type;
    dmg.damage = amount;
    world.AddComponent<ECS::DamageComponent>(e, dmg);
    return e;
}

f32 HealthOf(ECS::World& world, ECS::Entity e) {
    return world.GetComponent<ECS::HealthComponent>(e)->currentHealth;
}

bool Hit(ECS::World& world, ECS::Entity target, ECS::Entity attacker) {
    const auto* dmg = world.GetComponent<ECS::DamageComponent>(attacker);
    return Gameplay::GameplayLoop::ApplyDamage(&world, target, dmg->damage, attacker, dmg);
}

} // namespace

ENJIN_TEST(DamageResistance, test_a_resistance_reduces_the_damage_taken) {
    // Arrange
    ECS::World world;
    ECS::Entity target = MakeTarget(world, 100.0f);
    ECS::DamageResistanceComponent resist;
    resist.fireMult = 0.25f;
    world.AddComponent<ECS::DamageResistanceComponent>(target, resist);
    ECS::Entity attacker = MakeAttacker(world, DamageType::Fire, 40.0f);

    // Act
    Hit(world, target, attacker);

    // Assert: 40 * 0.25 = 10
    ENJIN_EXPECT_FLOAT_NEAR(HealthOf(world, target), 90.0f, 0.001f);
}

ENJIN_TEST(DamageResistance, test_a_weakness_increases_it) {
    // The same field above 1.0. Worth its own test because "resistance" reads
    // as a one-way clamp and the component is explicitly both.
    // Arrange
    ECS::World world;
    ECS::Entity target = MakeTarget(world, 100.0f);
    ECS::DamageResistanceComponent resist;
    resist.iceMult = 2.0f;
    world.AddComponent<ECS::DamageResistanceComponent>(target, resist);
    ECS::Entity attacker = MakeAttacker(world, DamageType::Ice, 10.0f);

    // Act
    Hit(world, target, attacker);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(HealthOf(world, target), 80.0f, 0.001f);
}

ENJIN_TEST(DamageResistance, test_each_type_is_keyed_separately) {
    // The bug this replaces applied the PHYSICAL multiplier to everything, so a
    // target resistant to fire and weak to ice behaved identically to one with
    // no resistance at all. Both channels in one test, because the failure is
    // them being confused with each other.
    // Arrange
    ECS::World world;
    ECS::Entity target = MakeTarget(world, 1000.0f);
    ECS::DamageResistanceComponent resist;
    resist.fireMult = 0.0f;      // immune
    resist.iceMult = 3.0f;       // very weak
    resist.physicalMult = 1.0f;  // normal
    world.AddComponent<ECS::DamageResistanceComponent>(target, resist);

    // Act
    Hit(world, target, MakeAttacker(world, DamageType::Fire, 100.0f));
    const f32 afterFire = HealthOf(world, target);
    Hit(world, target, MakeAttacker(world, DamageType::Physical, 100.0f));
    const f32 afterPhysical = HealthOf(world, target);
    Hit(world, target, MakeAttacker(world, DamageType::Ice, 100.0f));
    const f32 afterIce = HealthOf(world, target);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(afterFire, 1000.0f, 0.001f);      // nothing
    ENJIN_EXPECT_FLOAT_NEAR(afterPhysical, 900.0f, 0.001f);   // 100
    ENJIN_EXPECT_FLOAT_NEAR(afterIce, 600.0f, 0.001f);        // 300
}

ENJIN_TEST(DamageResistance, test_immunity_does_not_spend_shield) {
    // Resistance is applied BEFORE the shield on purpose. A fire-immune target
    // burning through its shield to absorb fire it cannot be hurt by is the
    // wrong order, and it is the order a later insertion point would give.
    // Arrange
    ECS::World world;
    ECS::Entity target = MakeTarget(world, 100.0f, 50.0f);
    ECS::DamageResistanceComponent resist;
    resist.fireMult = 0.0f;
    world.AddComponent<ECS::DamageResistanceComponent>(target, resist);
    ECS::Entity attacker = MakeAttacker(world, DamageType::Fire, 30.0f);

    // Act
    const bool resolved = Hit(world, target, attacker);

    // Assert: shield and health both untouched, and the hit still RESOLVED --
    // a damager that destroys itself on hit must still do so against an immune
    // target.
    const auto* hp = world.GetComponent<ECS::HealthComponent>(target);
    ENJIN_EXPECT_FLOAT_NEAR(hp->currentShield, 50.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(hp->currentHealth, 100.0f, 0.001f);
    ENJIN_EXPECT_TRUE(resolved);
}

ENJIN_TEST(DamageResistance, test_a_target_with_no_resistance_component_is_unchanged) {
    // The control. Without it, a change that zeroed all damage would pass every
    // test above.
    // Arrange
    ECS::World world;
    ECS::Entity target = MakeTarget(world, 100.0f);
    ECS::Entity attacker = MakeAttacker(world, DamageType::Fire, 40.0f);

    // Act
    Hit(world, target, attacker);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(HealthOf(world, target), 60.0f, 0.001f);
}

ENJIN_TEST(DamageResistance, test_a_source_less_call_is_treated_as_physical) {
    // The script binding calls ApplyDamage with no DamageComponent, so there is
    // no type to key on. Physical matches DamageComponent's own default, and
    // the alternative -- ignoring resistance entirely for script damage --
    // would make the component depend on who dealt the hit.
    // Arrange
    ECS::World world;
    ECS::Entity target = MakeTarget(world, 100.0f);
    ECS::DamageResistanceComponent resist;
    resist.physicalMult = 0.5f;
    resist.fireMult = 0.0f;
    world.AddComponent<ECS::DamageResistanceComponent>(target, resist);

    // Act
    Gameplay::GameplayLoop::ApplyDamage(&world, target, 40.0f, ECS::INVALID_ENTITY, nullptr);

    // Assert: halved, not ignored.
    ENJIN_EXPECT_FLOAT_NEAR(HealthOf(world, target), 80.0f, 0.001f);
}

ENJIN_TEST_MAIN()
