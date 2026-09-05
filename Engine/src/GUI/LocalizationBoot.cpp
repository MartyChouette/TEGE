#include "Enjin/GUI/LocalizationBoot.h"
#include "Enjin/GUI/Localization.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <filesystem>

namespace Enjin {
namespace GUI {

namespace {

LocalizationManager::TableFormat FormatFor(const std::string& path) {
    // The two loaders the manager has. Anything not obviously CSV is parsed as
    // JSON, which is also what a bare ".json" or an extensionless path means.
    const usize dot = path.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = path.substr(dot + 1);
        for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == "csv") return LocalizationManager::TableFormat::Csv;
    }
    return LocalizationManager::TableFormat::Json;
}

} // namespace

u32 ApplyLocalizationSettings(const std::string& localizationJson,
                              const std::string& assetRoot,
                              Build::AssetReader* reader) {
    if (localizationJson.empty()) return 0;

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(localizationJson);
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Localization: bad 'localization' block: %s", e.what());
        return 0;
    }
    if (!j.is_object()) return 0;

    LocalizationManager& loc = LocalizationManager::Get();

    // Locales first, so a table naming one of them does not have to re-add it
    // under its bare code as its display name.
    if (j.contains("locales") && j["locales"].is_array()) {
        for (const auto& entry : j["locales"]) {
            if (!entry.is_object() || !entry.contains("code")) continue;
            Locale l;
            l.code   = entry.value("code", std::string());
            l.name   = entry.value("name", l.code);
            l.region = entry.value("region", std::string());
            if (!l.code.empty()) loc.AddLocale(l);
        }
    }

    u32 loaded = 0;
    if (j.contains("tables") && j["tables"].is_array()) {
        for (const auto& entry : j["tables"]) {
            if (!entry.is_string()) continue;
            const std::string path = entry.get<std::string>();
            if (path.empty()) continue;

            // The pak first. On web this is the ONLY thing that can work, and
            // in an exported game it is what ships; the loose file is the
            // editor's case and the fallback.
            bool ok = false;
            if (reader && reader->IsOpen() && reader->HasFile(path)) {
                const std::vector<u8> bytes = reader->ReadFile(path);
                if (!bytes.empty()) {
                    ok = loc.LoadFromMemory(bytes.data(), bytes.size(), FormatFor(path), path);
                }
            }
            if (!ok) {
                namespace fs = std::filesystem;
                std::error_code ec;
                std::string full = path;
                if (fs::path(path).is_relative() && !assetRoot.empty() && !fs::exists(path, ec)) {
                    full = (fs::path(assetRoot) / path).string();
                }
                ok = (FormatFor(path) == LocalizationManager::TableFormat::Csv)
                         ? loc.LoadFromCSV(full)
                         : loc.LoadFromJSON(full);
            }
            if (ok) ++loaded;
            else ENJIN_LOG_WARN(Asset, "Localization: could not load string table '%s'", path.c_str());
        }
    }

    // Locale LAST: the tables have to exist before anything resolves against
    // the selected one.
    const std::string def = j.value("defaultLocale", std::string());
    if (!def.empty()) loc.SetLocale(def);

    if (loaded > 0) {
        ENJIN_LOG_INFO(Asset, "Localization: %u table(s) loaded, locale '%s', %u strings",
                       loaded, loc.GetCurrentLocale().c_str(),
                       loc.GetStringCount(loc.GetCurrentLocale()));
    }
    return loaded;
}

} // namespace GUI
} // namespace Enjin
