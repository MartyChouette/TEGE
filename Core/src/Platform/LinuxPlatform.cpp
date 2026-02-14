#include "Enjin/Platform/LinuxPlatform.h"

#include <cstdlib>
#include <filesystem>

#ifdef ENJIN_PLATFORM_LINUX
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <spawn.h>
#include <signal.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <climits>
#include <array>
#include <chrono>
#include <thread>

extern char** environ;
#endif

namespace Enjin::Platform {

// =============================================================================
// XDG Base Directory helpers
// =============================================================================

std::string GetXDGConfigDir() {
#ifdef ENJIN_PLATFORM_LINUX
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg);
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.config";
    }
    return "/tmp";
#else
    return ".";
#endif
}

std::string GetXDGDataDir() {
#ifdef ENJIN_PLATFORM_LINUX
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg);
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.local/share";
    }
    return "/tmp";
#else
    return ".";
#endif
}

std::string GetXDGCacheDir() {
#ifdef ENJIN_PLATFORM_LINUX
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg);
    }
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + "/.cache";
    }
    return "/tmp";
#else
    return ".";
#endif
}

std::string GetXDGRuntimeDir() {
#ifdef ENJIN_PLATFORM_LINUX
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] != '\0') {
        return std::string(xdg);
    }
    // Fallback: /tmp/enjin-<uid>
    return std::string("/tmp/enjin-") + std::to_string(getuid());
#else
    return ".";
#endif
}

// =============================================================================
// User directories for Enjin
// =============================================================================

std::string GetUserConfigDirectory() {
#ifdef ENJIN_PLATFORM_LINUX
    std::string dir = GetXDGConfigDir() + "/enjin";
    std::filesystem::create_directories(dir);
    return dir;
#else
    return ".";
#endif
}

std::string GetUserDataDirectory() {
#ifdef ENJIN_PLATFORM_LINUX
    std::string dir = GetXDGDataDir() + "/enjin";
    std::filesystem::create_directories(dir);
    return dir;
#else
    return ".";
#endif
}

std::string GetUserCacheDirectory() {
#ifdef ENJIN_PLATFORM_LINUX
    std::string dir = GetXDGCacheDir() + "/enjin";
    std::filesystem::create_directories(dir);
    return dir;
#else
    return ".";
#endif
}

std::string GetTempDirectory() {
#ifdef ENJIN_PLATFORM_LINUX
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
// Process creation
// =============================================================================

ProcessResult LaunchProcess(
    const std::string& executable,
    const std::vector<std::string>& args,
    i32 timeoutMs
) {
    ProcessResult result;

#ifdef ENJIN_PLATFORM_LINUX
    // Create pipes for stdout and stderr
    int stdoutPipe[2];
    int stderrPipe[2];
    if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0) {
        result.stderrOutput = "Failed to create pipes";
        return result;
    }

    // Build argv array (null-terminated)
    std::vector<const char*> argv;
    argv.push_back(executable.c_str());
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    // Set up file actions for posix_spawn
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, stdoutPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, stderrPipe[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, stdoutPipe[0]);
    posix_spawn_file_actions_addclose(&actions, stderrPipe[0]);
    posix_spawn_file_actions_addclose(&actions, stdoutPipe[1]);
    posix_spawn_file_actions_addclose(&actions, stderrPipe[1]);

    pid_t pid = 0;
    int spawnResult = posix_spawnp(
        &pid,
        executable.c_str(),
        &actions,
        nullptr,
        const_cast<char**>(reinterpret_cast<const char* const*>(argv.data())),
        environ
    );
    posix_spawn_file_actions_destroy(&actions);

    // Close write ends in parent
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    if (spawnResult != 0) {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        result.stderrOutput = std::string("posix_spawn failed: ") + strerror(spawnResult);
        return result;
    }

    // Set pipes to non-blocking for timeout support
    fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderrPipe[0], F_SETFL, O_NONBLOCK);

    // Read stdout and stderr with timeout
    auto startTime = std::chrono::steady_clock::now();
    bool timedOut = false;
    bool stdoutDone = false;
    bool stderrDone = false;
    std::array<char, 4096> buffer{};

    while (!stdoutDone || !stderrDone) {
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();
        if (timeoutMs > 0 && elapsed >= timeoutMs) {
            timedOut = true;
            kill(pid, SIGKILL);
            break;
        }

        bool didRead = false;

        if (!stdoutDone) {
            ssize_t n = read(stdoutPipe[0], buffer.data(), buffer.size() - 1);
            if (n > 0) {
                buffer[static_cast<usize>(n)] = '\0';
                result.stdoutOutput += buffer.data();
                didRead = true;
            } else if (n == 0) {
                stdoutDone = true;
            }
            // EAGAIN/EWOULDBLOCK = no data yet, keep trying
        }

        if (!stderrDone) {
            ssize_t n = read(stderrPipe[0], buffer.data(), buffer.size() - 1);
            if (n > 0) {
                buffer[static_cast<usize>(n)] = '\0';
                result.stderrOutput += buffer.data();
                didRead = true;
            } else if (n == 0) {
                stderrDone = true;
            }
        }

        if (!didRead) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    // Wait for process to finish
    int status = 0;
    waitpid(pid, &status, 0);

    if (timedOut) {
        result.exitCode = -1;
        result.stderrOutput += "\nProcess timed out";
        result.success = false;
    } else if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
        result.success = (result.exitCode == 0);
    } else {
        result.exitCode = -1;
        result.success = false;
    }
