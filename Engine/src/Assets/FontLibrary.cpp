#include "Enjin/Assets/FontLibrary.h"
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

FontLibrary::FontLibrary() {
    BuildCatalog();
}

void FontLibrary::Initialize(const std::string& fontsDirectory) {
    m_FontsDirectory = fontsDirectory;
}

std::vector<const FontEntry*> FontLibrary::GetByCategory(FontCategory category) const {
    std::vector<const FontEntry*> result;
    for (auto& e : m_Catalog)
        if (e.category == category) result.push_back(&e);
    return result;
}

std::vector<const FontEntry*> FontLibrary::Search(const std::string& query) const {
    std::vector<const FontEntry*> result;
    std::string q = ToLower(query);
    for (auto& e : m_Catalog) {
        if (ToLower(e.name).find(q) != std::string::npos ||
            ToLower(e.description).find(q) != std::string::npos ||
            ToLower(e.designer).find(q) != std::string::npos)
            result.push_back(&e);
    }
    return result;
}

bool FontLibrary::IsFontInstalled(const std::string& fontId) const {
    auto* entry = FindById(fontId);
    if (!entry) return false;
    std::string path = m_FontsDirectory + "/" + entry->filename;
    return std::filesystem::exists(path);
}

std::string FontLibrary::GetFontPath(const std::string& fontId) const {
    auto* entry = FindById(fontId);
    if (!entry) return "";
    return m_FontsDirectory + "/" + entry->filename;
}

const FontEntry* FontLibrary::FindById(const std::string& fontId) const {
    auto it = m_IdIndex.find(fontId);
    if (it == m_IdIndex.end()) return nullptr;
    return &m_Catalog[it->second];
}

const char* FontLibrary::GetCategoryName(FontCategory category) {
    switch (category) {
        case FontCategory::SansSerif:   return "Sans-Serif";
        case FontCategory::Serif:       return "Serif";
        case FontCategory::Monospace:   return "Monospace";
        case FontCategory::Display:     return "Display";
        case FontCategory::Handwriting: return "Handwriting";
        case FontCategory::Pixel:       return "Pixel";
        case FontCategory::Fantasy:     return "Fantasy";
        case FontCategory::SciFi:       return "Sci-Fi";
    }
    return "Unknown";
}

const char* FontLibrary::GetLicenseName(FontLicense license) {
    switch (license) {
        case FontLicense::OFL:          return "SIL OFL 1.1";
        case FontLicense::Apache2:      return "Apache 2.0";
        case FontLicense::MIT:          return "MIT";
        case FontLicense::PublicDomain: return "Public Domain";
    }
    return "Unknown";
}

