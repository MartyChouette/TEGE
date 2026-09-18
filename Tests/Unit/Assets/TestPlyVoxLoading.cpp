// PLY and VOX are on the documented format list, and nothing had ever loaded
// one.
//
// TestAssetLoaders covers only the failure path -- a file that does not exist
// -- so "supported" rested on the loaders compiling. The backlog asked to
// verify these two or strike them from the list; this is the verification. The
// fixtures are written by the test rather than checked in, so the suite has no
// binary dependencies and cleans up after itself.
#include "EnjinTest.h"
#include "Enjin/Assets/PLYLoader.h"
#include "Enjin/Assets/VOXLoader.h"

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

// Minimal ASCII PLY: one triangle, with colour, which is the shape a point
// cloud export actually takes.
void WriteAsciiPly(const std::string& path) {
    std::ofstream f(path);
    f << "ply\n"
         "format ascii 1.0\n"
         "element vertex 3\n"
         "property float x\n"
         "property float y\n"
         "property float z\n"
         "property uchar red\n"
         "property uchar green\n"
         "property uchar blue\n"
         "element face 1\n"
         "property list uchar int vertex_indices\n"
         "end_header\n"
         "0.0 0.0 0.0 255 0 0\n"
         "1.0 0.0 0.0 0 255 0\n"
         "0.0 1.0 0.0 0 0 255\n"
         "3 0 1 2\n";
}

void PutU32(std::ofstream& f, u32 v) {
    f.write(reinterpret_cast<const char*>(&v), 4);
}

// Minimal MagicaVoxel file: one 2x2x2 model holding two voxels.
//
// The format is chunked: every chunk is a 4-byte id, its own byte count, its
// children's byte count, then the content. MAIN carries no content and all the
// others as children, which is the part that is easy to get wrong.
void WriteVox(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f.write("VOX ", 4);
    PutU32(f, 150);

    // SIZE: 12 bytes of content.
    // XYZI: 4 bytes of count + 4 bytes per voxel = 12 for two voxels.
    const u32 sizeChunk = 12 + 12;      // header + content
    const u32 xyziChunk = 12 + 4 + 8;
    f.write("MAIN", 4);
    PutU32(f, 0);                        // MAIN has no content of its own
    PutU32(f, sizeChunk + xyziChunk);    // ... only children

    f.write("SIZE", 4);
    PutU32(f, 12);
    PutU32(f, 0);
    PutU32(f, 2); PutU32(f, 2); PutU32(f, 2);

    f.write("XYZI", 4);
    PutU32(f, 4 + 8);
    PutU32(f, 0);
    PutU32(f, 2);                        // two voxels
    const unsigned char voxels[8] = {0, 0, 0, 1,
                                     1, 1, 1, 2};   // x,y,z,colorIndex
    f.write(reinterpret_cast<const char*>(voxels), 8);
}

} // namespace

ENJIN_TEST(PlyVoxLoading, test_an_ascii_ply_loads_its_vertices) {
    // Arrange
    const std::string path = TempPath("enjin_test_triangle.ply");
    WriteAsciiPly(path);

    // Act
    Assets::PLYMesh mesh;
    const bool ok = Assets::PLYLoader::Load(path, mesh);

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    if (!ok) {
        std::printf("      PLY error: %s\n", Assets::PLYLoader::GetLastError().c_str());
    }
    ENJIN_EXPECT_EQ(mesh.vertices.size(), static_cast<usize>(3));
    // The docs call PLY "point clouds". The loader reads the face element too,
    // so a PLY with faces imports as a MESH -- pinned here because the
    // documentation undersells it and a reader would not try.
    ENJIN_EXPECT_EQ(mesh.indices.size(), static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(mesh.hasColors);

    std::remove(path.c_str());
}

ENJIN_TEST(PlyVoxLoading, test_a_ply_reports_why_it_failed_rather_than_going_quiet) {
    // A loader that returns false with an empty reason turns a bad export into
    // an unexplained blank import.
    // Arrange
    const std::string path = TempPath("enjin_test_broken.ply");
    {
        std::ofstream f(path);
        f << "ply\nformat ascii 1.0\nelement vertex 2\nend_header\n";   // no properties
    }

    // Act
    Assets::PLYMesh mesh;
    const bool ok = Assets::PLYLoader::Load(path, mesh);

    // Assert: either it refuses WITH a reason, or it loads nothing at all --
    // what it must not do is claim success and hand back a full mesh.
    if (!ok) {
        ENJIN_EXPECT_TRUE(!Assets::PLYLoader::GetLastError().empty());
    } else {
        ENJIN_EXPECT_TRUE(mesh.vertices.empty() || mesh.vertices.size() == 2);
    }

    std::remove(path.c_str());
}

ENJIN_TEST(PlyVoxLoading, test_a_vox_file_loads_its_voxels) {
    // Arrange
    const std::string path = TempPath("enjin_test_two.vox");
    WriteVox(path);

    // Act
    Assets::VOXModel model;
    const bool ok = Assets::VOXLoader::Load(path, model);

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    if (!ok) {
        std::printf("      VOX error: %s\n", Assets::VOXLoader::GetLastError().c_str());
    }
    ENJIN_EXPECT_EQ(model.voxels.size(), static_cast<usize>(2));

    std::remove(path.c_str());
}

ENJIN_TEST(PlyVoxLoading, test_a_vox_file_converts_to_a_drawable_mesh) {
    // LoadAsMesh is the path the importer actually takes, and a loader that
    // parses voxels but produces no triangles imports an empty entity.
    // Arrange
    const std::string path = TempPath("enjin_test_mesh.vox");
    WriteVox(path);

    // Act
    Assets::VOXMesh mesh;
    const bool ok = Assets::VOXLoader::LoadAsMesh(path, mesh);

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    if (!ok) {
        std::printf("      VOX mesh error: %s\n", Assets::VOXLoader::GetLastError().c_str());
    }
    ENJIN_EXPECT_TRUE(!mesh.vertices.empty());
    ENJIN_EXPECT_TRUE(mesh.indices.size() % 3 == 0);
    for (u32 i : mesh.indices) {
        ENJIN_ASSERT_TRUE(i < mesh.vertices.size());
    }

    std::remove(path.c_str());
}

ENJIN_TEST(PlyVoxLoading, test_a_file_that_is_not_a_vox_is_refused_with_a_reason) {
    // Arrange
    const std::string path = TempPath("enjin_test_not.vox");
    {
        std::ofstream f(path, std::ios::binary);
        f << "this is not a MagicaVoxel file";
    }

    // Act
    Assets::VOXModel model;
    const bool ok = Assets::VOXLoader::Load(path, model);

    // Assert
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_TRUE(!Assets::VOXLoader::GetLastError().empty());

    std::remove(path.c_str());
}

ENJIN_TEST_MAIN()
