#include "Enjin/Platform/Paths.h"
#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif !defined(__EMSCRIPTEN__)
#include <cstdio>
#include <unistd.h>
#endif

#include <algorithm>
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

// =============================================================================
// Path sanitization
// =============================================================================

bool IsSafeRelativePath(const std::string& relative) {
    if (relative.empty()) {
        return false;
    }
    // Validate identically on every platform: content files travel between
    // platforms, so a path that is unsafe on Windows ("..\\x", "C:evil") must
    // be rejected on Linux too, where '\\' is not a separator and "C:" is not
    // a root. Treat backslashes as separators and refuse colons outright
    // (':' is illegal in Windows file names, so no legitimate relative asset
    // path contains one).
    if (relative.find(':') != std::string::npos) {
        return false;
    }
    std::string normalized = relative;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::filesystem::path p(normalized);
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) {
        return false;
    }
    // After normalization any remaining ".." component escapes upward
    // ("a/../b" collapses harmlessly; "../b" survives as a ".." component).
    for (const auto& component : p.lexically_normal()) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

bool IsSafeFileName(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        return false;
    }
    if (name.find("..") != std::string::npos) {
        return false;
    }
    return true;
}

std::string ResolveWithinRoot(const std::string& root, const std::string& relative) {
    if (root.empty() || relative.empty()) {
        return "";
    }
    auto resolved = (std::filesystem::path(root) / relative).lexically_normal();
    auto rootNorm = std::filesystem::path(root).lexically_normal();
    auto resolvedStr = resolved.string();
    auto rootStr = rootNorm.string();
    // Accept the root itself, or anything under root + separator. The
    // separator boundary matters: "C:/proj2/x" must not pass for root
    // "C:/proj" even though it shares the string prefix.
    if (resolvedStr != rootStr &&
        resolvedStr.find(rootStr + std::string(1, std::filesystem::path::preferred_separator)) != 0) {
        return "";
    }
    return resolvedStr;
}

std::string MakeRelativeToRoot(const std::string& root, const std::string& absolute) {
    if (root.empty() || absolute.empty()) {
        return "";
    }
    auto rootNorm = std::filesystem::path(root).lexically_normal();
    auto absNorm = std::filesystem::path(absolute).lexically_normal();
#if defined(ENJIN_PLATFORM_WINDOWS)
    std::string rootStr = rootNorm.string();
    std::string absStr = absNorm.string();
    if (rootStr.size() >= 2 && rootStr[1] == ':') rootStr[0] = static_cast<char>(::tolower(static_cast<unsigned char>(rootStr[0])));
    if (absStr.size() >= 2 && absStr[1] == ':') absStr[0] = static_cast<char>(::tolower(static_cast<unsigned char>(absStr[0])));
    rootNorm = std::filesystem::path(rootStr);
    absNorm = std::filesystem::path(absStr);
#endif
    auto rel = absNorm.lexically_relative(rootNorm);
    if (rel.empty() || rel.begin()->string() == "..") {
        return "";
    }
    return rel.string();
}

u64 GetProcessMemoryBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<u64>(pmc.WorkingSetSize);
    }
    return 0;
#elif defined(__EMSCRIPTEN__)
    return 0;  // no meaningful RSS in wasm
#else
    // Linux: /proc/self/statm, second field = resident pages
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long size = 0, resident = 0;
    int n = std::fscanf(f, "%ld %ld", &size, &resident);
    std::fclose(f);
    if (n < 2) return 0;
    return static_cast<u64>(resident) * static_cast<u64>(sysconf(_SC_PAGESIZE));
#endif
}

} // namespace Enjin::Platform

