// A script teleport has to move a dynamic body, not just its transform.
//
// Physics_Teleport writes the TransformComponent and then calls
// ForceSetBodyState so the Jolt body goes with it. In Gobliny every goblin on a
// dynamic rigidbody came back to where it had been standing on the very next
// step: the transform said "spawn", the body said "home", and the sync after
// the step wrote the body's answer over the transform. Mini-games that place
// their players on spawn points started with everyone still at home.
//
// The body under test is the one that showed it: a capsule with its collider
// centre raised off the origin (feet at the origin), rotation frozen on every
// axis, resting on a static floor.
#include "EnjinTest.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Gameplay/SimulationClock.h"

#include <cmath>
#include <memory>

using namespace Enjin;

namespace {

void MakeFloor(ECS::World& world) {
    ECS::Entity floor = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = Math::Vector3(0.0f, -0.2f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(floor, xf);
    ECS::BoxColliderComponent col;
    col.size = Math::Vector3(100.0f, 0.4f, 100.0f);
    world.AddComponent<ECS::BoxColliderComponent>(floor, col);
}

ECS::Entity MakeGoblin(ECS::World& world, const Math::Vector3& at, bool freezeRotation) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = at;
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::RigidbodyComponent rb;
    rb.mass = 3.0f;
    rb.freezeRotationX = freezeRotation;
    rb.freezeRotationY = freezeRotation;
    rb.freezeRotationZ = freezeRotation;
    world.AddComponent<ECS::RigidbodyComponent>(e, rb);

    ECS::CapsuleColliderComponent cap;
    cap.radius = 0.35f;
    cap.height = 1.1f;                              // stem; 1.8 tip to tip
    cap.center = Math::Vector3(0.0f, 0.9f, 0.0f);   // feet at the origin
    world.AddComponent<ECS::CapsuleColliderComponent>(e, cap);
    return e;
}

// What Physics_Teleport does, minus the script engine.
void Teleport(ECS::World& world, Physics::IPhysicsBackend& physics, ECS::Entity e,
              const Math::Vector3& to) {
    auto* t = world.GetComponent<ECS::TransformComponent>(e);
    t->position = to;
    if (auto* rb = world.GetComponent<ECS::RigidbodyComponent>(e)) {
        rb->velocity = Math::Vector3(0.0f, 0.0f, 0.0f);
        rb->angularVelocity = Math::Vector3(0.0f, 0.0f, 0.0f);
    }
    physics.ForceSetBodyState(e, to, t->rotation, Math::Vector3(0.0f, 0.0f, 0.0f),
                              Math::Vector3(0.0f, 0.0f, 0.0f));
}

f32 HorizontalDistance(const Math::Vector3& a, const Math::Vector3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// Settle, teleport, step, and report how far from the target the body ended.
// Returns -1 when the build has no Jolt.
f32 TeleportAfterSettling(bool freezeRotation, int settleSteps) {
    ECS::World world;
    MakeFloor(world);
    const Math::Vector3 home(-1.0f, 0.0f, 8.0f);
    const Math::Vector3 spawn(-4.2f, 0.0f, 17.3f);
    ECS::Entity goblin = MakeGoblin(world, home, freezeRotation);

    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) return -1.0f;
    physics->SetWorld(&world);

    for (int i = 0; i < settleSteps; ++i) physics->Update(1.0f / 60.0f);

    Teleport(world, *physics, goblin, spawn);
    for (int i = 0; i < 3; ++i) physics->Update(1.0f / 60.0f);

    return HorizontalDistance(world.GetComponent<ECS::TransformComponent>(goblin)->position,
                              spawn);
}

// The same, through the fixed-step clock play mode runs physics under: ticks at
// 60 Hz, render frames at frameDt, poses interpolated between ticks. Scripts
// run after the clock each frame, so the teleport lands between two Ticks.
f32 TeleportUnderClock(f32 frameDt) {
    ECS::World world;
    MakeFloor(world);
    const Math::Vector3 home(-1.0f, 0.0f, 8.0f);
    const Math::Vector3 spawn(-4.2f, 0.0f, 17.3f);
    ECS::Entity goblin = MakeGoblin(world, home, true);

    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) return -1.0f;
    physics->SetWorld(&world);

    Gameplay::SimulationClock clock;
    clock.Configure(true, 60.0f);
    clock.Reset();
    auto step = [&](f32 dt) { physics->Update(dt); };

    for (int i = 0; i < 240; ++i) clock.Tick(&world, frameDt, step);
    Teleport(world, *physics, goblin, spawn);
    for (int i = 0; i < 10; ++i) clock.Tick(&world, frameDt, step);

    return HorizontalDistance(world.GetComponent<ECS::TransformComponent>(goblin)->position,
                              spawn);
}

} // namespace

ENJIN_TEST(TeleportDynamicBody, test_a_teleport_survives_the_fixed_step_clock_at_144_fps) {
    const f32 miss = TeleportUnderClock(1.0f / 144.0f);
    if (miss < 0.0f) ENJIN_SKIP("Jolt is not in this build");
    ENJIN_EXPECT_TRUE(miss < 0.05f);
}

ENJIN_TEST(TeleportDynamicBody, test_a_teleport_survives_the_fixed_step_clock_at_60_fps) {
    const f32 miss = TeleportUnderClock(1.0f / 60.0f);
    if (miss < 0.0f) ENJIN_SKIP("Jolt is not in this build");
    ENJIN_EXPECT_TRUE(miss < 0.05f);
}

ENJIN_TEST(TeleportDynamicBody, test_a_teleport_survives_the_fixed_step_clock_at_30_fps) {
    const f32 miss = TeleportUnderClock(1.0f / 30.0f);
    if (miss < 0.0f) ENJIN_SKIP("Jolt is not in this build");
    ENJIN_EXPECT_TRUE(miss < 0.05f);
}

ENJIN_TEST(TeleportDynamicBody, test_a_settled_goblin_stays_where_it_was_teleported) {
    // Arrange / Act: two seconds on the floor, long enough for Jolt to put it
    // to sleep, which is how a goblin waits out a menu.
    const f32 miss = TeleportAfterSettling(true, 120);

    // Assert
    if (miss < 0.0f) ENJIN_SKIP("Jolt is not in this build");
    ENJIN_EXPECT_TRUE(miss < 0.05f);
}

ENJIN_TEST(TeleportDynamicBody, test_a_goblin_teleported_straight_away_stays_there) {
    const f32 miss = TeleportAfterSettling(true, 1);
    if (miss < 0.0f) ENJIN_SKIP("Jolt is not in this build");
    ENJIN_EXPECT_TRUE(miss < 0.05f);
}

ENJIN_TEST(TeleportDynamicBody, test_a_free_rotating_body_stays_where_it_was_teleported) {
    const f32 miss = TeleportAfterSettling(false, 120);
    if (miss < 0.0f) ENJIN_SKIP("Jolt is not in this build");
    ENJIN_EXPECT_TRUE(miss < 0.05f);
}

ENJIN_TEST_MAIN()
