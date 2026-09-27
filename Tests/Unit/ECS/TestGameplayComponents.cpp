#include "EnjinTest.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Billboards.h"
#include "Enjin/ECS/Timers.h"
#include "Enjin/ECS/FollowTarget.h"
#include "Enjin/ECS/CameraZones.h"
#include "Enjin/Gameplay/InteractionSystem.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Transform.h"
#include <cmath>
#include <vector>
#include <string>

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
    // The default names the ACTION, not a key. It used to read "Press E",
    // which started lying the moment anyone rebound Interact -- and the engine
    // knew the real binding the whole time. InputActionMap::ResolvePromptText
    // substitutes the live binding at display time.
    ENJIN_EXPECT_STR_EQ(ic.promptText, "Press {Interact} to interact");
    ENJIN_EXPECT_TRUE(ic.promptText.find("{Interact}") != std::string::npos);
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

// ===========================================================================
// Billboard: the component had no reader, so nothing ever turned (SD-27)
// ===========================================================================

namespace {
// Where the billboard's front (+Z, the side a Quad faces) points in the world.
Math::Vector3 BillboardFront(World& w, Entity e) {
    Math::Vector3 pos; Math::Quaternion rot;
    GetWorldTransform(&w, e, pos, rot);
    return rot.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f));
}
bool Near3(const Math::Vector3& a, const Math::Vector3& b, f32 eps = 1e-3f) {
    return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps;
}
}

ENJIN_TEST(Billboard, TurnsItsFrontToTheCamera) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    auto& bb = w.AddComponent<BillboardComponent>(e);
    bb.lockY = false;
    ENJIN_EXPECT_EQ(FaceBillboards(&w, Math::Vector3(10.0f, 0.0f, 0.0f)), 1u);
    ENJIN_EXPECT_TRUE(Near3(BillboardFront(w, e), Math::Vector3(1.0f, 0.0f, 0.0f)));
    // Fully facing tilts up toward a camera above it
    FaceBillboards(&w, Math::Vector3(0.0f, 10.0f, 10.0f));
    const f32 h = std::sqrt(0.5f);
    ENJIN_EXPECT_TRUE(Near3(BillboardFront(w, e), Math::Vector3(0.0f, h, h)));
}

ENJIN_TEST(Billboard, LockYStaysUpright) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    w.AddComponent<BillboardComponent>(e);   // lockY defaults on
    FaceBillboards(&w, Math::Vector3(0.0f, 50.0f, -10.0f));
    ENJIN_EXPECT_TRUE(Near3(BillboardFront(w, e), Math::Vector3(0.0f, 0.0f, -1.0f)));
    // Straight overhead there is no direction to turn to: unchanged, not NaN
    ENJIN_EXPECT_EQ(FaceBillboards(&w, Math::Vector3(0.0f, 50.0f, 0.0f)), 0u);
    ENJIN_EXPECT_TRUE(Near3(BillboardFront(w, e), Math::Vector3(0.0f, 0.0f, -1.0f)));
}

ENJIN_TEST(Billboard, RotationOffsetTurnsAboutItsOwnY) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    w.AddComponent<BillboardComponent>(e).rotationOffset = 90.0f;
    FaceBillboards(&w, Math::Vector3(0.0f, 0.0f, 10.0f));
    // Facing +Z, then 90 degrees about Y: the front ends up along +X
    ENJIN_EXPECT_TRUE(Near3(BillboardFront(w, e), Math::Vector3(1.0f, 0.0f, 0.0f)));
}

ENJIN_TEST(Billboard, FaceCameraOffLeavesItAlone) {
    World w;
    Entity e = w.CreateEntity();
    auto& t = w.AddComponent<TransformComponent>(e);
    const Math::Quaternion authored = Math::Quaternion::FromEuler(Math::Vector3(0.3f, 0.2f, 0.1f));
    t.rotation = authored;
    w.AddComponent<BillboardComponent>(e).faceCamera = false;
    ENJIN_EXPECT_EQ(FaceBillboards(&w, Math::Vector3(10.0f, 0.0f, 0.0f)), 0u);
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<TransformComponent>(e)->rotation.w, authored.w);
}

