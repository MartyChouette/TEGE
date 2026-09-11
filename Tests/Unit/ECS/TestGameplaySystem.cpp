// Eight components with a full authoring surface and nothing behind them.
//
// LockComponent, PushableComponent, SwitchComponent, GoalZoneComponent,
// ConveyorComponent, TeleporterComponent, MovingPlatformComponent and
// SpawnPointComponent could each be added from the menu, filled in through a
// complete inspector, saved into a scene, reloaded, and read from script. Nothing
// made any of them do anything: outside the editor, the serializer and the script
// bindings, no file in the engine even named them.
//
// That is the worst shape a gap can take. A missing feature announces itself; a
// component with an inspector and no system does not. You place a conveyor, set
// its direction and speed, press Play, and nothing moves -- which reads as "I set
// it up wrong", and there was nothing you could have set up that would have worked.
//
// The decision these tests exist to pin is the one nothing in the components
// records: the position fields are OFFSETS from where the entity was placed, not
// world positions. LockComponent::openPosition defaults to (0, 3, 0), "slide up".
// Read as a world position that sends every door in the scene to three metres above
// the origin on the first frame of play. Every case below that moves something
// checks it moved RELATIVE to where the author put it.
#include "EnjinTest.h"
#include "Enjin/ECS/Systems/GameplaySystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

Entity MakeAt(World& w, const Vector3& position, const char* name = "entity") {
    Entity e = w.CreateEntity();
    auto& t = w.AddComponent<TransformComponent>(e, TransformComponent{});
    t.position = position;
    w.AddComponent<NameComponent>(e, NameComponent{name});
    return e;
}

void GiveBox(World& w, Entity e, const Vector3& size) {
    auto& box = w.AddComponent<BoxColliderComponent>(e, BoxColliderComponent{});
    box.size = size;
}

// A "player" is anything with a controller, which is how the system decides.
Entity MakePlayer(World& w, const Vector3& position) {
    Entity e = MakeAt(w, position, "Player");
    w.AddComponent<FirstPersonController>(e, FirstPersonController{});
    return e;
}

Vector3 PositionOf(World& w, Entity e) {
    return w.GetComponent<TransformComponent>(e)->position;
}

bool Near(f32 a, f32 b, f32 eps = 0.01f) { return (a - b) < eps && (b - a) < eps; }

// Run the system for a while at a fixed step.
void Run(GameplaySystem& sys, World& w, f32 seconds, f32 step = 1.0f / 60.0f) {
    for (f32 t = 0.0f; t < seconds; t += step) sys.Update(&w, step);
}

} // namespace

// ---------------------------------------------------------------------------
// The offset rule
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, ADoorOpensRelativeToWhereItWasPlaced) {
    // Arrange: a door at (10, 0, 5), authored to slide up 3 by the default
    // openPosition. Unlocked and auto-opening, with a player standing at it.
    World w;
    GameplaySystem sys;
    Entity door = MakeAt(w, Vector3(10.0f, 0.0f, 5.0f), "Door");
    auto& lock = w.AddComponent<LockComponent>(door, LockComponent{});
    lock.isLocked = false;
    lock.autoOpen = true;
    lock.interactRange = 3.0f;
    lock.openSpeed = 10.0f;
    MakePlayer(w, Vector3(11.0f, 0.0f, 5.0f));

    sys.OnPlayStart(&w);

    // Act
    Run(sys, w, 1.0f);

    // Assert: it went UP FROM ITS OWN POSITION. Read as a world position,
    // openPosition (0, 3, 0) would have put it at x=0, z=0.
    const Vector3 p = PositionOf(w, door);
    ENJIN_EXPECT_TRUE(Near(p.x, 10.0f));
    ENJIN_EXPECT_TRUE(Near(p.z, 5.0f));
    ENJIN_EXPECT_TRUE(Near(p.y, 3.0f));
    ENJIN_EXPECT_TRUE(w.GetComponent<LockComponent>(door)->isOpen);
}

