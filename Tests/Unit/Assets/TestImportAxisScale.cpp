// A node's NON-UNIFORM scale has to follow the axis conversion.
//
// The importers convert a Z-up file to Y-up by rewriting each node's position
// and rotation and every vertex. The node's scale was left in the source axes.
// A uniform scale cannot show that, and almost every test asset has one, so it
// went unseen until a Blender car arrived with an unapplied object scale of
// (77, 197, 6) on its body: the 197 of length landed on the height axis and the
// body imported as a 46 m pole (suv.fbx, 2026-10-06).
//
// The fixture is one mesh with unit extents under a node scaled (2, 3, 5), so
// the model is 2 x 3 x 5 in a Z-up file: 5 tall. Standing up in a Y-up engine
// it must measure 2 x 5 x 3. Ratios are compared rather than sizes because the
// Assimp path also applies a unit conversion, which is uniform.
#include "EnjinTest.h"

#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Enjin;
namespace fs = std::filesystem;

namespace {

fs::path FixtureDir() {
    return fs::temp_directory_path() / "enjin_import_axis_scale";
}

// Four corners of a unit tetrahedron: extent 1 on every axis, so the imported
// extents are the node scale and nothing else.
std::string WriteScaledNodeGltf(const char* generator = nullptr) {
    std::error_code ec;
    fs::create_directories(FixtureDir(), ec);

    const float positions[12] = {0, 0, 0,  1, 0, 0,  0, 1, 0,  0, 0, 1};
    const std::uint16_t indices[10] = {0, 1, 2,  0, 1, 3,  0, 2, 3,  0};  // last = padding
    std::vector<char> bin(sizeof(positions) + sizeof(indices));
    std::memcpy(bin.data(), positions, sizeof(positions));
    std::memcpy(bin.data() + sizeof(positions), indices, sizeof(indices));
    {
        std::ofstream out(FixtureDir() / "scaled.bin", std::ios::binary);
        if (!out) return "";
        out.write(bin.data(), static_cast<std::streamsize>(bin.size()));
    }

    const fs::path gltf = FixtureDir() / "scaled.gltf";
    std::ofstream out(gltf);
    if (!out) return "";
    out << R"({"asset":{"version":"2.0")";
    if (generator) out << R"(,"generator":")" << generator << R"(")";
    out << R"(},"scene":0,"scenes":[{"nodes":[0]}],)"
        << R"("nodes":[{"name":"Body","mesh":0,"scale":[2,3,5]}],)"
        << R"("meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],)"
        << R"("buffers":[{"uri":"scaled.bin","byteLength":68}],)"
        << R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48,"target":34962},)"
        << R"({"buffer":0,"byteOffset":48,"byteLength":18,"target":34963}],)"
        << R"("accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3",)"
        << R"("min":[0,0,0],"max":[1,1,1]},)"
        << R"({"bufferView":1,"componentType":5123,"count":9,"type":"SCALAR"}]})";
    return gltf.string();
}

Assets::ImportOptions BlenderOptions() {
    Assets::ImportOptions opts;
    opts.sourceApp = Assets::SourceApp::Blender;   // Z-up -> Y-up
    opts.generateColliders = false;
    opts.generateLODs = false;
    return opts;
}

// Extents of the one imported mesh after its entity's own scale. Returns false
// when the import produced no mesh entity.
bool ScaledMeshExtents(ECS::World& world, const Assets::ImportResult& r, Math::Vector3& out) {
    for (ECS::Entity e : r.entities) {
        auto* mesh = world.GetComponent<ECS::MeshComponent>(e);
        auto* xf = world.GetComponent<ECS::TransformComponent>(e);
        if (!mesh || !xf || mesh->vertices.empty()) continue;
        Math::Vector3 lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
        for (const auto& v : mesh->vertices) {
            const Math::Vector3 p(v.position.x * xf->scale.x,
                                  v.position.y * xf->scale.y,
                                  v.position.z * xf->scale.z);
            lo.x = std::fmin(lo.x, p.x); hi.x = std::fmax(hi.x, p.x);
            lo.y = std::fmin(lo.y, p.y); hi.y = std::fmax(hi.y, p.y);
            lo.z = std::fmin(lo.z, p.z); hi.z = std::fmax(hi.z, p.z);
        }
        out = Math::Vector3(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
        return true;
    }
    return false;
}

} // namespace

ENJIN_TEST(ImportAxisScale, test_gltf_import_z_up_non_uniform_scale_follows_the_axis_swap) {
    // Arrange
    const std::string path = WriteScaledNodeGltf();
    ENJIN_ASSERT_FALSE(path.empty());
    ECS::World world;

    // Act
    Assets::ImportResult r = Assets::SceneImporter::ImportGLTF(path, &world, BlenderOptions());

    // Assert: 2 x 3 x 5 Z-up stands up as 2 x 5 x 3.
    ENJIN_ASSERT_TRUE(r.success);
    Math::Vector3 ext;
    ENJIN_ASSERT_TRUE(ScaledMeshExtents(world, r, ext));
    ENJIN_ASSERT_TRUE(ext.x > 1e-6f);
    ENJIN_EXPECT_TRUE(std::fabs(ext.y / ext.x - 2.5f) < 0.01f);
    ENJIN_EXPECT_TRUE(std::fabs(ext.z / ext.x - 1.5f) < 0.01f);

    std::error_code ec;
    fs::remove_all(FixtureDir(), ec);
}

