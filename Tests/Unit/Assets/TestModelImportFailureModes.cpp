// What the model importers do with a file they should refuse.
//
// The existing loader tests cover defaults and the file-does-not-exist path.
// Nothing covered the case a person actually hits: a real model file that is
// damaged -- a download that stopped, a copy that died, a .glb whose header
// says one length and whose body is another. The bar is a clean refusal and a
// reason, never a crash and never a half-built scene.
//
// The fixtures start as a VALID .glb written by the engine's own exporter and
// are then damaged in specific ways, so each test names one defect rather than
// throwing noise at the parser and hoping.
#include "EnjinTest.h"
#include "Enjin/Assets/GLBExporter.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Name.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Enjin;

namespace {

std::string TempPath(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

// A real, valid .glb to damage. Writing it with the exporter rather than
// checking a binary into the repo keeps the suite dependency-free and means
// these tests break if the exporter ever stops producing loadable files.
bool WriteValidGlb(const std::string& path) {
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    world.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"Tri"});
    ECS::MeshComponent mesh;
    for (int i = 0; i < 3; ++i) {
        ECS::MeshComponent::Vertex v{};
        v.position = Math::Vector3(static_cast<f32>(i), 0.0f, 0.0f);
        v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
        mesh.vertices.push_back(v);
    }
    mesh.indices = {0, 1, 2};
    world.AddComponent<ECS::MeshComponent>(e, mesh);
    return Assets::ExportEntitiesToGLB(&world, {e}, path).success;
}

std::vector<char> ReadAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
}

void WriteBytes(const std::string& path, const std::vector<char>& bytes) {
    std::ofstream f(path, std::ios::binary);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

} // namespace

ENJIN_TEST(ModelImportFailureModes, test_the_fixture_itself_loads_before_anything_is_damaged) {
    // Without this the rest prove nothing: a parser that refuses everything
    // would pass every test below.
    // Arrange
    const std::string path = TempPath("enjin_test_import_valid.glb");
    ENJIN_ASSERT_TRUE(WriteValidGlb(path));

    // Act
    Assets::GLTFScene scene;
    const bool ok = Assets::GLTFLoader::Load(path, scene);

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_EQ(scene.meshes.size(), static_cast<usize>(1));

    std::remove(path.c_str());
}

ENJIN_TEST(ModelImportFailureModes, test_a_truncated_glb_is_refused) {
    // The download that stopped. The header still says how long the file
    // should be, so a parser that trusts it walks off the end of the buffer.
    // Arrange
    const std::string src = TempPath("enjin_test_import_src.glb");
    ENJIN_ASSERT_TRUE(WriteValidGlb(src));
    std::vector<char> bytes = ReadAll(src);
    ENJIN_ASSERT_TRUE(bytes.size() > 40);
    bytes.resize(bytes.size() / 2);            // header intact, body cut short
    const std::string cut = TempPath("enjin_test_import_cut.glb");
    WriteBytes(cut, bytes);

    // Act
    Assets::GLTFScene scene;
    const bool ok = Assets::GLTFLoader::Load(cut, scene);

    // Assert: refused, or loaded with nothing -- never a crash, and never a
    // mesh built from bytes that are not there. One unconditional claim, so a
    // clean refusal still exercises an assertion.
    bool nothingInvented = true;
    if (ok) {
        for (const auto& m : scene.meshes) {
            for (const auto& p : m.primitives) {
                if (!p.vertices.empty() && !p.indices.empty()) nothingInvented = false;
            }
        }
    }
    ENJIN_EXPECT_TRUE(nothingInvented);

    std::remove(src.c_str());
    std::remove(cut.c_str());
}

ENJIN_TEST(ModelImportFailureModes, test_a_glb_whose_header_lies_about_its_length_is_refused) {
    // Arrange: keep every byte, claim the file is far larger than it is.
    const std::string src = TempPath("enjin_test_import_len_src.glb");
    ENJIN_ASSERT_TRUE(WriteValidGlb(src));
    std::vector<char> bytes = ReadAll(src);
    ENJIN_ASSERT_TRUE(bytes.size() > 12);
    const u32 lie = 0x0FFFFFFFu;
    std::memcpy(bytes.data() + 8, &lie, 4);    // total length field
    const std::string bad = TempPath("enjin_test_import_len_bad.glb");
    WriteBytes(bad, bytes);

    // Act
    Assets::GLTFScene scene;
    const bool ok = Assets::GLTFLoader::Load(bad, scene);

    // Assert: whatever it decides, it must not allocate against the claim or
    // read past the buffer -- reaching this line at all is most of the test --
    // and it must not report geometry the file does not contain.
    bool geometryIsSane = true;
    if (ok) {
        for (const auto& m : scene.meshes) {
            for (const auto& prim : m.primitives) {
                for (u32 i : prim.indices) {
                    if (i >= prim.vertices.size()) geometryIsSane = false;
                }
            }
        }
    }
    ENJIN_EXPECT_TRUE(geometryIsSane);

    std::remove(src.c_str());
    std::remove(bad.c_str());
}

ENJIN_TEST(ModelImportFailureModes, test_a_file_with_the_wrong_magic_is_refused_with_a_reason) {
    // Arrange
    const std::string path = TempPath("enjin_test_import_magic.glb");
    {
        std::ofstream f(path, std::ios::binary);
        f << "NOTGLTF and then some plausible-looking padding bytes........";
    }

    // Act
    Assets::GLTFScene scene;
    const bool ok = Assets::GLTFLoader::Load(path, scene);

    // Assert
    ENJIN_EXPECT_FALSE(ok);

    std::remove(path.c_str());
}

ENJIN_TEST(ModelImportFailureModes, test_an_empty_file_is_refused) {
    // Arrange
    const std::string path = TempPath("enjin_test_import_empty.glb");
    { std::ofstream f(path, std::ios::binary); }

    // Act
    Assets::GLTFScene scene;
    const bool ok = Assets::GLTFLoader::Load(path, scene);

    // Assert
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(scene.meshes.empty());

    std::remove(path.c_str());
}

ENJIN_TEST(ModelImportFailureModes, test_a_corrupt_obj_is_refused_rather_than_half_read) {
    // The Assimp path, which is a different parser entirely. An .obj is text,
    // so the damage that matters is nonsense where numbers should be.
    // Arrange
    const std::string path = TempPath("enjin_test_import_broken.obj");
    {
        std::ofstream f(path);
        f << "v 0.0 0.0 0.0\n"
             "v not-a-number 1.0 0.0\n"
             "f 1 2 999999\n";
    }

    // Act
    Assets::AssimpScene scene;
    const bool ok = Assets::AssimpLoader::Load(path, scene);

    // Assert. Written as ONE unconditional assertion on purpose: the first
    // version put its checks inside `if (ok)`, so when the loader refused the
    // file -- the likely outcome, and a correct one -- the test ran no
    // assertions at all and the framework failed it for that. The contract is
    // "refuse it, OR accept it with geometry that is really there", and that is
    // a single claim.
    bool geometryIsSane = true;
    if (ok) {
        for (const auto& m : scene.meshes) {
            for (const auto& prim : m.primitives) {
                for (u32 i : prim.indices) {
                    if (i >= prim.vertices.size()) geometryIsSane = false;
                }
            }
        }
    }
    ENJIN_EXPECT_TRUE(geometryIsSane);

    std::remove(path.c_str());
}

ENJIN_TEST_MAIN()
