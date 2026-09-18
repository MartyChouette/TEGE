// A pickup bobs, spins and is pulled in, as its own fields have always said.
//
// bobSpeed (2.0), bobHeight (0.2) and rotationSpeed (90 deg/s) ship non-zero, so
// every coin in every scene was authored to float and turn, and every one of
// them sat perfectly still: nothing ticked a pickup per frame except the respawn
// timer. magnetToPlayer, magnetRange and magnetSpeed moved nothing at all.
// Found by tools/unread_field_audit.py.
//
// The bob swings around an ANCHOR captured on the first tick rather than adding
// to the live position, which is the part these tests are really pinning: an
// offset applied to the live position walks the pickup off its placement a
// little further every frame, always in one direction, and looks fine for the
// first second.
#include "EnjinTest.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"

using namespace Enjin;

namespace {

ECS::Entity MakePickup(ECS::World& world, const Math::Vector3& at) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = at;
    world.AddComponent<ECS::TransformComponent>(e, xf);
    world.AddComponent<ECS::PickupComponent>(e);
    return e;
}

ECS::Entity MakePlayer(ECS::World& world, const Math::Vector3& at) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = at;
    world.AddComponent<ECS::TransformComponent>(e, xf);
    world.AddComponent<ECS::FirstPersonController>(e);
    return e;
}

Math::Vector3 PositionOf(ECS::World& world, ECS::Entity e) {
    return world.GetComponent<ECS::TransformComponent>(e)->position;
}

void Tick(ECS::World& world, int frames, f32 dt = 1.0f / 60.0f) {
    for (int i = 0; i < frames; ++i) {
        Gameplay::GameplayLoop::UpdatePickupMotion(&world, dt);
    }
}

} // namespace

ENJIN_TEST(PickupMotion, test_a_default_pickup_bobs_up_and_down) {
    // Arrange
    ECS::World world;
    ECS::Entity coin = MakePickup(world, Math::Vector3(0.0f, 1.0f, 0.0f));

    // Act: a quarter of the default 2 rad/s bob period is about 0.79s.
    Tick(world, 47);
    const f32 high = PositionOf(world, coin).y;
    Tick(world, 94);                       // half a period further on
    const f32 low = PositionOf(world, coin).y;

    // Assert
    ENJIN_EXPECT_TRUE(high > 1.0f);
    ENJIN_EXPECT_TRUE(low < 1.0f);
}

ENJIN_TEST(PickupMotion, test_the_bob_does_not_walk_the_pickup_away_from_its_placement) {
    // The failure an offset-on-live-position implementation has, and the reason
    // the anchor exists. Over many periods the height must stay inside the
    // authored amplitude rather than drifting off in one direction.
    // Arrange
    ECS::World world;
    ECS::Entity coin = MakePickup(world, Math::Vector3(0.0f, 5.0f, 0.0f));
    const f32 amplitude = world.GetComponent<ECS::PickupComponent>(coin)->bobHeight;

    // Act
    f32 highest = -1e9f, lowest = 1e9f;
    for (int i = 0; i < 3000; ++i) {       // ~50 seconds, ~16 bob periods
        Tick(world, 1);
        const f32 y = PositionOf(world, coin).y;
        highest = Math::Max(highest, y);
        lowest  = Math::Min(lowest, y);
    }

    // Assert
    ENJIN_EXPECT_TRUE(highest <= 5.0f + amplitude + 0.001f);
    ENJIN_EXPECT_TRUE(lowest  >= 5.0f - amplitude - 0.001f);
}

ENJIN_TEST(PickupMotion, test_a_zero_bob_height_leaves_the_pickup_still) {
    // A scene that wants a static pickup turns it off, and off must mean off.
    // Arrange
    ECS::World world;
    ECS::Entity coin = MakePickup(world, Math::Vector3(2.0f, 3.0f, 4.0f));
    auto* pk = world.GetComponent<ECS::PickupComponent>(coin);
    pk->bobHeight = 0.0f;
    pk->rotationSpeed = 0.0f;

    // Act
    Tick(world, 600);

    // Assert
    const Math::Vector3 p = PositionOf(world, coin);
    ENJIN_EXPECT_FLOAT_NEAR(p.x, 2.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.y, 3.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.z, 4.0f, 0.0001f);
}

