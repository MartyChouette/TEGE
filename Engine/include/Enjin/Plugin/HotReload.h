#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>

namespace Enjin {
namespace Plugin {

// Macro for marking a class as hot-reloadable
#define ENJIN_GAMEPLAY_CLASS(ClassName) \
    extern "C" ENJIN_API void* Create_##ClassName() { return new ClassName(); } \
    extern "C" ENJIN_API void Destroy_##ClassName(void* ptr) { delete static_cast<ClassName*>(ptr); }

// State of a watched source file
struct WatchedFile {
    std::string path;
    std::filesystem::file_time_type lastWriteTime;
    bool changed = false;
};

// State snapshot for save/restore during reload
struct GameplayState {
    std::unordered_map<std::string, std::vector<u8>> serializedData;
};

// Hot reload system for gameplay C++ code
class ENJIN_API HotReloadSystem {
public:
    HotReloadSystem();
    ~HotReloadSystem();

    // Configuration
    void SetSourceDirectory(const std::string& dir) { m_SourceDir = dir; }
    void SetBuildDirectory(const std::string& dir) { m_BuildDir = dir; }
    void SetLibraryName(const std::string& name) { m_LibraryName = name; }
    void SetCompileCommand(const std::string& cmd) { m_CompileCommand = cmd; }

    // Watch for file changes (call every frame)
    bool PollChanges();

    // Full reload cycle: save state -> unload -> compile -> reload -> restore
    bool Reload();

    // Manual steps
    GameplayState SaveState();
    void UnloadGameplay();
    bool Compile();
    bool LoadGameplay();
    void RestoreState(const GameplayState& state);

    // Query
    bool IsLoaded() const { return m_LibraryHandle != nullptr; }
    bool IsCompiling() const { return m_IsCompiling; }
    const std::string& GetLastError() const { return m_LastError; }
    u32 GetReloadCount() const { return m_ReloadCount; }

    // Callbacks
    using ReloadCallback = std::function<void(bool success)>;
    void SetOnReload(ReloadCallback cb) { m_OnReload = cb; }

private:
    void ScanSourceFiles();
    void* LoadDynamicLib(const std::string& path);
    void FreeDynamicLib(void* handle);

    std::string m_SourceDir;
    std::string m_BuildDir;
    std::string m_LibraryName = "GameplayModule";
    std::string m_CompileCommand;
    std::string m_LastError;

    void* m_LibraryHandle = nullptr;
    std::vector<WatchedFile> m_WatchedFiles;
    bool m_IsCompiling = false;
    u32 m_ReloadCount = 0;
    u32 m_PollCounter = 0;

    ReloadCallback m_OnReload;
};

} // namespace Plugin
} // namespace Enjin