ENJIN_TEST(Billboard, ParentedFacesInWorldAndDirtiesChildren) {
    World w;
    Entity parent = w.CreateEntity();
    auto& pt = w.AddComponent<TransformComponent>(parent);
    pt.position = Math::Vector3(5.0f, 0.0f, 0.0f);
    pt.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.0f, 1.2f, 0.0f));
    Entity bbe = w.CreateEntity();
    w.AddComponent<TransformComponent>(bbe);
    w.AddComponent<BillboardComponent>(bbe);
    SetParent(&w, bbe, parent);
    Entity child = w.CreateEntity();
    w.AddComponent<TransformComponent>(child).position = Math::Vector3(0.0f, 0.0f, 1.0f);
    SetParent(&w, child, bbe);

    // Warm the caches, as a frame that already drew would have
    ComputeWorldMatrix(&w, child);
    ENJIN_EXPECT_FALSE(w.GetComponent<TransformComponent>(child)->worldMatrixDirty);

    // Camera straight down -Z from the billboard's world position (5,0,0)
    FaceBillboards(&w, Math::Vector3(5.0f, 0.0f, -10.0f));
    ENJIN_EXPECT_TRUE(Near3(BillboardFront(w, bbe), Math::Vector3(0.0f, 0.0f, -1.0f)));
    // The child sits one unit in front of the billboard, so it must be
    // re-placed rather than served last frame's matrix
    const Math::Matrix4 cm = ComputeWorldMatrix(&w, child);
    ENJIN_EXPECT_TRUE(Near3(Math::Vector3(cm.m[12], cm.m[13], cm.m[14]), Math::Vector3(5.0f, 0.0f, -1.0f)));
}

// ===========================================================================
// Timer: nothing ever ticked one (SD-27)
// ===========================================================================

namespace {
struct TimerRig {
    World w;
    EntityEventBus bus;
    std::vector<EntityEvent> heard;
    TimerRig() {
        bus.SetForwarder([this](const std::string&, const EntityEvent& ev) { heard.push_back(ev); });
    }
    Entity Add(const TimerComponent& t) {
        Entity e = w.CreateEntity();
        w.AddComponent<TimerComponent>(e, t);
        return e;
    }
    TimerComponent& T(Entity e) { return *w.GetComponent<TimerComponent>(e); }
};
}

ENJIN_TEST(Timer, AutoStartRunsAndCompletesOnce) {
    TimerRig r;
    TimerComponent t;
    t.duration = 1.0f;
    t.autoStart = true;
    Entity e = r.Add(t);
    UpdateTimers(&r.w, 0.6f, &r.bus);
    ENJIN_EXPECT_TRUE(r.T(e).isRunning);
    ENJIN_EXPECT_TRUE(r.heard.empty());
    UpdateTimers(&r.w, 0.6f, &r.bus);
    ENJIN_EXPECT_FALSE(r.T(e).isRunning);
    ENJIN_EXPECT_FLOAT_EQ(r.T(e).elapsed, 1.0f);
    ENJIN_EXPECT_EQ(r.T(e).loopCount, 1);
    ENJIN_ASSERT_EQ(r.heard.size(), size_t(1));
    ENJIN_EXPECT_EQ(r.heard[0].name, std::string("Timer_Complete"));
    ENJIN_EXPECT_EQ(r.heard[0].sender, e);
    ENJIN_EXPECT_EQ(r.heard[0].ints.at("loops"), 1);
    // Stopped: no second event
    UpdateTimers(&r.w, 5.0f, &r.bus);
    ENJIN_EXPECT_EQ(r.heard.size(), size_t(1));
}

ENJIN_TEST(Timer, WithoutAutoStartItWaits) {
    TimerRig r;
    TimerComponent t;
    Entity e = r.Add(t);
    UpdateTimers(&r.w, 5.0f, &r.bus);
    ENJIN_EXPECT_FLOAT_EQ(r.T(e).elapsed, 0.0f);
    ENJIN_EXPECT_TRUE(r.heard.empty());
}

ENJIN_TEST(Timer, LoopCarriesTheOvershootAndCountsEveryLap) {
    TimerRig r;
    TimerComponent t;
    t.duration = 0.5f;
    t.loop = true;
    t.isRunning = true;
    Entity e = r.Add(t);
    UpdateTimers(&r.w, 0.7f, &r.bus);
    ENJIN_EXPECT_TRUE(r.T(e).isRunning);
    ENJIN_EXPECT_TRUE(std::abs(r.T(e).elapsed - 0.2f) < 1e-4f);
    ENJIN_EXPECT_EQ(r.T(e).loopCount, 1);
    // A frame longer than two laps: counted as two, one event
    UpdateTimers(&r.w, 1.1f, &r.bus);
    ENJIN_EXPECT_EQ(r.T(e).loopCount, 3);
    ENJIN_EXPECT_EQ(r.heard.size(), size_t(2));
}

