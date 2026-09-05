#include "EnjinTest.h"
#include "Enjin/Assets/AssetPipeline.h"
#include "Enjin/Platform/Paths.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Enjin;
namespace fs = std::filesystem;

// Integration-style: the copier moves real files, so these use real files.
namespace {

struct TempDir {
    fs::path path;

    explicit TempDir(const char* name) {
        path = fs::path(Platform::GetAppTempDirectory()) / name;
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    void Write(const std::string& relative, const std::string& contents) const {
        fs::path full = path / relative;
        fs::create_directories(full.parent_path());
        std::ofstream out(full, std::ios::binary);
        out << contents;
    }

    bool Has(const std::string& relative) const {
        std::error_code ec;
        return fs::exists(path / relative, ec);
    }

    std::string Read(const std::string& relative) const {
        std::ifstream in(path / relative, std::ios::binary);
        if (!in) return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
};

} // namespace

// The case that started this: a model picked from somewhere else on disk. The
// model, its material library and the material's textures all have to come
// along, or the copy is a model with no materials - a quieter version of the
// same failure.
ENJIN_TEST(AssetPipeline, ObjCopiesMtlAndTextures) {
    // Arrange: a model outside the project, with a material library and a
    // texture it references, plus the engine's import sidecar.
    TempDir src("enjin_test_ap_src_obj");
    TempDir proj("enjin_test_ap_proj_obj");

    src.Write("city/model.obj", "mtllib materials.mtl\nv 0 0 0\n");
    src.Write("city/materials.mtl", "newmtl wall\nmap_Kd textures/wall.png\n");
    src.Write("city/textures/wall.png", "PNG");
    src.Write("city/model.obj.enjinasset", "{}");

    // Act
    std::vector<std::string> copied;
    std::string rel = Assets::CopyModelToProjectAssets(
        (src.path / "city/model.obj").string(), proj.path.string(), "assets/models", &copied);

    // Assert

    ENJIN_EXPECT_EQ(rel, std::string("assets/models/model/model.obj"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/model/model.obj"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/model/materials.mtl"));
    // Layout preserved, so the copied .mtl still finds its texture unchanged.
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/model/textures/wall.png"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/model/model.obj.enjinasset"));
    ENJIN_EXPECT_TRUE(proj.Read("assets/models/model/materials.mtl").find("textures/wall.png")
                      != std::string::npos);
}

// A texture stored beside the model's folder rather than inside it cannot keep
// its relative path, so it is flattened and the material is rewritten. Without
// the rewrite the copy would point back at the author's disk, which is the
// whole problem being fixed.
ENJIN_TEST(AssetPipeline, ObjRewritesReferenceThatEscapesTheModelFolder) {
    // Arrange: the texture sits beside the model's folder, not inside it.
    TempDir src("enjin_test_ap_src_esc");
    TempDir proj("enjin_test_ap_proj_esc");

    src.Write("city/model.obj", "mtllib materials.mtl\n");
    src.Write("city/materials.mtl", "newmtl wall\nmap_Kd ../shared/wall.png\n");
    src.Write("shared/wall.png", "PNG");

    // Act
    std::string rel = Assets::CopyModelToProjectAssets(
        (src.path / "city/model.obj").string(), proj.path.string(), "assets/models", nullptr);

    // Assert
    ENJIN_EXPECT_EQ(rel, std::string("assets/models/model/model.obj"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/model/wall.png"));
    const std::string mtl = proj.Read("assets/models/model/materials.mtl");
    ENJIN_EXPECT_TRUE(mtl.find("../shared/wall.png") == std::string::npos);
    ENJIN_EXPECT_TRUE(mtl.find("wall.png") != std::string::npos);
}

// glTF keeps its buffers and images beside it.
ENJIN_TEST(AssetPipeline, GltfCopiesBuffersAndImages) {
    // Arrange: a glTF with an external buffer, an external image, and an
    // embedded data: URI that must be left alone.
    TempDir src("enjin_test_ap_src_gltf");
    TempDir proj("enjin_test_ap_proj_gltf");

    src.Write("scene/scene.gltf",
              "{\"buffers\":[{\"uri\":\"scene.bin\"}],"
              "\"images\":[{\"uri\":\"tex/albedo.png\"},{\"uri\":\"data:image/png;base64,AAAA\"}]}");
    src.Write("scene/scene.bin", "BIN");
    src.Write("scene/tex/albedo.png", "PNG");

    // Act
    std::string rel = Assets::CopyModelToProjectAssets(
        (src.path / "scene/scene.gltf").string(), proj.path.string(), "assets/models", nullptr);

    // Assert

    ENJIN_EXPECT_EQ(rel, std::string("assets/models/scene/scene.gltf"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/scene/scene.bin"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/scene/tex/albedo.png"));
}

// A model already inside the project is left where it is and simply reported
// with a project-relative path.
ENJIN_TEST(AssetPipeline, ModelAlreadyInProjectIsNotCopied) {
    // Arrange
    TempDir proj("enjin_test_ap_proj_inside");
    proj.Write("assets/model.obj", "v 0 0 0\n");

    // Act
    std::vector<std::string> copied;
    std::string rel = Assets::CopyModelToProjectAssets(
        (proj.path / "assets/model.obj").string(), proj.path.string(), "assets/models", &copied);

    // Assert

    ENJIN_EXPECT_EQ(rel, std::string("assets/model.obj"));
    ENJIN_EXPECT_TRUE(copied.empty());
    ENJIN_EXPECT_TRUE(!proj.Has("assets/models/model/model.obj"));
}

// A missing sidecar is reported, not fatal: the model still copies, so the
// import degrades instead of failing outright.
ENJIN_TEST(AssetPipeline, MissingMtlStillCopiesTheModel) {
    // Arrange: the .obj names a material library that does not exist.
    TempDir src("enjin_test_ap_src_nomtl");
    TempDir proj("enjin_test_ap_proj_nomtl");
    src.Write("city/model.obj", "mtllib gone.mtl\nv 0 0 0\n");

    // Act
    std::string rel = Assets::CopyModelToProjectAssets(
        (src.path / "city/model.obj").string(), proj.path.string(), "assets/models", nullptr);

    // Assert
    ENJIN_EXPECT_EQ(rel, std::string("assets/models/model/model.obj"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/models/model/model.obj"));
}

// Textures go in relative too. This returned an absolute path, so a copied
// texture was portable on disk but not in the scene that referenced it.
ENJIN_TEST(AssetPipeline, CopyToProjectAssetsReturnsRelativePath) {
    // Arrange
    TempDir src("enjin_test_ap_src_tex");
    TempDir proj("enjin_test_ap_proj_tex");
    src.Write("wall.png", "PNG");

    // Act
    std::string rel = Assets::CopyToProjectAssets(
        (src.path / "wall.png").string(), proj.path.string(), "assets/textures");

    // Assert

    ENJIN_EXPECT_EQ(rel, std::string("assets/textures/wall.png"));
    ENJIN_EXPECT_TRUE(proj.Has("assets/textures/wall.png"));
}

ENJIN_TEST_MAIN()