ENJIN_TEST(ImportAxisScale, test_assimp_import_z_up_non_uniform_scale_follows_the_axis_swap) {
    // The path suv.fbx took. Assimp reads the same .gltf, so no binary fixture.
    // Arrange
    const std::string path = WriteScaledNodeGltf();
    ENJIN_ASSERT_FALSE(path.empty());
    ECS::World world;

    // Act
    Assets::ImportResult r = Assets::SceneImporter::ImportAssimp(path, &world, BlenderOptions());

    // Assert
    ENJIN_ASSERT_TRUE(r.success);
    Math::Vector3 ext;
    ENJIN_ASSERT_TRUE(ScaledMeshExtents(world, r, ext));
    ENJIN_ASSERT_TRUE(ext.x > 1e-6f);
    ENJIN_EXPECT_TRUE(std::fabs(ext.y / ext.x - 2.5f) < 0.01f);
    ENJIN_EXPECT_TRUE(std::fabs(ext.z / ext.x - 1.5f) < 0.01f);

    std::error_code ec;
    fs::remove_all(FixtureDir(), ec);
}

ENJIN_TEST(ImportAxisScale, test_assimp_import_detected_blender_y_up_file_is_not_turned_again) {
    // The other half of suv.fbx. The importer read "Blender" off the file and
    // applied the Z-up preset to a file that was already Y-up, so every Blender
    // FBX arrived face-down. Which program wrote a file says nothing about which
    // way up it is. The fixture is Y-up by the glTF spec and names Blender as its
    // generator, which Assimp hands over the same way it hands over an FBX creator.
    // Arrange
    const std::string path = WriteScaledNodeGltf("Khronos glTF Blender I/O v4.5");
    ENJIN_ASSERT_FALSE(path.empty());
    Assets::AssimpScene loaded;
    ENJIN_ASSERT_TRUE(Assets::AssimpLoader::Load(path, loaded));
    // Without this the test passes for the wrong reason: no detection, no turn.
    ENJIN_ASSERT_TRUE(loaded.creator.find("Blender") != std::string::npos);
    ECS::World world;
    Assets::ImportOptions opts = BlenderOptions();
    opts.sourceApp = Assets::SourceApp::Auto;

    // Act
    Assets::ImportResult r = Assets::SceneImporter::ImportAssimp(path, &world, opts);

    // Assert: still 2 x 3 x 5, exactly as authored.
    ENJIN_ASSERT_TRUE(r.success);
    Math::Vector3 ext;
    ENJIN_ASSERT_TRUE(ScaledMeshExtents(world, r, ext));
    ENJIN_ASSERT_TRUE(ext.x > 1e-6f);
    ENJIN_EXPECT_TRUE(std::fabs(ext.y / ext.x - 1.5f) < 0.01f);
    ENJIN_EXPECT_TRUE(std::fabs(ext.z / ext.x - 2.5f) < 0.01f);

    std::error_code ec;
    fs::remove_all(FixtureDir(), ec);
}

ENJIN_TEST(ImportAxisScale, test_importer_z_up_turn_file_stated_up_axis_outranks_the_chosen_app) {
    // suv.fbx was re-imported with "Blender" saved as an explicit choice in its
    // .enjinasset and was turned onto its face again. A file that states its up
    // axis is believed over any app, chosen or detected.
    // Arrange
    Assets::ImportOptions blender;
    blender.sourceApp = Assets::SourceApp::Blender;
    Assets::ImportOptions automatic;
    Assets::ImportOptions off = blender;
    off.convertAxes = false;

    // Act / Assert: the file states Y-up (1), so nothing turns it.
    ENJIN_EXPECT_FALSE(Assets::SceneImporter::AssimpAppliesZUpTurn(blender, true, 1));
    ENJIN_EXPECT_FALSE(Assets::SceneImporter::AssimpAppliesZUpTurn(automatic, true, 1));
    // The file states Z-up (2): turned once, whatever is selected.
    ENJIN_EXPECT_TRUE(Assets::SceneImporter::AssimpAppliesZUpTurn(blender, true, 2));
    ENJIN_EXPECT_TRUE(Assets::SceneImporter::AssimpAppliesZUpTurn(automatic, true, 2));
    // The file says nothing: only a Z-up app picked by hand turns it.
    ENJIN_EXPECT_TRUE(Assets::SceneImporter::AssimpAppliesZUpTurn(blender, false, 1));
    ENJIN_EXPECT_FALSE(Assets::SceneImporter::AssimpAppliesZUpTurn(automatic, false, 1));
    // "Apply Axis Conversion" off means off.
    ENJIN_EXPECT_FALSE(Assets::SceneImporter::AssimpAppliesZUpTurn(off, true, 2));
    ENJIN_EXPECT_FALSE(Assets::SceneImporter::AssimpAppliesZUpTurn(off, false, 1));
}

ENJIN_TEST_MAIN()
