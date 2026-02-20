#include "Enjin/Debug/CrashHandler.h"
#include "Enjin/Logging/Log.h"
#include <cstdio>
#include <ctime>
#include <cstring>

// Platform-specific includes
#if defined(ENJIN_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <DbgHelp.h>
    #pragma comment(lib, "dbghelp.lib")
#elif defined(ENJIN_PLATFORM_LINUX) || defined(ENJIN_PLATFORM_MACOS)
    #include <signal.h>
    #include <unistd.h>
#endif

namespace Enjin::Debug {

// File-scope state — no heap allocations, safe to read from crash handler
static CrashContext s_CrashContext = {};
static const char* s_CrashReportPath = "enjin_crash.txt";
static const char* s_CrashDumpPath   = "enjin_crash.dmp";

// ============================================================================
// Helpers (safe for use inside crash handler — no heap, no Logger, no exceptions)
// ============================================================================

static void SafeTimestamp(char* buf, size_t bufSize) {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    if (t) {
        snprintf(buf, bufSize, "%04d-%02d-%02d %02d:%02d:%02d",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        snprintf(buf, bufSize, "0000-00-00 00:00:00");
    }
}

static const char* SafeString(StringProvider provider, const char* fallback = "unknown") {
    if (provider) {
        const char* result = provider();
        if (result && result[0] != '\0') return result;
    }
    return fallback;
}

static void WriteRingBufferToFile(FILE* f) {
    usize count = 0, head = 0;
    const LogEntry* ring = Logger::Get().GetRingBuffer(count, head);
    if (!ring || count == 0) {
        fprintf(f, "  (no log entries)\n");
        return;
    }

    // Walk from oldest to newest
    usize start = (count < 128) ? 0 : head;
    for (usize i = 0; i < count; i++) {
        usize idx = (start + i) % 128;
        fprintf(f, "  %s", ring[idx].message);
        // message already ends with \n from the formatter
    }
}

// ============================================================================
// Windows implementation
// ============================================================================
#if defined(ENJIN_PLATFORM_WINDOWS)

static LPTOP_LEVEL_EXCEPTION_FILTER s_PreviousFilter = nullptr;

static const char* ExceptionCodeToString(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT:            return "BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLOAT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_OVERFLOW:          return "FLOAT_OVERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW:          return "INT_OVERFLOW";
        case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
        default:                              return "UNKNOWN";
    }
}

static void WriteMiniDump(EXCEPTION_POINTERS* exInfo) {
    HANDLE hFile = CreateFileA(s_CrashDumpPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId          = GetCurrentThreadId();
    mei.ExceptionPointers = exInfo;
    mei.ClientPointers    = FALSE;

    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                      MiniDumpNormal, &mei, nullptr, nullptr);
    CloseHandle(hFile);
}

static void WriteStackTrace(FILE* f, CONTEXT* ctx) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread  = GetCurrentThread();

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    if (!SymInitialize(process, nullptr, TRUE)) {
        fprintf(f, "  (failed to initialize symbols)\n");
        return;
    }

    STACKFRAME64 frame = {};
#if defined(_M_X64) || defined(__x86_64__)
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = ctx->Rip;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrStack.Offset = ctx->Rsp;
#elif defined(_M_IX86) || defined(__i386__)
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = ctx->Eip;
    frame.AddrFrame.Offset = ctx->Ebp;
    frame.AddrStack.Offset = ctx->Esp;
#else
    fprintf(f, "  (unsupported architecture)\n");
    SymCleanup(process);
    return;
#endif
    frame.AddrPC.Mode    = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    // Symbol name buffer (on stack, no heap)
    char symbolBuffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen   = 255;

