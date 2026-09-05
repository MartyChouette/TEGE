#include "Enjin/GUI/Localization.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Enjin {
namespace GUI {

LocalizationManager& LocalizationManager::Get() {
    static LocalizationManager instance;
    return instance;
}

void LocalizationManager::SetLocale(const std::string& localeCode) {
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    // Validate that the locale exists in our registered locales or string tables
    bool found = false;
    for (const auto& locale : m_Locales) {
        if (locale.code == localeCode) {
            found = true;
            break;
        }
    }

    if (!found && m_StringTables.find(localeCode) == m_StringTables.end()) {
        ENJIN_LOG_WARN(Editor, "SetLocale: locale '%s' not found, keeping '%s'",
                       localeCode.c_str(), m_CurrentLocale.c_str());
        return;
    }

    m_CurrentLocale = localeCode;
    ENJIN_LOG_INFO(Editor, "Locale set to '%s'", localeCode.c_str());
}

std::vector<Locale> LocalizationManager::GetAvailableLocales() const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    return m_Locales;
}

void LocalizationManager::AddLocale(const Locale& locale) {
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    // Don't add duplicates
    for (const auto& existing : m_Locales) {
        if (existing.code == locale.code) {
            return;
        }
    }
    m_Locales.push_back(locale);
}

void LocalizationManager::SetString(const std::string& localeCode,
                                     const std::string& key,
                                     const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_StringTables[localeCode][key] = value;
}

std::string LocalizationManager::GetString(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    // Try current locale
    auto localeIt = m_StringTables.find(m_CurrentLocale);
    if (localeIt != m_StringTables.end()) {
        auto keyIt = localeIt->second.find(key);
        if (keyIt != localeIt->second.end()) {
            return keyIt->second;
        }
    }

    // Fall back to English
    if (m_CurrentLocale != "en") {
        auto enIt = m_StringTables.find("en");
        if (enIt != m_StringTables.end()) {
            auto keyIt = enIt->second.find(key);
            if (keyIt != enIt->second.end()) {
                return keyIt->second;
            }
        }
    }

    // Last resort: return the key itself
    return key;
}

std::string LocalizationManager::GetString(const std::string& key,
                                            const std::string& fallback) const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    // Try current locale
    auto localeIt = m_StringTables.find(m_CurrentLocale);
    if (localeIt != m_StringTables.end()) {
        auto keyIt = localeIt->second.find(key);
        if (keyIt != localeIt->second.end()) {
            return keyIt->second;
        }
    }

    // Fall back to English
    if (m_CurrentLocale != "en") {
        auto enIt = m_StringTables.find("en");
        if (enIt != m_StringTables.end()) {
            auto keyIt = enIt->second.find(key);
            if (keyIt != enIt->second.end()) {
                return keyIt->second;
            }
        }
    }

    return fallback;
}

bool LocalizationManager::HasString(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    auto localeIt = m_StringTables.find(m_CurrentLocale);
    if (localeIt != m_StringTables.end()) {
        return localeIt->second.find(key) != localeIt->second.end();
    }
    return false;
}

std::string LocalizationManager::Format(
    const std::string& key,
    const std::unordered_map<std::string, std::string>& params) const {

    // No lock here on purpose: GetString takes the shared lock, and
    // re-entering a shared_mutex on one thread is undefined.
    std::string result = GetString(key);

    // Replace {paramName} patterns
    for (const auto& [paramName, paramValue] : params) {
        std::string placeholder = "{" + paramName + "}";
        usize pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), paramValue);
            pos += paramValue.length();
        }
    }

    return result;
}

bool LocalizationManager::LoadFromJSON(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Editor, "Localization: failed to open JSON file '%s'", filepath.c_str());
        return false;
    }
    return LoadJSONStream(file, filepath);
}

bool LocalizationManager::LoadJSONStream(std::istream& in, const std::string& label) {
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    const std::string& filepath = label;   // keep the existing log wording

    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::parse_error& e) {
        ENJIN_LOG_ERROR(Editor, "Localization: JSON parse error in '%s': %s",
                        filepath.c_str(), e.what());
        return false;
    }

    if (!j.is_object()) {
        ENJIN_LOG_ERROR(Editor, "Localization: root element must be an object in '%s'",
                        filepath.c_str());
        return false;
    }

    for (auto& [localeCode, strings] : j.items()) {
        if (!strings.is_object()) {
            continue;
        }

        // Auto-detect and add locale if not already registered
        bool localeRegistered = false;
        for (const auto& locale : m_Locales) {
            if (locale.code == localeCode) {
                localeRegistered = true;
                break;
            }
        }
        if (!localeRegistered) {
            Locale newLocale;
            newLocale.code = localeCode;
            newLocale.name = localeCode;  // Use code as name if unknown
            m_Locales.push_back(newLocale);
        }

        for (auto& [key, value] : strings.items()) {
            if (value.is_string()) {
                m_StringTables[localeCode][key] = value.get<std::string>();
            }
        }
    }

    ENJIN_LOG_INFO(Editor, "Localization: loaded %zu locales from '%s'",
                   j.size(), filepath.c_str());
    return true;
}

bool LocalizationManager::SaveToJSON(const std::string& filepath) const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    nlohmann::json j = nlohmann::json::object();

    for (const auto& [localeCode, strings] : m_StringTables) {
        nlohmann::json localeObj = nlohmann::json::object();
        for (const auto& [key, value] : strings) {
            localeObj[key] = value;
        }
        j[localeCode] = localeObj;
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Editor, "Localization: failed to open file for writing '%s'",
                        filepath.c_str());
        return false;
    }

    file << j.dump(2);
    ENJIN_LOG_INFO(Editor, "Localization: saved to '%s'", filepath.c_str());
    return true;
}