ENJIN_TEST(Timer, ZeroDurationLoopDoesNotSpin) {
    TimerRig r;
    TimerComponent t;
    t.duration = 0.0f;
    t.loop = true;
    t.isRunning = true;
    Entity e = r.Add(t);
    UpdateTimers(&r.w, 0.016f, &r.bus);
    ENJIN_EXPECT_FALSE(r.T(e).isRunning);
    ENJIN_EXPECT_EQ(r.heard.size(), size_t(1));
}

ENJIN_TEST(Timer, EventNameAndTargetComeFromTheTimer) {
    TimerRig r;
    Entity door = r.w.CreateEntity();
    TimerComponent t;
    t.duration = 0.1f;
    t.isRunning = true;
    t.completeEvent = "door_close";
    t.onCompleteNotify = door;
    r.Add(t);
    TimerComponent quiet;
    quiet.duration = 0.1f;
    quiet.isRunning = true;
    quiet.completeEvent.clear();   // empty sends nothing
    r.Add(quiet);
    UpdateTimers(&r.w, 0.2f, &r.bus);
    ENJIN_ASSERT_EQ(r.heard.size(), size_t(1));
    ENJIN_EXPECT_EQ(r.heard[0].name, std::string("door_close"));
    ENJIN_EXPECT_EQ(r.heard[0].target, door);
}

// ===========================================================================
// FollowTarget: only target, offset and speed were ever read (SD-27)
// ===========================================================================

namespace {
struct FollowRig {
    World w;
    Entity target, follower;
    FollowRig(const FollowTargetComponent& f, const Math::Vector3& followerAt) {
        target = w.CreateEntity();
        w.AddComponent<TransformComponent>(target);
        follower = w.CreateEntity();
        w.AddComponent<TransformComponent>(follower).position = followerAt;
        FollowTargetComponent c = f;
        c.target = target;
        w.AddComponent<FollowTargetComponent>(follower, c);
    }
    Math::Vector3 Pos() { return w.GetComponent<TransformComponent>(follower)->position; }
    void Run(f32 seconds) { for (f32 t = 0; t < seconds; t += 1.0f / 60.0f) UpdateFollowTargets(&w, 1.0f / 60.0f); }
};
}

ENJIN_TEST(FollowTarget, HoldsAtFollowDistance) {
    FollowTargetComponent f;
    f.followDistance = 3.0f;
    f.smoothTime = 0.1f;
    f.moveSpeed = 0.0f;
    FollowRig r(f, Math::Vector3(10.0f, 0.0f, 0.0f));
    r.Run(3.0f);
    ENJIN_EXPECT_TRUE(std::abs(r.Pos().x - 3.0f) < 0.05f);   // stopped 3 short, not on top
}