ENJIN_TEST(GameplaySystem, StopPutsTheDoorBackWhereTheAuthorLeftIt) {
    World w;
    GameplaySystem sys;
    Entity door = MakeAt(w, Vector3(-4.0f, 1.0f, 2.0f), "Door");
    auto& lock = w.AddComponent<LockComponent>(door, LockComponent{});
    lock.isLocked = false;
    lock.autoOpen = true;
    lock.interactRange = 3.0f;
    lock.openSpeed = 10.0f;
    MakePlayer(w, Vector3(-4.0f, 1.0f, 2.5f));

    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);
    ENJIN_ASSERT_TRUE(PositionOf(w, door).y > 2.0f);   // it did move

    sys.Reset(&w);

    const Vector3 p = PositionOf(w, door);
    ENJIN_EXPECT_TRUE(Near(p.x, -4.0f) && Near(p.y, 1.0f) && Near(p.z, 2.0f));
    // And the component's play state is cleared, or a second Play starts open.
    const auto* after = w.GetComponent<LockComponent>(door);
    ENJIN_EXPECT_FALSE(after->isOpen);
    ENJIN_EXPECT_TRUE(Near(after->openProgress, 0.0f));
}

// ---------------------------------------------------------------------------
// Locks
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, ALockedDoorStaysShutWithoutTheKey) {
    World w;
    GameplaySystem sys;
    Entity door = MakeAt(w, Vector3(0, 0, 0), "Door");
    auto& lock = w.AddComponent<LockComponent>(door, LockComponent{});
    lock.isLocked = true;
    lock.requiredKey = "brass";
    lock.autoOpen = true;
    lock.interactRange = 3.0f;
    MakePlayer(w, Vector3(1.0f, 0, 0));     // no inventory at all

    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);

    ENJIN_EXPECT_FALSE(w.GetComponent<LockComponent>(door)->isOpen);
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, door).y, 0.0f));
}

ENJIN_TEST(GameplaySystem, TheRightKeyOpensItAndConsumeKeyTakesIt) {
    World w;
    GameplaySystem sys;
    Entity door = MakeAt(w, Vector3(0, 0, 0), "Door");
    auto& lock = w.AddComponent<LockComponent>(door, LockComponent{});
    lock.isLocked = true;
    lock.requiredKey = "brass";
    lock.consumeKey = true;
    lock.autoOpen = true;
    lock.interactRange = 3.0f;
    lock.openSpeed = 10.0f;

    Entity player = MakePlayer(w, Vector3(1.0f, 0, 0));
    auto& inv = w.AddComponent<InventoryComponent>(player, InventoryComponent{});
    inv.keys.push_back("brass");

    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);

    ENJIN_EXPECT_TRUE(w.GetComponent<LockComponent>(door)->isOpen);
    // consumeKey means the key is gone, not just checked.
    ENJIN_EXPECT_TRUE(w.GetComponent<InventoryComponent>(player)->keys.empty());
}

ENJIN_TEST(GameplaySystem, ATimedDoorClosesItself) {
    World w;
    GameplaySystem sys;
    Entity door = MakeAt(w, Vector3(0, 0, 0), "Door");
    auto& lock = w.AddComponent<LockComponent>(door, LockComponent{});
    lock.isLocked = false;
    lock.autoOpen = true;
    lock.interactRange = 3.0f;
    lock.openMode = LockComponent::OpenMode::Timed;
    lock.openDuration = 0.5f;
    lock.openSpeed = 20.0f;

    // The player has to leave, or autoOpen re-opens it the frame it closes.
    Entity player = MakePlayer(w, Vector3(1.0f, 0, 0));
    sys.OnPlayStart(&w);
    Run(sys, w, 0.2f);
    ENJIN_ASSERT_TRUE(w.GetComponent<LockComponent>(door)->isOpen);

    w.GetComponent<TransformComponent>(player)->position = Vector3(50.0f, 0, 0);
    Run(sys, w, 1.0f);

    ENJIN_EXPECT_FALSE(w.GetComponent<LockComponent>(door)->isOpen);
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, door).y, 0.0f, 0.05f));
}

