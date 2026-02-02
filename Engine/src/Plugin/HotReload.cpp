#include "Enjin/Plugin/HotReload.h"
#include "Enjin/Logging/Log.h"
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace Enjin {
namespace Plugin {

HotReloadSystem::HotReloadSystem() = default;

HotReloadSystem::~HotReloadSystem()
{
    if (m_LibraryHandle) {
        UnloadGameplay();
    }
}

void HotReloadSystem::ScanSourceFiles()
{
    m_WatchedFiles.clear();

    if (m_SourceDir.empty() || !fs::exists(m_SourceDir)) {
        ENJIN_LOG_WARN(Script, "Source directory does not exist: %s", m_SourceDir.c_str());
        return;
    }

    for (const auto& entry : fs::recursive_directory_iterator(m_SourceDir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".cxx") {
            WatchedFile wf;
            wf.path = entry.path().string();
            wf.lastWriteTime = entry.last_write_time();
            wf.changed = false;
            m_WatchedFiles.push_back(std::move(wf));
        }
    }

    ENJIN_LOG_INFO(Script, "Watching %zu source files in %s", m_WatchedFiles.size(), m_SourceDir.c_str());
}

bool HotReloadSystem::PollChanges()
{
    // Only check every 30 frames to reduce filesystem overhead
    m_PollCounter++;
    if (m_PollCounter < 30) return false;
    m_PollCounter = 0;

    if (m_IsCompiling) return false;

    // Rescan if no files are being watched yet
    if (m_WatchedFiles.empty() && !m_SourceDir.empty()) {
        ScanSourceFiles();
    }

    bool anyChanged = false;

    for (auto& wf : m_WatchedFiles) {
        try {
            if (!fs::exists(wf.path)) continue;

            auto currentTime = fs::last_write_time(wf.path);
            if (currentTime != wf.lastWriteTime) {
                wf.lastWriteTime = currentTime;
                wf.changed = true;
                anyChanged = true;
                ENJIN_LOG_INFO(Script, "File changed: %s", wf.path.c_str());
            }
        } catch (const fs::filesystem_error& e) {
            ENJIN_LOG_WARN(Script, "Error checking file %s: %s", wf.path.c_str(), e.what());
        }
    }

    // Check for new files that were added since last scan
    if (!m_SourceDir.empty() && fs::exists(m_SourceDir)) {
        for (const auto& entry : fs::recursive_directory_iterator(m_SourceDir)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".h" && ext != ".hpp" && ext != ".cxx") continue;

            std::string pathStr = entry.path().string();
            bool found = false;
            for (const auto& wf : m_WatchedFiles) {
                if (wf.path == pathStr) { found = true; break; }
            }

            if (!found) {
                WatchedFile wf;
                wf.path = pathStr;
                wf.lastWriteTime = entry.last_write_time();
                wf.changed = true;
                m_WatchedFiles.push_back(std::move(wf));
                anyChanged = true;
                ENJIN_LOG_INFO(Script, "New file detected: %s", pathStr.c_str());
            }
        }
    }

    return anyChanged;
}

bool HotReloadSystem::Reload()
{
    ENJIN_LOG_INFO(Script, "Starting hot reload cycle #%u", m_ReloadCount + 1);

    // Save current gameplay state
    GameplayState state = SaveState();

    // Unload the current gameplay library
    UnloadGameplay();

    // Compile the updated source
    if (!Compile()) {
        ENJIN_LOG_ERROR(Script, "Hot reload failed: compilation error");
        if (m_OnReload) m_OnReload(false);
        return false;
    }

    // Load the newly compiled library
    if (!LoadGameplay()) {
        ENJIN_LOG_ERROR(Script, "Hot reload failed: could not load library");
        if (m_OnReload) m_OnReload(false);
        return false;
    }

    // Restore the gameplay state
    RestoreState(state);

    m_ReloadCount++;

    // Clear changed flags
    for (auto& wf : m_WatchedFiles) {
        wf.changed = false;
    }

    ENJIN_LOG_INFO(Script, "Hot reload #%u completed successfully", m_ReloadCount);
    if (m_OnReload) m_OnReload(true);
    return true;
}

GameplayState HotReloadSystem::SaveState()
{
    ENJIN_LOG_INFO(Script, "Saving gameplay state before reload");

    // Placeholder: returns empty state
    // Users can extend this by serializing gameplay-specific data into
    // the serializedData map before a reload occurs
    GameplayState state;
    return state;
}

void HotReloadSystem::UnloadGameplay()
{
    if (m_LibraryHandle) {
        ENJIN_LOG_INFO(Script, "Unloading gameplay library");
        FreeDynamicLib(m_LibraryHandle);
        m_LibraryHandle = nullptr;
    }
}

bool HotReloadSystem::Compile()
{
    if (m_CompileCommand.empty()) {
        m_LastError = "No compile command configured";
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    m_IsCompiling = true;
    ENJIN_LOG_INFO(Script, "Compiling gameplay module: %s", m_CompileCommand.c_str());

    int result = std::system(m_CompileCommand.c_str());

    m_IsCompiling = false;

    if (result != 0) {
        m_LastError = "Compile command returned error code " + std::to_string(result);
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Script, "Compilation succeeded");
    return true;
}

bool HotReloadSystem::LoadGameplay()
{
    std::string libraryPath;

#ifdef _WIN32
    libraryPath = m_BuildDir + "/" + m_LibraryName + ".dll";
#elif defined(__APPLE__)
    libraryPath = m_BuildDir + "/lib" + m_LibraryName + ".dylib";
#else
    libraryPath = m_BuildDir + "/lib" + m_LibraryName + ".so";
#endif

    if (!fs::exists(libraryPath)) {
        m_LastError = "Library not found: " + libraryPath;
        ENJIN_LOG_ERROR(Script, "%s", m_LastError.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Script, "Loading gameplay library: %s", libraryPath.c_str());

    m_LibraryHandle = LoadDynamicLib(libraryPath);
    if (!m_LibraryHandle) {
        ENJIN_LOG_ERROR(Script, "Failed to load library: %s", m_LastError.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Script, "Gameplay library loaded successfully");
    return true;
}

void HotReloadSystem::RestoreState(const GameplayState& state)
{
    ENJIN_LOG_INFO(Script, "Restoring gameplay state (%zu entries)", state.serializedData.size());

    // Placeholder: users extend this to deserialize their gameplay data
    // back into the newly loaded module
    (void)state;
}

void* HotReloadSystem::LoadDynamicLib(const std::string& path)
{
#ifdef _WIN32
    HMODULE handle = ::LoadLibraryA(path.c_str());
    if (!handle) {
        DWORD error = ::GetLastError();
        m_LastError = "LoadLibraryA failed with error code " + std::to_string(error);
        return nullptr;
    }
    return static_cast<void*>(handle);
#else
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        m_LastError = dlerror();
        return nullptr;
    }
    return handle;
#endif
}

void HotReloadSystem::FreeDynamicLib(void* handle)
{
    if (!handle) return;

#ifdef _WIN32
    ::FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

} // namespace Plugin
} // namespace Enjin
