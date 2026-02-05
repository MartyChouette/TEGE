#include "Enjin/Assets/AssetMetadata.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>

using json = nlohmann::json;

namespace Enjin {
namespace Assets {

std::string AssetMetadata::GetMetadataPath(const std::string& assetPath) {
    return assetPath + ".enjinasset";
}

bool AssetMetadata::Exists(const std::string& assetPath) {
    return std::filesystem::exists(GetMetadataPath(assetPath));
}

void AssetMetadata::PopulateFromResult(const ImportResult& result, const std::string& filePath,
                                       const ImportOptions& options) {
    originalPath = filePath;

    std::filesystem::path p(filePath);
    format = p.extension().string();

    // Read file size and modification time
    try {
        if (std::filesystem::exists(p)) {
            fileSize = std::filesystem::file_size(p);
            auto ftime = std::filesystem::last_write_time(p);
            auto sctp = std::chrono::time_point_cast<std::chrono::seconds>(
                std::chrono::clock_cast<std::chrono::system_clock>(ftime));
            lastModified = std::to_string(sctp.time_since_epoch().count());
        }
    } catch (...) {
        // Silently ignore filesystem errors for metadata
    }

    // Current timestamp
    auto now = std::chrono::system_clock::now();
    auto nowSec = std::chrono::time_point_cast<std::chrono::seconds>(now);
    importTimestamp = std::to_string(nowSec.time_since_epoch().count());

    importOptions = options;

    meshCount = result.meshCount;
    materialCount = result.materialCount;
    animationCount = result.animationCount;
    totalVertexCount = result.totalVertexCount;
    totalIndexCount = result.totalIndexCount;
    entityNames = result.entityNames;
    texturePathsResolved = result.texturePathsResolved;
    texturePathsMissing = result.texturePathsMissing;
}

bool AssetMetadata::Save(const std::string& assetPath) const {
    std::string metaPath = GetMetadataPath(assetPath);

    json j;
    j["enjinAssetVersion"] = 1;
    j["originalPath"] = originalPath;
    j["format"] = format;
    j["fileSize"] = fileSize;
    j["lastModified"] = lastModified;
    j["importTimestamp"] = importTimestamp;

    // Import options
    json opts;
    opts["scale"] = importOptions.scale;
    opts["importMaterials"] = importOptions.importMaterials;
    opts["importLights"] = importOptions.importLights;
    opts["importAnimations"] = importOptions.importAnimations;
    opts["generateColliders"] = importOptions.generateColliders;
    j["importOptions"] = opts;

    // Stats
    json stats;
    stats["meshCount"] = meshCount;
    stats["materialCount"] = materialCount;
    stats["animationCount"] = animationCount;
    stats["totalVertexCount"] = totalVertexCount;
    stats["totalIndexCount"] = totalIndexCount;
    j["stats"] = stats;

    j["entityNames"] = entityNames;
    j["texturePathsResolved"] = texturePathsResolved;
    j["texturePathsMissing"] = texturePathsMissing;

    try {
        std::ofstream file(metaPath);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Asset, "Failed to save asset metadata: %s", metaPath.c_str());
            return false;
        }
        file << j.dump(2);
        ENJIN_LOG_INFO(Asset, "Saved asset metadata: %s", metaPath.c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to write asset metadata: %s", e.what());
        return false;
    }
}

bool AssetMetadata::Load(const std::string& assetPath) {
    std::string metaPath = GetMetadataPath(assetPath);

    try {
        std::ifstream file(metaPath);
        if (!file.is_open()) return false;

        json j = json::parse(file);

        if (j.contains("originalPath")) originalPath = j["originalPath"].get<std::string>();
        if (j.contains("format")) format = j["format"].get<std::string>();
        if (j.contains("fileSize")) fileSize = j["fileSize"].get<u64>();
        if (j.contains("lastModified")) lastModified = j["lastModified"].get<std::string>();
        if (j.contains("importTimestamp")) importTimestamp = j["importTimestamp"].get<std::string>();

        // Import options
        if (j.contains("importOptions")) {
            const auto& opts = j["importOptions"];
            if (opts.contains("scale")) importOptions.scale = opts["scale"].get<f32>();
            if (opts.contains("importMaterials")) importOptions.importMaterials = opts["importMaterials"].get<bool>();
            if (opts.contains("importLights")) importOptions.importLights = opts["importLights"].get<bool>();
            if (opts.contains("importAnimations")) importOptions.importAnimations = opts["importAnimations"].get<bool>();
            if (opts.contains("generateColliders")) importOptions.generateColliders = opts["generateColliders"].get<bool>();
        }

        // Stats
        if (j.contains("stats")) {
            const auto& stats = j["stats"];
            if (stats.contains("meshCount")) meshCount = stats["meshCount"].get<u32>();
            if (stats.contains("materialCount")) materialCount = stats["materialCount"].get<u32>();
            if (stats.contains("animationCount")) animationCount = stats["animationCount"].get<u32>();
            if (stats.contains("totalVertexCount")) totalVertexCount = stats["totalVertexCount"].get<u32>();
            if (stats.contains("totalIndexCount")) totalIndexCount = stats["totalIndexCount"].get<u32>();
        }

        if (j.contains("entityNames")) entityNames = j["entityNames"].get<std::vector<std::string>>();
        if (j.contains("texturePathsResolved")) texturePathsResolved = j["texturePathsResolved"].get<std::vector<std::string>>();
        if (j.contains("texturePathsMissing")) texturePathsMissing = j["texturePathsMissing"].get<std::vector<std::string>>();

        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Asset, "Failed to load asset metadata '%s': %s", metaPath.c_str(), e.what());
        return false;
    }
}

} // namespace Assets
} // namespace Enjin
