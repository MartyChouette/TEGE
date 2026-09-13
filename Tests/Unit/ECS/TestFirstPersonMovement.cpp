// Does holding W actually move a first-person player?
//
// Nothing asserted that before this file. The controller suites check struct
// defaults and re-implement the grid-movement arithmetic locally; the physics
// suites step bodies but never a character. So the one question a player asks
// first -- "can I walk?" -- was answered by nobody, and a scene that could not
// move looked exactly like a scene that could until someone pressed a key.
//
// These drive the REAL ControllerSystem against a REAL Jolt backend, with keys
// fed through Input's replay-injection stream, which is as close to a person at
// the keyboard as a headless test gets.

#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Platform/Input.h"
#include <memory>
#include <cmath>

using namespace Enjin;

namespace {

// The HandIK demo's player, component for component: a capsule collider, a
// first-person controller, and no rigidbody.
struct Walker {
    ECS::World world;
    ECS::Entity floor = ECS::INVALID_ENTITY;
    ECS::Entity player = ECS::INVALID_ENTITY;
    std::unique_ptr<Physics::IPhysicsBackend> physics;
    ECS::ControllerSystem controllers;
    InputSystem::InputActionMap map;

    void Build(bool givePlayerARigidbody) {
        floor = world.CreateEntity();
        {
            ECS::TransformComponent t;
            t.position = Math::Vector3(0.0f, -0.5f, 0.0f);   // slab top at y = 0
            world.AddComponent<ECS::TransformComponent>(floor, t);
            ECS::BoxColliderComponent bc;
            bc.size = Math::Vector3(40.0f, 1.0f, 40.0f);
            world.AddComponent<ECS::BoxColliderComponent>(floor, bc);
        }

        player = world.CreateEntity();
        {
            ECS::TransformComponent t;
            // Capsule centre rests at radius + height/2 = 0.85 above the floor.
            t.position = Math::Vector3(0.0f, 0.85f, 0.0f);
            world.AddComponent<ECS::TransformComponent>(player, t);
            ECS::CapsuleColliderComponent cc;
            cc.radius = 0.3f;
            cc.height = 1.1f;
            world.AddComponent<ECS::CapsuleColliderComponent>(player, cc);
            ECS::FirstPersonController fp;
            fp.moveSpeed = 3.2f;
            fp.standingHeight = 0.85f;
            fp.currentHeight = 0.85f;
            fp.yaw = 0.0f;                                   // forward is -Z
            world.AddComponent<ECS::FirstPersonController>(player, fp);
            if (givePlayerARigidbody) {
                ECS::RigidbodyComponent rb;
                rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
                rb.mass = 70.0f;
                rb.useGravity = true;
                world.AddComponent<ECS::RigidbodyComponent>(player, rb);
            }
        }

        physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
        physics->SetWorld(&world);
        physics->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f));

        // ControllerSystem defaults to DISABLED so the editor's fly camera is
        // not fought over while a scene is being built. A runtime turns it on.
        controllers.SetEnabled(true);
        controllers.SetWorld(&world);
        controllers.SetPhysics(physics.get());
        map.LoadDefaults();
        controllers.SetInputActionMap(&map);
    }

    // One second of held input, ticked the way a runtime ticks it.
    Math::Vector3 Run(KeyCode held, int frames = 60) {
        const f32 dt = 1.0f / 60.0f;
        bool keys[512] = {};
        bool mouse[8] = {};
        if (held != KeyCode::Unknown) keys[static_cast<int>(held)] = true;

        Input::SetInputFocus(Input::InputFocus::Gameplay);
        Input::SetUIConsumedPointer(false);
        Input::SetReplayInjection(true);
        for (int i = 0; i < frames; ++i) {
            Input::InjectFrameState(keys, mouse, Math::Vector2(0.0f, 0.0f));
            Input::Update();
            map.Update(dt);
            physics->Update(dt);
            controllers.Update(dt);
        }
        Input::SetReplayInjection(false);

        return world.GetComponent<ECS::TransformComponent>(player)->position;
    }
};

}  // namespace

