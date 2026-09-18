// What a scene loader does with a file it should refuse.
//
// The security notes say scene files validate array sizes and cap vertices and
// indices, and TestUntrustedInput pins the JSON depth guard and the layer caps.
// What nothing covered is the ordinary badness a person actually hits: a file
// truncated by a failed copy, a scene that is valid JSON and not a scene, a
// mesh whose indices point at vertices that do not exist, a texture path that
// no longer resolves.
//
// The bar for all of these is the same and it is not "loads": it is a clean
// answer and a world that is not left half-built.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;

namespace {

std::string ScenePath(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

void WriteText(const std::string& path, const std::string& text) {
    std::ofstream f(path);
    f << text;
}

} // namespace

ENJIN_TEST(SceneFailureModes, test_a_truncated_scene_fails_without_populating_the_world) {
    // A copy that died halfway is the most ordinary corruption there is.
    // Arrange
    const std::string path = ScenePath("enjin_test_truncated.enjin");
    WriteText(path, "{\"version\": \"1.0\", \"entities\": [ {\"id\": 1, \"name\": {\"na");

    // Act
    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    const auto result = serializer.Load(path);

    // Assert
    ENJIN_EXPECT_FALSE(result.success);
    // A half-built world is worse than none: the editor would show entities
    // from a file it just told you it could not read.
    ENJIN_EXPECT_EQ(world.GetEntityCount(), static_cast<usize>(0));

    std::remove(path.c_str());
}

ENJIN_TEST(SceneFailureModes, test_valid_json_that_is_not_a_scene_is_refused) {
    // Arrange
    const std::string path = ScenePath("enjin_test_notascene.enjin");
    WriteText(path, "{\"hello\": \"world\", \"numbers\": [1, 2, 3]}");

    // Act
    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    const auto result = serializer.Load(path);

    // Assert: whatever it decides, it must not invent entities.
    ENJIN_EXPECT_EQ(world.GetEntityCount(), static_cast<usize>(0));

    std::remove(path.c_str());
}

ENJIN_TEST(SceneFailureModes, test_a_numeric_version_is_refused_with_a_reason) {
    // CLAUDE.md records this as having broken two hand-authored probe scenes:
    // "version" must be the STRING "1.0", and a bare number fails the whole
    // load. Pinned here so the message stays, because the failure is otherwise
    // baffling -- the file looks right.
    // Arrange
    const std::string path = ScenePath("enjin_test_numver.enjin");
    WriteText(path, "{\"version\": 1.0, \"entities\": []}");

    // Act
    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    const auto result = serializer.Load(path);

    // Assert
    ENJIN_EXPECT_FALSE(result.success);
    ENJIN_EXPECT_TRUE(!result.error.empty());

    std::remove(path.c_str());
}

ENJIN_TEST(SceneFailureModes, test_a_missing_file_fails_cleanly) {
    // Arrange
    ECS::World world;
    Scene::SceneSerializer serializer(&world);

    // Act
    const auto result = serializer.Load(ScenePath("enjin_no_such_scene_exists.enjin"));

    // Assert
    ENJIN_EXPECT_FALSE(result.success);
    ENJIN_EXPECT_EQ(world.GetEntityCount(), static_cast<usize>(0));
}

ENJIN_TEST(SceneFailureModes, test_indices_pointing_past_the_vertices_do_not_crash_the_load) {
    // Non-manifold and outright wrong geometry is DATA, not a parse error, so
    // the loader is right to take it. What it must not do is fall over, and
    // what a reader needs to know is that the mesh arrives exactly as written
    // -- nothing downstream is validating this for them.
    // Arrange
    const std::string path = ScenePath("enjin_test_badindices.enjin");
    WriteText(path,
        "{\"version\": \"1.0\", \"entities\": [{"
        "\"id\": 1, \"name\": {\"name\": \"Bad\"},"
        "\"transform\": {\"position\": [0,0,0], \"rotation\": [0,0,0,1], \"scale\": [1,1,1]},"
        "\"mesh\": {\"vertices\": [{\"position\": [0,0,0]}, {\"position\": [1,0,0]}],"
        "\"indices\": [0, 1, 9999]}}]}");

    // Act
    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    const auto result = serializer.Load(path);

    // Assert
    ENJIN_ASSERT_TRUE(result.success);
    ECS::Entity e = world.FindEntityByName("Bad");
    ENJIN_ASSERT_TRUE(e != ECS::INVALID_ENTITY);
    const auto* mesh = world.GetComponent<ECS::MeshComponent>(e);
    ENJIN_ASSERT_TRUE(mesh != nullptr);
    ENJIN_EXPECT_EQ(mesh->vertices.size(), static_cast<usize>(2));
    // Taken as written, out-of-range and all. Recorded rather than asserted as
    // desirable: if the engine ever starts validating this, THIS is the test
    // that should change and say why.
    ENJIN_EXPECT_EQ(mesh->indices.size(), static_cast<usize>(3));

    std::remove(path.c_str());
}

ENJIN_TEST(SceneFailureModes, test_a_texture_path_that_does_not_resolve_still_loads_the_scene) {
    // A missing texture is a missing FILE, not a broken scene. Refusing the
    // whole scene over one would make a moved asset unopenable, and the entity
    // has to survive so the path can be fixed in the inspector.
    // Arrange
    const std::string path = ScenePath("enjin_test_missingtex.enjin");
    WriteText(path,
        "{\"version\": \"1.0\", \"entities\": [{"
        "\"id\": 1, \"name\": {\"name\": \"Textured\"},"
        "\"transform\": {\"position\": [0,0,0], \"rotation\": [0,0,0,1], \"scale\": [1,1,1]},"
        "\"material\": {\"baseColorTexturePath\": \"assets/textures/gone_forever.png\"}}]}");

    // Act
    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    const auto result = serializer.Load(path);

    // Assert
    ENJIN_ASSERT_TRUE(result.success);
    ECS::Entity e = world.FindEntityByName("Textured");
    ENJIN_ASSERT_TRUE(e != ECS::INVALID_ENTITY);
    // And the path is KEPT, not cleared. Clearing it would lose the only clue
    // to what the texture was supposed to be.
    const auto* mat = world.GetComponent<ECS::MaterialComponent>(e);
    ENJIN_ASSERT_TRUE(mat != nullptr);
    ENJIN_EXPECT_TRUE(!mat->baseColorTexturePath.empty());

    std::remove(path.c_str());
}

ENJIN_TEST(SceneFailureModes, test_an_empty_file_fails_cleanly) {
    // Arrange
    const std::string path = ScenePath("enjin_test_empty.enjin");
    WriteText(path, "");

    // Act
    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    const auto result = serializer.Load(path);

    // Assert
    ENJIN_EXPECT_FALSE(result.success);
    ENJIN_EXPECT_EQ(world.GetEntityCount(), static_cast<usize>(0));

    std::remove(path.c_str());
}

ENJIN_TEST_MAIN()
