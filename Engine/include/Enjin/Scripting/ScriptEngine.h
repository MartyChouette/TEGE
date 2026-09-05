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
namespace Build { class AssetReader; }
namespace ECS { class World; }
namespace Scripting {

class ENJIN_API ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    bool Initialize();
    void Shutdown();

    // Set AssetReader for packed script loading from .enjpak
    void SetAssetReader(Build::AssetReader* reader) { m_AssetReader = reader; }
    Build::AssetReader* GetAssetReader() const { return m_AssetReader; }

    // Read script source from AssetReader or disk
    bool ReadScriptSource(const std::string& path, std::string& outSource) const;

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

    // Call after registering bindings on GetASEngine() outside Initialize().
    // AngelScript resolves each native function's return-value ABI once, when
    // the first context is created, and marks that work stale on every later
    // registration without ever redoing it. Retiring the pool means the next
    // context is a new one, which is what makes AngelScript redo it. Calling
    // this is cheap and safe at any time; not calling it after a late
    // registration means those functions return garbage.
    void InvalidateContextPool();

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

    // ---- diagnostic capture ----
    //
    // Compile errors reach the console and nowhere else, so a broken script
    // surfaces at PLAY time as "class not found" in whatever scene referenced
    // it -- often a different file from the one with the error. There was no
    // way to ask "do these scripts compile" without launching the editor,
    // which is why one project ships its own linter that cannot catch a type
    // error.
    //
    // While capture is on, every message the compiler emits is collected here
    // as well as logged. m_LastError only ever held the LAST one, which is no
    // use when a file has six errors.
    struct Diagnostic {
        std::string file;      // the script section, i.e. the source path
        i32 row = 0;
        i32 col = 0;
        std::string message;
        bool isError = false;  // false = warning
    };

    void BeginDiagnosticCapture();
    void EndDiagnosticCapture();
    const std::vector<Diagnostic>& GetDiagnostics() const { return m_Diagnostics; }
    void ClearDiagnostics() { m_Diagnostics.clear(); }

    // Monotonic count of runtime script exceptions this session. Pollable:
    // the replay recorder auto-bookmarks the frame where the count rises.
    u32 GetExceptionCount() const { return m_ExceptionCount; }

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
    // Filled on first use, never in Initialize: a context created before the
    // application's bindings are registered carries a stale view of them.
    std::vector<asIScriptContext*> m_ContextPool;
    bool m_ContextPoolPrimed = false;
    void PrimeContextPool();

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
    std::vector<Diagnostic> m_Diagnostics;
    bool m_CapturingDiagnostics = false;
    u32 m_ExceptionCount = 0;
    u32 m_PollCounter = 0;
    static constexpr u32 POLL_INTERVAL = 30; // Check every 30 frames

    // Execution timeout (instruction limit per Execute call)
    static constexpr u32 MAX_INSTRUCTIONS = 1000000u; // 1M instructions
    static void LineCallback(asIScriptContext* ctx, void* param);

    ECS::World* m_World = nullptr;
    Build::AssetReader* m_AssetReader = nullptr;
};

} // namespace Scripting
} // namespace Enjin
