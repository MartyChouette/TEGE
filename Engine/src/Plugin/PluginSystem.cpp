#include "Enjin/Plugin/PluginSystem.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace Enjin {
namespace Plugin {

namespace fs = std::filesystem;

PluginSystem::PluginSystem() = default;

PluginSystem::~PluginSystem() {
    UnloadAll();
}

void PluginSystem::ScanPluginDirectory(const std::string& directory) {
    m_PluginDirectory = directory;

    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        ENJIN_LOG_WARN(Script, "Plugin directory does not exist: %s", directory.c_str());
        return;
    }

    ENJIN_LOG_INFO(Script, "Scanning plugin directory: %s", directory.c_str());

    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().filename() == "plugin.json") {
            PluginManifest manifest;
            if (LoadManifest(entry.path().string(), manifest)) {
                // Check if plugin already known
                bool found = false;
                for (auto& existing : m_Plugins) {
                    if (existing.manifest.name == manifest.name) {
                        existing.manifest = manifest;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    PluginEntry pluginEntry;
                    pluginEntry.manifest = manifest;

                    // Resolve library path relative to the plugin.json directory
                    fs::path manifestDir = entry.path().parent_path();
                    fs::path libPath = manifestDir / manifest.libraryPath;
                    pluginEntry.manifest.libraryPath = libPath.string();

                    m_Plugins.push_back(pluginEntry);
                    ENJIN_LOG_INFO(Script, "Found plugin: %s v%s",
                        manifest.name.c_str(), manifest.version.c_str());
                }
            }
        }
    }

    ENJIN_LOG_INFO(Script, "Found %zu plugin(s)", m_Plugins.size());

    // Auto-load plugins marked for startup
    for (auto& plugin : m_Plugins) {
        if (plugin.manifest.loadOnStartup && !plugin.loaded) {
            LoadPlugin(plugin.manifest.name);
        }
    }
}

bool PluginSystem::LoadPlugin(const std::string& name) {
    PluginEntry* entry = FindPlugin(name);
    if (!entry) {
        ENJIN_LOG_ERROR(Script, "Plugin not found: %s", name.c_str());
        return false;
    }

    if (entry->loaded) {
        ENJIN_LOG_WARN(Script, "Plugin already loaded: %s", name.c_str());
        return true;
    }

    // Check dependencies are loaded
    for (const auto& dep : entry->manifest.dependencies) {
        if (!IsLoaded(dep)) {
            ENJIN_LOG_ERROR(Script, "Plugin '%s' depends on '%s' which is not loaded",
                name.c_str(), dep.c_str());
            entry->error = "Missing dependency: " + dep;
            return false;
        }
    }

    // Load the shared library
    ENJIN_LOG_INFO(Script, "Loading plugin library: %s", entry->manifest.libraryPath.c_str());
    entry->libraryHandle = LoadDynamicLib(entry->manifest.libraryPath);
    if (!entry->libraryHandle) {
        ENJIN_LOG_ERROR(Script, "Failed to load library for plugin: %s", name.c_str());
        entry->error = "Failed to load library";
        return false;
    }

    // Find the CreatePlugin symbol
    auto createFunc = reinterpret_cast<PluginCreateFunc>(
        GetSymbol(entry->libraryHandle, "CreatePlugin"));
    if (!createFunc) {
        ENJIN_LOG_ERROR(Script, "Plugin '%s' missing CreatePlugin symbol", name.c_str());
        entry->error = "Missing CreatePlugin export";
        FreeDynamicLib(entry->libraryHandle);
        entry->libraryHandle = nullptr;
        return false;
    }

    // Create the plugin instance
    entry->instance = createFunc();
    if (!entry->instance) {
        ENJIN_LOG_ERROR(Script, "CreatePlugin returned null for plugin: %s", name.c_str());
        entry->error = "CreatePlugin returned null";
        FreeDynamicLib(entry->libraryHandle);
        entry->libraryHandle = nullptr;
        return false;
    }

    // Call OnLoad
    if (!entry->instance->OnLoad()) {
        ENJIN_LOG_ERROR(Script, "Plugin '%s' OnLoad failed", name.c_str());
        entry->error = "OnLoad failed";

        // Try to destroy the instance
        auto destroyFunc = reinterpret_cast<PluginDestroyFunc>(
            GetSymbol(entry->libraryHandle, "DestroyPlugin"));
        if (destroyFunc) {
            destroyFunc(entry->instance);
        }
        entry->instance = nullptr;

        FreeDynamicLib(entry->libraryHandle);
        entry->libraryHandle = nullptr;
        return false;
    }

    entry->loaded = true;
    entry->enabled = true;
    entry->error.clear();
    ENJIN_LOG_INFO(Script, "Plugin loaded successfully: %s v%s",
        entry->instance->GetName(), entry->instance->GetVersion());
    return true;
}

void PluginSystem::UnloadPlugin(const std::string& name) {
    PluginEntry* entry = FindPlugin(name);
    if (!entry || !entry->loaded) {
        return;
    }

    ENJIN_LOG_INFO(Script, "Unloading plugin: %s", name.c_str());

    // Call OnUnload
    if (entry->instance) {
        entry->instance->OnUnload();
    }

    // Destroy the instance via the library's DestroyPlugin
    if (entry->libraryHandle && entry->instance) {
        auto destroyFunc = reinterpret_cast<PluginDestroyFunc>(
            GetSymbol(entry->libraryHandle, "DestroyPlugin"));
        if (destroyFunc) {
            destroyFunc(entry->instance);
        }
    }
    entry->instance = nullptr;

    // Free the library
    if (entry->libraryHandle) {
        FreeDynamicLib(entry->libraryHandle);
        entry->libraryHandle = nullptr;
    }

    entry->loaded = false;
    entry->enabled = false;
}

