#include "Enjin/Assets/FileWatcher.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Assets {

void FileWatcher::Watch(const std::string& path, Callback callback) {
    try {
        if (!std::filesystem::exists(path)) {
            ENJIN_LOG_WARN(Asset, "FileWatcher: file does not exist: %s", path.c_str());
            return;
        }

        WatchEntry entry;
        entry.path = path;
        entry.lastModTime = std::filesystem::last_write_time(path);
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
            if (!std::filesystem::exists(path)) continue;

            auto currentTime = std::filesystem::last_write_time(path);
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
