#include "Enjin/Assets/AssetLibrary.h"
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <cstdint>

namespace Enjin {
namespace Assets {

static std::string ToLower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

AssetLibrary::AssetLibrary() {
    BuildCatalog3D();
    BuildCatalog2D();
}

void AssetLibrary::Initialize(const std::string& assets3DDir, const std::string& assets2DDir) {
    m_Assets3DDir = assets3DDir;
    m_Assets2DDir = assets2DDir;
}

std::vector<const LibraryAssetEntry*> AssetLibrary::Filter(LibraryAssetType type, const std::string& category) const {
    std::vector<const LibraryAssetEntry*> result;
    auto check = [&](const std::vector<LibraryAssetEntry>& catalog) {
        for (auto& e : catalog) {
            if (e.type == type && (category.empty() || e.category == category))
                result.push_back(&e);
        }
    };
    check(m_Catalog3D);
    check(m_Catalog2D);
    return result;
}

std::vector<const LibraryAssetEntry*> AssetLibrary::FilterByStyle(ArtStyle style) const {
    std::vector<const LibraryAssetEntry*> result;
    for (auto& e : m_Catalog3D) if (e.style == style) result.push_back(&e);
    for (auto& e : m_Catalog2D) if (e.style == style) result.push_back(&e);
    return result;
}

std::vector<const LibraryAssetEntry*> AssetLibrary::Search(const std::string& query) const {
    std::vector<const LibraryAssetEntry*> result;
    std::string q = ToLower(query);
    auto check = [&](const std::vector<LibraryAssetEntry>& catalog) {
        for (auto& e : catalog) {
            bool match = ToLower(e.name).find(q) != std::string::npos ||
                         ToLower(e.description).find(q) != std::string::npos ||
                         ToLower(e.category).find(q) != std::string::npos;
            if (!match) {
                for (auto& tag : e.tags) {
                    if (ToLower(tag).find(q) != std::string::npos) { match = true; break; }
                }
            }
            if (match) result.push_back(&e);
        }
    };
    check(m_Catalog3D);
    check(m_Catalog2D);
    return result;
}

bool AssetLibrary::IsInstalled(const std::string& assetId) const {
    auto* entry = FindById(assetId);
    if (!entry) return false;
    bool is3D = (entry->type == LibraryAssetType::Model3D);
    std::string dir = is3D ? m_Assets3DDir : m_Assets2DDir;
    return std::filesystem::exists(dir + "/" + entry->filename);
}

std::string AssetLibrary::GetAssetPath(const std::string& assetId) const {
    auto* entry = FindById(assetId);
    if (!entry) return "";
    bool is3D = (entry->type == LibraryAssetType::Model3D);
    return (is3D ? m_Assets3DDir : m_Assets2DDir) + "/" + entry->filename;
}

const LibraryAssetEntry* AssetLibrary::FindById(const std::string& assetId) const {
    auto it = m_IdIndex3D.find(assetId);
    if (it != m_IdIndex3D.end()) return &m_Catalog3D[it->second];
    it = m_IdIndex2D.find(assetId);
    if (it != m_IdIndex2D.end()) return &m_Catalog2D[it->second];
    return nullptr;
}

std::vector<std::string> AssetLibrary::GetCategories(LibraryAssetType type) const {
    std::vector<std::string> cats;
    auto check = [&](const std::vector<LibraryAssetEntry>& catalog) {
        for (auto& e : catalog) {
            if (e.type == type) {
                if (std::find(cats.begin(), cats.end(), e.category) == cats.end())
                    cats.push_back(e.category);
            }
        }
    };
    check(m_Catalog3D);
    check(m_Catalog2D);
    std::sort(cats.begin(), cats.end());
    return cats;
}

const char* AssetLibrary::GetTypeName(LibraryAssetType type) {
    switch (type) {
        case LibraryAssetType::Model3D:    return "3D Model";
        case LibraryAssetType::Sprite2D:   return "Sprite";
        case LibraryAssetType::Tileset:    return "Tileset";
        case LibraryAssetType::UIKit:      return "UI Kit";
        case LibraryAssetType::VFXSprite:  return "VFX Sprite";
        case LibraryAssetType::Background: return "Background";
        case LibraryAssetType::Icon:       return "Icon";
        case LibraryAssetType::Texture:    return "Texture";
    }
    return "Unknown";
}

const char* AssetLibrary::GetStyleName(ArtStyle style) {
    switch (style) {
        case ArtStyle::Realistic:       return "Realistic";
        case ArtStyle::StylizedLowPoly: return "Stylized Low-Poly";
        case ArtStyle::PixelArt:        return "Pixel Art";
        case ArtStyle::HandDrawn:       return "Hand-Drawn";
        case ArtStyle::VectorFlat:      return "Vector/Flat";
        case ArtStyle::Voxel:           return "Voxel";
        case ArtStyle::Anime:           return "Anime";
    }
    return "Unknown";
}

const char* AssetLibrary::GetLicenseName(AssetLicense license) {
    switch (license) {
        case AssetLicense::CC0:     return "CC0 (Public Domain)";
        case AssetLicense::OFL:     return "SIL OFL 1.1";
        case AssetLicense::Apache2: return "Apache 2.0";
        case AssetLicense::MIT:     return "MIT";
    }
    return "Unknown";
}

void AssetLibrary::BuildCatalog3D() {
    m_Catalog3D.clear();
    m_IdIndex3D.clear();

    auto add = [&](const char* id, const char* name, const char* creator,
                    const char* category, ArtStyle style, AssetLicense lic,
                    const char* filename, const char* desc, const char* url,
                    std::uint32_t polys, bool pbr, bool lod, bool anim,
                    std::vector<std::string> tags) {
        LibraryAssetEntry e;
        e.id = id; e.name = name; e.creator = creator; e.description = desc;
        e.type = LibraryAssetType::Model3D; e.style = style; e.license = lic;
        e.category = category; e.filename = filename; e.sourceUrl = url;
        e.polyCount = polys; e.hasPBR = pbr; e.hasLOD = lod; e.hasAnimation = anim;
        e.tags = std::move(tags);
        m_IdIndex3D[e.id] = m_Catalog3D.size();
        m_Catalog3D.push_back(std::move(e));
    };

    // === ARCHITECTURE (Kenney.nl CC0) ===
    add("kenney_castle_kit", "Castle Kit", "Kenney", "Architecture", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_castle_kit.glb", "Modular castle building blocks (40+ pieces)",
        "https://kenney.nl/assets/castle-kit", 500, false, false, false,
        {"castle", "medieval", "modular", "building", "tower", "wall"});
    add("kenney_city_kit_suburban", "Suburban City Kit", "Kenney", "Architecture", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_city_kit_suburban.glb", "Suburban buildings, houses, and props",
        "https://kenney.nl/assets/city-kit-suburban", 400, false, false, false,
        {"city", "house", "suburban", "building", "modern"});
    add("kenney_city_kit_commercial", "Commercial City Kit", "Kenney", "Architecture", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_city_kit_commercial.glb", "Commercial buildings and storefronts",
        "https://kenney.nl/assets/city-kit-commercial", 400, false, false, false,
        {"city", "shop", "store", "commercial", "building"});

    // === NATURE ===
    add("kenney_nature_kit", "Nature Kit", "Kenney", "Nature", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_nature_kit.glb", "Trees, rocks, flowers, mushrooms, and ground cover",
        "https://kenney.nl/assets/nature-kit", 300, false, false, false,
        {"tree", "rock", "plant", "nature", "forest", "mushroom"});
    add("quaternius_lowpoly_forest", "Low-Poly Forest Pack", "Quaternius", "Nature", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "quaternius_forest.glb", "Low-poly trees, bushes, and forest elements",
        "https://quaternius.com/packs/ultimatenaturepack.html", 200, false, true, false,
        {"tree", "bush", "forest", "nature", "lowpoly"});

    // === PROPS ===
    add("kenney_furniture_kit", "Furniture Kit", "Kenney", "Props", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_furniture_kit.glb", "Indoor furniture: tables, chairs, shelves, beds",
        "https://kenney.nl/assets/furniture-kit", 350, false, false, false,
        {"furniture", "table", "chair", "bed", "interior"});
    add("kenney_food_kit", "Food Kit", "Kenney", "Props", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_food_kit.glb", "Food items: fruits, bread, dishes, cooking utensils",
        "https://kenney.nl/assets/food-kit", 200, false, false, false,
        {"food", "fruit", "bread", "kitchen", "item"});
    add("kenney_pirate_kit", "Pirate Kit", "Kenney", "Props", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_pirate_kit.glb", "Pirate-themed props: ships, barrels, treasure, cannons",
        "https://kenney.nl/assets/pirate-kit", 500, false, false, false,
        {"pirate", "ship", "barrel", "treasure", "cannon"});

    // === CHARACTERS ===
    add("quaternius_animated_char", "Animated Characters", "Quaternius", "Characters", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "quaternius_characters.glb", "Low-poly animated characters with walk/idle/attack",
        "https://quaternius.com/packs/ultimateanimatedcharpack.html", 800, false, false, true,
        {"character", "animated", "humanoid", "walk", "idle"});
    add("kenney_minifig", "Minifig Characters", "Kenney", "Characters", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_minifig.glb", "Simple block-style character figures",
        "https://kenney.nl/assets/minifig", 150, false, false, false,
        {"character", "simple", "figure", "blocky"});

    // === VEHICLES ===
    add("kenney_car_kit", "Car Kit", "Kenney", "Vehicles", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_car_kit.glb", "Low-poly cars, trucks, and road vehicles",
        "https://kenney.nl/assets/car-kit", 400, false, false, false,
        {"car", "truck", "vehicle", "road"});
    add("kenney_space_kit", "Space Kit", "Kenney", "Vehicles", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_space_kit.glb", "Spaceships, rockets, satellites, and space stations",
        "https://kenney.nl/assets/space-kit", 500, false, false, false,
        {"spaceship", "rocket", "scifi", "space"});

    // === WEAPONS/TOOLS ===
    add("quaternius_weapons", "Fantasy Weapons", "Quaternius", "Weapons", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "quaternius_weapons.glb", "Swords, axes, bows, staffs, and shields",
        "https://quaternius.com/packs/ultimateweaponpack.html", 300, false, false, false,
        {"sword", "weapon", "axe", "bow", "shield", "fantasy"});

    // === DUNGEON/FANTASY ===
    add("kenney_dungeon_kit", "Dungeon Kit", "Kenney", "Dungeon", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "kenney_dungeon_kit.glb", "Modular dungeon tiles, walls, doors, and traps",
        "https://kenney.nl/assets/dungeon-kit", 400, false, false, false,
        {"dungeon", "modular", "tile", "wall", "door", "fantasy"});

    // === SCI-FI ===
    add("quaternius_scifi_interior", "Sci-Fi Interior", "Quaternius", "Sci-Fi", ArtStyle::StylizedLowPoly, AssetLicense::CC0,
        "quaternius_scifi_interior.glb", "Sci-fi interior modules: corridors, rooms, doors",
        "https://quaternius.com/packs/scifimodularinterior.html", 600, false, false, false,
        {"scifi", "interior", "corridor", "modular", "space station"});
}

void AssetLibrary::BuildCatalog2D() {
    m_Catalog2D.clear();
    m_IdIndex2D.clear();

    auto add = [&](const char* id, const char* name, const char* creator,
                    LibraryAssetType type, const char* category, ArtStyle style,
                    AssetLicense lic, const char* filename, const char* desc,
                    const char* url, std::uint32_t w, std::uint32_t h, bool anim,
                    std::vector<std::string> tags) {
        LibraryAssetEntry e;
        e.id = id; e.name = name; e.creator = creator; e.description = desc;
        e.type = type; e.style = style; e.license = lic;
        e.category = category; e.filename = filename; e.sourceUrl = url;
        e.spriteWidth = w; e.spriteHeight = h; e.hasAnimation = anim;
        e.tags = std::move(tags);
        m_IdIndex2D[e.id] = m_Catalog2D.size();
        m_Catalog2D.push_back(std::move(e));
    };

    // === UI KITS ===
    add("kenney_ui_pack", "UI Pack", "Kenney", LibraryAssetType::UIKit, "UI Elements",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_ui_pack.png",
        "Complete UI kit: buttons, sliders, checkboxes, panels, frames",
        "https://kenney.nl/assets/ui-pack", 1024, 1024, false,
        {"ui", "button", "slider", "checkbox", "panel"});
    add("kenney_ui_pack_rpg", "UI Pack RPG Expansion", "Kenney", LibraryAssetType::UIKit, "UI Elements",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_ui_pack_rpg.png",
        "RPG-themed UI: health bars, inventory slots, stats panels",
        "https://kenney.nl/assets/ui-pack-rpg-expansion", 1024, 1024, false,
        {"ui", "rpg", "health", "inventory", "stats"});
    add("kenney_game_icons", "Game Icons", "Kenney", LibraryAssetType::Icon, "Icons",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_game_icons.png",
        "700+ game icons: items, skills, stats, equipment",
        "https://kenney.nl/assets/game-icons", 1024, 1024, false,
        {"icon", "item", "skill", "equipment", "inventory"});

    // === PLATFORMER TILESETS ===
    add("kenney_platformer_base", "Platformer Base Pack", "Kenney", LibraryAssetType::Tileset, "Platformer",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_platformer_base.png",
        "Complete platformer tileset: ground, blocks, items, enemies",
        "https://kenney.nl/assets/platformer-pack-redux", 128, 128, false,
        {"platformer", "tile", "ground", "block", "2d"});
    add("kenney_pixel_platformer", "Pixel Platformer", "Kenney", LibraryAssetType::Tileset, "Platformer",
        ArtStyle::PixelArt, AssetLicense::CC0, "kenney_pixel_platformer.png",
        "Pixel art platformer tileset with characters and props",
        "https://kenney.nl/assets/pixel-platformer", 18, 18, false,
        {"platformer", "pixel", "tile", "character"});

    // === RPG TILESETS ===
    add("kenney_rpg_base", "RPG Base Tileset", "Kenney", LibraryAssetType::Tileset, "RPG",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_rpg_base.png",
        "Top-down RPG tileset: terrain, buildings, paths, vegetation",
        "https://kenney.nl/assets/rpg-base", 64, 64, false,
        {"rpg", "topdown", "tile", "terrain", "building"});
    add("kenney_tiny_dungeon", "Tiny Dungeon", "Kenney", LibraryAssetType::Tileset, "Dungeon",
        ArtStyle::PixelArt, AssetLicense::CC0, "kenney_tiny_dungeon.png",
        "16x16 pixel dungeon tileset with characters and items",
        "https://kenney.nl/assets/tiny-dungeon", 16, 16, false,
        {"dungeon", "pixel", "tile", "character", "item"});

    // === CHARACTER SPRITES ===
    add("kenney_toon_characters", "Toon Characters", "Kenney", LibraryAssetType::Sprite2D, "Characters",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_toon_characters.png",
        "Animated character sprites with walk/idle/jump cycles",
        "https://kenney.nl/assets/toon-characters-1", 256, 256, true,
        {"character", "animated", "walk", "idle", "jump"});
    add("kenney_animal_pack", "Animal Pack", "Kenney", LibraryAssetType::Sprite2D, "Characters",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_animals.png",
        "Flat-style animal sprites: cat, dog, bird, fish, and more",
        "https://kenney.nl/assets/animal-pack-redux", 128, 128, false,
        {"animal", "cat", "dog", "bird", "character"});

    // === VFX SPRITES ===
    add("kenney_particle_pack", "Particle Pack", "Kenney", LibraryAssetType::VFXSprite, "Particles",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_particles.png",
        "Particle textures: circles, sparks, smoke, stars, magic",
        "https://kenney.nl/assets/particle-pack", 128, 128, false,
        {"particle", "spark", "smoke", "vfx", "effect"});
    add("kenney_vfx_pack", "VFX Pack", "Kenney", LibraryAssetType::VFXSprite, "Effects",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_vfx.png",
        "Visual effects sprites: explosions, impacts, trails",
        "https://kenney.nl/assets/vfx-pack", 256, 256, true,
        {"explosion", "impact", "trail", "vfx", "effect"});

    // === BACKGROUNDS ===
    add("kenney_background_elements", "Background Elements", "Kenney", LibraryAssetType::Background, "Backgrounds",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_backgrounds.png",
        "Parallax background layers: mountains, clouds, trees, cities",
        "https://kenney.nl/assets/background-elements-redux", 1920, 1080, false,
        {"background", "parallax", "mountain", "sky", "cloud"});

    // === TEXTURES ===
    add("kenney_prototype_textures", "Prototype Textures", "Kenney", LibraryAssetType::Texture, "Prototype",
        ArtStyle::VectorFlat, AssetLicense::CC0, "kenney_prototype_textures.zip",
        "Color-coded prototype/greybox textures with grid and measurement markings",
        "https://kenney.nl/assets/prototype-textures", 512, 512, false,
        {"texture", "prototype", "greybox", "grid", "development"});
}

} // namespace Assets
} // namespace Enjin
