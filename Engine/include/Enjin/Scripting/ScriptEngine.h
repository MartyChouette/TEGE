#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Components/Script.h"
#include <angelscript.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <functional>

class CScriptBuilder;

namespace Enjin {
namespace ECS { class World; }
namespace Scripting {

class ENJIN_API ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    bool Initialize();
    void Shutdown();

    // Compile a script file into a module, returns true on success
    bool CompileScript(const std::string& path);

    // Compile script from in-memory source (for packed assets)
    bool CompileScriptFromMemory(const std::string& moduleName, const std::string& source);

    // Create an instance of a class from a compiled module
    // Returns asIScriptObject* (caller must release)
    asIScriptObject* CreateInstance(const std::string& moduleName, const std::string& className);

    // Release a script instance
    void ReleaseInstance(asIScriptObject* obj);

    // Get a context from the pool (returns to pool when done)
    asIScriptContext* AcquireContext();
    void ReturnContext(asIScriptContext* ctx);

    // Find a method on a script object type
    asIScriptFunction* FindMethod(asIScriptObject* obj, const std::string& decl);

    // Execute a method on an object with error handling
    // Returns true if execution succeeded
    bool ExecuteMethod(asIScriptContext* ctx, asIScriptObject* obj, asIScriptFunction* func);

    // Hot reload
    void SetScriptDirectory(const std::string& dir);
    void PollFileChanges();  // Call each frame (throttled internally)
    bool ProcessHotReload(); // Recompile changed modules, returns true if anything reloaded

    // Locate the engine-provided enjin_api/ script headers (TegeBehavior.as
    // etc.). Searches: <scriptDir>/enjin_api, ./enjin_api (exe dir), then a
    // walk up from the CWD (dev tree: repo-root/enjin_api from build/bin/...).
    // Returns empty path if not found. scriptDir may be empty.
    static std::filesystem::path FindApiDirectory(const std::string& scriptDir);

    // Get the underlying AngelScript engine
    asIScriptEngine* GetASEngine() { return m_Engine; }

    // Get last compilation error
    const std::string& GetLastError() const { return m_LastError; }

    // List all compiled class names that derive from TegeBehavior
    std::vector<std::string> GetBehaviorClasses() const;

    // Get all script files in the script directory
    std::vector<std::string> GetScriptFiles() const;

    // Set the ECS world (needed for entity bindings)
    void SetWorld(ECS::World* world) { m_World = world; }
    ECS::World* GetWorld() const { return m_World; }

    // Apply property values from ScriptAttachment to a live instance
    void ApplyProperties(asIScriptObject* obj, const std::vector<ECS::ScriptProperty>& properties);

    // Read property values from a live instance back into ScriptProperty list
    void ReadProperties(asIScriptObject* obj, std::vector<ECS::ScriptProperty>& properties);

private:
    // AngelScript message callback
    static void MessageCallback(const asSMessageInfo* msg, void* param);

    // Include callback for scriptbuilder
    static int IncludeCallback(const char* include, const char* from, CScriptBuilder* builder, void* userParam);

    asIScriptEngine* m_Engine = nullptr;
    std::vector<asIScriptContext*> m_ContextPool;

    // Module tracking
    struct ModuleInfo {
        std::string filePath;
        std::string moduleName;
        std::filesystem::file_time_type lastModified;
        bool needsReload = false;
    };
    std::unordered_map<std::string, ModuleInfo> m_Modules; // moduleName -> info

    std::string m_ScriptDirectory;
    std::string m_LastError;
    u32 m_PollCounter = 0;
    static constexpr u32 POLL_INTERVAL = 30; // Check every 30 frames

    // Execution timeout (instruction limit per Execute call)
    static constexpr u32 MAX_INSTRUCTIONS = 1000000u; // 1M instructions
    static void LineCallback(asIScriptContext* ctx, void* param);

    ECS::World* m_World = nullptr;
};

} // namespace Scripting
} // namespace Enjin
