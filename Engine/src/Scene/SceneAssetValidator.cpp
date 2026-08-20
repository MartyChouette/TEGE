#include "Enjin/Scene/SceneAssetValidator.h"

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/Platform/Paths.h"

#include <filesystem>

namespace Enjin {
namespace Scene {

namespace {

bool AssetExists(const std::string& path, const std::vector<std::string>& searchRoots) {
    if (std::filesystem::path(path).is_absolute()) {
        return std::filesystem::exists(path);
    }
    for (const auto& root : searchRoots) {
        std::string resolved = Platform::ResolveWithinRoot(root, path);
        if (!resolved.empty() && std::filesystem::exists(resolved)) {
            return true;
        }
    }
    return false;
}

} // namespace

std::vector<std::string> FindMissingAssetPaths(
    ECS::World* world,
    const std::vector<std::string>& searchRoots) {

    std::vector<std::string> warnings;
    if (!world || searchRoots.empty()) {
        return warnings;
    }

    auto check = [&](const std::string& path, ECS::Entity entity,
                     const char* assetType, const char* componentName) {
        if (path.empty()) {
            return;
        }
        if (!AssetExists(path, searchRoots)) {
            warnings.push_back("Missing " + std::string(assetType) + ": " + path +
                " (entity " + std::to_string(entity) + ", " + componentName + ")");
        }
    };

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::MaterialComponent>()) {
        const auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);
        if (!mat) continue;
        check(mat->baseColorTexturePath, entity, "texture", "Material");
        check(mat->normalTexturePath, entity, "texture", "Material");
        check(mat->metallicRoughnessTexturePath, entity, "texture", "Material");
        check(mat->emissiveTexturePath, entity, "texture", "Material");
        check(mat->specularTexturePath, entity, "texture", "Material");
        check(mat->matcapTexturePath, entity, "texture", "Material");
        check(mat->heightTexturePath, entity, "texture", "Material");
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::Sprite2DComponent>()) {
        const auto* sprite = world->GetComponent<ECS::Sprite2DComponent>(entity);
        if (!sprite) continue;
        check(sprite->texturePath, entity, "sprite texture", "Sprite2D");
        check(sprite->normalMapPath, entity, "sprite normal map", "Sprite2D");
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::AudioSourceComponent>()) {
        const auto* audio = world->GetComponent<ECS::AudioSourceComponent>(entity);
        if (!audio) continue;
        check(audio->clipPath, entity, "audio clip", "AudioSource");
        for (const auto& variation : audio->clipVariations) {
            check(variation, entity, "audio clip", "AudioSource");
        }
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ScriptComponent>()) {
        const auto* script = world->GetComponent<ECS::ScriptComponent>(entity);
        if (!script) continue;
        for (const auto& attachment : script->scripts) {
            check(attachment.scriptPath, entity, "script", "Script");
        }
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::TilemapComponent>()) {
        const auto* tilemap = world->GetComponent<ECS::TilemapComponent>(entity);
        if (!tilemap) continue;
        check(tilemap->tilesetPath, entity, "tileset texture", "Tilemap");
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::TreeVolumeComponent>()) {
        const auto* tree = world->GetComponent<ECS::TreeVolumeComponent>(entity);
        if (!tree) continue;
        check(tree->barkTexturePath, entity, "tree texture", "TreeVolume");
        check(tree->canopyTexturePath, entity, "tree texture", "TreeVolume");
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ShrubVolumeComponent>()) {
        const auto* shrub = world->GetComponent<ECS::ShrubVolumeComponent>(entity);
        if (!shrub) continue;
        check(shrub->customAssetPath, entity, "shrub asset", "ShrubVolume");
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::GrassVolumeComponent>()) {
        const auto* grass = world->GetComponent<ECS::GrassVolumeComponent>(entity);
        if (!grass) continue;
        check(grass->customAssetPath, entity, "grass asset", "GrassVolume");
    }

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::WeatherZoneComponent>()) {
        const auto* zone = world->GetComponent<ECS::WeatherZoneComponent>(entity);
        if (!zone) continue;
        check(zone->rainTexturePath, entity, "weather sprite", "WeatherZone");
        check(zone->snowTexturePath, entity, "weather sprite", "WeatherZone");
    }

    return warnings;
}

} // namespace Scene
} // namespace Enjin
