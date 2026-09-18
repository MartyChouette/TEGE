// A trigger zone tells the entity it names.
//
// onEnterNotify, onExitNotify and onStayNotify were authored fields with no
// reader: UpdateTriggerZones computed exactly who was inside each zone and told
// nobody, while the function's own header comment claimed it fired them. So a
// zone wired to open a door did nothing, and the only way to react to a zone
// was a physics sensor, which is a different component.
//
// These tests drive the zone update directly with no VisualScriptSystem, which
// is the null-callback path a caller with no scripting takes. What they pin is
// the OCCUPANCY bookkeeping the notifications are derived from -- enter, stay
// and exit edges -- because that is what decides which notification fires and
// it is testable without a script engine.
#include "EnjinTest.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"

using namespace Enjin;

namespace {

ECS::Entity MakeZone(ECS::World& world, const Math::Vector3& at, f32 radius) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = at;
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::TriggerZoneComponent zone;
    zone.shape = ECS::TriggerZoneComponent::Shape::Sphere;
    zone.sphereRadius = radius;
    world.AddComponent<ECS::TriggerZoneComponent>(e, zone);
    return e;
}

// Only PLAYER-CONTROLLED entities are tested for overlap: the zone update
// gathers the five controller types and nothing else, so a crate rolling
// through a zone is invisible to it. FirstPersonController stands in for any
// of them here.
ECS::Entity MakePlayer(ECS::World& world, const Math::Vector3& at) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = at;
    world.AddComponent<ECS::TransformComponent>(e, xf);
    world.AddComponent<ECS::FirstPersonController>(e);
    return e;
}

void MoveTo(ECS::World& world, ECS::Entity e, const Math::Vector3& to) {
    world.GetComponent<ECS::TransformComponent>(e)->position = to;
}

usize OccupantCount(ECS::World& world, ECS::Entity zone) {
    const auto* z = world.GetComponent<ECS::TriggerZoneComponent>(zone);
    return z ? z->entitiesInside.size() : 0;
}

} // namespace

ENJIN_TEST(TriggerZoneNotify, test_walking_in_registers_an_occupant) {
    // Arrange
    ECS::World world;
    ECS::Entity zone = MakeZone(world, Math::Vector3(0, 0, 0), 5.0f);
    ECS::Entity player = MakePlayer(world, Math::Vector3(100, 0, 0));

    // Act
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    const usize before = OccupantCount(world, zone);
    MoveTo(world, player, Math::Vector3(1, 0, 0));
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);

    // Assert
    ENJIN_EXPECT_EQ(before, static_cast<usize>(0));
    ENJIN_EXPECT_EQ(OccupantCount(world, zone), static_cast<usize>(1));
}

ENJIN_TEST(TriggerZoneNotify, test_walking_out_clears_the_occupant) {
    // The exit edge is the one most likely to be wrong, because it has to look
    // at the PREVIOUS occupants rather than the current ones -- an exit is the
    // absence of something, and absences are easy to iterate past.
    // Arrange
    ECS::World world;
    ECS::Entity zone = MakeZone(world, Math::Vector3(0, 0, 0), 5.0f);
    ECS::Entity player = MakePlayer(world, Math::Vector3(1, 0, 0));
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    ENJIN_ASSERT_EQ(OccupantCount(world, zone), static_cast<usize>(1));

    // Act
    MoveTo(world, player, Math::Vector3(100, 0, 0));
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);

    // Assert
    ENJIN_EXPECT_EQ(OccupantCount(world, zone), static_cast<usize>(0));
}

ENJIN_TEST(TriggerZoneNotify, test_staying_inside_keeps_one_occupant_not_two) {
    // The stay path adds nobody. A zone that re-registered its occupant every
    // frame would grow its list without bound and fire enter repeatedly.
    // Arrange
    ECS::World world;
    ECS::Entity zone = MakeZone(world, Math::Vector3(0, 0, 0), 5.0f);
    MakePlayer(world, Math::Vector3(1, 0, 0));

    // Act
    for (int i = 0; i < 10; ++i) Gameplay::GameplayLoop::UpdateTriggerZones(&world);

    // Assert
    ENJIN_EXPECT_EQ(OccupantCount(world, zone), static_cast<usize>(1));
}

ENJIN_TEST(TriggerZoneNotify, test_a_null_script_system_still_tracks_occupancy) {
    // The path every caller without scripting takes. Notifications are skipped;
    // the bookkeeping the victory condition depends on must not be.
    // Arrange
    ECS::World world;
    ECS::Entity zone = MakeZone(world, Math::Vector3(0, 0, 0), 3.0f);
    ECS::Entity player = MakePlayer(world, Math::Vector3(0, 0, 0));

    // Act
    Gameplay::GameplayLoop::UpdateTriggerZones(&world, nullptr, 0.016f);

    // Assert
    ENJIN_EXPECT_EQ(OccupantCount(world, zone), static_cast<usize>(1));
    const auto* z = world.GetComponent<ECS::TriggerZoneComponent>(zone);
    ENJIN_ASSERT_TRUE(z != nullptr);
    ENJIN_EXPECT_TRUE(z->hasTriggered);
    (void)player;
}

ENJIN_TEST(TriggerZoneNotify, test_a_trigger_once_zone_latches) {
    // Arrange
    ECS::World world;
    ECS::Entity zone = MakeZone(world, Math::Vector3(0, 0, 0), 3.0f);
    world.GetComponent<ECS::TriggerZoneComponent>(zone)->triggerOnce = true;
    ECS::Entity player = MakePlayer(world, Math::Vector3(0, 0, 0));
    Gameplay::GameplayLoop::UpdateTriggerZones(&world);
    ENJIN_ASSERT_TRUE(world.GetComponent<ECS::TriggerZoneComponent>(zone)->hasTriggered);

    // Act: leave, and keep ticking
    MoveTo(world, player, Math::Vector3(100, 0, 0));
    for (int i = 0; i < 5; ++i) Gameplay::GameplayLoop::UpdateTriggerZones(&world);

    // Assert: a latched zone keeps its occupants and stops updating, which is
    // what "once ever" means here.
    ENJIN_EXPECT_TRUE(world.GetComponent<ECS::TriggerZoneComponent>(zone)->hasTriggered);
}

ENJIN_TEST_MAIN()
