// Exporting entities as a .glb, checked by reading the file back in.
//
// The engine could read glTF and never write it, so geometry authored here --
// a remeshed model, a spline chain, a group of placed pieces -- could not leave
// the editor. A writer is only worth having if what it writes is real, and the
// strongest available check is that the engine's OWN importer accepts it: the
// two were written years apart by different code paths, so agreement between
// them is evidence rather than a tautology.
#include "EnjinTest.h"
#include "Enjin/Assets/GLBExporter.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;

namespace {

std::string TempPath(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

// A triangle is enough: this is testing the container and the accessors, not
// the geometry.
ECS::Entity MakeTriangle(ECS::World& world, const char* name,
                         const Math::Vector3& at) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = at;
    world.AddComponent<ECS::TransformComponent>(e, xf);
    world.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{name});

    ECS::MeshComponent mesh;
    for (int i = 0; i < 3; ++i) {
        ECS::MeshComponent::Vertex v{};
        v.position = Math::Vector3(static_cast<f32>(i), static_cast<f32>(i * 2), 0.0f);
        v.normal = Math::Vector3(0.0f, 0.0f, 1.0f);
        v.uv = Math::Vector2(static_cast<f32>(i) * 0.5f, 0.25f);
        mesh.vertices.push_back(v);
    }
    mesh.indices = {0, 1, 2};
    world.AddComponent<ECS::MeshComponent>(e, mesh);
    return e;
}

} // namespace

ENJIN_TEST(GLBExport, test_an_exported_file_is_a_well_formed_glb_container) {
    // The twelve-byte header is what every viewer reads first, and a wrong
    // total length produces a file that opens in one tool and not another.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeTriangle(world, "Tri", Math::Vector3(0, 0, 0));
    const std::string path = TempPath("enjin_test_export_header.glb");

    // Act
    const auto r = Assets::ExportEntitiesToGLB(&world, {e}, path);

    // Assert
    ENJIN_ASSERT_TRUE(r.success);
    std::ifstream f(path, std::ios::binary);
    ENJIN_ASSERT_TRUE(static_cast<bool>(f));
    u32 magic = 0, version = 0, total = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&version), 4);
    f.read(reinterpret_cast<char*>(&total), 4);
    ENJIN_EXPECT_EQ(magic, 0x46546C67u);   // "glTF"
    ENJIN_EXPECT_EQ(version, 2u);
    f.close();

    // The header's length must be the file's actual length, not an estimate.
    const auto onDisk = std::filesystem::file_size(path);
    ENJIN_EXPECT_EQ(static_cast<u64>(total), static_cast<u64>(onDisk));

    std::remove(path.c_str());
}

ENJIN_TEST(GLBExport, test_the_engine_can_read_back_what_it_wrote) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeTriangle(world, "Tri", Math::Vector3(0, 0, 0));
    const std::string path = TempPath("enjin_test_export_roundtrip.glb");

    // Act
    const auto r = Assets::ExportEntitiesToGLB(&world, {e}, path);
    ENJIN_ASSERT_TRUE(r.success);

    Assets::GLTFScene scene;
    const bool loaded = Assets::GLTFLoader::Load(path, scene);

    // Assert
    ENJIN_ASSERT_TRUE(loaded);
    ENJIN_ASSERT_EQ(scene.meshes.size(), static_cast<usize>(1));
    ENJIN_ASSERT_EQ(scene.meshes[0].primitives.size(), static_cast<usize>(1));

    const auto& prim = scene.meshes[0].primitives[0];
    ENJIN_ASSERT_EQ(prim.vertices.size(), static_cast<usize>(3));
    ENJIN_ASSERT_EQ(prim.indices.size(), static_cast<usize>(3));

    // Positions survive the trip. Checked per component rather than as a count,
    // because an accessor with the right COUNT and the wrong byte offset is the
    // failure this is really looking for.
    for (usize i = 0; i < 3; ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(prim.vertices[i].position.x, static_cast<f32>(i), 0.0001f);
        ENJIN_EXPECT_FLOAT_NEAR(prim.vertices[i].position.y, static_cast<f32>(i * 2), 0.0001f);
        ENJIN_EXPECT_FLOAT_NEAR(prim.vertices[i].position.z, 0.0f, 0.0001f);
    }

    std::remove(path.c_str());
}

