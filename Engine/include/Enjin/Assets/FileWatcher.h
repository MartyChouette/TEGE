#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <chrono>

namespace Enjin {
namespace Assets {

class ENJIN_API FileWatcher {
public:
    using Callback = std::function<void(const std::string& path)>;

    FileWatcher() = default;
    ~FileWatcher() = default;

    void Watch(const std::string& path, Callback callback);
    void Unwatch(const std::string& path);
    void Poll();
    void Clear();
    usize GetWatchCount() const { return m_Entries.size(); }

private:
    struct WatchEntry {
        std::string path;
        std::filesystem::file_time_type lastModTime;
        Callback callback;
    };

    std::unordered_map<std::string, WatchEntry> m_Entries;
};

} // namespace Assets
} // namespace Enjin
