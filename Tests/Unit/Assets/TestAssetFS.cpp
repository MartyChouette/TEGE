// Reading a game's files through the package when one is mounted.
//
// The desktop player read only its manifest, scenes and scripts from the pak;
// every other loader read the disk, so anything the build did not copy loose
// was missing from the game (EP-18). AssetFS is the one read path both cases
// go through. These pin how a path maps to a package entry, that the package
// wins over the disk, and that the disk is still there for what it lacks.
#include "EnjinTest.h"
#include "Enjin/Platform/AssetFS.h"
#include "Enjin/Assets/Prefab.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>

using namespace Enjin;
namespace AssetFS = Enjin::Platform::AssetFS;

namespace {

// A fake package: virtual path -> contents
struct FakePackage {
    std::map<std::string, std::string> files;
    void MountAt(const std::string& root) {
        AssetFS::Mount(root,
            [this](const std::string& v, std::vector<u8>& out) {
                auto it = files.find(v);
                if (it == files.end()) return false;
                out.assign(it->second.begin(), it->second.end());
                return true;
            },
            [this](const std::string& v) { return files.count(v) != 0; });
    }
};

struct TempDir {
    std::filesystem::path root;
    explicit TempDir(const std::string& tag) {
        root = std::filesystem::temp_directory_path() / ("enjin_assetfs_" + tag);
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "assets");
    }
    ~TempDir() {
        AssetFS::Unmount();
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }
};

std::string Text(const std::vector<u8>& b) { return std::string(b.begin(), b.end()); }

} // namespace

ENJIN_TEST(AssetFS, test_assetfs_absolute_and_relative_paths_read_the_same_entry) {
    // Arrange
    TempDir dir("paths");
    FakePackage pak;
    pak.files["assets/wood.png"] = "packed";
    pak.MountAt(dir.root.string());
    const std::string abs = (dir.root / "assets" / "wood.png").string();

    // Act
    std::vector<u8> a, b;
    const bool readAbs = AssetFS::ReadBytes(abs, a);
    const bool readRel = AssetFS::ReadBytes("assets/wood.png", b);

    // Assert
    ENJIN_EXPECT_EQ(AssetFS::ToVirtualPath(abs), std::string("assets/wood.png"));
    ENJIN_EXPECT_EQ(AssetFS::ToVirtualPath("assets/wood.png"), std::string("assets/wood.png"));
    ENJIN_EXPECT_EQ(AssetFS::ToVirtualPath("./assets/../assets/wood.png"), std::string("assets/wood.png"));

    ENJIN_ASSERT_TRUE(readAbs);
    ENJIN_ASSERT_TRUE(readRel);
    ENJIN_EXPECT_EQ(Text(a), std::string("packed"));
    ENJIN_EXPECT_EQ(Text(b), std::string("packed"));
}

ENJIN_TEST(AssetFS, test_assetfs_path_outside_root_maps_to_no_entry) {
    // Arrange
    TempDir dir("outside");
    FakePackage pak;
    pak.MountAt((dir.root / "game").string());

    // Act + Assert: a sibling whose name starts with the root's is not inside it
    ENJIN_EXPECT_EQ(AssetFS::ToVirtualPath((dir.root / "game2" / "a.png").string()), std::string());
    ENJIN_EXPECT_EQ(AssetFS::ToVirtualPath("../a.png"), std::string());
    ENJIN_EXPECT_EQ(AssetFS::ToVirtualPath((dir.root / "game").string()), std::string());
}

ENJIN_TEST(AssetFS, test_assetfs_package_entry_wins_and_disk_fills_gaps) {
    // Arrange
    TempDir dir("fallback");
    std::ofstream(dir.root / "assets" / "both.txt") << "loose";
    std::ofstream(dir.root / "assets" / "diskonly.txt") << "disk";
    FakePackage pak;
    pak.files["assets/both.txt"] = "packed";
    pak.files["assets/pakonly.txt"] = "pak";
    pak.MountAt(dir.root.string());

    // Act
    std::string both, pakOnly, diskOnly;
    const bool okBoth = AssetFS::ReadText((dir.root / "assets" / "both.txt").string(), both);
    const bool okPak = AssetFS::ReadText((dir.root / "assets" / "pakonly.txt").string(), pakOnly);
    const bool okDisk = AssetFS::ReadText((dir.root / "assets" / "diskonly.txt").string(), diskOnly);

    // Assert
    ENJIN_ASSERT_TRUE(okBoth && okPak && okDisk);
    ENJIN_EXPECT_EQ(both, std::string("packed"));
    ENJIN_EXPECT_EQ(pakOnly, std::string("pak"));
    ENJIN_EXPECT_EQ(diskOnly, std::string("disk"));

    ENJIN_EXPECT_TRUE(AssetFS::Exists((dir.root / "assets" / "pakonly.txt").string()));
    ENJIN_EXPECT_TRUE(AssetFS::Exists((dir.root / "assets" / "diskonly.txt").string()));
    ENJIN_EXPECT_FALSE(AssetFS::Exists((dir.root / "assets" / "neither.txt").string()));
}

ENJIN_TEST(AssetFS, test_assetfs_unmounted_reads_disk_only) {
    // Arrange
    TempDir dir("unmounted");
    AssetFS::Unmount();
    std::ofstream(dir.root / "assets" / "a.txt") << "disk";

    // Act
    std::string s, missing;
    const bool ok = AssetFS::ReadText((dir.root / "assets" / "a.txt").string(), s);
    const bool okMissing = AssetFS::ReadText("assets/does_not_exist.txt", missing);

    // Assert
    ENJIN_EXPECT_FALSE(AssetFS::IsMounted());
    ENJIN_EXPECT_EQ(AssetFS::ToVirtualPath("assets/a.txt"), std::string());
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_EQ(s, std::string("disk"));
    ENJIN_EXPECT_FALSE(okMissing);
}

// The case that was broken in a shipped game: GeneratedGeometry keeps its
// prefabs in prefabs/, outside assets/, so no loose copy carried them and a
// scatter placed nothing. A prefab that exists only in the package must load.
ENJIN_TEST(AssetFS, test_assetfs_prefab_only_in_package_loads) {
    // Arrange
    TempDir dir("prefab");
    FakePackage pak;
    pak.files["prefabs/Boulder.prefab"] = R"({"name":"Boulder","entities":[]})";
    pak.MountAt(dir.root.string());
    auto& prefabs = Enjin::Assets::PrefabManager::Get();
    prefabs.ClearCache();
    prefabs.SetAssetRoot(dir.root.string());

    // Act
    auto prefab = prefabs.LoadPrefab("prefabs/Boulder.prefab");

    // Assert
    ENJIN_ASSERT_TRUE(prefab != nullptr);
    ENJIN_EXPECT_FALSE(std::filesystem::exists(dir.root / "prefabs" / "Boulder.prefab"));

    prefabs.ClearCache();
    prefabs.SetAssetRoot("");
}

ENJIN_TEST_MAIN()
