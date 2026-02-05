#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Assets/SceneImporter.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

// Metadata sidecar for imported assets (.enjinasset files)
// Stores import settings and statistics so re-imports can use the same options
struct ENJIN_API AssetMetadata {
    std::string originalPath;
    std::string format;          // e.g. ".gltf", ".fbx"
    u64 fileSize = 0;
    std::string lastModified;    // Epoch seconds as string
    std::string importTimestamp;

    ImportOptions importOptions;

    u32 meshCount = 0;
    u32 materialCount = 0;
    u32 animationCount = 0;
    u32 totalVertexCount = 0;
    u32 totalIndexCount = 0;
    std::vector<std::string> entityNames;
    std::vector<std::string> texturePathsResolved;
    std::vector<std::string> texturePathsMissing;

    // Save metadata as JSON sidecar file next to the asset
    bool Save(const std::string& assetPath) const;

    // Load metadata from the JSON sidecar file
    bool Load(const std::string& assetPath);

    // Get the .enjinasset sidecar path for a given asset path
    static std::string GetMetadataPath(const std::string& assetPath);

    // Check if a .enjinasset file exists for the given asset
    static bool Exists(const std::string& assetPath);

    // Populate metadata fields from an ImportResult and file info
    void PopulateFromResult(const ImportResult& result, const std::string& filePath,
                           const ImportOptions& options);
};

} // namespace Assets
} // namespace Enjin