ENJIN_TEST(GLBExport, test_a_multi_entity_selection_keeps_each_piece_where_it_was) {
    // The point of exporting a GROUP: the arrangement on screen is the thing
    // being exported. Each node carries its world matrix, so a re-import puts
    // the pieces back rather than stacking them at the origin.
    // Arrange
    ECS::World world;
    ECS::Entity a = MakeTriangle(world, "A", Math::Vector3(0, 0, 0));
    ECS::Entity b = MakeTriangle(world, "B", Math::Vector3(10, 0, -5));
    const std::string path = TempPath("enjin_test_export_group.glb");

    // Act
    const auto r = Assets::ExportEntitiesToGLB(&world, {a, b}, path);
    ENJIN_ASSERT_TRUE(r.success);
    ENJIN_EXPECT_EQ(r.meshesWritten, 2u);

    Assets::GLTFScene scene;
    ENJIN_ASSERT_TRUE(Assets::GLTFLoader::Load(path, scene));

    // Assert
    ENJIN_ASSERT_EQ(scene.meshes.size(), static_cast<usize>(2));
    ENJIN_ASSERT_EQ(scene.nodes.size(), static_cast<usize>(2));

    // One node sits at the origin and the other ten units along X. Which is
    // which depends on node order, so assert the SET rather than the order.
    bool sawOrigin = false, sawOffset = false;
    for (const auto& n : scene.nodes) {
        if (std::fabs(n.translation.x) < 0.001f) sawOrigin = true;
        if (std::fabs(n.translation.x - 10.0f) < 0.001f &&
            std::fabs(n.translation.z + 5.0f) < 0.001f) sawOffset = true;
    }
    ENJIN_EXPECT_TRUE(sawOrigin);
    ENJIN_EXPECT_TRUE(sawOffset);

    std::remove(path.c_str());
}

ENJIN_TEST(GLBExport, test_material_colour_survives_as_a_base_colour_factor) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeTriangle(world, "Red", Math::Vector3(0, 0, 0));
    ECS::MaterialComponent mat;
    mat.baseColor = Math::Vector3(1.0f, 0.0f, 0.0f);
    mat.metallic = 0.25f;
    mat.roughness = 0.75f;
    world.AddComponent<ECS::MaterialComponent>(e, mat);
    const std::string path = TempPath("enjin_test_export_material.glb");

    // Act
    ENJIN_ASSERT_TRUE(Assets::ExportEntitiesToGLB(&world, {e}, path).success);
    Assets::GLTFScene scene;
    ENJIN_ASSERT_TRUE(Assets::GLTFLoader::Load(path, scene));

    // Assert
    ENJIN_ASSERT_TRUE(!scene.materials.empty());

    std::remove(path.c_str());
}

ENJIN_TEST(GLBExport, test_a_selection_with_no_geometry_fails_with_a_reason) {
    // Selecting a light and asking to export it is an ordinary mistake. It has
    // to say so rather than writing a file containing nothing, which opens as
    // an empty scene and looks like the exporter is broken.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    const std::string path = TempPath("enjin_test_export_empty.glb");

    // Act
    const auto r = Assets::ExportEntitiesToGLB(&world, {e}, path);

    // Assert
    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(!r.error.empty());
    ENJIN_EXPECT_EQ(r.skipped, 1u);
    // And no file left behind to confuse whoever looks in the folder.
    ENJIN_EXPECT_FALSE(std::filesystem::exists(path));
}

ENJIN_TEST(GLBExport, test_exporting_nothing_is_refused) {
    // Arrange
    ECS::World world;

    // Act
    const auto r = Assets::ExportEntitiesToGLB(&world, {}, TempPath("enjin_never.glb"));

    // Assert
    ENJIN_EXPECT_FALSE(r.success);
    ENJIN_EXPECT_TRUE(!r.error.empty());
}

ENJIN_TEST_MAIN()
