#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace Editor {

// Categories for marketplace templates
enum class MarketplaceCategory : std::uint8_t {
    All = 0,
    Starter,
    Genre,
    Systems,
    Retro,
    Advanced
};

// Quality tier for marketplace entries
enum class TemplateQuality : std::uint8_t {
    Official = 0,   // Engine-shipped, high quality
    Community,       // Community-contributed
    Experimental     // Work-in-progress / beta
};

// Sort options for marketplace browsing
enum class MarketplaceSortBy : std::uint8_t {
    Name = 0,
    Rating,
    Downloads,
    Recent
};

// A single template entry in the marketplace catalog
struct MarketplaceEntry {
    std::string id;
    std::string name;
    std::string description;
    std::string category;       // "Starter", "Genre", "Systems", "Retro", "Advanced"
    std::string author;
    std::string version;        // e.g. "1.0.0"
    std::string license;        // "CC0", "MIT", etc.
    std::string projectMode;    // "2D", "3D", "Mixed"
    std::vector<std::string> tags;
    f32 accentColor[4] = { 0.3f, 0.6f, 1.0f, 1.0f };
    u32 downloadCount = 0;
    f32 rating = 0.0f;          // 0-5
    u32 ratingCount = 0;
    usize fileSizeBytes = 0;
    TemplateQuality quality = TemplateQuality::Official;
};

// Template Marketplace — browse, search, install, and manage templates
class ENJIN_API TemplateMarketplace {
public:
    void Initialize(const std::string& templatesDir);

    // Catalog access
    const std::vector<MarketplaceEntry>& GetCatalog() const { return m_Catalog; }

    // Search and filter
    std::vector<const MarketplaceEntry*> Search(const std::string& query) const;
    std::vector<const MarketplaceEntry*> FilterByCategory(const std::string& category) const;
    std::vector<const MarketplaceEntry*> FilterAndSearch(const std::string& query,
                                                         const std::string& category) const;
    const MarketplaceEntry* FindById(const std::string& id) const;

    // Installation
    bool Install(const std::string& templateId);
    bool Uninstall(const std::string& templateId);
    bool IsInstalled(const std::string& templateId) const;

    // Helpers
    static const char* GetCategoryName(MarketplaceCategory cat);
    static const char* GetQualityName(TemplateQuality quality);

    // Panel open state (no EditorPanel bits left)
    bool IsOpen() const { return m_Open; }
    void SetOpen(bool open) { m_Open = open; }

private:
    void BuildCatalog();
    void ScanInstalled();

    std::vector<MarketplaceEntry> m_Catalog;
    std::unordered_map<std::string, std::size_t> m_IdIndex;
    std::unordered_set<std::string> m_InstalledIds;
    std::string m_TemplatesDir;
    bool m_Open = false;
};

} // namespace Editor
} // namespace Enjin
