// The car drives into walls, not through them.
//
// ControllerSystem::UpdateVehicle moved the transform straight through anything
// and only checked the ground, so a car held at full throttle went through
// every wall in its way. These drive the real ControllerSystem against a real
// Jolt backend, with keys injected the way a runtime receives them.
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

using namespace Enjin;

namespace {

struct Track {
    ECS::World world;
    ECS::Entity car = ECS::INVALID_ENTITY;
    std::unique_ptr<Physics::IPhysicsBackend> physics;
    ECS::ControllerSystem controllers;
    InputSystem::InputActionMap map;

    void AddBox(const Math::Vector3& position, const Math::Vector3& size) {
        const ECS::Entity e = world.CreateEntity();
        ECS::TransformComponent t;
        t.position = position;
        world.AddComponent<ECS::TransformComponent>(e, t);
        ECS::BoxColliderComponent bc;
        bc.size = size;
        world.AddComponent<ECS::BoxColliderComponent>(e, bc);
    }

    void Build() {
        AddBox(Math::Vector3(0.0f, -0.5f, 0.0f), Math::Vector3(60.0f, 1.0f, 60.0f));   // floor, top at 0
        AddBox(Math::Vector3(0.0f, 1.5f, -12.0f), Math::Vector3(20.0f, 3.0f, 1.0f));   // wall, front face at z = -11.5

        car = world.CreateEntity();
        ECS::TransformComponent t;
        t.position = Math::Vector3(0.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(car, t);
        ECS::VehicleController v;
        v.heading = 0.0f;   // forward is -Z
        world.AddComponent<ECS::VehicleController>(car, v);

        physics = Physics::CreatePhysicsBackend(Physics::PhysicsBackendType::Auto);
        physics->SetWorld(&world);
        physics->SetGravity(Math::Vector3(0.0f, -9.81f, 0.0f));
        controllers.SetEnabled(true);
        controllers.SetWorld(&world);
        controllers.SetPhysics(physics.get());
        map.LoadDefaults();
        controllers.SetInputActionMap(&map);
    }

    Math::Vector3 Drive(KeyCode held, int frames) {
        const f32 dt = 1.0f / 60.0f;
        bool keys[512] = {};
        bool mouse[8] = {};
        keys[static_cast<int>(held)] = true;
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
        return world.GetComponent<ECS::TransformComponent>(car)->position;
    }
};

}  // namespace

ENJIN_TEST(VehicleWalls, test_vehicle_full_throttle_stops_at_wall) {
    // Arrange
    Track track;
    track.Build();

    // Act: four seconds flat out at a wall 11.5 m ahead
    const Math::Vector3 end = track.Drive(KeyCode::W, 240);

    // Assert: it got near the wall and did not pass its front face
    ENJIN_EXPECT_TRUE(end.z < -5.0f);
    ENJIN_EXPECT_TRUE(end.z > -11.5f);
}

ENJIN_TEST(VehicleWalls, test_vehicle_drives_freely_with_no_wall_ahead) {
    // Arrange
    Track track;
    track.Build();

    // Act: reverse, away from the wall
    const Math::Vector3 end = track.Drive(KeyCode::S, 180);

    // Assert: it moved backward, unhindered
    ENJIN_EXPECT_TRUE(end.z > 1.0f);
}

ENJIN_TEST_MAIN()