ENJIN_TEST(FollowTarget, ZeroDistanceSitsOnTheOffsetAndLocalOffsetTurns) {
    FollowTargetComponent f;
    f.followDistance = 0.0f;
    f.minDistance = 0.0f;
    f.smoothTime = 0.0f;
    f.offset = Math::Vector3(0.0f, 2.0f, -5.0f);
    f.useLocalOffset = true;
    FollowRig r(f, Math::Vector3(0.0f));
    // Target turned 90 degrees about Y: "behind" (-Z local) is now -X world
    r.w.GetComponent<TransformComponent>(r.target)->rotation =
        Math::Quaternion::FromEuler(Math::Vector3(0.0f, Math::Radians(90.0f), 0.0f));
    UpdateFollowTargets(&r.w, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(Near3(r.Pos(), Math::Vector3(-5.0f, 2.0f, 0.0f)));
}

ENJIN_TEST(FollowTarget, GivesUpBeyondMaxAndStopsInsideMin) {
    FollowTargetComponent f;
    f.maxDistance = 20.0f;
    f.smoothTime = 0.0f;
    FollowRig distant(f, Math::Vector3(50.0f, 0.0f, 0.0f));
    UpdateFollowTargets(&distant.w, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(Near3(distant.Pos(), Math::Vector3(50.0f, 0.0f, 0.0f)));

    FollowRig close(f, Math::Vector3(0.5f, 0.0f, 0.0f));   // inside minDistance 1
    UpdateFollowTargets(&close.w, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(Near3(close.Pos(), Math::Vector3(0.5f, 0.0f, 0.0f)));
}

ENJIN_TEST(FollowTarget, MoveSpeedCapsAndRotationFollows) {
    FollowTargetComponent f;
    f.followDistance = 0.0f;
    f.minDistance = 0.0f;
    f.smoothTime = 0.05f;
    f.moveSpeed = 2.0f;
    f.matchTargetRotation = true;
    f.rotationSpeed = 90.0f;
    FollowRig r(f, Math::Vector3(10.0f, 0.0f, 0.0f));
    r.w.GetComponent<TransformComponent>(r.target)->rotation =
        Math::Quaternion::FromEuler(Math::Vector3(0.0f, Math::Radians(180.0f), 0.0f));
    r.Run(1.0f);
    // Two units a second: after one second it has come about 2 of the 10
    ENJIN_EXPECT_TRUE(r.Pos().x > 7.5f && r.Pos().x < 8.5f);
    // Ninety degrees a second: halfway round the 180 turn
    Math::Vector3 fwd = r.w.GetComponent<TransformComponent>(r.follower)->rotation.Rotate(Math::Vector3(0, 0, 1));
    ENJIN_EXPECT_TRUE(std::abs(fwd.z) < 0.1f);
}

// ===========================================================================
// Camera zone Blend Time: saved and never read, so a zone cut hard (SD-27)
// ===========================================================================

namespace {
Entity AddCam(World& w, const Math::Vector3& at, f32 fov) {
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e).position = at;
    CameraComponent c;
    c.fieldOfView = fov;
    w.AddComponent<CameraComponent>(e, c);
    return e;
}
}

ENJIN_TEST(CameraBlend, AChangeOfCameraEasesOverTheBlendTime) {
    World w;
    Entity a = AddCam(w, Math::Vector3(0.0f), 60.0f);
    Entity b = AddCam(w, Math::Vector3(10.0f, 0.0f, 0.0f), 90.0f);
    GameCameraBlend st;
    GameCameraPose p;
    ENJIN_ASSERT_TRUE(BlendGameCamera(&w, st, a, 1.0f, 0.016f, p));
    ENJIN_EXPECT_TRUE(Near3(p.position, Math::Vector3(0.0f)));   // the first camera does not blend in
    BlendGameCamera(&w, st, b, 1.0f, 0.5f, p);
    ENJIN_EXPECT_TRUE(Near3(p.position, Math::Vector3(5.0f, 0.0f, 0.0f)));   // smoothstep at half = half
    ENJIN_EXPECT_TRUE(std::abs(p.fieldOfView - 75.0f) < 0.01f);
    BlendGameCamera(&w, st, b, 1.0f, 0.6f, p);
    ENJIN_EXPECT_TRUE(Near3(p.position, Math::Vector3(10.0f, 0.0f, 0.0f)));
}

ENJIN_TEST(CameraBlend, SwitchingBackMidBlendStartsFromTheCurrentView) {
    World w;
    Entity a = AddCam(w, Math::Vector3(0.0f), 60.0f);
    Entity b = AddCam(w, Math::Vector3(10.0f, 0.0f, 0.0f), 60.0f);
    GameCameraBlend st;
    GameCameraPose p;
    BlendGameCamera(&w, st, a, 1.0f, 0.016f, p);
    BlendGameCamera(&w, st, b, 1.0f, 0.5f, p);                // at x = 5
    BlendGameCamera(&w, st, a, 1.0f, 0.0f, p);                // turn back: no jump
    ENJIN_EXPECT_TRUE(Near3(p.position, Math::Vector3(5.0f, 0.0f, 0.0f)));
}

ENJIN_TEST(CameraBlend, ZeroBlendTimeCuts) {
    World w;
    Entity a = AddCam(w, Math::Vector3(0.0f), 60.0f);
    Entity b = AddCam(w, Math::Vector3(10.0f, 0.0f, 0.0f), 60.0f);
    GameCameraBlend st;
    GameCameraPose p;
    BlendGameCamera(&w, st, a, 0.0f, 0.016f, p);
    BlendGameCamera(&w, st, b, 0.0f, 0.016f, p);
    ENJIN_EXPECT_TRUE(Near3(p.position, Math::Vector3(10.0f, 0.0f, 0.0f)));
}

// ===========================================================================
// Interactable: there was no interaction system at all (SD-27)
// ===========================================================================

namespace {
struct InteractRig {
    World w;
    EntityEventBus bus;
    RenderSystem rs{&w, nullptr};
    Gameplay::InteractionSystem sys;
    Entity player, camera;
    std::vector<EntityEvent> heard;
    InteractRig() {
        player = w.CreateEntity();
        w.AddComponent<TransformComponent>(player);
        w.AddComponent<ThirdPersonController>(player);
        camera = AddCam(w, Math::Vector3(0.0f, 0.0f, 0.0f), 60.0f);   // looks down -Z
        bus.SetForwarder([this](const std::string&, const EntityEvent& ev) { heard.push_back(ev); });
        sys.SetWorld(&w);
        sys.SetEventBus(&bus);
        sys.SetRenderSystem(&rs);
    }
    Entity Add(const Math::Vector3& at, f32 range = 2.0f) {
        Entity e = w.CreateEntity();
        w.AddComponent<TransformComponent>(e).position = at;
        InteractableComponent ic;
        ic.interactionRange = range;
        ic.promptText = "Open";
        w.AddComponent<InteractableComponent>(e, ic);
        return e;
    }
};
}

ENJIN_TEST(Interaction, TheNearestOneInReachAndInViewGetsFocus) {
    InteractRig r;
    Entity behind = r.Add(Math::Vector3(0.0f, 0.0f, 1.0f));     // in range, behind the camera
    Entity distant = r.Add(Math::Vector3(0.0f, 0.0f, -5.0f));   // in view, out of range
    Entity ahead = r.Add(Math::Vector3(0.0f, 0.0f, -1.5f));     // in view and range
    (void)behind; (void)distant;
    r.sys.Update(0.016f, false);
    ENJIN_EXPECT_EQ(r.sys.GetFocused(), ahead);
    ENJIN_EXPECT_EQ(r.sys.GetPrompt(), std::string("Open"));
    ENJIN_EXPECT_EQ(r.rs.GetInteractionFocus(), ahead);         // highlighted
}

ENJIN_TEST(Interaction, InteractSendsTheEventAndSpendsASingleUse) {
    InteractRig r;
    Entity chest = r.Add(Math::Vector3(0.0f, 0.0f, -1.0f));
    Entity lid = r.w.CreateEntity();
    auto* ic = r.w.GetComponent<InteractableComponent>(chest);
    ic->singleUse = true;
    ic->interactEvent = "chest_open";
    ic->onInteractNotify = lid;
    r.sys.Update(0.016f, false);
    ENJIN_EXPECT_TRUE(r.heard.empty());
    r.sys.Update(0.016f, true);
    ENJIN_ASSERT_EQ(r.heard.size(), size_t(1));
    ENJIN_EXPECT_EQ(r.heard[0].name, std::string("chest_open"));
    ENJIN_EXPECT_EQ(r.heard[0].sender, chest);
    ENJIN_EXPECT_EQ(r.heard[0].target, lid);
    ENJIN_EXPECT_EQ(r.heard[0].entities.at("interactor"), r.player);
    // Spent: no longer offered, nothing more sent
    r.sys.Update(0.016f, true);
    ENJIN_EXPECT_EQ(r.sys.GetFocused(), INVALID_ENTITY);
    ENJIN_EXPECT_EQ(r.heard.size(), size_t(1));
    ENJIN_EXPECT_EQ(r.rs.GetInteractionFocus(), INVALID_ENTITY);
}

ENJIN_TEST(Interaction, NoLookRequirementAndResetClearsTheHighlight) {
    InteractRig r;
    Entity lever = r.Add(Math::Vector3(0.0f, 0.0f, 1.0f));      // behind
    r.w.GetComponent<InteractableComponent>(lever)->requiresLookAt = false;
    r.sys.Update(0.016f, false);
    ENJIN_EXPECT_EQ(r.sys.GetFocused(), lever);
    r.sys.Reset();
    ENJIN_EXPECT_EQ(r.rs.GetInteractionFocus(), INVALID_ENTITY);
}

ENJIN_TEST_MAIN()