#else
    (void)executable;
    (void)args;
    (void)timeoutMs;
    result.stderrOutput = "LaunchProcess not available on this platform";
#endif

    return result;
}

i64 LaunchProcessAsync(
    const std::string& executable,
    const std::vector<std::string>& args
) {
#ifdef ENJIN_PLATFORM_LINUX
    // Build argv
    std::vector<const char*> argv;
    argv.push_back(executable.c_str());
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    pid_t pid = 0;
    int spawnResult = posix_spawnp(
        &pid,
        executable.c_str(),
        nullptr,
        nullptr,
        const_cast<char**>(reinterpret_cast<const char* const*>(argv.data())),
        environ
    );

    if (spawnResult != 0) {
        return -1;
    }
    return static_cast<i64>(pid);
#else
    (void)executable;
    (void)args;
    return -1;
#endif
}

// =============================================================================
// Desktop integration
// =============================================================================

bool OpenWithDefault(const std::string& pathOrUrl) {
#ifdef ENJIN_PLATFORM_LINUX
    return LaunchProcessAsync("xdg-open", {pathOrUrl}) > 0;
#else
    (void)pathOrUrl;
    return false;
#endif
}

bool IsCommandAvailable(const std::string& command) {
#ifdef ENJIN_PLATFORM_LINUX
    // Validate command name: only alphanumeric, dash, underscore
    for (char c : command) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return false;
        }
    }

    auto result = LaunchProcess("/bin/sh", {"-c", "which " + command + " >/dev/null 2>&1"}, 5000);
    return result.success;
#else
    (void)command;
    return false;
#endif
}

std::string GetDesktopEnvironment() {
#ifdef ENJIN_PLATFORM_LINUX
    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    if (desktop && desktop[0] != '\0') {
        return std::string(desktop);
    }
    const char* session = std::getenv("DESKTOP_SESSION");
    if (session && session[0] != '\0') {
        return std::string(session);
    }
    return "Unknown";
#else
    return "N/A";
#endif
}

std::string GetDisplayServerType() {
#ifdef ENJIN_PLATFORM_LINUX
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    if (waylandDisplay && waylandDisplay[0] != '\0') {
        return "Wayland";
    }
    const char* display = std::getenv("DISPLAY");
    if (display && display[0] != '\0') {
        return "X11";
    }
    return "";
#else
    return "";
#endif
}

} // namespace Enjin::Platform
