#include "EnjinTest.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using namespace Enjin::ECS;

// ===========================================================================
// HealthComponent Defaults
// ===========================================================================

ENJIN_TEST(HealthComp, MaxHealth) {
    HealthComponent hp;
    ENJIN_EXPECT_FLOAT_EQ(hp.maxHealth, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(hp.currentHealth, 100.0f);
}

ENJIN_TEST(HealthComp, RegenDefaults) {
    HealthComponent hp;
    ENJIN_EXPECT_FLOAT_EQ(hp.regenRate, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(hp.regenDelay, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(hp.timeSinceLastDamage, 0.0f);
}

ENJIN_TEST(HealthComp, StateDefaults) {
    HealthComponent hp;
    ENJIN_EXPECT_FALSE(hp.isDead);
    ENJIN_EXPECT_FALSE(hp.isInvulnerable);
    ENJIN_EXPECT_FLOAT_EQ(hp.invulnerabilityTime, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(hp.invulnerabilityTimer, 0.0f);
}

ENJIN_TEST(HealthComp, ShieldDefaults) {
    HealthComponent hp;
    ENJIN_EXPECT_FLOAT_EQ(hp.maxShield, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(hp.currentShield, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(hp.shieldRegenRate, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(hp.shieldRegenDelay, 5.0f);
}

ENJIN_TEST(HealthComp, PercentFull) {
    HealthComponent hp;
    ENJIN_EXPECT_FLOAT_EQ(hp.GetHealthPercent(), 1.0f);
    ENJIN_EXPECT_TRUE(hp.IsFullHealth());
}

ENJIN_TEST(HealthComp, PercentHalf) {
    HealthComponent hp;
    hp.currentHealth = 50.0f;
    ENJIN_EXPECT_FLOAT_NEAR(hp.GetHealthPercent(), 0.5f, 0.001f);
    ENJIN_EXPECT_FALSE(hp.IsFullHealth());
}

ENJIN_TEST(HealthComp, ShieldPercent) {
    HealthComponent hp;
    hp.maxShield = 50.0f;
    hp.currentShield = 25.0f;
    ENJIN_EXPECT_FLOAT_NEAR(hp.GetShieldPercent(), 0.5f, 0.001f);
}

ENJIN_TEST(HealthComp, ShieldPercentZeroMax) {
    HealthComponent hp;
    // maxShield = 0 by default
    ENJIN_EXPECT_FLOAT_EQ(hp.GetShieldPercent(), 0.0f);
}

ENJIN_TEST(HealthComp, EventEntities) {
    HealthComponent hp;
    ENJIN_EXPECT_EQ(hp.onDamageNotify, (Entity)0);
    ENJIN_EXPECT_EQ(hp.onDeathNotify, (Entity)0);
    ENJIN_EXPECT_EQ(hp.onHealNotify, (Entity)0);
}

// ===========================================================================
// DamageComponent Defaults
// ===========================================================================

ENJIN_TEST(DamageComp, Defaults) {
    DamageComponent dmg;
    ENJIN_EXPECT_FLOAT_EQ(dmg.damage, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(dmg.knockbackForce, 0.0f);
    ENJIN_EXPECT_TRUE(dmg.destroyOnHit);
    ENJIN_EXPECT_TRUE(dmg.damageOnce);
    ENJIN_EXPECT_FLOAT_EQ(dmg.damageInterval, 0.0f);
}

ENJIN_TEST(DamageComp, DamageType) {
    DamageComponent dmg;
    ENJIN_EXPECT_EQ((int)dmg.type, (int)DamageComponent::DamageType::Physical);
}

ENJIN_TEST(DamageComp, DamageTypeEnum) {
    ENJIN_EXPECT_EQ((int)DamageComponent::DamageType::Physical, 0);
    ENJIN_EXPECT_EQ((int)DamageComponent::DamageType::Fire, 1);
    ENJIN_EXPECT_EQ((int)DamageComponent::DamageType::Ice, 2);
    ENJIN_EXPECT_EQ((int)DamageComponent::DamageType::Electric, 3);
    ENJIN_EXPECT_EQ((int)DamageComponent::DamageType::Poison, 4);
    ENJIN_EXPECT_EQ((int)DamageComponent::DamageType::Magic, 5);
}

ENJIN_TEST(DamageComp, EmptyDamagedEntities) {
    DamageComponent dmg;
    ENJIN_EXPECT_EQ(dmg.damagedEntities.size(), (size_t)0);
}

// ===========================================================================
// RigidbodyComponent Defaults
// ===========================================================================

ENJIN_TEST(RigidbodyComp, PhysicsDefaults) {
    RigidbodyComponent rb;
    ENJIN_EXPECT_FLOAT_EQ(rb.mass, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(rb.drag, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(rb.angularDrag, 0.05f);
    ENJIN_EXPECT_TRUE(rb.useGravity);
    ENJIN_EXPECT_FLOAT_EQ(rb.gravityScale, 1.0f);
}

ENJIN_TEST(RigidbodyComp, VelocityDefaults) {
    RigidbodyComponent rb;
    ENJIN_EXPECT_FLOAT_EQ(rb.velocity.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(rb.velocity.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(rb.velocity.z, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(rb.angularVelocity.x, 0.0f);
}

ENJIN_TEST(RigidbodyComp, StabilityDefaults) {
    RigidbodyComponent rb;
    ENJIN_EXPECT_FLOAT_EQ(rb.maxVelocity, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(rb.maxAngularVelocity, 50.0f);
}

ENJIN_TEST(RigidbodyComp, ConstraintsOff) {
    RigidbodyComponent rb;
    ENJIN_EXPECT_FALSE(rb.freezePositionX);
    ENJIN_EXPECT_FALSE(rb.freezePositionY);
    ENJIN_EXPECT_FALSE(rb.freezePositionZ);
    ENJIN_EXPECT_FALSE(rb.freezeRotationX);
    ENJIN_EXPECT_FALSE(rb.freezeRotationY);
    ENJIN_EXPECT_FALSE(rb.freezeRotationZ);
}

ENJIN_TEST(RigidbodyComp, BodyTypeDefault) {
    RigidbodyComponent rb;
    ENJIN_EXPECT_EQ((int)rb.bodyType, (int)RigidbodyComponent::BodyType::Dynamic);
}

ENJIN_TEST(RigidbodyComp, CollisionModeDefault) {
    RigidbodyComponent rb;
    ENJIN_EXPECT_EQ((int)rb.collisionMode, (int)RigidbodyComponent::CollisionMode::Discrete);
}

ENJIN_TEST(RigidbodyComp, StateDefaults) {
    RigidbodyComponent rb;
    ENJIN_EXPECT_FALSE(rb.isGrounded);
    ENJIN_EXPECT_FALSE(rb.isSleeping);
}

// ===========================================================================
// Collider Components
// ===========================================================================

ENJIN_TEST(BoxCollider, Defaults) {
    BoxColliderComponent bc;
    ENJIN_EXPECT_FLOAT_EQ(bc.size.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(bc.size.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(bc.size.z, 1.0f);
    ENJIN_EXPECT_FALSE(bc.isTrigger);
    ENJIN_EXPECT_FLOAT_EQ(bc.friction, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(bc.bounciness, 0.0f);
    ENJIN_EXPECT_EQ(bc.categoryBits, 1u);
    ENJIN_EXPECT_EQ(bc.collisionMask, 0xFFFFFFFFu);
}

ENJIN_TEST(SphereCollider, Defaults) {
    SphereColliderComponent sc;
    ENJIN_EXPECT_FLOAT_EQ(sc.radius, 0.5f);
    ENJIN_EXPECT_FALSE(sc.isTrigger);
    ENJIN_EXPECT_EQ(sc.categoryBits, 1u);
    ENJIN_EXPECT_EQ(sc.collisionMask, 0xFFFFFFFFu);
}

ENJIN_TEST(CapsuleCollider, Defaults) {
    CapsuleColliderComponent cc;
    ENJIN_EXPECT_FLOAT_EQ(cc.radius, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(cc.height, 2.0f);
    ENJIN_EXPECT_EQ((int)cc.direction, (int)CapsuleColliderComponent::Direction::Y);
    ENJIN_EXPECT_FALSE(cc.isTrigger);
    ENJIN_EXPECT_EQ(cc.categoryBits, 1u);
}

// ===========================================================================
// TriggerZoneComponent Defaults
// ===========================================================================

ENJIN_TEST(TriggerZone, ShapeDefault) {
    TriggerZoneComponent tz;
    ENJIN_EXPECT_EQ((int)tz.shape, (int)TriggerZoneComponent::Shape::Box);
}

ENJIN_TEST(TriggerZone, SizeDefaults) {
    TriggerZoneComponent tz;
    ENJIN_EXPECT_FLOAT_EQ(tz.boxSize.x, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(tz.sphereRadius, 1.0f);
}

ENJIN_TEST(TriggerZone, FilteringDefaults) {
    TriggerZoneComponent tz;
    ENJIN_EXPECT_EQ(tz.triggerMask, 0xFFFFFFFFu);
    ENJIN_EXPECT_FALSE(tz.triggerOnce);
    ENJIN_EXPECT_FALSE(tz.hasTriggered);
    ENJIN_EXPECT_EQ(tz.entitiesInside.size(), (size_t)0);
}

// ===========================================================================
// AudioSourceComponent Defaults
// ===========================================================================

ENJIN_TEST(AudioSource, Defaults) {
    AudioSourceComponent src;
    ENJIN_EXPECT_TRUE(src.clipPath.empty());
    ENJIN_EXPECT_FLOAT_EQ(src.volume, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(src.pitch, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(src.minDistance, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(src.maxDistance, 500.0f);
}

ENJIN_TEST(AudioSource, PlaybackDefaults) {
    AudioSourceComponent src;
    ENJIN_EXPECT_FALSE(src.playOnAwake);
    ENJIN_EXPECT_FALSE(src.loop);
    ENJIN_EXPECT_TRUE(src.is3D);
    ENJIN_EXPECT_FALSE(src.isPlaying);
    ENJIN_EXPECT_FLOAT_EQ(src.playbackPosition, 0.0f);
    ENJIN_EXPECT_EQ(src.soundHandle, 0u);
}

ENJIN_TEST(AudioSource, SpatialAndPriority) {
    AudioSourceComponent src;
    ENJIN_EXPECT_FLOAT_EQ(src.spatialBlend, 1.0f);
    ENJIN_EXPECT_EQ((int)src.rolloff, (int)AudioSourceComponent::Rolloff::Logarithmic);
    ENJIN_EXPECT_EQ(src.priority, 128);
}

// ===========================================================================
// AudioListenerComponent Defaults
// ===========================================================================

ENJIN_TEST(AudioListener, Defaults) {
    AudioListenerComponent listener;
    ENJIN_EXPECT_TRUE(listener.isActive);
    ENJIN_EXPECT_FLOAT_EQ(listener.volumeScale, 1.0f);
}

// ===========================================================================
// InteractableComponent Defaults
// ===========================================================================

ENJIN_TEST(Interactable, Defaults) {
    InteractableComponent ic;
    ENJIN_EXPECT_STR_EQ(ic.promptText, "Press E to interact");
    ENJIN_EXPECT_FLOAT_EQ(ic.interactionRange, 2.0f);
    ENJIN_EXPECT_TRUE(ic.requiresLookAt);
    ENJIN_EXPECT_FLOAT_EQ(ic.lookAtAngle, 45.0f);
    ENJIN_EXPECT_TRUE(ic.isEnabled);
    ENJIN_EXPECT_FALSE(ic.singleUse);
    ENJIN_EXPECT_FALSE(ic.hasBeenUsed);
    ENJIN_EXPECT_TRUE(ic.highlightOnHover);
}

// ===========================================================================
// PickupComponent Defaults
// ===========================================================================

ENJIN_TEST(Pickup, TypeDefault) {
    PickupComponent pc;
    ENJIN_EXPECT_EQ((int)pc.type, (int)PickupComponent::PickupType::Coin);
    ENJIN_EXPECT_FLOAT_EQ(pc.value, 1.0f);
}

ENJIN_TEST(Pickup, RangeDefaults) {
    PickupComponent pc;
    ENJIN_EXPECT_FLOAT_EQ(pc.pickupRange, 1.0f);
    ENJIN_EXPECT_TRUE(pc.destroyOnPickup);
    ENJIN_EXPECT_FALSE(pc.magnetToPlayer);
    ENJIN_EXPECT_FLOAT_EQ(pc.magnetRange, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(pc.magnetSpeed, 10.0f);
}

ENJIN_TEST(Pickup, RespawnDefaults) {
    PickupComponent pc;
    ENJIN_EXPECT_FALSE(pc.canRespawn);
    ENJIN_EXPECT_FLOAT_EQ(pc.respawnTime, 10.0f);
    ENJIN_EXPECT_FALSE(pc.isCollected);
}

ENJIN_TEST(Pickup, VisualDefaults) {
    PickupComponent pc;
    ENJIN_EXPECT_FLOAT_EQ(pc.bobSpeed, 2.0f);
    ENJIN_EXPECT_FLOAT_NEAR(pc.bobHeight, 0.2f, 0.01f);
    ENJIN_EXPECT_FLOAT_EQ(pc.rotationSpeed, 90.0f);
}

// ===========================================================================
// InventoryComponent Defaults
// ===========================================================================

ENJIN_TEST(Inventory, Defaults) {
    InventoryComponent inv;
    ENJIN_EXPECT_EQ(inv.slots.size(), (size_t)0);
    ENJIN_EXPECT_EQ(inv.maxSlots, (usize)20);
    ENJIN_EXPECT_EQ(inv.coins, 0);
    ENJIN_EXPECT_EQ(inv.gems, 0);
    ENJIN_EXPECT_EQ(inv.keys.size(), (size_t)0);
}

// ===========================================================================
// AIControllerComponent Defaults
// ===========================================================================

ENJIN_TEST(AIController, StateDefault) {
    AIControllerComponent ai;
    ENJIN_EXPECT_EQ((int)ai.currentState, (int)AIControllerComponent::AIState::Idle);
}

ENJIN_TEST(AIController, DetectionDefaults) {
    AIControllerComponent ai;
    ENJIN_EXPECT_FLOAT_EQ(ai.detectionRange, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(ai.attackRange, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(ai.loseTargetRange, 15.0f);
    ENJIN_EXPECT_FLOAT_EQ(ai.fieldOfView, 120.0f);
}

ENJIN_TEST(AIController, MovementDefaults) {
    AIControllerComponent ai;
    ENJIN_EXPECT_FLOAT_EQ(ai.moveSpeed, 3.0f);
    ENJIN_EXPECT_FLOAT_EQ(ai.turnSpeed, 180.0f);
    ENJIN_EXPECT_FLOAT_EQ(ai.stoppingDistance, 1.0f);
}

ENJIN_TEST(AIController, NavigationDefaults) {
    AIControllerComponent ai;
    ENJIN_EXPECT_TRUE(ai.useNavmesh);
    ENJIN_EXPECT_FLOAT_EQ(ai.repathInterval, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(ai.arrivalRadius, 0.5f);
    ENJIN_EXPECT_FALSE(ai.is2D);
}

ENJIN_TEST_MAIN()