    for (int i = 0; i < 32; i++) {
        if (!StackWalk64(machineType, process, thread, &frame, ctx,
                         nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }

        if (frame.AddrPC.Offset == 0) break;

        DWORD64 displacement64 = 0;
        DWORD   displacement32 = 0;
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        bool hasSymbol = SymFromAddr(process, frame.AddrPC.Offset, &displacement64, symbol) != 0;
        bool hasLine   = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &displacement32, &line) != 0;

        // Get module name
        HMODULE hModule = nullptr;
        char moduleName[128] = "???";
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(frame.AddrPC.Offset), &hModule);
        if (hModule) {
            GetModuleFileNameA(hModule, moduleName, sizeof(moduleName));
            // Extract just the filename
            const char* slash = strrchr(moduleName, '\\');
            if (slash) memmove(moduleName, slash + 1, strlen(slash + 1) + 1);
        }

        if (hasSymbol && hasLine) {
            // Extract just the filename from line info
            const char* srcFile = line.FileName;
            const char* srcSlash = strrchr(srcFile, '\\');
            if (srcSlash) srcFile = srcSlash + 1;
            fprintf(f, "  [%2d] %s + 0x%llX  (%s:%lu)\n",
                    i, symbol->Name, (unsigned long long)displacement64,
                    srcFile, line.LineNumber);
        } else if (hasSymbol) {
            fprintf(f, "  [%2d] %s + 0x%llX  (%s)\n",
                    i, symbol->Name, (unsigned long long)displacement64, moduleName);
        } else {
            fprintf(f, "  [%2d] 0x%016llX  (%s)\n",
                    i, (unsigned long long)frame.AddrPC.Offset, moduleName);
        }
    }

    SymCleanup(process);
}

