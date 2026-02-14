#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Enjin {
namespace Assets {

// Asset type for the library
enum class LibraryAssetType : std::uint8_t {
    Model3D,
    Sprite2D,
    Tileset,
    UIKit,
    VFXSprite,
    Background,
    Icon,
    Texture
};

// Art style classification
enum class ArtStyle : std::uint8_t {
    Realistic,
    StylizedLowPoly,
    PixelArt,
    HandDrawn,
    VectorFlat,
    Voxel,
    Anime
};

// Asset license (all must be commercially redistributable)
enum class AssetLicense : std::uint8_t {
    CC0,            // Public domain — no attribution required
    OFL,            // SIL Open Font License
    Apache2,        // Apache 2.0
    MIT             // MIT License
};

// A single entry in the asset library catalog
struct LibraryAssetEntry {
    std::string id;             // Unique identifier
    std::string name;           // Display name
    std::string creator;        // Original creator/source
    std::string description;    // Brief description
    LibraryAssetType type;
    ArtStyle style;
    AssetLicense license;
    std::string category;       // Sub-category (e.g., "Architecture", "Nature", "UI Buttons")
    std::string filename;       // Expected filename in assets directory
    std::string sourceUrl;      // Where to download
    std::string previewImage;   // Thumbnail filename (optional)
    std::uint32_t polyCount = 0;          // For 3D models
    std::uint32_t spriteWidth = 0;        // For 2D sprites
    std::uint32_t spriteHeight = 0;
    bool hasPBR = false;        // Has PBR textures
    bool hasLOD = false;        // Has LOD levels
    bool hasAnimation = false;  // Has animation data
    std::vector<std::string> tags; // Searchable tags
};

// Manages the bundled 2D and 3D asset library catalogs
class AssetLibrary {
public:
    AssetLibrary();

    // Initialize with base directories
    void Initialize(const std::string& assets3DDir, const std::string& assets2DDir);

    // Get full catalogs
    const std::vector<LibraryAssetEntry>& GetCatalog3D() const { return m_Catalog3D; }
    const std::vector<LibraryAssetEntry>& GetCatalog2D() const { return m_Catalog2D; }

    // Filter by type/style/category
    std::vector<const LibraryAssetEntry*> Filter(LibraryAssetType type, const std::string& category = "") const;
    std::vector<const LibraryAssetEntry*> FilterByStyle(ArtStyle style) const;

    // Search by name/tag (case-insensitive)
    std::vector<const LibraryAssetEntry*> Search(const std::string& query) const;

    // Check if an asset exists on disk
    bool IsInstalled(const std::string& assetId) const;

    // Get full path to an installed asset
    std::string GetAssetPath(const std::string& assetId) const;

    // Find by ID
    const LibraryAssetEntry* FindById(const std::string& assetId) const;

    // Get unique categories for a type
    std::vector<std::string> GetCategories(LibraryAssetType type) const;

    // Display name helpers
    static const char* GetTypeName(LibraryAssetType type);
    static const char* GetStyleName(ArtStyle style);
    static const char* GetLicenseName(AssetLicense license);

private:
    void BuildCatalog3D();
    void BuildCatalog2D();

    std::string m_Assets3DDir;
    std::string m_Assets2DDir;
    std::vector<LibraryAssetEntry> m_Catalog3D;
    std::vector<LibraryAssetEntry> m_Catalog2D;
    std::unordered_map<std::string, std::size_t> m_IdIndex3D;
    std::unordered_map<std::string, std::size_t> m_IdIndex2D;
};

} // namespace Assets
} // namespace Enjin