// ---------------------------------------------------------------------------
// Switches
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, APressurePlateFiresWhileSomethingIsOnIt) {
    World w;
    GameplaySystem sys;
    Entity plate = MakeAt(w, Vector3(0, 0, 0), "Plate");
    GiveBox(w, plate, Vector3(2.0f, 0.2f, 2.0f));
    auto& sw = w.AddComponent<SwitchComponent>(plate, SwitchComponent{});
    sw.type = SwitchComponent::SwitchType::PressurePlate;

    Entity crate = MakeAt(w, Vector3(0.0f, 0.1f, 0.0f), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(w.GetComponent<SwitchComponent>(plate)->isActive);

    // Take it away again and the plate releases. A latching pressure plate would
    // be a different component.
    w.GetComponent<TransformComponent>(crate)->position = Vector3(20.0f, 0, 0);
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(w.GetComponent<SwitchComponent>(plate)->isActive);
}

ENJIN_TEST(GameplaySystem, APlateWithAWeightThresholdIgnoresSomethingTooLight) {
    World w;
    GameplaySystem sys;
    Entity plate = MakeAt(w, Vector3(0, 0, 0), "Plate");
    GiveBox(w, plate, Vector3(2.0f, 0.2f, 2.0f));
    auto& sw = w.AddComponent<SwitchComponent>(plate, SwitchComponent{});
    sw.activationWeight = 10.0f;

    Entity pebble = MakeAt(w, Vector3(0, 0.1f, 0), "pebble");
    GiveBox(w, pebble, Vector3(0.5f, 0.5f, 0.5f));
    auto& light = w.AddComponent<PushableComponent>(pebble, PushableComponent{});
    light.mass = 1.0f;

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(w.GetComponent<SwitchComponent>(plate)->isActive);

    light.mass = 25.0f;
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(w.GetComponent<SwitchComponent>(plate)->isActive);
}

ENJIN_TEST(GameplaySystem, ASwitchOpensTheLockItIsLinkedTo) {
    // The whole point of linkedEntities, and the reason the field exists.
    World w;
    GameplaySystem sys;
    Entity door = MakeAt(w, Vector3(5.0f, 0, 0), "Door");
    auto& lock = w.AddComponent<LockComponent>(door, LockComponent{});
    lock.isLocked = true;             // no key anywhere; the switch IS the key
    lock.autoOpen = false;
    lock.interactRange = 0.0f;
    lock.openSpeed = 20.0f;

    Entity plate = MakeAt(w, Vector3(0, 0, 0), "Plate");
    GiveBox(w, plate, Vector3(2.0f, 0.2f, 2.0f));
    auto& sw = w.AddComponent<SwitchComponent>(plate, SwitchComponent{});
    sw.linkedEntities.push_back(door);

    Entity crate = MakeAt(w, Vector3(0, 0.1f, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    sys.OnPlayStart(&w);
    Run(sys, w, 0.5f);

    ENJIN_EXPECT_TRUE(w.GetComponent<LockComponent>(door)->isOpen);
    ENJIN_EXPECT_TRUE(PositionOf(w, door).y > 2.0f);
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, door).x, 5.0f));   // still relative
}

