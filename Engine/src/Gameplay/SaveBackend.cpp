#include "Enjin/Gameplay/SaveBackend.h"
#include "Enjin/Gameplay/SaveSystem.h"
#include "Enjin/Logging/Log.h"

#include <fstream>
#include <filesystem>
#include <sstream>

namespace Enjin {
namespace Gameplay {

LocalSaveBackend::LocalSaveBackend(const std::string& baseDirectory) {
    if (baseDirectory.empty()) {
        m_BaseDirectory = SaveSystem::GetSaveDirectory();
    } else {
        m_BaseDirectory = baseDirectory;
        // Ensure trailing separator
        if (!m_BaseDirectory.empty() && m_BaseDirectory.back() != '/' && m_BaseDirectory.back() != '\\') {
#ifdef _WIN32
            m_BaseDirectory += '\\';
#else
            m_BaseDirectory += '/';
#endif
        }
    }
}

std::string LocalSaveBackend::GetFilePath(const std::string& key) const {
    return m_BaseDirectory + key;
}

bool LocalSaveBackend::Write(const std::string& key, const std::string& data) {
    std::string path = GetFilePath(key);
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Editor, "LocalSaveBackend: Failed to open '%s' for writing", path.c_str());
            return false;
        }
        file << data;
        file.close();
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "LocalSaveBackend: Write failed: %s", e.what());
        return false;
    }
}

bool LocalSaveBackend::Read(const std::string& key, std::string& outData) {
    std::string path = GetFilePath(key);
    if (!std::filesystem::exists(path)) return false;

    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        std::ostringstream oss;
        oss << file.rdbuf();
        outData = oss.str();
        file.close();
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "LocalSaveBackend: Read failed: %s", e.what());
        return false;
    }
}

bool LocalSaveBackend::Delete(const std::string& key) {
    std::string path = GetFilePath(key);
    if (!std::filesystem::exists(path)) return true;
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

bool LocalSaveBackend::Exists(const std::string& key) {
    return std::filesystem::exists(GetFilePath(key));
}

} // namespace Gameplay
} // namespace Enjin
