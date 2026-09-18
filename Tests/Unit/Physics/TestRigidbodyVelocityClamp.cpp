// The speed ceilings RigidbodyComponent has always documented and never had.
//
// maxVelocity and maxAngularVelocity carry the comment "clamped each frame".
// Nothing clamped them -- not the physics backend, not the sync, not anywhere --
// so a body could reach whatever speed the solver produced while the setting sat
// in the inspector looking applied. Found by tools/unread_field_audit.py.
//
// They are handed to Jolt as body settings rather than clamped after the step,
// because Jolt enforces them inside the solver. Clamping afterwards leaves the
// body having already MOVED at the speed it was not allowed to reach, which is
// the tunnelling the ceiling exists to prevent.
#include "EnjinTest.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <cmath>
#include <memory>

using namespace Enjin;

namespace {

// A dynamic box in freefall, with gravity turned up so it would comfortably
// exceed the ceiling within the simulated time if nothing held it back.
ECS::Entity MakeFallingBody(ECS::World& world, f32 maxVel) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = Math::Vector3(0.0f, 500.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::RigidbodyComponent rb;
    rb.mass = 1.0f;
    rb.useGravity = true;
    rb.gravityScale = 10.0f;      // ~98 m/s^2, so terminal speed arrives fast
    rb.drag = 0.0f;
    rb.maxVelocity = maxVel;
    world.AddComponent<ECS::RigidbodyComponent>(e, rb);

    ECS::BoxColliderComponent col;
    col.size = Math::Vector3(1.0f, 1.0f, 1.0f);
    world.AddComponent<ECS::BoxColliderComponent>(e, col);
    return e;
}

f32 SimulateAndGetSpeed(ECS::World& world, ECS::Entity e) {
    // Same shape the other physics tests use: create the backend, hand it the
    // world, tick it at a fixed step. Bodies are created from the components.
    auto physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Jolt);
    if (!physics) return -1.0f;          // no Jolt in this build; caller skips
    physics->SetWorld(&world);

    for (int i = 0; i < 240; ++i) physics->Update(1.0f / 60.0f);

    const auto* rb = world.GetComponent<ECS::RigidbodyComponent>(e);
    return rb ? rb->velocity.Length() : 0.0f;
}

} // namespace

ENJIN_TEST(RigidbodyVelocityClamp, test_a_falling_body_does_not_exceed_its_ceiling) {
    // Arrange
    ECS::World world;
    const f32 ceiling = 12.0f;
    ECS::Entity e = MakeFallingBody(world, ceiling);

    // Act
    const f32 speed = SimulateAndGetSpeed(world, e);

    // Assert
    if (speed < 0.0f) {
        ENJIN_SKIP("Jolt is not in this build; no solver to test");
    }
    // A little slack: the ceiling is enforced per step, so the reported speed
    // can sit a fraction above it between the solve and the sync.
    ENJIN_EXPECT_TRUE(speed <= ceiling * 1.25f);
}

ENJIN_TEST(RigidbodyVelocityClamp, test_a_higher_ceiling_lets_the_body_go_faster) {
    // The ceiling has to be the thing doing the work. Without this, a body that
    // simply never got fast enough would pass the test above.
    // Arrange
    ECS::World slowWorld, fastWorld;
    ECS::Entity slow = MakeFallingBody(slowWorld, 8.0f);
    ECS::Entity fast = MakeFallingBody(fastWorld, 60.0f);

    // Act
    const f32 slowSpeed = SimulateAndGetSpeed(slowWorld, slow);
    const f32 fastSpeed = SimulateAndGetSpeed(fastWorld, fast);

    // Assert
    if (slowSpeed < 0.0f || fastSpeed < 0.0f) {
        ENJIN_SKIP("Jolt is not in this build; no solver to test");
    }
    ENJIN_EXPECT_TRUE(fastSpeed > slowSpeed);
}

ENJIN_TEST_MAIN()
