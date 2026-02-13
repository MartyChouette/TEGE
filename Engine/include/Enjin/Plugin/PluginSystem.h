#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Enjin {

// Forward declarations for plugin context
namespace ECS { class World; class RenderSystem; }
namespace Scripting { class ScriptEngine; }
namespace Audio { class SimpleAudio; }
class SceneManager;

namespace Plugin {

// Context passed to plugins on load — provides access to engine systems
struct PluginContext {
    ECS::World* world = nullptr;
    ECS::RenderSystem* renderSystem = nullptr;
    Scripting::ScriptEngine* scriptEngine = nullptr;
    Audio::SimpleAudio* audio = nullptr;
    SceneManager* sceneManager = nullptr;
};

// Plugin interface — implement to create engine plugins
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual const char* GetName() const = 0;
    virtual const char* GetVersion() const = 0;
    virtual bool OnLoad() = 0;
    virtual bool OnLoad(PluginContext& context) { (void)context; return OnLoad(); }  // Context-aware overload
    virtual void OnUnload() = 0;
    virtual void OnUpdate(f32 deltaTime) = 0;
    virtual void OnEditorUI() {}  // Optional ImGui panel
    virtual void OnSaveState(std::string& outState) { (void)outState; }    // Hot-reload save
    virtual void OnRestoreState(const std::string& state) { (void)state; } // Hot-reload restore
};

// Plugin manifest (loaded from JSON)
struct PluginManifest {
    std::string name;
    std::string version;
    std::string libraryPath;  // .dll / .so / .dylib
    std::string description;
    std::string author;
    std::string category;
    std::vector<std::string> dependencies;
    std::vector<std::string> tags;
    bool loadOnStartup = true;
};

// Plugin entry — runtime state for a loaded plugin
struct PluginEntry {
    PluginManifest manifest;
    IPlugin* instance = nullptr;
    void* libraryHandle = nullptr;
    bool loaded = false;
    bool enabled = true;
    std::string error;
};

// Factory function type exported by plugin libraries
using PluginCreateFunc = IPlugin* (*)();
using PluginDestroyFunc = void (*)(IPlugin*);

class ENJIN_API PluginSystem {
public:
    PluginSystem();
    ~PluginSystem();

    // Scan a directory for plugin manifests (plugin.json files)
    void ScanPluginDirectory(const std::string& directory);

    // Load/unload individual plugins
    bool LoadPlugin(const std::string& name);
    void UnloadPlugin(const std::string& name);
    void UnloadAll();

    // Update all loaded plugins
    void UpdateAll(f32 deltaTime);

    // Query
    const std::vector<PluginEntry>& GetPlugins() const { return m_Plugins; }
    PluginEntry* FindPlugin(const std::string& name);
    bool IsLoaded(const std::string& name) const;

    // Set context that will be passed to plugins on load
    void SetContext(const PluginContext& ctx) { m_Context = ctx; }

    // Draw ImGui management panel
    void DrawPluginPanel();

private:
    void* LoadDynamicLib(const std::string& path);
    void FreeDynamicLib(void* handle);
    void* GetSymbol(void* handle, const std::string& symbolName);
    bool LoadManifest(const std::string& jsonPath, PluginManifest& out);

    std::vector<PluginEntry> m_Plugins;
    std::string m_PluginDirectory;
    PluginContext m_Context;
};

} // namespace Plugin
} // namespace Enjin
