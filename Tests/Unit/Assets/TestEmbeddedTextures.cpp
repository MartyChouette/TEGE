// Embedded glTF textures, which the importer parsed and then dropped.
//
// A .glb carries its textures inside the file. GLTFLoader has always filled
// GLTFImage::data from the buffer view, and the importer's texture resolver
// returned "" whenever the image had no `uri` -- which is exactly the embedded
// case. So every texture packed into a GLB was read and thrown away: the model
// imported, the materials came through, and the surfaces were untextured, which
// reads as a bad export rather than an importer bug. Found by the validation
// triage 2026-09-18.
//
// These test the extraction directly rather than through a hand-built .glb. The
// engine's own GLB exporter does not embed textures, so an end-to-end fixture
// would mean checking a binary into the repo or writing a glTF JSON assembler
// for one test. What is actually new is this function, and it is what these
// cover: the naming rule, the deduplication, and the failure paths.
#include "EnjinTest.h"
#include "Enjin/Assets/SceneImporter.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Enjin;
using Assets::SceneImporter;

namespace {

std::string ScratchDir(const char* name) {
    auto d = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(d, ec);
    std::filesystem::create_directories(d, ec);
    return d.string();
}

std::vector<u8> Bytes(std::initializer_list<int> vals) {
    std::vector<u8> out;
    for (int v : vals) out.push_back(static_cast<u8>(v));
    return out;
}

usize FileSize(const std::string& path) {
    std::error_code ec;
    auto n = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<usize>(n);
}

} // namespace

ENJIN_TEST(EmbeddedTextures, test_bytes_are_written_out_and_the_path_points_at_them) {
    // Arrange
    const std::string dir = ScratchDir("enjin_embedded_tex_write");
    const std::vector<u8> png = Bytes({0x89, 'P', 'N', 'G', 1, 2, 3, 4});

    // Act
    const std::string path = SceneImporter::ExtractEmbeddedTexture(png, "image/png", dir);

    // Assert
    ENJIN_ASSERT_TRUE(!path.empty());
    ENJIN_EXPECT_TRUE(std::filesystem::exists(path));
    ENJIN_EXPECT_EQ(FileSize(path), png.size());
    ENJIN_EXPECT_TRUE(path.find("extracted_textures") != std::string::npos);
}

ENJIN_TEST(EmbeddedTextures, test_the_same_bytes_produce_the_same_file_rather_than_a_second_copy) {
    // The reason the name is a hash of the CONTENT. Two models in one folder
    // sharing a texture write one file, and re-importing a model does not pile
    // up copies beside it.
    // Arrange
    const std::string dir = ScratchDir("enjin_embedded_tex_dedupe");
    const std::vector<u8> png = Bytes({0x89, 'P', 'N', 'G', 9, 9, 9});

    // Act
    const std::string first = SceneImporter::ExtractEmbeddedTexture(png, "image/png", dir);
    const std::string second = SceneImporter::ExtractEmbeddedTexture(png, "image/png", dir);

    // Assert
    ENJIN_EXPECT_STR_EQ(first, second);
    usize count = 0;
    for (auto& e : std::filesystem::directory_iterator(
             std::filesystem::path(dir) / "extracted_textures")) {
        (void)e; ++count;
    }
    ENJIN_EXPECT_EQ(count, static_cast<usize>(1));
}

ENJIN_TEST(EmbeddedTextures, test_different_bytes_do_not_collide) {
    // The control for the test above. A namer that returned a constant would
    // pass the dedupe test and silently give every texture in a model the same
    // file -- so the last one written would win and every surface would wear it.
    // Arrange
    const std::string dir = ScratchDir("enjin_embedded_tex_distinct");

    // Act
    const std::string a = SceneImporter::ExtractEmbeddedTexture(
        Bytes({1, 2, 3, 4}), "image/png", dir);
    const std::string b = SceneImporter::ExtractEmbeddedTexture(
        Bytes({4, 3, 2, 1}), "image/png", dir);

    // Assert
    ENJIN_ASSERT_TRUE(!a.empty() && !b.empty());
    ENJIN_EXPECT_TRUE(a != b);
    ENJIN_EXPECT_TRUE(std::filesystem::exists(a));
    ENJIN_EXPECT_TRUE(std::filesystem::exists(b));
}

ENJIN_TEST(EmbeddedTextures, test_the_extension_follows_the_mime_type) {
    // The loader that reads this back picks its decoder off the extension, so a
    // JPEG written as .png is a texture that fails to load later, far from here.
    // Arrange
    const std::string dir = ScratchDir("enjin_embedded_tex_mime");
    const std::vector<u8> data = Bytes({0xFF, 0xD8, 0xFF, 0xE0});

    // Act
    const std::string jpg = SceneImporter::ExtractEmbeddedTexture(data, "image/jpeg", dir);
    const std::string png = SceneImporter::ExtractEmbeddedTexture(
        Bytes({0x89, 'P', 'N', 'G'}), "image/png", dir);

    // Assert
    ENJIN_ASSERT_TRUE(!jpg.empty() && !png.empty());
    ENJIN_EXPECT_TRUE(jpg.size() > 4 && jpg.substr(jpg.size() - 4) == ".jpg");
    ENJIN_EXPECT_TRUE(png.size() > 4 && png.substr(png.size() - 4) == ".png");
}

ENJIN_TEST(EmbeddedTextures, test_an_unknown_mime_type_defaults_to_png) {
    // glTF allows image/png and image/jpeg, and exporters have been known to
    // omit mimeType entirely. PNG is the safer default: the loaders sniff magic
    // bytes, so a wrong extension is recoverable where an empty path is not.
    // Arrange
    const std::string dir = ScratchDir("enjin_embedded_tex_unknown_mime");

    // Act
    const std::string p = SceneImporter::ExtractEmbeddedTexture(
        Bytes({1, 2, 3}), "", dir);

    // Assert
    ENJIN_ASSERT_TRUE(!p.empty());
    ENJIN_EXPECT_TRUE(p.substr(p.size() - 4) == ".png");
}

ENJIN_TEST(EmbeddedTextures, test_no_data_means_no_path_and_no_file) {
    // An image entry with neither a uri nor bytes. Returning a path to an empty
    // file would be worse than returning nothing: the material would carry a
    // texture that cannot decode, and the failure would surface at render time.
    // Arrange
    const std::string dir = ScratchDir("enjin_embedded_tex_empty");

    // Act
    const std::string p = SceneImporter::ExtractEmbeddedTexture({}, "image/png", dir);

    // Assert
    ENJIN_EXPECT_TRUE(p.empty());
    ENJIN_EXPECT_FALSE(std::filesystem::exists(
        std::filesystem::path(dir) / "extracted_textures"));
}

ENJIN_TEST_MAIN()
