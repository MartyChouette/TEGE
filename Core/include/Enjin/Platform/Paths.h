#pragma once

#include "Enjin/Platform/Platform.h"
#include <string>

namespace Enjin::Platform {

// Returns absolute path to the current executable, if available.
ENJIN_API std::string GetExecutablePath();

// Returns the directory containing the current executable (no trailing slash).
ENJIN_API std::string GetExecutableDirectory();

// Sets current working directory. Returns true on success.
ENJIN_API bool SetCurrentWorkingDirectory(const std::string& path);

// Best-effort: sets working directory to the executable directory.
// This helps relative paths (logs, shaders, assets) work when launching via double-click.
ENJIN_API void SetWorkingDirectoryToExecutableDirectory();

// =============================================================================
// Platform-specific user directories
// =============================================================================

// Returns the user data directory for save games and project data.
// Windows: %APPDATA%/Enjin   Linux: $XDG_DATA_HOME/enjin   macOS: ~/Library/Application Support/Enjin
ENJIN_API std::string GetAppUserDataDirectory();

// Returns the user config directory for settings.
// Windows: %APPDATA%/Enjin   Linux: $XDG_CONFIG_HOME/enjin   macOS: ~/Library/Preferences/Enjin
ENJIN_API std::string GetAppUserConfigDirectory();

// Returns the user cache directory for temporary/regenerable data.
// Windows: %LOCALAPPDATA%/Enjin/cache   Linux: $XDG_CACHE_HOME/enjin   macOS: ~/Library/Caches/Enjin
ENJIN_API std::string GetAppUserCacheDirectory();

// Returns the system temp directory.
// Windows: %TEMP%   Linux: $TMPDIR or /tmp   macOS: NSTemporaryDirectory()
ENJIN_API std::string GetAppTempDirectory();

} // namespace Enjin::Platform

