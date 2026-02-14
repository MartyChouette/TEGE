#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace Enjin {
namespace Assets {

// Font category for filtering
enum class FontCategory : std::uint8_t {
    SansSerif,      // UI/HUD: clean, modern
    Serif,          // Narrative/books: classic, literary
    Monospace,      // Code/terminal: fixed-width
    Display,        // Titles/logos: decorative, high-impact
    Handwriting,    // Script/cursive: personal, organic
    Pixel,          // Retro/8-bit: game-specific
    Fantasy,        // Medieval/RPG: ornate
    SciFi           // Futuristic: technical, angular
};

// Font license type
enum class FontLicense : std::uint8_t {
    OFL,            // SIL Open Font License 1.1
    Apache2,        // Apache License 2.0
    MIT,            // MIT License
    PublicDomain    // CC0 / Unlicense
};

// Single font entry in the library catalog
struct FontEntry {
    std::string id;             // Unique identifier (e.g., "inter")
    std::string name;           // Display name (e.g., "Inter")
    std::string designer;       // Font designer/foundry
    FontCategory category;
    FontLicense license;
    std::string filename;       // Expected filename (e.g., "Inter-Regular.ttf")
    std::string description;    // Brief description of the font
    std::string sourceUrl;      // Where to download (Google Fonts, etc.)
    bool hasItalic = false;
    bool hasBold = false;
    bool hasBoldItalic = false;
    std::uint32_t weightCount = 1;        // Number of weight variants available
};

// Manages the bundled font library catalog
class FontLibrary {
public:
    FontLibrary();

    // Initialize with a base directory for font files
    void Initialize(const std::string& fontsDirectory);

    // Get the full catalog of available fonts
    const std::vector<FontEntry>& GetCatalog() const { return m_Catalog; }

    // Filter by category
    std::vector<const FontEntry*> GetByCategory(FontCategory category) const;

    // Search by name (case-insensitive substring match)
    std::vector<const FontEntry*> Search(const std::string& query) const;

    // Check if a font file exists on disk
    bool IsFontInstalled(const std::string& fontId) const;

    // Get the full path to an installed font file
    std::string GetFontPath(const std::string& fontId) const;

    // Get entry by ID
    const FontEntry* FindById(const std::string& fontId) const;

    // Category/license display names
    static const char* GetCategoryName(FontCategory category);
    static const char* GetLicenseName(FontLicense license);

    // Get the fonts directory
    const std::string& GetFontsDirectory() const { return m_FontsDirectory; }

private:
    void BuildCatalog();

    std::string m_FontsDirectory;
    std::vector<FontEntry> m_Catalog;
    std::unordered_map<std::string, std::size_t> m_IdIndex; // id -> catalog index
};

} // namespace Assets
} // namespace Enjin