// ---------------------------------------------------------------------------
// Conveyors
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, AConveyorCarriesWhatIsOnIt) {
    World w;
    GameplaySystem sys;
    Entity belt = MakeAt(w, Vector3(0, 0, 0), "Belt");
    GiveBox(w, belt, Vector3(10.0f, 0.2f, 2.0f));
    auto& conveyor = w.AddComponent<ConveyorComponent>(belt, ConveyorComponent{});
    conveyor.direction = Vector3(1, 0, 0);
    conveyor.speed = 2.0f;

    Entity crate = MakeAt(w, Vector3(0, 0.1f, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);

    // 2 units/second for a second, give or take the step.
    ENJIN_EXPECT_TRUE(PositionOf(w, crate).x > 1.8f);
    ENJIN_EXPECT_TRUE(PositionOf(w, crate).x < 2.2f);
}

ENJIN_TEST(GameplaySystem, AnInactiveConveyorCarriesNothing) {
    World w;
    GameplaySystem sys;
    Entity belt = MakeAt(w, Vector3(0, 0, 0), "Belt");
    GiveBox(w, belt, Vector3(10.0f, 0.2f, 2.0f));
    auto& conveyor = w.AddComponent<ConveyorComponent>(belt, ConveyorComponent{});
    conveyor.isActive = false;

    Entity crate = MakeAt(w, Vector3(0, 0.1f, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, crate).x, 0.0f));
}

ENJIN_TEST(GameplaySystem, AConveyorWithNoDirectionMovesNothingRatherThanNaN) {
    // Normalising a zero vector is the classic way to turn a blank field into NaN
    // and send the crate to nowhere, permanently.
    World w;
    GameplaySystem sys;
    Entity belt = MakeAt(w, Vector3(0, 0, 0), "Belt");
    GiveBox(w, belt, Vector3(10.0f, 0.2f, 2.0f));
    auto& conveyor = w.AddComponent<ConveyorComponent>(belt, ConveyorComponent{});
    conveyor.direction = Vector3(0, 0, 0);

    Entity crate = MakeAt(w, Vector3(0, 0.1f, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);

    const Vector3 p = PositionOf(w, crate);
    ENJIN_EXPECT_TRUE(Near(p.x, 0.0f));
    ENJIN_EXPECT_TRUE(p.x == p.x);   // not NaN
}

// ---------------------------------------------------------------------------
// Teleporters
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, ATeleporterMovesWhatStandsOnIt) {
    World w;
    GameplaySystem sys;
    Entity pad = MakeAt(w, Vector3(0, 0, 0), "Pad");
    GiveBox(w, pad, Vector3(2.0f, 1.0f, 2.0f));
    auto& tp = w.AddComponent<TeleporterComponent>(pad, TeleporterComponent{});
    tp.targetPosition = Vector3(100.0f, 5.0f, -20.0f);

    Entity player = MakePlayer(w, Vector3(0, 0, 0));

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);

    const Vector3 p = PositionOf(w, player);
    ENJIN_EXPECT_TRUE(Near(p.x, 100.0f) && Near(p.y, 5.0f) && Near(p.z, -20.0f));
}

ENJIN_TEST(GameplaySystem, LinkedTeleportersDoNotPingPongForever) {
    // The classic failure: A sends you to B, you arrive inside B, B sends you back.
    // Putting the FAR end on cooldown too is what stops it.
    World w;
    GameplaySystem sys;
    Entity a = MakeAt(w, Vector3(0, 0, 0), "A");
    Entity b = MakeAt(w, Vector3(50.0f, 0, 0), "B");
    GiveBox(w, a, Vector3(2.0f, 2.0f, 2.0f));
    GiveBox(w, b, Vector3(2.0f, 2.0f, 2.0f));

    // Both components are added BEFORE either is written to. Holding the
    // reference from the first AddComponent across the second one is reading freed
    // memory -- ComponentStorage is a dense vector and Add is a push_back (see
    // TestComponentPointerLifetime). The first draft of this test did exactly that,
    // and the symptom was a teleporter with linkedTeleporter silently still 0.
    w.AddComponent<TeleporterComponent>(a, TeleporterComponent{});
    w.AddComponent<TeleporterComponent>(b, TeleporterComponent{});
    {
        auto* ta = w.GetComponent<TeleporterComponent>(a);
        auto* tb = w.GetComponent<TeleporterComponent>(b);
        ta->linkedTeleporter = b;
        tb->linkedTeleporter = a;
        ta->cooldown = 1.0f;
        tb->cooldown = 1.0f;
    }

    Entity player = MakePlayer(w, Vector3(0, 0, 0));

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);
    // One hop, and it stays there rather than bouncing every frame.
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, player).x, 50.0f));

    Run(sys, w, 0.5f);
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, player).x, 50.0f));
}