// CSV parsing helper: split a line respecting quoted fields
static std::vector<std::string> ParseCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    for (usize i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (inQuotes) {
            if (c == '"') {
                // Check for escaped quote (double quote)
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
    }

    fields.push_back(field);
    return fields;
}

// CSV writing helper: quote a field if it contains commas, quotes, or newlines
static std::string QuoteCSVField(const std::string& field) {
    bool needsQuoting = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuoting = true;
            break;
        }
    }

    if (!needsQuoting) {
        return field;
    }

    std::string quoted = "\"";
    for (char c : field) {
        if (c == '"') {
            quoted += "\"\"";
        } else {
            quoted += c;
        }
    }
    quoted += '"';
    return quoted;
}

bool LocalizationManager::LoadFromMemory(const u8* data, usize size, TableFormat format,
                                         const std::string& label) {
    if (!data || size == 0) {
        ENJIN_LOG_ERROR(Editor, "Localization: empty table buffer for '%s'", label.c_str());
        return false;
    }
    // istringstream copies, which is fine: a string table is kilobytes and
    // this runs once at boot, not per frame.
    std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
    return (format == TableFormat::Json) ? LoadJSONStream(in, label)
                                         : LoadCSVStream(in, label);
}

bool LocalizationManager::LoadFromCSV(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Editor, "Localization: failed to open CSV file '%s'", filepath.c_str());
        return false;
    }
    return LoadCSVStream(file, filepath);
}

bool LocalizationManager::LoadCSVStream(std::istream& file, const std::string& label) {
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    const std::string& filepath = label;   // keep the existing log wording

    std::string line;

    // Read header row: key,en,fr,ja,...
    if (!std::getline(file, line)) {
        ENJIN_LOG_ERROR(Editor, "Localization: CSV file '%s' is empty", filepath.c_str());
        return false;
    }

    // Strip trailing \r if present (Windows line endings)
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::vector<std::string> header = ParseCSVLine(line);
    if (header.size() < 2) {
        ENJIN_LOG_ERROR(Editor, "Localization: CSV header must have at least 'key' and one locale");
        return false;
    }

    // header[0] should be "key", header[1..] are locale codes
    std::vector<std::string> localeCodes;
    for (usize i = 1; i < header.size(); ++i) {
        const std::string& code = header[i];
        localeCodes.push_back(code);

        // Auto-detect and add locale
        bool found = false;
        for (const auto& locale : m_Locales) {
            if (locale.code == code) {
                found = true;
                break;
            }
        }
        if (!found) {
            Locale newLocale;
            newLocale.code = code;
            newLocale.name = code;
            m_Locales.push_back(newLocale);
        }
    }

    // Read data rows
    u32 rowCount = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> fields = ParseCSVLine(line);
        if (fields.empty()) {
            continue;
        }

        const std::string& key = fields[0];

        for (usize i = 0; i < localeCodes.size() && (i + 1) < fields.size(); ++i) {
            if (!fields[i + 1].empty()) {
                m_StringTables[localeCodes[i]][key] = fields[i + 1];
            }
        }
        ++rowCount;
    }

    ENJIN_LOG_INFO(Editor, "Localization: loaded %u keys from CSV '%s'",
                   rowCount, filepath.c_str());
    return true;
}

bool LocalizationManager::SaveToCSV(const std::string& filepath) const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    std::ofstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Editor, "Localization: failed to open CSV file for writing '%s'",
                        filepath.c_str());
        return false;
    }

    // Collect all locale codes
    std::vector<std::string> localeCodes;
    for (const auto& [code, _] : m_StringTables) {
        localeCodes.push_back(code);
    }
    std::sort(localeCodes.begin(), localeCodes.end());

    // Collect all unique keys across all locales
    std::vector<std::string> allKeys;
    {
        std::unordered_map<std::string, bool> keySet;
        for (const auto& [code, strings] : m_StringTables) {
            for (const auto& [key, _] : strings) {
                if (!keySet[key]) {
                    keySet[key] = true;
                    allKeys.push_back(key);
                }
            }
        }
    }
    std::sort(allKeys.begin(), allKeys.end());

    // Write header
    file << "key";
    for (const auto& code : localeCodes) {
        file << "," << code;
    }
    file << "\n";

    // Write rows
    for (const auto& key : allKeys) {
        file << QuoteCSVField(key);
        for (const auto& code : localeCodes) {
            file << ",";
            auto localeIt = m_StringTables.find(code);
            if (localeIt != m_StringTables.end()) {
                auto keyIt = localeIt->second.find(key);
                if (keyIt != localeIt->second.end()) {
                    file << QuoteCSVField(keyIt->second);
                }
            }
        }
        file << "\n";
    }

    ENJIN_LOG_INFO(Editor, "Localization: saved %zu keys to CSV '%s'",
                   allKeys.size(), filepath.c_str());
    return true;
}

void LocalizationManager::Clear() {
    std::unique_lock<std::shared_mutex> lock(m_Mutex);
    m_StringTables.clear();
    m_Locales.clear();
    m_CurrentLocale = "en";
}

std::vector<std::string> LocalizationManager::GetAllKeys() const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    std::vector<std::string> keys;
    auto localeIt = m_StringTables.find(m_CurrentLocale);
    if (localeIt != m_StringTables.end()) {
        keys.reserve(localeIt->second.size());
        for (const auto& [key, _] : localeIt->second) {
            keys.push_back(key);
        }
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

u32 LocalizationManager::GetStringCount(const std::string& localeCode) const {
    std::shared_lock<std::shared_mutex> lock(m_Mutex);
    auto it = m_StringTables.find(localeCode);
    if (it != m_StringTables.end()) {
        return static_cast<u32>(it->second.size());
    }
    return 0;
}

} // namespace GUI
} // namespace Enjin
