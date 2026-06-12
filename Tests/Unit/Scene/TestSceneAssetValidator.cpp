#include "EnjinTest.h"
#include "Enjin/Scene/SceneAssetValidator.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Platform/Paths.h"

#include <filesystem>
#include <fstream>

using namespace Enjin;
namespace fs = std::filesystem;

// Integration-style: needs real files to test existence checks.
// Each test creates its own temp root and removes it before returning.
namespace {

struct TempRoot {
    fs::path path;

    explicit TempRoot(const char* name) {
        path = fs::path(Platform::GetAppTempDirectory()) / name;
        fs::remove_all(path);
        fs::create_directories(path);
    }

    ~TempRoot() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    void CreateFile(const std::string& relative) {
        fs::path full = path / relative;
        fs::create_directories(full.parent_path());
        std::ofstream out(full);
        out << "x";
    }
};

} // namespace

ENJIN_TEST(AssetValidator, EmptyWorld_NoWarnings) {
    TempRoot root("enjin_test_av_empty");
    ECS::World world;

    auto warnings = Scene::FindMissingAssetPaths(&world, { root.path.string() });

    ENJIN_EXPECT_EQ(warnings.size(), (usize)0);
}

ENJIN_TEST(AssetValidator, MissingTexture_ProducesOneWarning) {
    // Arrange
    TempRoot root("enjin_test_av_missing");
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    auto& mat = world.AddComponent<ECS::MaterialComponent>(e);
    mat.baseColorTexturePath = "textures/does_not_exist.png";

    // Act
    auto warnings = Scene::FindMissingAssetPaths(&world, { root.path.string() });

    // Assert
    ENJIN_ASSERT_TRUE(warnings.size() == 1);
    ENJIN_EXPECT_TRUE(warnings[0].find("does_not_exist.png") != std::string::npos);
    ENJIN_EXPECT_TRUE(warnings[0].find("Material") != std::string::npos);
}

ENJIN_TEST(AssetValidator, ExistingAsset_NoWarning) {
    // Arrange
    TempRoot root("enjin_test_av_exists");
    root.CreateFile("textures/rock.png");
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    auto& mat = world.AddComponent<ECS::MaterialComponent>(e);
    mat.baseColorTexturePath = "textures/rock.png";

    // Act
    auto warnings = Scene::FindMissingAssetPaths(&world, { root.path.string() });

    // Assert
    ENJIN_EXPECT_EQ(warnings.size(), (usize)0);
}

ENJIN_TEST(AssetValidator, SecondRootFallback_Found) {
    // Arrange: asset exists only under the second root (scene-dir fallback)
    TempRoot rootA("enjin_test_av_root_a");
    TempRoot rootB("enjin_test_av_root_b");
    rootB.CreateFile("sounds/ping.wav");
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    auto& audio = world.AddComponent<ECS::AudioSourceComponent>(e);
    audio.clipPath = "sounds/ping.wav";

    // Act
    auto warnings = Scene::FindMissingAssetPaths(
        &world, { rootA.path.string(), rootB.path.string() });

    // Assert
    ENJIN_EXPECT_EQ(warnings.size(), (usize)0);
}

ENJIN_TEST(AssetValidator, EmptySearchRoots_NoWarnings) {
    // No roots to resolve against: validation is skipped, not spammed
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    auto& mat = world.AddComponent<ECS::MaterialComponent>(e);
    mat.baseColorTexturePath = "textures/missing.png";

    auto warnings = Scene::FindMissingAssetPaths(&world, {});

    ENJIN_EXPECT_EQ(warnings.size(), (usize)0);
}

ENJIN_TEST(AssetValidator, EmptyPathFields_Skipped) {
    // Components with default (empty) paths must not produce warnings
    TempRoot root("enjin_test_av_blank");
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::MaterialComponent>(e);
    world.AddComponent<ECS::AudioSourceComponent>(e);
    world.AddComponent<ECS::Sprite2DComponent>(e);

    auto warnings = Scene::FindMissingAssetPaths(&world, { root.path.string() });

    ENJIN_EXPECT_EQ(warnings.size(), (usize)0);
}

ENJIN_TEST_MAIN()
