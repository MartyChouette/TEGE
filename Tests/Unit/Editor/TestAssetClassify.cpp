// The Asset Browser opened every .json as a scene, so double-clicking a
// localization table or a meta.json replaced the world with nothing (GR-9).
#include "EnjinTest.h"
#include "Enjin/Editor/AssetClassify.h"
#include <filesystem>
#include <fstream>

using namespace Enjin;
namespace fs = std::filesystem;

namespace {
fs::path WriteTemp(const char* name, const char* text) {
    const fs::path dir = fs::temp_directory_path() / "enjin_test_asset_classify";
    fs::create_directories(dir);
    const fs::path p = dir / name;
    std::ofstream(p) << text;
    return p;
}
}  // namespace

ENJIN_TEST(AssetClassify, test_asset_classify_scene_json_is_a_scene) {
    // Arrange
    const fs::path p = WriteTemp("scene.json", "{\"version\":\"1.0\",\"entities\":[]}");
    // Act / Assert
    ENJIN_EXPECT_TRUE(Editor::JsonLooksLikeScene(p));
    fs::remove_all(p.parent_path());
}

ENJIN_TEST(AssetClassify, test_asset_classify_localization_json_is_not_a_scene) {
    // Arrange
    const fs::path strings = WriteTemp("en.json", "{\"menu.play\":\"Play\",\"menu.quit\":\"Quit\"}");
    const fs::path meta = WriteTemp("meta.json", "{\"name\":\"MyGame\",\"template\":\"blank\"}");
    // Act / Assert
    ENJIN_EXPECT_FALSE(Editor::JsonLooksLikeScene(strings));
    ENJIN_EXPECT_FALSE(Editor::JsonLooksLikeScene(meta));
    ENJIN_EXPECT_FALSE(Editor::JsonLooksLikeScene(strings.parent_path() / "missing.json"));
    fs::remove_all(strings.parent_path());
}

ENJIN_TEST_MAIN()
