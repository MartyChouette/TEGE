#include "Enjin/Assets/FileWatcher.h"
#include "Enjin/Logging/Log.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace Enjin {
namespace Assets {

// Convert a UTF-8 string to a filesystem path (handles Unicode on Windows)
static std::filesystem::path ToFsPath(const std::string& utf8Path) {
#ifdef _WIN32
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, nullptr, 0);
    if (wlen > 0) {
        std::wstring wpath(wlen - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(), -1, wpath.data(), wlen);
        return std::filesystem::path(wpath);
    }
#endif
    return std::filesystem::path(utf8Path);
}

void FileWatcher::Watch(const std::string& path, Callback callback) {
    try {
        auto fsPath = ToFsPath(path);
        if (!std::filesystem::exists(fsPath)) {
            ENJIN_LOG_WARN(Asset, "FileWatcher: file does not exist: %s", path.c_str());
            return;
        }

        WatchEntry entry;
        entry.path = path;
        entry.lastModTime = std::filesystem::last_write_time(fsPath);
        entry.callback = callback;
        m_Entries[path] = std::move(entry);
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "FileWatcher: failed to watch %s: %s", path.c_str(), e.what());
    }
}

void FileWatcher::Unwatch(const std::string& path) {
    m_Entries.erase(path);
}

void FileWatcher::Poll() {
    for (auto& [path, entry] : m_Entries) {
        try {
            auto fsPath = ToFsPath(path);
            if (!std::filesystem::exists(fsPath)) continue;

            auto currentTime = std::filesystem::last_write_time(fsPath);
            if (currentTime != entry.lastModTime) {
                entry.lastModTime = currentTime;
                if (entry.callback) {
                    entry.callback(path);
                }
            }
        } catch (const std::exception&) {
            // File may be temporarily locked during save; ignore
        }
    }
}

void FileWatcher::Clear() {
    m_Entries.clear();
}

} // namespace Assets
} // namespace Enjin