ENJIN_TEST(GameplaySystem, ATeleporterWithARequiredTagIgnoresEverythingElse) {
    World w;
    GameplaySystem sys;
    Entity pad = MakeAt(w, Vector3(0, 0, 0), "Pad");
    GiveBox(w, pad, Vector3(4.0f, 2.0f, 4.0f));
    auto& tp = w.AddComponent<TeleporterComponent>(pad, TeleporterComponent{});
    tp.targetPosition = Vector3(80.0f, 0, 0);
    tp.requiredTag = "crate";

    Entity player = MakePlayer(w, Vector3(0, 0, 0));
    Entity crate = MakeAt(w, Vector3(0.5f, 0, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);

    ENJIN_EXPECT_TRUE(Near(PositionOf(w, player).x, 0.0f));   // not tagged
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, crate).x, 80.0f));   // tagged by name
}

// ---------------------------------------------------------------------------
// Moving platforms
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, APlatformFollowsItsWaypointsRelativeToWhereItWasPlaced) {
    World w;
    GameplaySystem sys;
    Entity platform = MakeAt(w, Vector3(7.0f, 2.0f, -3.0f), "Platform");
    auto& mp = w.AddComponent<MovingPlatformComponent>(platform, MovingPlatformComponent{});
    mp.waypoints = { Vector3(0, 0, 0), Vector3(4.0f, 0, 0) };
    mp.speed = 4.0f;
    mp.waitTime = 0.0f;
    mp.mode = MovingPlatformComponent::PlatformMode::PingPong;

    sys.OnPlayStart(&w);
    Run(sys, w, 0.5f);

    // Halfway along a 4-unit leg at 4 units/second, measured from (7, 2, -3).
    const Vector3 p = PositionOf(w, platform);
    ENJIN_EXPECT_TRUE(p.x > 7.5f && p.x < 10.5f);
    ENJIN_EXPECT_TRUE(Near(p.y, 2.0f));
    ENJIN_EXPECT_TRUE(Near(p.z, -3.0f));
}

ENJIN_TEST(GameplaySystem, APlatformCarriesWhatIsStandingOnTop) {
    // Without this the rider is left behind one frame at a time, which looks like
    // the platform sliding out from under them.
    World w;
    GameplaySystem sys;
    Entity platform = MakeAt(w, Vector3(0, 0, 0), "Platform");
    GiveBox(w, platform, Vector3(4.0f, 0.5f, 4.0f));
    auto& mp = w.AddComponent<MovingPlatformComponent>(platform, MovingPlatformComponent{});
    mp.waypoints = { Vector3(0, 0, 0), Vector3(10.0f, 0, 0) };
    mp.speed = 4.0f;
    mp.waitTime = 0.0f;
    mp.carryEntities = true;

    // Standing ON it: the platform's top face is at y = 0.25.
    Entity rider = MakePlayer(w, Vector3(0.0f, 0.3f, 0.0f));
    GiveBox(w, rider, Vector3(0.6f, 1.8f, 0.6f));

    sys.OnPlayStart(&w);
    Run(sys, w, 0.5f);

    const f32 platformX = PositionOf(w, platform).x;
    const f32 riderX = PositionOf(w, rider).x;
    ENJIN_EXPECT_TRUE(platformX > 1.0f);              // it moved
    ENJIN_EXPECT_TRUE(Near(riderX, platformX, 0.2f)); // and took the rider
}

