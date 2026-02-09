#include "Enjin/Plugin/PluginRepository.h"
#include "Enjin/Plugin/PluginSystem.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace Enjin {
namespace Plugin {

namespace fs = std::filesystem;
using json = nlohmann::json;

// ============================================================================
// SOURCE MANAGEMENT
// ============================================================================

void PluginRepository::AddSource(const RepositorySource& source) {
    // Check for duplicates
    for (const auto& s : m_Sources) {
        if (s.name == source.name) return;
    }
    m_Sources.push_back(source);
    ENJIN_LOG_INFO(Script, "Added plugin repository source: %s (%s)",
                   source.name.c_str(), source.path.c_str());
}

void PluginRepository::RemoveSource(const std::string& name) {
    m_Sources.erase(
        std::remove_if(m_Sources.begin(), m_Sources.end(),
            [&name](const RepositorySource& s) { return s.name == name; }),
        m_Sources.end());
}

// ============================================================================
// CATALOG
// ============================================================================

void PluginRepository::RefreshCatalog() {
    m_Catalog.clear();

    for (const auto& source : m_Sources) {
        if (!source.enabled) continue;

        // Look for repository.json in the source path
        std::string repoJsonPath = source.path + "/repository.json";
        if (fs::exists(repoJsonPath)) {
            LoadRepositoryJson(repoJsonPath);
        } else if (fs::exists(source.path) && fs::is_directory(source.path)) {
            // Scan for individual plugin.json files
            for (const auto& entry : fs::recursive_directory_iterator(source.path)) {
                if (entry.is_regular_file() && entry.path().filename() == "plugin.json") {
                    try {
                        std::ifstream file(entry.path().string());
                        json j;
                        file >> j;

                        PluginCatalogEntry catalog;
                        catalog.name = j.value("name", "");
                        catalog.version = j.value("version", "0.0.0");
                        catalog.author = j.value("author", "");
                        catalog.description = j.value("description", "");
                        catalog.category = j.value("category", "General");
                        catalog.localPath = entry.path().parent_path().string();
                        catalog.minEngineVersion = j.value("minEngineVersion", "");

                        if (j.contains("dependencies") && j["dependencies"].is_array()) {
                            for (const auto& dep : j["dependencies"]) {
                                catalog.dependencies.push_back(dep.get<std::string>());
                            }
                        }
                        if (j.contains("tags") && j["tags"].is_array()) {
                            for (const auto& tag : j["tags"]) {
                                catalog.tags.push_back(tag.get<std::string>());
                            }
                        }

                        if (!catalog.name.empty()) {
                            m_Catalog.push_back(catalog);
                        }
                    } catch (const std::exception& e) {
                        ENJIN_LOG_WARN(Script, "Failed to parse plugin.json: %s", e.what());
                    }
                }
            }
        }
    }

    ENJIN_LOG_INFO(Script, "Plugin catalog refreshed: %zu entries", m_Catalog.size());
}

bool PluginRepository::LoadRepositoryJson(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        json j;
        file >> j;

        if (!j.contains("plugins") || !j["plugins"].is_array()) return false;

        fs::path repoDir = fs::path(path).parent_path();

        for (const auto& pj : j["plugins"]) {
            PluginCatalogEntry entry;
            entry.name = pj.value("name", "");
            entry.version = pj.value("version", "0.0.0");
            entry.author = pj.value("author", "");
            entry.description = pj.value("description", "");
            entry.category = pj.value("category", "General");
            entry.minEngineVersion = pj.value("minEngineVersion", "");
            entry.sizeBytes = pj.value("sizeBytes", (u64)0);

            // Resolve local path relative to repository
            std::string relPath = pj.value("path", "");
            if (!relPath.empty()) {
                entry.localPath = (repoDir / relPath).string();
            }

            if (pj.contains("dependencies") && pj["dependencies"].is_array()) {
                for (const auto& dep : pj["dependencies"]) {
                    entry.dependencies.push_back(dep.get<std::string>());
                }
            }
            if (pj.contains("tags") && pj["tags"].is_array()) {
                for (const auto& tag : pj["tags"]) {
                    entry.tags.push_back(tag.get<std::string>());
                }
            }

            if (!entry.name.empty()) {
                m_Catalog.push_back(entry);
            }
        }

        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to parse repository.json '%s': %s", path.c_str(), e.what());
        return false;
    }
}

// ============================================================================
// SEARCH
// ============================================================================

std::vector<const PluginCatalogEntry*> PluginRepository::Search(const std::string& query) const {
    std::vector<const PluginCatalogEntry*> results;
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (const auto& entry : m_Catalog) {
        // Search in name, description, category, tags
        std::string searchText = entry.name + " " + entry.description + " " +
                                 entry.category + " " + entry.author;
        for (const auto& tag : entry.tags) {
            searchText += " " + tag;
        }
        std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::tolower);

        if (searchText.find(lowerQuery) != std::string::npos) {
            results.push_back(&entry);
        }
    }
    return results;
}

