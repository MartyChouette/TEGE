#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <iosfwd>
#include <shared_mutex>

namespace Enjin {
namespace GUI {

// Supported locale identifiers
struct Locale {
    std::string code;       // e.g. "en", "fr", "ja", "es"
    std::string name;       // e.g. "English", "Français", "日本語"
    std::string region;     // e.g. "US", "FR", "JP" (optional)
};

// Localization manager - singleton for runtime string lookup
class ENJIN_API LocalizationManager {
public:
    static LocalizationManager& Get();

    // Locale management
    void SetLocale(const std::string& localeCode);
    const std::string& GetCurrentLocale() const { return m_CurrentLocale; }
    std::vector<Locale> GetAvailableLocales() const;
    void AddLocale(const Locale& locale);

    // String table management
    void SetString(const std::string& localeCode, const std::string& key, const std::string& value);
    std::string GetString(const std::string& key) const;
    std::string GetString(const std::string& key, const std::string& fallback) const;
    bool HasString(const std::string& key) const;

    // Parameterized strings: "Hello {name}, you have {count} items"
    std::string Format(const std::string& key,
                       const std::unordered_map<std::string, std::string>& params) const;

    // Import/Export
    bool LoadFromJSON(const std::string& filepath);
    bool SaveToJSON(const std::string& filepath) const;
    bool LoadFromCSV(const std::string& filepath);  // Columns: key, locale1, locale2, ...
    bool SaveToCSV(const std::string& filepath) const;

    enum class TableFormat { Json, Csv };

    // Load a table already in memory.
    //
    // The file loaders open an ifstream, which the web player and any packed
    // game cannot use: on web there are no loose files, and AssetReader hands
    // back a byte vector, not a path. Without this entry point a shipped game
    // physically could not load a string table, whatever the table said.
    // `label` is only used in log messages to name the source.
    bool LoadFromMemory(const u8* data, usize size, TableFormat format,
                        const std::string& label = "<memory>");

    // Clear all data
    void Clear();

    // Get all keys for current locale
    std::vector<std::string> GetAllKeys() const;

    // Get string count for a locale
    u32 GetStringCount(const std::string& localeCode) const;

private:
    LocalizationManager() = default;

    // The stream parsers both loaders share. The file and memory entry points
    // are thin wrappers so a table cannot parse differently depending on where
    // it came from.
    bool LoadJSONStream(std::istream& in, const std::string& label);
    bool LoadCSVStream(std::istream& in, const std::string& label);

    // Strings are read while a frame is composing text and written when a
    // locale is switched or a table loads, so every access is guarded. Reads
    // are shared: one lookup per drawn string is not hot-path work, but a
    // SetLocale landing mid-frame is a real data race.
    mutable std::shared_mutex m_Mutex;

    std::string m_CurrentLocale = "en";
    std::vector<Locale> m_Locales;
    // Map: locale code -> (key -> translated string)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_StringTables;
};

// Helper macro for localized strings
// Usage: LOC("greeting") or LOC_FMT("greeting", {{"name", playerName}})
#define LOC(key) ::Enjin::GUI::LocalizationManager::Get().GetString(key)
#define LOC_FMT(key, params) ::Enjin::GUI::LocalizationManager::Get().Format(key, params)

} // namespace GUI
} // namespace Enjin