void FontLibrary::BuildCatalog() {
    m_Catalog.clear();
    m_IdIndex.clear();

    auto add = [&](const char* id, const char* name, const char* designer,
                    FontCategory cat, FontLicense lic, const char* filename,
                    const char* desc, const char* url,
                    bool italic, bool bold, bool boldItalic, std::uint32_t weights) {
        FontEntry e;
        e.id = id; e.name = name; e.designer = designer;
        e.category = cat; e.license = lic; e.filename = filename;
        e.description = desc; e.sourceUrl = url;
        e.hasItalic = italic; e.hasBold = bold; e.hasBoldItalic = boldItalic;
        e.weightCount = weights;
        m_IdIndex[e.id] = m_Catalog.size();
        m_Catalog.push_back(std::move(e));
    };

    // === SANS-SERIF (UI/HUD) ===
    add("inter", "Inter", "Rasmus Andersson", FontCategory::SansSerif, FontLicense::OFL,
        "Inter-Regular.ttf", "Modern UI font with excellent screen readability",
        "https://fonts.google.com/specimen/Inter", true, true, true, 9);
    add("roboto", "Roboto", "Christian Robertson / Google", FontCategory::SansSerif, FontLicense::Apache2,
        "Roboto-Regular.ttf", "Android system font, clean and versatile",
        "https://fonts.google.com/specimen/Roboto", true, true, true, 6);
    add("opensans", "Open Sans", "Steve Matteson", FontCategory::SansSerif, FontLicense::OFL,
        "OpenSans-Regular.ttf", "Friendly, neutral sans-serif for body text",
        "https://fonts.google.com/specimen/Open+Sans", true, true, true, 5);
    add("lato", "Lato", "Lukasz Dziedzic", FontCategory::SansSerif, FontLicense::OFL,
        "Lato-Regular.ttf", "Warm, semi-rounded sans-serif",
        "https://fonts.google.com/specimen/Lato", true, true, true, 5);
    add("nunito", "Nunito", "Vernon Adams", FontCategory::SansSerif, FontLicense::OFL,
        "Nunito-Regular.ttf", "Rounded sans-serif, friendly and modern",
        "https://fonts.google.com/specimen/Nunito", true, true, true, 7);
    add("poppins", "Poppins", "Indian Type Foundry", FontCategory::SansSerif, FontLicense::OFL,
        "Poppins-Regular.ttf", "Geometric sans-serif with Indian design influence",
        "https://fonts.google.com/specimen/Poppins", true, true, true, 9);
    add("sourcesans3", "Source Sans 3", "Paul D. Hunt / Adobe", FontCategory::SansSerif, FontLicense::OFL,
        "SourceSans3-Regular.ttf", "Adobe's open source UI font family",
        "https://fonts.google.com/specimen/Source+Sans+3", true, true, true, 6);

    // === SERIF (Narrative/Books) ===
    add("merriweather", "Merriweather", "Sorkin Type", FontCategory::Serif, FontLicense::OFL,
        "Merriweather-Regular.ttf", "Screen-optimized serif for extended reading",
        "https://fonts.google.com/specimen/Merriweather", true, true, true, 4);
    add("playfair", "Playfair Display", "Claus Eggers Sorensen", FontCategory::Serif, FontLicense::OFL,
        "PlayfairDisplay-Regular.ttf", "High-contrast transitional serif for headings",
        "https://fonts.google.com/specimen/Playfair+Display", true, true, true, 6);
    add("lora", "Lora", "Cyreal", FontCategory::Serif, FontLicense::OFL,
        "Lora-Regular.ttf", "Elegant, contemporary serif for body text",
        "https://fonts.google.com/specimen/Lora", true, true, true, 4);
    add("crimsontext", "Crimson Text", "Sebastian Kosch", FontCategory::Serif, FontLicense::OFL,
        "CrimsonText-Regular.ttf", "Book-style old-style serif",
        "https://fonts.google.com/specimen/Crimson+Text", true, true, true, 3);
    add("ebgaramond", "EB Garamond", "Georg Mayr-Duffner", FontCategory::Serif, FontLicense::OFL,
        "EBGaramond-Regular.ttf", "Revival of Claude Garamont's classic typeface",
        "https://fonts.google.com/specimen/EB+Garamond", true, true, true, 4);

    // === MONOSPACE (Code/Terminal) ===
    add("jetbrainsmono", "JetBrains Mono", "JetBrains", FontCategory::Monospace, FontLicense::OFL,
        "JetBrainsMono-Regular.ttf", "Developer font with ligatures and excellent readability",
        "https://fonts.google.com/specimen/JetBrains+Mono", true, true, true, 4);
    add("firacode", "Fira Code", "Nikita Prokopov / Mozilla", FontCategory::Monospace, FontLicense::OFL,
        "FiraCode-Regular.ttf", "Monospace with programming ligatures",
        "https://fonts.google.com/specimen/Fira+Code", false, true, false, 5);
    add("sourcecodepro", "Source Code Pro", "Paul D. Hunt / Adobe", FontCategory::Monospace, FontLicense::OFL,
        "SourceCodePro-Regular.ttf", "Adobe's monospace companion to Source Sans",
        "https://fonts.google.com/specimen/Source+Code+Pro", true, true, true, 7);
    add("spacemono", "Space Mono", "Colophon Foundry", FontCategory::Monospace, FontLicense::OFL,
        "SpaceMono-Regular.ttf", "Geometric monospace with retro-futuristic character",
        "https://fonts.google.com/specimen/Space+Mono", true, true, true, 2);
    add("ibmplexmono", "IBM Plex Mono", "Mike Abbink / IBM", FontCategory::Monospace, FontLicense::OFL,
        "IBMPlexMono-Regular.ttf", "IBM's corporate monospace, clean and technical",
        "https://fonts.google.com/specimen/IBM+Plex+Mono", true, true, true, 6);

    // === DISPLAY (Titles/Logos) ===
    add("bebasneue", "Bebas Neue", "Ryoichi Tsunekawa", FontCategory::Display, FontLicense::OFL,
        "BebasNeue-Regular.ttf", "Bold condensed all-caps display typeface",
        "https://fonts.google.com/specimen/Bebas+Neue", false, false, false, 1);
    add("oswald", "Oswald", "Vernon Adams", FontCategory::Display, FontLicense::OFL,
        "Oswald-Regular.ttf", "Condensed gothic for headings and titles",
        "https://fonts.google.com/specimen/Oswald", false, true, false, 6);
    add("righteous", "Righteous", "Astigmatic", FontCategory::Display, FontLicense::OFL,
        "Righteous-Regular.ttf", "Retro rounded display font inspired by vintage signage",
        "https://fonts.google.com/specimen/Righteous", false, false, false, 1);
    add("bungee", "Bungee", "David Jonathan Ross", FontCategory::Display, FontLicense::OFL,
        "Bungee-Regular.ttf", "Chromatic signage display font with layering support",
        "https://fonts.google.com/specimen/Bungee", false, false, false, 1);
    add("rubikmonoone", "Rubik Mono One", "Hubert and Fischer", FontCategory::Display, FontLicense::OFL,
        "RubikMonoOne-Regular.ttf", "Heavy rounded mono-weight display",
        "https://fonts.google.com/specimen/Rubik+Mono+One", false, false, false, 1);
    add("blackopsone", "Black Ops One", "James Grieshaber", FontCategory::Display, FontLicense::OFL,
        "BlackOpsOne-Regular.ttf", "Military stencil display font",
        "https://fonts.google.com/specimen/Black+Ops+One", false, false, false, 1);

    // === HANDWRITING (Script/Cursive) ===
    add("caveat", "Caveat", "Pablo Impallari", FontCategory::Handwriting, FontLicense::OFL,
        "Caveat-Regular.ttf", "Casual handwriting with natural variation",
        "https://fonts.google.com/specimen/Caveat", false, true, false, 2);
    add("indieflower", "Indie Flower", "Kimberly Geswein", FontCategory::Handwriting, FontLicense::OFL,
        "IndieFlower-Regular.ttf", "Whimsical hand-lettered style",
        "https://fonts.google.com/specimen/Indie+Flower", false, false, false, 1);
    add("dancingscript", "Dancing Script", "Pablo Impallari", FontCategory::Handwriting, FontLicense::OFL,
        "DancingScript-Regular.ttf", "Lively casual script with bounce",
        "https://fonts.google.com/specimen/Dancing+Script", false, true, false, 4);
    add("permanentmarker", "Permanent Marker", "Font Diner", FontCategory::Handwriting, FontLicense::Apache2,
        "PermanentMarker-Regular.ttf", "Bold marker pen handwriting",
        "https://fonts.google.com/specimen/Permanent+Marker", false, false, false, 1);

    // === PIXEL (Retro/8-bit) ===
    add("pressstart2p", "Press Start 2P", "CodeMan38", FontCategory::Pixel, FontLicense::OFL,
        "PressStart2P-Regular.ttf", "Classic 8-bit arcade pixel font",
        "https://fonts.google.com/specimen/Press+Start+2P", false, false, false, 1);
    add("vt323", "VT323", "Peter Hull", FontCategory::Pixel, FontLicense::OFL,
        "VT323-Regular.ttf", "VT320 terminal screen pixel font",
        "https://fonts.google.com/specimen/VT323", false, false, false, 1);
    add("silkscreen", "Silkscreen", "Jason Kottke", FontCategory::Pixel, FontLicense::OFL,
        "Silkscreen-Regular.ttf", "Clean bitmap pixel font for small sizes",
        "https://fonts.google.com/specimen/Silkscreen", false, true, false, 2);
    add("dotgothic16", "DotGothic16", "FontWorks", FontCategory::Pixel, FontLicense::OFL,
        "DotGothic16-Regular.ttf", "Japanese-style pixel gothic with CJK support",
        "https://fonts.google.com/specimen/DotGothic16", false, false, false, 1);
    add("pixelify", "Pixelify Sans", "Stefie Justprince", FontCategory::Pixel, FontLicense::OFL,
        "PixelifySans-Regular.ttf", "Modern pixel font with multiple weights",
        "https://fonts.google.com/specimen/Pixelify+Sans", false, true, false, 4);

    // === FANTASY (Medieval/RPG) ===
    add("uncialantiqua", "Uncial Antiqua", "Astigmatic", FontCategory::Fantasy, FontLicense::OFL,
        "UncialAntiqua-Regular.ttf", "Celtic/medieval uncial for RPG and fantasy",
        "https://fonts.google.com/specimen/Uncial+Antiqua", false, false, false, 1);
    add("medievalsharp", "MedievalSharp", "Wojciech Kalinowski", FontCategory::Fantasy, FontLicense::OFL,
        "MedievalSharp-Regular.ttf", "Sharp blackletter-inspired medieval display",
        "https://fonts.google.com/specimen/MedievalSharp", false, false, false, 1);
    add("almendra", "Almendra", "Ana Sanfelippo", FontCategory::Fantasy, FontLicense::OFL,
        "Almendra-Regular.ttf", "Calligraphic fantasy font with old-world charm",
        "https://fonts.google.com/specimen/Almendra", true, true, true, 2);
    add("cinzel", "Cinzel", "Natanael Gama", FontCategory::Fantasy, FontLicense::OFL,
        "Cinzel-Regular.ttf", "Roman-inspired display serif for epic/historical themes",
        "https://fonts.google.com/specimen/Cinzel", false, true, false, 3);

    // === SCI-FI (Futuristic) ===
    add("orbitron", "Orbitron", "Matt McInerney", FontCategory::SciFi, FontLicense::OFL,
        "Orbitron-Regular.ttf", "Geometric sans-serif with a space-age aesthetic",
        "https://fonts.google.com/specimen/Orbitron", false, true, false, 4);
    add("rajdhani", "Rajdhani", "Indian Type Foundry", FontCategory::SciFi, FontLicense::OFL,
        "Rajdhani-Regular.ttf", "Technical Devanagari-inspired sans-serif",
        "https://fonts.google.com/specimen/Rajdhani", false, true, false, 5);
    add("exo2", "Exo 2", "Natanael Gama", FontCategory::SciFi, FontLicense::OFL,
        "Exo2-Regular.ttf", "Geometric futuristic sans-serif with wide language support",
        "https://fonts.google.com/specimen/Exo+2", true, true, true, 9);
    add("audiowide", "Audiowide", "Astigmatic", FontCategory::SciFi, FontLicense::OFL,
        "Audiowide-Regular.ttf", "Wide techno display font for HUDs and UI",
        "https://fonts.google.com/specimen/Audiowide", false, false, false, 1);
    add("sharetechtmono", "Share Tech Mono", "Carrois Apostrophe", FontCategory::SciFi, FontLicense::OFL,
        "ShareTechMono-Regular.ttf", "Technical monospace for sci-fi terminals",
        "https://fonts.google.com/specimen/Share+Tech+Mono", false, false, false, 1);
}

} // namespace Assets
} // namespace Enjin