void PluginSystem::UnloadAll() {
    // Unload in reverse order to respect dependencies
    for (auto it = m_Plugins.rbegin(); it != m_Plugins.rend(); ++it) {
        if (it->loaded) {
            UnloadPlugin(it->manifest.name);
        }
    }
}

void PluginSystem::UpdateAll(f32 deltaTime) {
    for (auto& plugin : m_Plugins) {
        if (plugin.loaded && plugin.enabled && plugin.instance) {
            plugin.instance->OnUpdate(deltaTime);
        }
    }
}

PluginEntry* PluginSystem::FindPlugin(const std::string& name) {
    for (auto& plugin : m_Plugins) {
        if (plugin.manifest.name == name) {
            return &plugin;
        }
    }
    return nullptr;
}

bool PluginSystem::IsLoaded(const std::string& name) const {
    for (const auto& plugin : m_Plugins) {
        if (plugin.manifest.name == name) {
            return plugin.loaded;
        }
    }
    return false;
}

void PluginSystem::DrawPluginPanel() {
    if (!ImGui::Begin("Plugins")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Plugin Directory: %s", m_PluginDirectory.c_str());
    ImGui::Separator();

    if (ImGui::Button("Rescan Plugins")) {
        if (!m_PluginDirectory.empty()) {
            ScanPluginDirectory(m_PluginDirectory);
        }
    }

    ImGui::Separator();

    if (m_Plugins.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No plugins found.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTable("PluginTable", 5,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (auto& plugin : m_Plugins) {
            ImGui::TableNextRow();

            // Name
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", plugin.manifest.name.c_str());
            if (!plugin.manifest.description.empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", plugin.manifest.description.c_str());
            }

            // Version
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", plugin.manifest.version.c_str());

            // Status
            ImGui::TableSetColumnIndex(2);
            if (plugin.loaded) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Loaded");
            } else if (!plugin.error.empty()) {
                ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "Error");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", plugin.error.c_str());
                }
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Unloaded");
            }

            // Enabled checkbox
            ImGui::TableSetColumnIndex(3);
            if (plugin.loaded) {
                ImGui::PushID(&plugin);
                ImGui::Checkbox("##enabled", &plugin.enabled);
                ImGui::PopID();
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "--");
            }

            // Load/Unload button
            ImGui::TableSetColumnIndex(4);
            ImGui::PushID(&plugin);
            if (plugin.loaded) {
                if (ImGui::Button("Unload")) {
                    UnloadPlugin(plugin.manifest.name);
                }
            } else {
                if (ImGui::Button("Load")) {
                    LoadPlugin(plugin.manifest.name);
                }
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

// --- Platform-specific library loading ---

void* PluginSystem::LoadDynamicLib(const std::string& path) {
#ifdef _WIN32
    HMODULE handle = ::LoadLibraryA(path.c_str());
    if (!handle) {
        DWORD err = ::GetLastError();
        ENJIN_LOG_ERROR(Script, "LoadLibrary failed (error %lu): %s", err, path.c_str());
    }
    return static_cast<void*>(handle);
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* err = dlerror();
        ENJIN_LOG_ERROR(Script, "dlopen failed: %s", err ? err : "unknown error");
    }
    return handle;
#endif
}

void PluginSystem::FreeDynamicLib(void* handle) {
    if (!handle) return;
#ifdef _WIN32
    ::FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* PluginSystem::GetSymbol(void* handle, const std::string& symbolName) {
    if (!handle) return nullptr;
#ifdef _WIN32
    void* sym = reinterpret_cast<void*>(
        ::GetProcAddress(static_cast<HMODULE>(handle), symbolName.c_str()));
    if (!sym) {
        ENJIN_LOG_WARN(Script, "GetProcAddress failed for symbol: %s", symbolName.c_str());
    }
    return sym;
#else
    dlerror(); // Clear any previous error
    void* sym = dlsym(handle, symbolName.c_str());
    const char* err = dlerror();
    if (err) {
        ENJIN_LOG_WARN(Script, "dlsym failed for symbol '%s': %s", symbolName.c_str(), err);
        return nullptr;
    }
    return sym;
#endif
}

bool PluginSystem::LoadManifest(const std::string& jsonPath, PluginManifest& out) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Script, "Failed to open plugin manifest: %s", jsonPath.c_str());
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        out.name = j.value("name", "");
        out.version = j.value("version", "0.0.0");
        out.libraryPath = j.value("library", "");
        out.description = j.value("description", "");
        out.author = j.value("author", "");
        out.category = j.value("category", "");
        out.loadOnStartup = j.value("loadOnStartup", true);

        if (j.contains("dependencies") && j["dependencies"].is_array()) {
            for (const auto& dep : j["dependencies"]) {
                out.dependencies.push_back(dep.get<std::string>());
            }
        }

        if (j.contains("tags") && j["tags"].is_array()) {
            for (const auto& tag : j["tags"]) {
                out.tags.push_back(tag.get<std::string>());
            }
        }

        if (out.name.empty()) {
            ENJIN_LOG_ERROR(Script, "Plugin manifest missing 'name' field: %s", jsonPath.c_str());
            return false;
        }
        if (out.libraryPath.empty()) {
            ENJIN_LOG_ERROR(Script, "Plugin manifest missing 'library' field: %s", jsonPath.c_str());
            return false;
        }

        return true;
    } catch (const nlohmann::json::exception& e) {
        ENJIN_LOG_ERROR(Script, "Failed to parse plugin manifest '%s': %s", jsonPath.c_str(), e.what());
        return false;
    }
}

} // namespace Plugin
} // namespace Enjin
