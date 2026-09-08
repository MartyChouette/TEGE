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

// Put our handler back if something displaced it, and say so when that happens.
//
// On Windows SetUnhandledExceptionFilter is a single global slot and the LAST
// caller wins. We install it early (Application::InitializeEngine, before the
// window even exists), and everything that loads afterwards -- GLFW, the Vulkan
// loader, the graphics driver, capture and overlay hooks -- is free to install
// its own on top and silently take ours out of the chain.
//
// Not hypothetical: editor access violations on 2026-09-08 produced no
// enjin_crash.dmp at all, while an exported game had produced one a week
// earlier. The engine sets its CWD to the exe directory before installing, so
// the artifacts were not merely landing elsewhere -- the handler was not
// running. Called periodically from the frame loop, because whatever displaces
// us need not have loaded by the time startup finishes.
ENJIN_API void ReassertCrashHandler();

// Deliberately access-violate, so the crash path itself can be tested. Reachable
// from the editor with --crash-test. A crash reporter nobody has watched fire is
// a crash reporter being trusted on faith.
ENJIN_API void TriggerTestCrash();

// Register context providers for crash report enrichment
ENJIN_API void SetCrashContext(const CrashContext& ctx);

// Check for crash report from a previous session
ENJIN_API bool HasPreviousCrashReport();
ENJIN_API std::string ReadPreviousCrashReport();
ENJIN_API void ClearPreviousCrashReport();

} // namespace Enjin::Debug