ENJIN_TEST(GameplaySystem, CarryDoesNotDragSomethingUnderneathThePlatform) {
    // Only the top face carries. A box-shaped carry volume would drag anything
    // inside the platform's whole bounds, including something walking beneath it.
    World w;
    GameplaySystem sys;
    Entity platform = MakeAt(w, Vector3(0, 5.0f, 0), "Platform");
    GiveBox(w, platform, Vector3(4.0f, 0.5f, 4.0f));
    auto& mp = w.AddComponent<MovingPlatformComponent>(platform, MovingPlatformComponent{});
    mp.waypoints = { Vector3(0, 0, 0), Vector3(10.0f, 0, 0) };
    mp.speed = 4.0f;
    mp.waitTime = 0.0f;

    Entity below = MakePlayer(w, Vector3(0.0f, 0.0f, 0.0f));   // five metres under
    GiveBox(w, below, Vector3(0.6f, 1.8f, 0.6f));

    sys.OnPlayStart(&w);
    Run(sys, w, 0.5f);

    ENJIN_EXPECT_TRUE(PositionOf(w, platform).x > 1.0f);
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, below).x, 0.0f));
}

ENJIN_TEST(GameplaySystem, AOneWayPlatformStopsAtTheEnd) {
    World w;
    GameplaySystem sys;
    Entity platform = MakeAt(w, Vector3(0, 0, 0), "Platform");
    auto& mp = w.AddComponent<MovingPlatformComponent>(platform, MovingPlatformComponent{});
    mp.waypoints = { Vector3(0, 0, 0), Vector3(2.0f, 0, 0) };
    mp.speed = 8.0f;
    mp.waitTime = 0.0f;
    mp.mode = MovingPlatformComponent::PlatformMode::OneWay;

    sys.OnPlayStart(&w);
    Run(sys, w, 2.0f);

    ENJIN_EXPECT_TRUE(Near(PositionOf(w, platform).x, 2.0f, 0.2f));
    ENJIN_EXPECT_FALSE(w.GetComponent<MovingPlatformComponent>(platform)->isMoving);
}

