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

    // --- Starter Templates (Stable — core ECS + renderer + physics) ---
    add("empty_canvas", "Empty Canvas", "Minimal starting point with just a camera and directional light. Perfect for starting from scratch.",
        "Starter", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.3f, 0.6f, 1.0f, 1250, 4.5f, 42, 8200,
        {"minimal", "beginner", "blank"});

    add("hello_sprite", "Hello Sprite", "Simple 2D scene with animated sprite, basic movement script, and sprite atlas setup.",
        "Starter", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Stable,
        0.4f, 0.8f, 0.4f, 980, 4.3f, 31, 15400,
        {"2d", "sprite", "beginner", "animation"});

    add("physics_playground", "Physics Playground", "Interactive physics sandbox with boxes, spheres, ramps, and configurable gravity zones.",
        "Starter", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Stable,
        0.9f, 0.5f, 0.2f, 720, 4.6f, 28, 22100,
        {"physics", "sandbox", "rigidbody", "collision"});

    // --- Genre Showcases ---
    add("neon_runner", "Neon Runner", "Synthwave-themed infinite runner with procedural obstacle generation, particle trails, and score system.",
        "Genre", "Enjin Team", "1.2.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        0.9f, 0.2f, 0.9f, 1560, 4.7f, 55, 45300,
        {"runner", "procedural", "synthwave", "scoring"});

    add("dungeon_crawler", "Dungeon Crawler", "Top-down roguelike dungeon with BSP room generation, enemy AI patrols, and loot pickups.",
        "Genre", "Enjin Team", "1.1.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Preview,
        0.6f, 0.3f, 0.1f, 890, 4.4f, 37, 52800,
        {"roguelike", "dungeon", "procedural", "ai", "topdown"});

    add("fps_arena", "FPS Arena", "Fast-paced first-person shooter arena with weapon pickups, respawn points, and basic AI bots.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Beta,
        0.8f, 0.1f, 0.1f, 1100, 4.5f, 40, 68200,
        {"fps", "shooter", "arena", "weapons", "ai"});

    add("cozy_farm", "Cozy Farm", "Relaxed farming game with day-night cycle, crop growth, inventory, and NPC dialogue.",
        "Genre", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        0.4f, 0.7f, 0.3f, 670, 4.8f, 25, 41500,
        {"farming", "simulation", "dialogue", "inventory"});

    // --- Systems Deep-Dives ---
    add("save_system_demo", "Save System Demo", "Complete save/load implementation with 3-tier persistence, auto-save, and meta-progression showcase.",
        "Systems", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        0.2f, 0.5f, 0.8f, 430, 4.2f, 18, 19800,
        {"save", "persistence", "autosave", "tutorial"});

    add("dialogue_rpg", "Dialogue & Quest RPG", "Branching dialogue system with quest tracking, localization, and cinematic camera showcase.",
        "Systems", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        0.7f, 0.5f, 0.9f, 510, 4.6f, 22, 35200,
        {"dialogue", "quest", "localization", "cinematic"});

    add("networking_lobby", "Multiplayer Lobby", "LAN multiplayer setup with lobby creation, entity sync, RPC calls, and client-side prediction.",
        "Systems", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Preview,
        0.1f, 0.6f, 0.7f, 340, 4.1f, 14, 28900,
        {"multiplayer", "networking", "lan", "lobby"});

    add("ui_toolkit", "UI Toolkit Showcase", "Comprehensive UI canvas demo with buttons, sliders, panels, theming, and focus navigation.",
        "Systems", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        0.5f, 0.7f, 0.9f, 620, 4.3f, 20, 24600,
        {"ui", "canvas", "widgets", "theming", "accessibility"});

    // --- Retro & Flash ---
    add("flash_tower_defense", "Flash Tower Defense", "Classic browser-game-era tower defense with SWF-style vector art, waves, and upgrade paths.",
        "Retro", "Enjin Team", "1.0.0", "CC0", "2D", TemplateQuality::Official, MaturityTier::Beta,
        0.9f, 0.7f, 0.1f, 780, 4.5f, 33, 38400,
        {"flash", "tower defense", "retro", "strategy"});

    add("ps1_horror", "PS1 Horror", "Low-poly horror with PS1-era vertex jitter, affine textures, CRT filter, and fixed camera angles.",
        "Retro", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Beta,
        0.1f, 0.15f, 0.1f, 920, 4.7f, 38, 55100,
        {"ps1", "horror", "retro", "crt", "lo-fi"});

    // --- Advanced ---
    add("ray_tracing_showcase", "Ray Tracing Showcase", "Real-time ray-traced scene with reflections, soft shadows, ambient occlusion, and SVGF denoising.",
        "Advanced", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Experimental,
        1.0f, 0.85f, 0.4f, 280, 4.9f, 12, 72400,
        {"raytracing", "reflections", "shadows", "advanced"});

    add("procedural_world", "Procedural World", "Fractal terrain generation with erosion, L-system vegetation, cellular automata caves, and WFC dungeons.",
        "Advanced", "Enjin Team", "1.0.0", "CC0", "3D", TemplateQuality::Official, MaturityTier::Preview,
        0.3f, 0.8f, 0.5f, 350, 4.6f, 15, 48700,
        {"procedural", "terrain", "lsystem", "wfc", "generation"});
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