std::vector<const PluginCatalogEntry*> PluginRepository::GetByCategory(const std::string& category) const {
    std::vector<const PluginCatalogEntry*> results;
    for (const auto& entry : m_Catalog) {
        if (entry.category == category) {
            results.push_back(&entry);
        }
    }
    return results;
}

std::vector<std::string> PluginRepository::GetCategories() const {
    std::vector<std::string> categories;
    for (const auto& entry : m_Catalog) {
        if (std::find(categories.begin(), categories.end(), entry.category) == categories.end()) {
            categories.push_back(entry.category);
        }
    }
    std::sort(categories.begin(), categories.end());
    return categories;
}

// ============================================================================
// INSTALL / UNINSTALL
// ============================================================================

bool PluginRepository::Install(const std::string& name, const std::string& targetDir) {
    // Find the catalog entry
    const PluginCatalogEntry* entry = nullptr;
    for (const auto& e : m_Catalog) {
        if (e.name == name) {
            entry = &e;
            break;
        }
    }
    if (!entry) {
        ENJIN_LOG_ERROR(Script, "Plugin '%s' not found in catalog", name.c_str());
        return false;
    }

    if (entry->localPath.empty() || !fs::exists(entry->localPath)) {
        ENJIN_LOG_ERROR(Script, "Plugin '%s' source path not found: %s",
                        name.c_str(), entry->localPath.c_str());
        return false;
    }

    // Copy plugin directory to target
    try {
        fs::path destDir = fs::path(targetDir) / name;
        fs::create_directories(destDir);
        fs::copy(entry->localPath, destDir,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        ENJIN_LOG_INFO(Script, "Installed plugin '%s' to %s",
                       name.c_str(), destDir.string().c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to install plugin '%s': %s", name.c_str(), e.what());
        return false;
    }
}

bool PluginRepository::Uninstall(const std::string& name, const std::string& pluginsDir) {
    fs::path pluginDir = fs::path(pluginsDir) / name;
    if (!fs::exists(pluginDir)) {
        ENJIN_LOG_WARN(Script, "Plugin directory not found for uninstall: %s",
                       pluginDir.string().c_str());
        return false;
    }

    try {
        fs::remove_all(pluginDir);
        ENJIN_LOG_INFO(Script, "Uninstalled plugin '%s'", name.c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to uninstall plugin '%s': %s", name.c_str(), e.what());
        return false;
    }
}

// ============================================================================
// STATUS
// ============================================================================

bool PluginRepository::IsInstalled(const std::string& name, const PluginSystem* system) const {
    if (!system) return false;
    for (const auto& plugin : system->GetPlugins()) {
        if (plugin.manifest.name == name) return true;
    }
    return false;
}

bool PluginRepository::HasUpdate(const std::string& name, const PluginSystem* system) const {
    if (!system) return false;

    // Find installed version
    std::string installedVersion;
    for (const auto& plugin : system->GetPlugins()) {
        if (plugin.manifest.name == name) {
            installedVersion = plugin.manifest.version;
            break;
        }
    }
    if (installedVersion.empty()) return false;

    // Find catalog version
    for (const auto& entry : m_Catalog) {
        if (entry.name == name) {
            return CompareVersions(entry.version, installedVersion);
        }
    }
    return false;
}

bool PluginRepository::CompareVersions(const std::string& a, const std::string& b) const {
    // Simple semantic version comparison: a > b
    auto parseVersion = [](const std::string& v) -> std::vector<int> {
        std::vector<int> parts;
        std::istringstream stream(v);
        std::string segment;
        while (std::getline(stream, segment, '.')) {
            try { parts.push_back(std::stoi(segment)); }
            catch (...) { parts.push_back(0); }
        }
        while (parts.size() < 3) parts.push_back(0);
        return parts;
    };

    auto va = parseVersion(a);
    auto vb = parseVersion(b);

    for (size_t i = 0; i < 3; i++) {
        if (va[i] > vb[i]) return true;
        if (va[i] < vb[i]) return false;
    }
    return false;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

bool PluginRepository::SaveSources(const std::string& path) const {
    try {
        json j = json::array();
        for (const auto& source : m_Sources) {
            json sj;
            sj["name"] = source.name;
            sj["path"] = source.path;
            sj["enabled"] = source.enabled;
            j.push_back(sj);
        }

        // Ensure parent directory exists
        fs::path parentDir = fs::path(path).parent_path();
        if (!parentDir.empty()) {
            fs::create_directories(parentDir);
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to save repository sources: %s", e.what());
        return false;
    }
}

bool PluginRepository::LoadSources(const std::string& path) {
    if (!fs::exists(path)) return false;

    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        json j;
        file >> j;

        m_Sources.clear();
        if (j.is_array()) {
            for (const auto& sj : j) {
                RepositorySource source;
                source.name = sj.value("name", "");
                source.path = sj.value("path", "");
                source.enabled = sj.value("enabled", true);
                if (!source.name.empty() && !source.path.empty()) {
                    m_Sources.push_back(source);
                }
            }
        }

        ENJIN_LOG_INFO(Script, "Loaded %zu repository sources", m_Sources.size());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to load repository sources: %s", e.what());
        return false;
    }
}

} // namespace Plugin
} // namespace Enjin
