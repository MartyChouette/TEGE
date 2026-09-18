// An entity following an authored path, with no script involved.
//
// PathFollowerComponent shipped with a helper class and nothing else: no system
// called it, no serializer saved it, no inspector edited it. Placing one did
// nothing, and the only way to give it a route was a script calling SetPath --
// which fails the bar that a person with no AI and no scripting must be able to
// reach every capability through the editor.
#include "EnjinTest.h"
#include "Enjin/AI/Navmesh.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Systems/AISystem.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace Enjin;

namespace {

ECS::Entity MakeFollower(ECS::World& world, const Math::Vector3& start,
                         const std::vector<Math::Vector3>& route) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = start;
    world.AddComponent<ECS::TransformComponent>(e, xf);

    AI::PathFollowerComponent f;
    f.waypoints = route;
    f.speed = 4.0f;
    f.arrivalRadius = 0.3f;
    f.slowdownRadius = 0.0f;   // constant speed keeps the arithmetic checkable
    world.AddComponent<AI::PathFollowerComponent>(e, f);
    return e;
}

f32 DistanceXZ(const Math::Vector3& a, const Math::Vector3& b) {
    const f32 dx = a.x - b.x;
    const f32 dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

ENJIN_TEST(PathFollowing, test_an_authored_route_moves_the_entity_with_no_script) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeFollower(world, Math::Vector3(0, 0, 0),
                                 {Math::Vector3(0, 0, 10)});

    ECS::AISystem ai;
    ai.SetWorld(&world);
    ai.SetEnabled(true);

    const Math::Vector3 before = world.GetComponent<ECS::TransformComponent>(e)->position;

    // Act
    for (int i = 0; i < 30; ++i) ai.UpdatePathFollowers(1.0f / 60.0f);

    // Assert
    const Math::Vector3 after = world.GetComponent<ECS::TransformComponent>(e)->position;

    // Half a second at 4 units/sec is about 2 units along +Z.
    ENJIN_EXPECT_TRUE(after.z > before.z + 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(after.x, 0.0f, 0.01f);
}

ENJIN_TEST(PathFollowing, test_it_arrives_and_stops) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeFollower(world, Math::Vector3(0, 0, 0),
                                 {Math::Vector3(0, 0, 2)});

    ECS::AISystem ai;
    ai.SetWorld(&world);
    ai.SetEnabled(true);

    // Act
    for (int i = 0; i < 300; ++i) ai.UpdatePathFollowers(1.0f / 60.0f);

    // Assert
    auto* f = world.GetComponent<AI::PathFollowerComponent>(e);
    ENJIN_ASSERT_TRUE(f != nullptr);
    ENJIN_EXPECT_TRUE(f->hasArrived);
    ENJIN_EXPECT_FALSE(f->isFollowing);

    const Math::Vector3 rest = world.GetComponent<ECS::TransformComponent>(e)->position;
    ENJIN_EXPECT_TRUE(DistanceXZ(rest, Math::Vector3(0, 0, 2)) < 0.5f);

    // And STAYS there. A follower that keeps integrating after arrival drifts
    // off the end of its own route, which reads as the path being wrong.
    for (int i = 0; i < 300; ++i) ai.UpdatePathFollowers(1.0f / 60.0f);
    const Math::Vector3 still = world.GetComponent<ECS::TransformComponent>(e)->position;
    ENJIN_EXPECT_FLOAT_NEAR(still.z, rest.z, 0.0001f);
}

ENJIN_TEST(PathFollowing, test_a_looping_route_starts_again) {
    ECS::World world;
    ECS::Entity e = MakeFollower(world, Math::Vector3(0, 0, 0),
                                 {Math::Vector3(0, 0, 2), Math::Vector3(2, 0, 2)});
    world.GetComponent<AI::PathFollowerComponent>(e)->loop = true;

    ECS::AISystem ai;
    ai.SetWorld(&world);
    ai.SetEnabled(true);

    // Act: long enough to finish the route several times over.
    for (int i = 0; i < 1200; ++i) ai.UpdatePathFollowers(1.0f / 60.0f);

    // Assert
    auto* f = world.GetComponent<AI::PathFollowerComponent>(e);
    ENJIN_ASSERT_TRUE(f != nullptr);
    // Still going round: a loop that quietly stopped would show as hasArrived
    // with nothing moving, which is exactly what the non-looping case does.
    ENJIN_EXPECT_TRUE(f->isFollowing);
}