ENJIN_TEST(PickupMotion, test_the_default_rotation_speed_turns_the_pickup) {
    // Arrange
    ECS::World world;
    ECS::Entity coin = MakePickup(world, Math::Vector3(0.0f, 0.0f, 0.0f));
    const Math::Quaternion start = world.GetComponent<ECS::TransformComponent>(coin)->rotation;

    // Act: a second at the default 90 deg/s.
    Tick(world, 60);

    // Assert: the forward axis has swung about a quarter turn.
    const Math::Quaternion now = world.GetComponent<ECS::TransformComponent>(coin)->rotation;
    const Math::Vector3 before = start.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f));
    const Math::Vector3 after  = now.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f));
    ENJIN_EXPECT_FLOAT_NEAR(before.Dot(after), 0.0f, 0.05f);
    // Still a unit quaternion after 60 multiplies -- the renormalise is doing its job.
    ENJIN_EXPECT_FLOAT_NEAR(now.Length(), 1.0f, 0.0001f);
}

ENJIN_TEST(PickupMotion, test_a_magnet_pickup_travels_toward_a_player_in_range) {
    // Arrange
    ECS::World world;
    ECS::Entity coin = MakePickup(world, Math::Vector3(2.0f, 0.0f, 0.0f));
    auto* pk = world.GetComponent<ECS::PickupComponent>(coin);
    pk->magnetToPlayer = true;
    pk->bobHeight = 0.0f;
    MakePlayer(world, Math::Vector3(0.0f, 0.0f, 0.0f));

    // Act: 2 units at 10 units/s is well inside the default 3-unit range.
    Tick(world, 6);
    const f32 closer = PositionOf(world, coin).x;
    Tick(world, 60);
    const f32 arrived = PositionOf(world, coin).x;

    // Assert
    ENJIN_EXPECT_TRUE(closer < 2.0f && closer > 0.0f);
    // Arrived and STOPPED. Overshooting would oscillate across the player and
    // make the overlap test flicker with it.
    ENJIN_EXPECT_FLOAT_NEAR(arrived, 0.0f, 0.0001f);
}

ENJIN_TEST(PickupMotion, test_a_player_outside_the_magnet_range_pulls_nothing) {
    // Arrange
    ECS::World world;
    ECS::Entity coin = MakePickup(world, Math::Vector3(20.0f, 0.0f, 0.0f));
    auto* pk = world.GetComponent<ECS::PickupComponent>(coin);
    pk->magnetToPlayer = true;
    pk->bobHeight = 0.0f;
    MakePlayer(world, Math::Vector3(0.0f, 0.0f, 0.0f));   // 20 units, range is 3

    // Act
    Tick(world, 120);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(PositionOf(world, coin).x, 20.0f, 0.0001f);
}

ENJIN_TEST(PickupMotion, test_a_collected_pickup_stops_moving) {
    // It is hidden, waiting on a respawn. A coin quietly bobbing where nothing
    // is drawn would still be magneted around and collected on arrival.
    // Arrange
    ECS::World world;
    ECS::Entity coin = MakePickup(world, Math::Vector3(2.0f, 1.0f, 0.0f));
    auto* pk = world.GetComponent<ECS::PickupComponent>(coin);
    pk->magnetToPlayer = true;
    pk->isCollected = true;
    MakePlayer(world, Math::Vector3(0.0f, 1.0f, 0.0f));

    // Act
    Tick(world, 120);

    // Assert
    const Math::Vector3 p = PositionOf(world, coin);
    ENJIN_EXPECT_FLOAT_NEAR(p.x, 2.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.y, 1.0f, 0.0001f);
}

ENJIN_TEST_MAIN()
