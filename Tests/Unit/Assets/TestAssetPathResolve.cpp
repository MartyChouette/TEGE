// Resolving an authored asset path against the project root.
//
// This exists because the sprite texture atlas opened authored paths raw. The
// editor and the player both run from their exe directory, so "assets/hero.png"
// resolved against the wrong place, every sprite texture failed to load, every
// sprite was excluded from the atlas, and textured sprites drew NOTHING in any
// project. The only trace was a warning per texture.
#include "EnjinTest.h"
#include "Enjin/Assets/MeshAssetCache.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Assets;

namespace {

// A project root with one asset in it, cleaned up by the caller.
struct TempProject {
    std::filesystem::path root;
    explicit TempProject(const std::string& tag) {
        root = std::filesystem::temp_directory_path() / ("enjin_resolve_" + tag);
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "assets");
        std::ofstream(root / "assets" / "hero.png") << "not a real png";
    }
    ~TempProject() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

} // namespace

ENJIN_TEST(AssetPathResolve, test_resolve_project_relative_path_returns_rooted_path) {
    // Arrange
    TempProject proj("relative");
    MeshAssetCache::Get().SetSearchRoot(proj.root.string());

    // Act
    const std::string resolved = MeshAssetCache::ResolveAgainstSearchRoot("assets/hero.png");

    // Assert: it points at the file that actually exists, not at the bare path.
    ENJIN_EXPECT_TRUE(std::filesystem::exists(resolved));
    ENJIN_EXPECT_TRUE(resolved != "assets/hero.png");

    MeshAssetCache::Get().SetSearchRoot("");
}

ENJIN_TEST(AssetPathResolve, test_resolve_absolute_path_is_returned_unchanged) {
    // Arrange: an absolute path is already openable and must not be re-rooted.
    TempProject proj("absolute");
    MeshAssetCache::Get().SetSearchRoot(proj.root.string());
    const std::string absolute = (proj.root / "assets" / "hero.png").string();

    // Act
    const std::string resolved = MeshAssetCache::ResolveAgainstSearchRoot(absolute);

    // Assert
    ENJIN_EXPECT_TRUE(resolved == absolute);

    MeshAssetCache::Get().SetSearchRoot("");
}

ENJIN_TEST(AssetPathResolve, test_resolve_missing_file_returns_original_path) {
    // A caller's error message must name the path the AUTHOR wrote, not our
    // guess at where it should have been.
    TempProject proj("missing");
    MeshAssetCache::Get().SetSearchRoot(proj.root.string());

    const std::string resolved = MeshAssetCache::ResolveAgainstSearchRoot("assets/nope.png");

    ENJIN_EXPECT_TRUE(resolved == "assets/nope.png");

    MeshAssetCache::Get().SetSearchRoot("");
}

ENJIN_TEST(AssetPathResolve, test_resolve_with_no_search_root_returns_original_path) {
    // Before a project is open there is nothing to root against, and inventing
    // one would be worse than failing.
    MeshAssetCache::Get().SetSearchRoot("");

    ENJIN_EXPECT_TRUE(MeshAssetCache::ResolveAgainstSearchRoot("assets/hero.png") == "assets/hero.png");
    ENJIN_EXPECT_TRUE(MeshAssetCache::ResolveAgainstSearchRoot("").empty());
}

ENJIN_TEST_MAIN()
