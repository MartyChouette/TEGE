#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin::Debug {

// Context providers — registered by editor/player for crash report enrichment.
// These are plain function pointers returning static/persistent data.
using StringProvider = const char*(*)();
using U32Provider = u32(*)();

struct CrashContext {
    StringProvider gpuName       = nullptr;
    StringProvider sceneName     = nullptr;
    U32Provider    entityCount   = nullptr;
    StringProvider engineVersion = nullptr;
};

// Install/uninstall the platform crash handler (SEH on Windows, signals on Linux/Mac)
ENJIN_API void InstallCrashHandler();
ENJIN_API void UninstallCrashHandler();

// Register context providers for crash report enrichment
ENJIN_API void SetCrashContext(const CrashContext& ctx);

// Check for crash report from a previous session
ENJIN_API bool HasPreviousCrashReport();
ENJIN_API std::string ReadPreviousCrashReport();
ENJIN_API void ClearPreviousCrashReport();

} // namespace Enjin::Debug
