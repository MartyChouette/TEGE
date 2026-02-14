#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin::Platform {

// =============================================================================
// XDG Base Directory helpers (Linux desktop standard)
// =============================================================================

// Returns XDG_CONFIG_HOME or ~/.config
ENJIN_API std::string GetXDGConfigDir();

// Returns XDG_DATA_HOME or ~/.local/share
ENJIN_API std::string GetXDGDataDir();

// Returns XDG_CACHE_HOME or ~/.cache
ENJIN_API std::string GetXDGCacheDir();

// Returns XDG_RUNTIME_DIR or /tmp/enjin-<uid>
ENJIN_API std::string GetXDGRuntimeDir();

// =============================================================================
// User directories for Enjin (appends /enjin to XDG paths)
// =============================================================================

// Config: ~/.config/enjin (editor settings, keybindings)
ENJIN_API std::string GetUserConfigDirectory();

// Data: ~/.local/share/enjin (save games, projects)
ENJIN_API std::string GetUserDataDirectory();

// Cache: ~/.cache/enjin (shader cache, thumbnails)
ENJIN_API std::string GetUserCacheDirectory();

// Temp: /tmp or TMPDIR
ENJIN_API std::string GetTempDirectory();

// =============================================================================
// Process creation (Linux equivalent of CreateProcessA)
// =============================================================================

struct ProcessResult {
    i32 exitCode = -1;
    std::string stdoutOutput;
    std::string stderrOutput;
    bool success = false;
};

// Launch a process and wait for completion, capturing stdout/stderr.
// Uses fork/exec (no shell injection risk).
// args[0] should be the executable path.
ENJIN_API ProcessResult LaunchProcess(
    const std::string& executable,
    const std::vector<std::string>& args,
    i32 timeoutMs = 60000
);

// Launch a process and return immediately (fire-and-forget).
// Returns the child PID on success, -1 on failure.
ENJIN_API i64 LaunchProcessAsync(
    const std::string& executable,
    const std::vector<std::string>& args
);

// =============================================================================
// Desktop integration
// =============================================================================

// Open a URL or file with the default application (xdg-open)
ENJIN_API bool OpenWithDefault(const std::string& pathOrUrl);

// Query if a command is available on PATH
ENJIN_API bool IsCommandAvailable(const std::string& command);

// Get the current desktop environment name (GNOME, KDE, XFCE, etc.)
ENJIN_API std::string GetDesktopEnvironment();

// Get the display server type (X11, Wayland, or empty)
ENJIN_API std::string GetDisplayServerType();

} // namespace Enjin::Platform
