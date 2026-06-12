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

// =============================================================================
// Path sanitization
// =============================================================================
// Shared validation for untrusted relative paths (scene files, asset packs,
// plugins, save keys). Purely lexical — no filesystem access, no logging;
// callers report failures under their own log categories.

// True if 'relative' is a safe project-relative path: non-empty, not absolute,
// no drive/root name, and no ".." component after lexical normalization.
// ".." as a substring inside a name (e.g. "a..b.png") is allowed.
ENJIN_API bool IsSafeRelativePath(const std::string& relative);

// True if 'name' is a single safe file name: non-empty, no '/' or '\', and
// not containing "..". Stricter than IsSafeRelativePath — for save keys and
// plugin names that must never address subdirectories.
ENJIN_API bool IsSafeFileName(const std::string& name);

// Joins root/relative, lexically normalizes, and verifies the result stays
// within root (separator-boundary prefix check). Returns the normalized
// absolute path, or "" if either input is empty or the path escapes root.
ENJIN_API std::string ResolveWithinRoot(const std::string& root, const std::string& relative);

// Returns 'absolute' expressed relative to 'root' if it lies within root
// after normalization; otherwise "" (caller keeps the original path).
ENJIN_API std::string MakeRelativeToRoot(const std::string& root, const std::string& absolute);

} // namespace Enjin::Platform

