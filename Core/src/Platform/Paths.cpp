#include "Enjin/Platform/Paths.h"

#include <filesystem>
#include <cstdlib>

#if defined(ENJIN_PLATFORM_WINDOWS)
    #include <Windows.h>
    #include <ShlObj.h>
#elif defined(ENJIN_PLATFORM_LINUX)
    #include <unistd.h>
    #include <limits.h>
    #include <cerrno>
    #include <pwd.h>
#elif defined(ENJIN_PLATFORM_MACOS)
    #include <mach-o/dyld.h>
#endif

namespace Enjin::Platform {

static std::string NormalizeDir(std::filesystem::path p) {
    p = p.lexically_normal();
    // parent_path() can be empty if path is relative or unknown
    return p.string();
}

std::string GetExecutablePath() {
#if defined(ENJIN_PLATFORM_WINDOWS)
    char buffer[MAX_PATH]{};
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return {};
    }
    return std::string(buffer, buffer + len);
#elif defined(ENJIN_PLATFORM_LINUX)
    char buffer[PATH_MAX]{};
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0) {
        return {};
    }
    buffer[len] = '\0';
    return std::string(buffer);
#elif defined(ENJIN_PLATFORM_MACOS)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size == 0) {
        return {};
    }
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        return {};
    }
    // Trim possible trailing nulls
    while (!path.empty() && path.back() == '\0') {
        path.pop_back();
    }
    return path;
#else
    return {};
#endif
}

std::string GetExecutableDirectory() {
    const std::string exePath = GetExecutablePath();
    if (exePath.empty()) {
        return {};
    }
    std::filesystem::path p(exePath);
    auto dir = p.parent_path();
    if (dir.empty()) {
        return {};
    }
    return NormalizeDir(dir);
}

bool SetCurrentWorkingDirectory(const std::string& path) {
    if (path.empty()) {
        return false;
    }
#if defined(ENJIN_PLATFORM_WINDOWS)
    return SetCurrentDirectoryA(path.c_str()) != 0;
#else
    return ::chdir(path.c_str()) == 0;
#endif
}

void SetWorkingDirectoryToExecutableDirectory() {
    const std::string dir = GetExecutableDirectory();
    if (dir.empty()) {
        return;
    }
    (void)SetCurrentWorkingDirectory(dir);
}

// =============================================================================
// Platform-specific user directories
// =============================================================================

static std::string EnsureDir(const std::string& path) {
    if (!path.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
    }
    return path;
}

#if defined(ENJIN_PLATFORM_LINUX)
static std::string GetHomeDir() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home);
    }
    // Fallback: passwd entry
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::string(pw->pw_dir);
    }
    return "/tmp";
}
#endif

std::string GetAppUserDataDirectory() {
#if defined(ENJIN_PLATFORM_WINDOWS)
    char path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return EnsureDir(std::string(path) + "\\Enjin");
    }
    return EnsureDir(".\\Enjin");
#elif defined(ENJIN_PLATFORM_LINUX)
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0') {
        return EnsureDir(std::string(xdg) + "/enjin");
    }
    return EnsureDir(GetHomeDir() + "/.local/share/enjin");
#elif defined(ENJIN_PLATFORM_MACOS)
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return EnsureDir(std::string(home) + "/Library/Application Support/Enjin");
    }
    return EnsureDir("./Enjin");
#else
    return ".";
#endif
}

std::string GetAppUserConfigDirectory() {
#if defined(ENJIN_PLATFORM_WINDOWS)
    char path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        return EnsureDir(std::string(path) + "\\Enjin");
    }
    return EnsureDir(".\\Enjin");
#elif defined(ENJIN_PLATFORM_LINUX)
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return EnsureDir(std::string(xdg) + "/enjin");
    }
    return EnsureDir(GetHomeDir() + "/.config/enjin");
#elif defined(ENJIN_PLATFORM_MACOS)
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return EnsureDir(std::string(home) + "/Library/Preferences/Enjin");
    }
    return EnsureDir("./Enjin");
#else
    return ".";
#endif
}

std::string GetAppUserCacheDirectory() {
#if defined(ENJIN_PLATFORM_WINDOWS)
    char path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        return EnsureDir(std::string(path) + "\\Enjin\\cache");
    }
    return EnsureDir(".\\Enjin\\cache");
#elif defined(ENJIN_PLATFORM_LINUX)
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg && xdg[0] != '\0') {
        return EnsureDir(std::string(xdg) + "/enjin");
    }
    return EnsureDir(GetHomeDir() + "/.cache/enjin");
#elif defined(ENJIN_PLATFORM_MACOS)
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return EnsureDir(std::string(home) + "/Library/Caches/Enjin");
    }
    return EnsureDir("./Enjin/cache");
#else
    return ".";
#endif
}

std::string GetAppTempDirectory() {
#if defined(ENJIN_PLATFORM_WINDOWS)
    char path[MAX_PATH]{};
    DWORD len = GetTempPathA(MAX_PATH, path);
    if (len > 0 && len < MAX_PATH) {
        // Remove trailing backslash
        std::string result(path, len);
        while (!result.empty() && (result.back() == '\\' || result.back() == '/')) {
            result.pop_back();
        }
        return result;
    }
    return ".";
#elif defined(ENJIN_PLATFORM_LINUX)
    const char* tmpdir = std::getenv("TMPDIR");
    if (tmpdir && tmpdir[0] != '\0') {
        return std::string(tmpdir);
    }
    return "/tmp";
#elif defined(ENJIN_PLATFORM_MACOS)
    const char* tmpdir = std::getenv("TMPDIR");
    if (tmpdir && tmpdir[0] != '\0') {
        return std::string(tmpdir);
    }
    return "/tmp";
#else
    return ".";
#endif
}

} // namespace Enjin::Platform