ENJIN_TEST(PathFollowing, test_a_follower_with_no_waypoints_does_nothing) {
    // The empty case has to be quiet rather than clever: a component added and
    // not yet given a route must not wander off to the origin.
    ECS::World world;
    ECS::Entity e = MakeFollower(world, Math::Vector3(5, 0, 5), {});

    ECS::AISystem ai;
    ai.SetWorld(&world);
    ai.SetEnabled(true);

    // Act
    for (int i = 0; i < 120; ++i) ai.UpdatePathFollowers(1.0f / 60.0f);

    // Assert
    const Math::Vector3 p = world.GetComponent<ECS::TransformComponent>(e)->position;
    ENJIN_EXPECT_FLOAT_NEAR(p.x, 5.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.z, 5.0f, 0.0001f);
}

ENJIN_TEST(PathFollowing, test_auto_start_off_leaves_the_path_to_a_script) {
    // autoStart is what separates "authored route" from "a script or a navmesh
    // query owns this". With it off, the authored waypoints are inert.
    ECS::World world;
    ECS::Entity e = MakeFollower(world, Math::Vector3(0, 0, 0),
                                 {Math::Vector3(0, 0, 10)});
    world.GetComponent<AI::PathFollowerComponent>(e)->autoStart = false;

    ECS::AISystem ai;
    ai.SetWorld(&world);
    ai.SetEnabled(true);

    // Act
    for (int i = 0; i < 120; ++i) ai.UpdatePathFollowers(1.0f / 60.0f);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(world.GetComponent<ECS::TransformComponent>(e)->position.z,
                            0.0f, 0.0001f);
}

ENJIN_TEST(PathFollowing, test_a_disabled_ai_system_moves_nothing) {
    // The editor keeps AI disabled outside play mode. A follower that moved
    // anyway would drag authored entities around while someone was placing
    // them, and the scene would save in the moved position.
    ECS::World world;
    ECS::Entity e = MakeFollower(world, Math::Vector3(0, 0, 0),
                                 {Math::Vector3(0, 0, 10)});

    ECS::AISystem ai;
    ai.SetWorld(&world);
    ai.SetEnabled(false);

    // Act
    for (int i = 0; i < 120; ++i) ai.UpdatePathFollowers(1.0f / 60.0f);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(world.GetComponent<ECS::TransformComponent>(e)->position.z,
                            0.0f, 0.0001f);
}

ENJIN_TEST(PathFollowing, test_an_authored_route_survives_a_scene_round_trip) {
    // CLAUDE.md's standing trap: an unknown entity key is silently ignored on
    // load and erased by the next save. A component with no serializer looks
    // like it works until the scene is reopened, at which point the route is
    // simply gone and nothing says why.
    // Arrange
    const std::string path =
        (std::filesystem::temp_directory_path() / "enjin_test_path_follower.enjin").string();
    {
        ECS::World world;
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e);
        world.AddComponent<ECS::NameComponent>(e, "Patrol");

        AI::PathFollowerComponent f;
        f.waypoints = {Math::Vector3(1, 0, 2), Math::Vector3(3, 0, 4)};
        f.speed = 7.5f;
        f.turnSpeed = 90.0f;
        f.arrivalRadius = 0.25f;
        f.slowdownRadius = 1.5f;
        f.smoothRotation = false;
        f.autoStart = false;
        f.loop = true;
        world.AddComponent<AI::PathFollowerComponent>(e, f);

        Scene::SceneSerializer serializer(&world);
        ENJIN_ASSERT_TRUE(serializer.Save(path).success);
    }

    // Act
    ECS::World loaded;
    Scene::SceneSerializer serializer(&loaded);
    ENJIN_ASSERT_TRUE(serializer.Load(path).success);

    // Assert
    ECS::Entity found = loaded.FindEntityByName("Patrol");
    ENJIN_ASSERT_TRUE(found != ECS::INVALID_ENTITY);
    const auto* f = loaded.GetComponent<AI::PathFollowerComponent>(found);
    ENJIN_ASSERT_TRUE(f != nullptr);

    // Every authored field, because one unread field is a setting that
    // silently reverts to its default on load.
    ENJIN_ASSERT_EQ(f->waypoints.size(), static_cast<usize>(2));
    ENJIN_EXPECT_FLOAT_NEAR(f->waypoints[0].x, 1.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(f->waypoints[1].z, 4.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(f->speed, 7.5f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(f->turnSpeed, 90.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(f->arrivalRadius, 0.25f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(f->slowdownRadius, 1.5f, 0.0001f);
    ENJIN_EXPECT_FALSE(f->smoothRotation);
    ENJIN_EXPECT_FALSE(f->autoStart);
    ENJIN_EXPECT_TRUE(f->loop);

    // And NOT where it had got to: saving progress would open the scene with
    // the entity already partway along its route.
    ENJIN_EXPECT_TRUE(f->currentPath.waypoints.empty());
    ENJIN_EXPECT_FALSE(f->isFollowing);

    std::remove(path.c_str());
}

ENJIN_TEST_MAIN()
