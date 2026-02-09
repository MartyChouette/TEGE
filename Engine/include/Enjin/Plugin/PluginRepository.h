#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Plugin {

// Forward declaration
class PluginSystem;

// A single entry in a plugin catalog
struct PluginCatalogEntry {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string category;
    std::string localPath;          // Relative path within the repository
    std::string minEngineVersion;
    u64 sizeBytes = 0;
    std::vector<std::string> dependencies;
    std::vector<std::string> tags;
};

// A repository source (local directory or network URL)
struct RepositorySource {
    std::string name;
    std::string path;
    bool enabled = true;
};

class ENJIN_API PluginRepository {
public:
    // Source management
    void AddSource(const RepositorySource& source);
    void RemoveSource(const std::string& name);
    const std::vector<RepositorySource>& GetSources() const { return m_Sources; }

    // Catalog
    void RefreshCatalog();
    const std::vector<PluginCatalogEntry>& GetCatalog() const { return m_Catalog; }

    // Search
    std::vector<const PluginCatalogEntry*> Search(const std::string& query) const;
    std::vector<const PluginCatalogEntry*> GetByCategory(const std::string& category) const;
    std::vector<std::string> GetCategories() const;

    // Install/Uninstall
    bool Install(const std::string& name, const std::string& targetDir);
    bool Uninstall(const std::string& name, const std::string& pluginsDir);

    // Status checks
    bool IsInstalled(const std::string& name, const PluginSystem* system) const;
    bool HasUpdate(const std::string& name, const PluginSystem* system) const;

    // Persistence
    bool SaveSources(const std::string& path) const;
    bool LoadSources(const std::string& path);

private:
    bool LoadRepositoryJson(const std::string& path);
    bool CompareVersions(const std::string& a, const std::string& b) const; // true if a > b

    std::vector<RepositorySource> m_Sources;
    std::vector<PluginCatalogEntry> m_Catalog;
};

} // namespace Plugin
} // namespace Enjin
