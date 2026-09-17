// A fluid recording survives a scene save and reload.
//
// CLAUDE.md's standing trap: unknown entity keys are silently ignored on load
// and erased by the next save. A component with a system and no serializer
// therefore looks like it works right up until someone reopens the scene, at
// which point the setting is simply gone and nothing says why. This is the
// round trip that catches that for FluidPlaybackComponent.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/FluidPlayback.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <filesystem>
#include <string>

using namespace Enjin;

namespace {

std::string ScenePath(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

} // namespace

ENJIN_TEST(FluidPlaybackSerialization, test_a_recording_reference_survives_a_round_trip) {
    // Arrange
    const std::string path = ScenePath("enjin_test_fluid_playback_scene.enjin");
    {
        ECS::World world;
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e);
        world.AddComponent<ECS::NameComponent>(e, "Chimney");

        ECS::FluidVolumeComponent vol;
        vol.dimension = ECS::FluidDimension::Mode3D;
        vol.fluidType = ECS::FluidType::Smoke;
        world.AddComponent<ECS::FluidVolumeComponent>(e, vol);

        ECS::FluidPlaybackComponent play;
        play.bakePath = "assets/fluid/Chimney.enjfluid";
        play.playing = false;
        play.speed = 0.5f;
        play.time = 1.25f;
        world.AddComponent<ECS::FluidPlaybackComponent>(e, play);

        Scene::SceneSerializer serializer(&world);
        ENJIN_ASSERT_TRUE(serializer.Save(path).success);
    }

    // Act
    ECS::World loaded;
    Scene::SceneSerializer serializer(&loaded);
    ENJIN_ASSERT_TRUE(serializer.Load(path).success);

    // Assert
    ECS::Entity found = loaded.FindEntityByName("Chimney");
    ENJIN_ASSERT_TRUE(found != ECS::INVALID_ENTITY);

    const auto* play = loaded.GetComponent<ECS::FluidPlaybackComponent>(found);
    ENJIN_ASSERT_TRUE(play != nullptr);
    ENJIN_EXPECT_STR_EQ(play->bakePath.c_str(), "assets/fluid/Chimney.enjfluid");
    ENJIN_EXPECT_FALSE(play->playing);
    ENJIN_EXPECT_FLOAT_NEAR(play->speed, 0.5f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(play->time, 1.25f, 0.0001f);

    std::remove(path.c_str());
}

ENJIN_TEST(FluidPlaybackSerialization, test_a_remembered_load_failure_is_not_persisted) {
    // loadAttempted and loadFailed are the playback system's memory of having
    // already tried a file. Saving them into the scene would mean a recording
    // that was missing once is never retried -- not even after the file is put
    // back -- and the only cure would be hand-editing the scene JSON.
    // Arrange
    const std::string path = ScenePath("enjin_test_fluid_playback_failure.enjin");
    {
        ECS::World world;
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e);
        world.AddComponent<ECS::NameComponent>(e, "Broken");
        world.AddComponent<ECS::FluidVolumeComponent>(e);

        ECS::FluidPlaybackComponent play;
        play.bakePath = "assets/fluid/Missing.enjfluid";
        play.loadAttempted = true;
        play.loadFailed = true;
        world.AddComponent<ECS::FluidPlaybackComponent>(e, play);

        Scene::SceneSerializer serializer(&world);
        ENJIN_ASSERT_TRUE(serializer.Save(path).success);
    }

    // Act
    ECS::World loaded;
    Scene::SceneSerializer serializer(&loaded);
    ENJIN_ASSERT_TRUE(serializer.Load(path).success);

    // Assert: the path came back, the remembered failure did not.
    ECS::Entity found = loaded.FindEntityByName("Broken");
    ENJIN_ASSERT_TRUE(found != ECS::INVALID_ENTITY);
    const auto* play = loaded.GetComponent<ECS::FluidPlaybackComponent>(found);
    ENJIN_ASSERT_TRUE(play != nullptr);
    ENJIN_EXPECT_STR_EQ(play->bakePath.c_str(), "assets/fluid/Missing.enjfluid");
    ENJIN_EXPECT_FALSE(play->loadAttempted);
    ENJIN_EXPECT_FALSE(play->loadFailed);

    std::remove(path.c_str());
}

ENJIN_TEST(FluidPlaybackSerialization, test_an_existing_scene_keeps_its_authored_buoyancy) {
    // The Water and Lava presets changed from buoyancy 0 to negative, so they
    // now fall. That is only safe for already-authored scenes because a scene
    // stores the VALUE and never re-applies the preset on load -- if loading
    // called ApplyPreset, every shipped water volume in every project would
    // quietly start sinking after an engine update.
    // Arrange: a water volume authored the old way.
    const std::string path = ScenePath("enjin_test_fluid_legacy_buoyancy.enjin");
    {
        ECS::World world;
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e);
        world.AddComponent<ECS::NameComponent>(e, "OldPond");
        ECS::FluidVolumeComponent vol;
        vol.fluidType = ECS::FluidType::Water;
        vol.buoyancy = 0.0f;               // what the old preset produced
        world.AddComponent<ECS::FluidVolumeComponent>(e, vol);

        Scene::SceneSerializer serializer(&world);
        ENJIN_ASSERT_TRUE(serializer.Save(path).success);
    }

    // Act
    ECS::World loaded;
    Scene::SceneSerializer serializer(&loaded);
    ENJIN_ASSERT_TRUE(serializer.Load(path).success);

    // Assert: still 0, not the new preset's negative value.
    ECS::Entity found = loaded.FindEntityByName("OldPond");
    ENJIN_ASSERT_TRUE(found != ECS::INVALID_ENTITY);
    const auto* vol = loaded.GetComponent<ECS::FluidVolumeComponent>(found);
    ENJIN_ASSERT_TRUE(vol != nullptr);
    ENJIN_EXPECT_FLOAT_NEAR(vol->buoyancy, 0.0f, 0.0001f);

    std::remove(path.c_str());
}

ENJIN_TEST_MAIN()
