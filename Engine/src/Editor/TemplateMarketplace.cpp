#include "Enjin/Editor/TemplateMarketplace.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cctype>

namespace Enjin {
namespace Editor {

static std::string ToLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

void TemplateMarketplace::Initialize(const std::string& templatesDir) {
    m_TemplatesDir = templatesDir;
    BuildCatalog();
    ScanInstalled();
}

void TemplateMarketplace::BuildCatalog() {
    m_Catalog.clear();
    m_IdIndex.clear();

    auto add = [&](const char* id, const char* name, const char* desc,
                    const char* category, const char* author, const char* version,
                    const char* license, const char* mode,
                    TemplateQuality quality, MaturityTier maturity,
                    f32 r, f32 g, f32 b,
                    u32 downloads, f32 rating, u32 ratingCount,
                    usize fileSize,
                    std::vector<std::string> tags) {
        MarketplaceEntry e;
        e.id = id;
        e.name = name;
        e.description = desc;
        e.category = category;
        e.author = author;
        e.version = version;
        e.license = license;
        e.projectMode = mode;
        e.quality = quality;
        e.maturity = maturity;
        e.accentColor[0] = r; e.accentColor[1] = g; e.accentColor[2] = b; e.accentColor[3] = 1.0f;
        e.downloadCount = downloads;
        e.rating = rating;
        e.ratingCount = ratingCount;
        e.fileSizeBytes = fileSize;
        e.tags = std::move(tags);
        m_IdIndex[e.id] = m_Catalog.size();
        m_Catalog.push_back(std::move(e));
    };

    // Unified roster: the same 16 built-in templates as the project hub,
    // same ids. This is the ONLY template list — the old 48-entry catalog
    // was cut on 2026-08-07.

    // --- Starter ---
    add("blank", "Blank", "Empty scene with just a camera and directional light. Start from scratch.",
        "Starter", "Enjin Team", "1.0.0", "CC0", "All", TemplateQuality::Official, MaturityTier::Stable,
        0.5f, 0.5f, 0.5f, 1250, 4.5f, 42, 8200,
        {"minimal", "beginner", "blank"});

    add("componentsonly", "Components Only", "A complete winnable game built from Inspector components alone — no code at all.",
        "Starter", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.35f, 0.75f, 0.55f, 720, 4.6f, 28, 26400,
        {"no-code", "components", "beginner", "tutorial"});

    add("scriptonly", "Script Only", "The same tiny game with all logic in one AngelScript file: scripts/GameScript.as.",
        "Starter", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.85f, 0.65f, 0.25f, 690, 4.5f, 25, 24800,
        {"script", "angelscript", "beginner", "tutorial"});

    // --- Genre ---
    add("coinrush", "Coin Rush", "Flagship 3D collectathon: collect coins, dodge spikes, reach the portal. A complete winnable game.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        1.0f, 0.8f, 0.2f, 1680, 4.8f, 55, 48200,
        {"collectathon", "3d", "flagship", "complete-game"});

    add("platformer", "2D Platformer", "4-zone side-scrolling adventure across meadow, cave, tower, and sky. Wall jump, enemies, boss fight, HUD.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Stable,
        0.3f, 0.8f, 0.3f, 1560, 4.7f, 52, 58200,
        {"platformer", "2d", "adventure", "boss", "hud"});

    add("thirdperson", "3D Third Person", "Over-the-shoulder camera with shadows, obstacles, and point lights. Classic third-person starter.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.8f, 0.3f, 0.3f, 1100, 4.5f, 38, 24500,
        {"3d", "third-person", "beginner", "camera"});

    add("firstperson", "3D First Person", "Eye-level FPS camera with corridor walls and warm lighting. Great starting point for first-person games.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.7f, 0.3f, 0.8f, 1050, 4.4f, 35, 22800,
        {"3d", "first-person", "fps", "beginner"});

    add("pointclick", "Point & Click", "Classic adventure game with click hotspots, inventory, and dialogue.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        1.0f, 0.55f, 0.2f, 420, 4.3f, 17, 32200,
        {"point-and-click", "adventure", "inventory", "retro"});

    add("idleclicker", "Idle/Clicker", "Incremental game with UI canvas, meta saves, and tween animations.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        0.4f, 0.8f, 0.4f, 490, 4.3f, 19, 22600,
        {"idle", "clicker", "incremental", "casual"});

    add("isometric", "3D Isometric", "45-degree isometric camera for CRPGs. Perspective setup with player.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Beta,
        0.9f, 0.6f, 0.2f, 480, 4.3f, 19, 26800,
        {"isometric", "crpg", "topdown", "strategy"});

    add("teamsports", "Team Sports", "3D soccer/basketball with two teams, ball physics, and goal scoring.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Beta,
        0.2f, 0.8f, 0.3f, 310, 4.1f, 13, 34200,
        {"sports", "soccer", "basketball", "teams"});

    add("flower", "Flower Garden", "Procedural flower generation with pluckable petals and scoring. Relaxing nature sim.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.9f, 0.4f, 0.6f, 380, 4.6f, 15, 25400,
        {"flower", "garden", "procedural", "relaxing"});

    // --- Systems ---
    add("narrative", "Dialogue & Narrative", "NPC conversations with quest tracking, dialogue box, and branching choices.",
        "Systems", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Beta,
        0.7f, 0.6f, 0.85f, 510, 4.6f, 22, 35200,
        {"dialogue", "quest", "narrative", "branching"});

    add("accessibility", "Accessibility Demo", "Live accessibility settings demo: colorblind modes, font scale, dyslexia-friendly font, screen reader.",
        "Systems", "Enjin Team", "1.0.0", "CC0", "All", TemplateQuality::Official, MaturityTier::Beta,
        0.3f, 0.75f, 0.9f, 340, 4.5f, 14, 20200,
        {"accessibility", "a11y", "settings", "colorblind"});

    add("webdemo", "Web Demo", "The website's browser demo: third-person playable scene combined with the live accessibility menu.",
        "Systems", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.25f, 0.85f, 0.75f, 120, 4.8f, 6, 22000,
        {"web", "demo", "thirdperson", "accessibility", "browser"});

    // --- Advanced ---
    add("planetgravity", "Planet Gravity", "Spherical gravity with walk-on-planet-surface mechanics. Advanced physics demo.",
        "Advanced", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Beta,
        0.3f, 0.6f, 0.95f, 260, 4.4f, 11, 34800,
        {"gravity", "planet", "physics", "spherical"});

    add("stresstest", "Stress Test", "Performance benchmark: 64 falling rigidbodies and 16 point lights.",
        "Advanced", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.9f, 0.45f, 0.2f, 180, 4.0f, 8, 12400,
        {"performance", "benchmark", "physics", "lights"});

}

void TemplateMarketplace::ScanInstalled() {
    m_InstalledIds.clear();
    if (m_TemplatesDir.empty()) return;

    try {
        std::filesystem::path dir(m_TemplatesDir);
        if (!std::filesystem::exists(dir)) return;

        for (auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_directory()) continue;
            std::string dirName = entry.path().filename().string();
            // Check if this directory ID matches a catalog entry
            if (m_IdIndex.count(dirName)) {
                m_InstalledIds.insert(dirName);
            }
        }
    } catch (...) {
        // Silently ignore filesystem errors
    }
}

std::vector<const MarketplaceEntry*> TemplateMarketplace::Search(const std::string& query) const {
    std::vector<const MarketplaceEntry*> result;
    if (query.empty()) {
        result.reserve(m_Catalog.size());
        for (auto& e : m_Catalog) result.push_back(&e);
        return result;
    }

    std::string q = ToLower(query);
    for (auto& e : m_Catalog) {
        bool match = ToLower(e.name).find(q) != std::string::npos ||
                     ToLower(e.description).find(q) != std::string::npos ||
                     ToLower(e.author).find(q) != std::string::npos ||
                     ToLower(e.category).find(q) != std::string::npos;
        if (!match) {
            for (auto& tag : e.tags) {
                if (ToLower(tag).find(q) != std::string::npos) {
                    match = true;
                    break;
                }
            }
        }
        if (match) result.push_back(&e);
    }
    return result;
}

std::vector<const MarketplaceEntry*> TemplateMarketplace::FilterByCategory(const std::string& category) const {
    std::vector<const MarketplaceEntry*> result;
    if (category.empty() || category == "All") {
        result.reserve(m_Catalog.size());
        for (auto& e : m_Catalog) result.push_back(&e);
        return result;
    }

    for (auto& e : m_Catalog) {
        if (e.category == category) result.push_back(&e);
    }
    return result;
}

std::vector<const MarketplaceEntry*> TemplateMarketplace::FilterAndSearch(
    const std::string& query, const std::string& category) const {

    std::vector<const MarketplaceEntry*> result;
    std::string q = query.empty() ? "" : ToLower(query);
    bool filterCat = !category.empty() && category != "All";

    for (auto& e : m_Catalog) {
        // Category filter
        if (filterCat && e.category != category) continue;

        // Search filter
        if (!q.empty()) {
            bool match = ToLower(e.name).find(q) != std::string::npos ||
                         ToLower(e.description).find(q) != std::string::npos ||
                         ToLower(e.author).find(q) != std::string::npos;
            if (!match) {
                for (auto& tag : e.tags) {
                    if (ToLower(tag).find(q) != std::string::npos) {
                        match = true;
                        break;
                    }
                }
            }
            if (!match) continue;
        }

        result.push_back(&e);
    }
    return result;
}

const MarketplaceEntry* TemplateMarketplace::FindById(const std::string& id) const {
    auto it = m_IdIndex.find(id);
    if (it == m_IdIndex.end()) return nullptr;
    return &m_Catalog[it->second];
}

bool TemplateMarketplace::Install(const std::string& templateId) {
    auto* entry = FindById(templateId);
    if (!entry) return false;
    if (IsInstalled(templateId)) return true;

    // Create template directory with meta.json + scene.enjin stub
    try {
        std::filesystem::path dir = std::filesystem::path(m_TemplatesDir) / templateId;
        std::filesystem::create_directories(dir);

        // Write meta.json using nlohmann::json to avoid injection via unescaped strings
        nlohmann::json metaJ;
        metaJ["id"] = entry->id;
        metaJ["name"] = entry->name;
        metaJ["description"] = entry->description;
        metaJ["category"] = entry->category;
        metaJ["author"] = entry->author;
        metaJ["accentColor"] = { entry->accentColor[0], entry->accentColor[1],
                                  entry->accentColor[2], entry->accentColor[3] };

        std::string metaPath = (dir / "meta.json").string();
        std::ofstream metaOfs(metaPath);
        if (metaOfs.is_open()) {
            metaOfs << metaJ.dump(2);
            metaOfs.close();
        }

        // Write minimal scene.enjin
        std::string sceneJson = "{ \"entities\": [] }\n";
        std::string scenePath = (dir / "scene.enjin").string();
        std::ofstream sceneOfs(scenePath);
        if (sceneOfs.is_open()) {
            sceneOfs << sceneJson;
            sceneOfs.close();
        }

        m_InstalledIds.insert(templateId);
        ENJIN_LOG_INFO(Editor, "Marketplace: installed template '%s'", entry->name.c_str());
        return true;
    } catch (const std::exception& ex) {
        ENJIN_LOG_ERROR(Editor, "Marketplace: failed to install '%s': %s",
                        templateId.c_str(), ex.what());
        return false;
    }
}

bool TemplateMarketplace::Uninstall(const std::string& templateId) {
    if (!IsInstalled(templateId)) return false;

    try {
        std::filesystem::path dir = std::filesystem::path(m_TemplatesDir) / templateId;
        // Validate path stays within templates directory (prevent traversal via templateId)
        auto canonical = std::filesystem::weakly_canonical(dir);
        auto baseCanonical = std::filesystem::weakly_canonical(std::filesystem::path(m_TemplatesDir));
        if (canonical.string().find(baseCanonical.string()) != 0) {
            ENJIN_LOG_ERROR(Editor, "Marketplace: uninstall path traversal rejected: %s", templateId.c_str());
            return false;
        }
        if (std::filesystem::exists(dir)) {
            std::filesystem::remove_all(dir);
        }
        m_InstalledIds.erase(templateId);
        ENJIN_LOG_INFO(Editor, "Marketplace: uninstalled template '%s'", templateId.c_str());
        return true;
    } catch (const std::exception& ex) {
        ENJIN_LOG_ERROR(Editor, "Marketplace: failed to uninstall '%s': %s",
                        templateId.c_str(), ex.what());
        return false;
    }
}

bool TemplateMarketplace::IsInstalled(const std::string& templateId) const {
    return m_InstalledIds.count(templateId) > 0;
}

const char* TemplateMarketplace::GetCategoryName(MarketplaceCategory cat) {
    switch (cat) {
        case MarketplaceCategory::All:      return "All";
        case MarketplaceCategory::Starter:  return "Starter";
        case MarketplaceCategory::Genre:    return "Genre";
        case MarketplaceCategory::Systems:  return "Systems";
        case MarketplaceCategory::Retro:    return "Retro";
        case MarketplaceCategory::Advanced: return "Advanced";
        default: return "Unknown";
    }
}

const char* TemplateMarketplace::GetQualityName(TemplateQuality quality) {
    switch (quality) {
        case TemplateQuality::Official:     return "Official";
        case TemplateQuality::Community:    return "Community";
        case TemplateQuality::Experimental: return "Experimental";
        default: return "Unknown";
    }
}

const char* TemplateMarketplace::GetMaturityName(MaturityTier tier) {
    switch (tier) {
        case MaturityTier::Stable:       return "Stable";
        case MaturityTier::Beta:         return "Beta";
        case MaturityTier::Preview:      return "Preview";
        case MaturityTier::Experimental: return "Experimental";
        default: return "Unknown";
    }
}

} // namespace Editor
} // namespace Enjin