static LONG WINAPI EnjinCrashFilter(EXCEPTION_POINTERS* exInfo) {
    // Write minidump first (smallest, most likely to succeed)
    WriteMiniDump(exInfo);

    // Write human-readable crash report
    FILE* f = fopen(s_CrashReportPath, "w");
    if (f) {
        char timestamp[32];
        SafeTimestamp(timestamp, sizeof(timestamp));

        fprintf(f, "========== ENJIN CRASH REPORT ==========\n");
        fprintf(f, "Time:       %s\n", timestamp);
        fprintf(f, "Version:    %s\n", SafeString(s_CrashContext.engineVersion, "unknown"));
        fprintf(f, "Platform:   Windows\n");
        fprintf(f, "GPU:        %s\n", SafeString(s_CrashContext.gpuName, "unknown"));
        fprintf(f, "Scene:      %s\n", SafeString(s_CrashContext.sceneName, "none"));
        if (s_CrashContext.entityCount) {
            fprintf(f, "Entities:   %u\n", s_CrashContext.entityCount());
        }
        fprintf(f, "\n");

        // Exception info
        DWORD code = exInfo->ExceptionRecord->ExceptionCode;
        ULONG_PTR addr = (ULONG_PTR)exInfo->ExceptionRecord->ExceptionAddress;
        fprintf(f, "Exception:  %s (0x%08lX)\n", ExceptionCodeToString(code), code);
        fprintf(f, "Address:    0x%016llX\n", (unsigned long long)addr);

        // Module + offset
        HMODULE hModule = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(addr), &hModule);
        if (hModule) {
            char modName[256];
            GetModuleFileNameA(hModule, modName, sizeof(modName));
            const char* slash = strrchr(modName, '\\');
            if (slash) memmove(modName, slash + 1, strlen(slash + 1) + 1);
            ULONG_PTR offset = addr - (ULONG_PTR)hModule;
            fprintf(f, "Module:     %s + 0x%llX\n", modName, (unsigned long long)offset);
        }

        // Access violation details
        if (code == EXCEPTION_ACCESS_VIOLATION && exInfo->ExceptionRecord->NumberParameters >= 2) {
            const char* accessType = exInfo->ExceptionRecord->ExceptionInformation[0] == 0 ? "read" : "write";
            fprintf(f, "Details:    %s of 0x%016llX\n", accessType,
                    (unsigned long long)exInfo->ExceptionRecord->ExceptionInformation[1]);
        }
        fprintf(f, "\n");

        // Stack trace
        fprintf(f, "Stack Trace:\n");
        WriteStackTrace(f, exInfo->ContextRecord);
        fprintf(f, "\n");

        // Ring buffer log context
        fprintf(f, "Last Log Lines:\n");
        WriteRingBufferToFile(f);

        fprintf(f, "=========================================\n");
        fclose(f);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void InstallCrashHandler() {
    s_PreviousFilter = SetUnhandledExceptionFilter(EnjinCrashFilter);
}

void UninstallCrashHandler() {
    SetUnhandledExceptionFilter(s_PreviousFilter);
    s_PreviousFilter = nullptr;
}

// ============================================================================
// Linux / macOS implementation (stub — ring buffer dump only)
// ============================================================================
#elif defined(ENJIN_PLATFORM_LINUX) || defined(ENJIN_PLATFORM_MACOS)

static struct sigaction s_PrevSIGSEGV = {};
static struct sigaction s_PrevSIGABRT = {};

static void CrashSignalHandler(int sig) {
    FILE* f = fopen(s_CrashReportPath, "w");
    if (f) {
        char timestamp[32];
        SafeTimestamp(timestamp, sizeof(timestamp));

        fprintf(f, "========== ENJIN CRASH REPORT ==========\n");
        fprintf(f, "Time:       %s\n", timestamp);
        fprintf(f, "Version:    %s\n", SafeString(s_CrashContext.engineVersion, "unknown"));
#if defined(ENJIN_PLATFORM_LINUX)
        fprintf(f, "Platform:   Linux\n");
#else
        fprintf(f, "Platform:   macOS\n");
#endif
        fprintf(f, "GPU:        %s\n", SafeString(s_CrashContext.gpuName, "unknown"));
        fprintf(f, "Scene:      %s\n", SafeString(s_CrashContext.sceneName, "none"));
        if (s_CrashContext.entityCount) {
            fprintf(f, "Entities:   %u\n", s_CrashContext.entityCount());
        }
        fprintf(f, "\nSignal:     %d (%s)\n\n", sig, sig == SIGSEGV ? "SIGSEGV" : "SIGABRT");

        fprintf(f, "Last Log Lines:\n");
        WriteRingBufferToFile(f);

        fprintf(f, "=========================================\n");
        fclose(f);
    }

    // Re-raise to get default behavior (core dump)
    signal(sig, SIG_DFL);
    raise(sig);
}

void InstallCrashHandler() {
    struct sigaction sa = {};
    sa.sa_handler = CrashSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;  // One-shot: restore default after first delivery
    sigaction(SIGSEGV, &sa, &s_PrevSIGSEGV);
    sigaction(SIGABRT, &sa, &s_PrevSIGABRT);
}

void UninstallCrashHandler() {
    sigaction(SIGSEGV, &s_PrevSIGSEGV, nullptr);
    sigaction(SIGABRT, &s_PrevSIGABRT, nullptr);
}

#else

// Unsupported platform — no-op
void InstallCrashHandler() {}
void UninstallCrashHandler() {}

#endif

// ============================================================================
// Common (all platforms)
// ============================================================================

void SetCrashContext(const CrashContext& ctx) {
    s_CrashContext = ctx;
}

bool HasPreviousCrashReport() {
    FILE* f = fopen(s_CrashReportPath, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

std::string ReadPreviousCrashReport() {
    FILE* f = fopen(s_CrashReportPath, "r");
    if (!f) return {};

    std::string result;
    char buf[1024];
    while (size_t n = fread(buf, 1, sizeof(buf), f)) {
        result.append(buf, n);
    }
    fclose(f);
    return result;
}

void ClearPreviousCrashReport() {
    remove(s_CrashReportPath);
    remove(s_CrashDumpPath);
}

} // namespace Enjin::Debug