ENJIN_TEST(GameplaySystem, ATriggeredPlatformWaitsForItsSwitch) {
    World w;
    GameplaySystem sys;
    Entity platform = MakeAt(w, Vector3(0, 0, 0), "Platform");
    auto& mp = w.AddComponent<MovingPlatformComponent>(platform, MovingPlatformComponent{});
    mp.waypoints = { Vector3(0, 0, 0), Vector3(5.0f, 0, 0) };
    mp.speed = 4.0f;
    mp.waitTime = 0.0f;
    mp.mode = MovingPlatformComponent::PlatformMode::Triggered;
    mp.isMoving = false;

    Entity plate = MakeAt(w, Vector3(20.0f, 0, 0), "Plate");
    GiveBox(w, plate, Vector3(2.0f, 0.2f, 2.0f));
    auto& sw = w.AddComponent<SwitchComponent>(plate, SwitchComponent{});
    sw.linkedEntities.push_back(platform);

    sys.OnPlayStart(&w);
    Run(sys, w, 0.5f);
    ENJIN_EXPECT_TRUE(Near(PositionOf(w, platform).x, 0.0f));   // nothing on the plate

    Entity crate = MakeAt(w, Vector3(20.0f, 0.1f, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    Run(sys, w, 0.5f);
    ENJIN_EXPECT_TRUE(PositionOf(w, platform).x > 1.0f);
}

// ---------------------------------------------------------------------------
// Goal zones
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, APushTargetIsSatisfiedByAPushableNotByThePlayer) {
    World w;
    GameplaySystem sys;
    Entity goal = MakeAt(w, Vector3(0, 0, 0), "Goal");
    GiveBox(w, goal, Vector3(2.0f, 2.0f, 2.0f));
    auto& gz = w.AddComponent<GoalZoneComponent>(goal, GoalZoneComponent{});
    gz.type = GoalZoneComponent::GoalType::PushTarget;

    Entity player = MakePlayer(w, Vector3(0, 0, 0));
    GiveBox(w, player, Vector3(0.6f, 1.8f, 0.6f));

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(w.GetComponent<GoalZoneComponent>(goal)->isSatisfied);

    Entity crate = MakeAt(w, Vector3(0, 0, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(w.GetComponent<GoalZoneComponent>(goal)->isSatisfied);
    ENJIN_EXPECT_EQ(w.GetComponent<GoalZoneComponent>(goal)->satisfiedBy, crate);
}

ENJIN_TEST(GameplaySystem, AGoalUnsatisfiesWhenTheCrateIsPushedBackOff) {
    // A latching goal would be a different design, and Sokoban needs this one:
    // pushing a box off a target has to un-solve it.
    World w;
    GameplaySystem sys;
    Entity goal = MakeAt(w, Vector3(0, 0, 0), "Goal");
    GiveBox(w, goal, Vector3(2.0f, 2.0f, 2.0f));
    w.AddComponent<GoalZoneComponent>(goal, GoalZoneComponent{});

    Entity crate = MakeAt(w, Vector3(0, 0, 0), "crate");
    GiveBox(w, crate, Vector3(1.0f, 1.0f, 1.0f));
    w.AddComponent<PushableComponent>(crate, PushableComponent{});

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_ASSERT_TRUE(w.GetComponent<GoalZoneComponent>(goal)->isSatisfied);

    w.GetComponent<TransformComponent>(crate)->position = Vector3(20.0f, 0, 0);
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(w.GetComponent<GoalZoneComponent>(goal)->isSatisfied);
}

ENJIN_TEST(GameplaySystem, AnItemDepositNeedsTheNamedItem) {
    World w;
    GameplaySystem sys;
    Entity goal = MakeAt(w, Vector3(0, 0, 0), "Altar");
    GiveBox(w, goal, Vector3(2.0f, 2.0f, 2.0f));
    auto& gz = w.AddComponent<GoalZoneComponent>(goal, GoalZoneComponent{});
    gz.type = GoalZoneComponent::GoalType::ItemDeposit;
    gz.requiredItem = "idol";

    Entity player = MakePlayer(w, Vector3(0, 0, 0));
    auto& inv = w.AddComponent<InventoryComponent>(player, InventoryComponent{});

    sys.OnPlayStart(&w);
    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_FALSE(w.GetComponent<GoalZoneComponent>(goal)->isSatisfied);

    InventoryComponent::InventorySlot slot;
    slot.itemId = "idol";
    slot.quantity = 1;
    w.GetComponent<InventoryComponent>(player)->slots.push_back(slot);

    sys.Update(&w, 1.0f / 60.0f);
    ENJIN_EXPECT_TRUE(w.GetComponent<GoalZoneComponent>(goal)->isSatisfied);
}

// ---------------------------------------------------------------------------
// Spawn points
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, ASpawnPointWithNoPrefabSaysSoAndStops) {
    // The field is blank, so there is nothing to spawn. It must not spin forever
    // silently, and it must not invent an empty entity that makes the count go up
    // while nothing appears in the scene.
    World w;
    GameplaySystem sys;
    Entity point = MakeAt(w, Vector3(0, 0, 0), "Spawner");
    auto& spawn = w.AddComponent<SpawnPointComponent>(point, SpawnPointComponent{});
    spawn.spawnId = "wave";
    spawn.spawnOnStart = true;
    spawn.maxSpawns = 5;
    // prefabToSpawn deliberately left empty

    const usize before = w.GetEntityCount();
    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);

    ENJIN_EXPECT_EQ(w.GetEntityCount(), before);
    // And the component's own count still says what really happened. Stopping the
    // retries by writing currentSpawns = maxSpawns would report five spawns.
    ENJIN_EXPECT_EQ(w.GetComponent<SpawnPointComponent>(point)->currentSpawns, 0);
}

ENJIN_TEST(GameplaySystem, ASpawnPointThatCannotLoadItsPrefabSpawnsNothing) {
    // A missing prefab file must not produce a placeholder. An empty entity named
    // after the prefab would make the spawn count rise with nothing on screen,
    // which is the exact failure this whole system exists to remove.
    World w;
    GameplaySystem sys;
    Entity point = MakeAt(w, Vector3(0, 0, 0), "Spawner");
    auto& spawn = w.AddComponent<SpawnPointComponent>(point, SpawnPointComponent{});
    spawn.spawnId = "wave";
    spawn.prefabToSpawn = "prefabs/definitely_not_here.enjprefab";
    spawn.spawnOnStart = true;
    spawn.maxSpawns = 3;

    const usize before = w.GetEntityCount();
    sys.OnPlayStart(&w);
    Run(sys, w, 1.0f);

    ENJIN_EXPECT_EQ(w.GetEntityCount(), before);
    ENJIN_EXPECT_TRUE(w.GetComponent<SpawnPointComponent>(point)->spawnedEntities.empty());
}

// ---------------------------------------------------------------------------
// Bounds
// ---------------------------------------------------------------------------

ENJIN_TEST(GameplaySystem, ColliderBoundsAreWorldSpaceAndIgnoreTransformScale) {
    // The engine's convention: Jolt and Box2D do NOT multiply a collider by the
    // transform scale. If this system did, every overlap in it would disagree with
    // every physics query in the rest of the engine.
    World w;
    Entity e = MakeAt(w, Vector3(0, 0, 0), "Box");
    w.GetComponent<TransformComponent>(e)->scale = Vector3(10.0f, 10.0f, 10.0f);
    GiveBox(w, e, Vector3(2.0f, 2.0f, 2.0f));

    const WorldBounds b = ComputeWorldBounds(&w, e);
    ENJIN_EXPECT_TRUE(Near(b.max.x, 1.0f));     // half of 2, not half of 20
    ENJIN_EXPECT_TRUE(Near(b.min.x, -1.0f));
}

ENJIN_TEST(GameplaySystem, AnEntityWithNoColliderIsAPointNotAMiss) {
    // A crate with no collider standing on a conveyor should still ride it.
    // Treating "no collider" as "no bounds, skip it" would silently exclude it.
    World w;
    Entity e = MakeAt(w, Vector3(3.0f, 1.0f, -2.0f), "Loose");
    const WorldBounds b = ComputeWorldBounds(&w, e);

    ENJIN_EXPECT_TRUE(Near(b.min.x, 3.0f) && Near(b.max.x, 3.0f));
    ENJIN_EXPECT_TRUE(Near(b.min.y, 1.0f) && Near(b.max.y, 1.0f));

    WorldBounds around;
    around.min = Vector3(2.0f, 0.0f, -3.0f);
    around.max = Vector3(4.0f, 2.0f, -1.0f);
    ENJIN_EXPECT_TRUE(around.Overlaps(b));
}

ENJIN_TEST(GameplaySystem, CapsuleBoundsUseTheHeightPlusTwoRadiiConvention) {
    // height is the CYLINDER only; total is height + 2 * radius. Getting this
    // wrong makes every character half a metre shorter than the physics thinks.
    World w;
    Entity e = MakeAt(w, Vector3(0, 0, 0), "Character");
    auto& cap = w.AddComponent<CapsuleColliderComponent>(e, CapsuleColliderComponent{});
    cap.height = 1.0f;
    cap.radius = 0.5f;

    const WorldBounds b = ComputeWorldBounds(&w, e);
    ENJIN_EXPECT_TRUE(Near(b.max.y, 1.0f));     // 1.0/2 + 0.5
    ENJIN_EXPECT_TRUE(Near(b.min.y, -1.0f));
}

ENJIN_TEST_MAIN()