// The bug Marty hit: the player stood still with the keyboard held down.
ENJIN_TEST(FirstPersonMovement, HeldForwardKeyWalksTheCapsuleForward) {
    Walker w;
    w.Build(/*givePlayerARigidbody=*/false);

    const Math::Vector3 end = w.Run(KeyCode::W);

    // Yaw 0 looks down -Z, and one second at 3.2 m/s covers most of 3.2 m
    // (acceleration eats the first fraction). Anything under half a metre is
    // "it did not walk".
    ENJIN_EXPECT_TRUE(end.z < -0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(end.x, 0.0f, 0.1f);
    // And it stayed on the floor rather than sinking or being shoved upward.
    ENJIN_EXPECT_FLOAT_NEAR(end.y, 0.85f, 0.15f);
}

ENJIN_TEST(FirstPersonMovement, StrafeKeyWalksTheCapsuleSideways) {
    Walker w;
    w.Build(/*givePlayerARigidbody=*/false);

    const Math::Vector3 end = w.Run(KeyCode::D);

    ENJIN_EXPECT_TRUE(end.x > 0.5f);
    ENJIN_EXPECT_FLOAT_NEAR(end.z, 0.0f, 0.1f);
}

// Nothing pressed, nothing moves. Without this the two above would pass just as
// happily on a player that drifts across the room on its own, which is the
// other half of what Marty reported.
ENJIN_TEST(FirstPersonMovement, NoInputMeansNoDrift) {
    Walker w;
    w.Build(/*givePlayerARigidbody=*/false);

    const Math::Vector3 end = w.Run(KeyCode::Unknown, 180);

    ENJIN_EXPECT_FLOAT_NEAR(end.x, 0.0f, 0.02f);
    ENJIN_EXPECT_FLOAT_NEAR(end.z, 0.0f, 0.02f);
}

// Walking into a wall and having no input reaching the controller look the same
// from the chair, which is how a demo that spawned the player 0.12 m from a
// worktop got reported as "I cannot move at all". The controller can tell them
// apart -- it knows it is pushing and knows it has not moved -- and now says so.
ENJIN_TEST(FirstPersonMovement, WalkingIntoAWallIsReportedAsBlocked) {
    Walker w;
    w.Build(/*givePlayerARigidbody=*/false);

    // A wall right where the demo's counter was: just ahead, across the path.
    ECS::Entity wall = w.world.CreateEntity();
    {
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 1.5f, -1.0f);
        w.world.AddComponent<ECS::TransformComponent>(wall, t);
        ECS::BoxColliderComponent bc;
        bc.size = Math::Vector3(8.0f, 3.0f, 0.4f);
        w.world.AddComponent<ECS::BoxColliderComponent>(wall, bc);
    }

    const Math::Vector3 end = w.Run(KeyCode::W, 120);

    ENJIN_EXPECT_TRUE(w.controllers.IsMovementBlocked(w.player));
    // And it really did stop at the wall rather than pass through it.
    ENJIN_EXPECT_TRUE(end.z > -1.1f);
}

// The other half of the claim: open floor must NOT report blocked, or the
// warning is noise and gets ignored exactly when it matters.
ENJIN_TEST(FirstPersonMovement, WalkingAcrossOpenFloorIsNotReportedAsBlocked) {
    Walker w;
    w.Build(/*givePlayerARigidbody=*/false);
    w.Run(KeyCode::W, 120);
    ENJIN_EXPECT_TRUE(!w.controllers.IsMovementBlocked(w.player));
}

// A NOTE ON THE DYNAMIC RIGIDBODY, because the commit that removed one from
// this demo's player claimed more than it could show.
//
// Putting a dynamic rigidbody on an entity that also has a character controller
// really is wrong -- two owners write the same transform every frame. But the
// two symptoms it was blamed for do not reproduce: a player carrying both walks
// the same distance per second as one carrying neither (the controller writes
// last and wins), and standing still it does not drift. Both were measured here
// before this comment replaced the test that asserted them. Whatever moved the
// capsule across the room that afternoon, it was not this, and the honest state
// of it is unexplained rather than fixed.
//
// The build that supported those tests is in the history if someone wants to
// take another run at it; what is NOT wanted is a passing test that quietly
// asserts the weaker claim and lets the original one stand.

ENJIN_TEST_MAIN()
