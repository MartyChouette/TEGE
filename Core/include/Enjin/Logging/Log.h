#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <string_view>
#include <mutex>
#include <fstream>
#include <memory>

namespace Enjin {

enum class LogLevel : u8 {
    Trace = 0,
    Debug = 1,
    Info  = 2,
    Warn  = 3,
    Error = 4,
    Fatal = 5
};

enum class LogCategory : u8 {
    Core      = 0,
    Renderer  = 1,
    Physics   = 2,
    Audio     = 3,
    Asset     = 4,
    Script    = 5,
    Editor    = 6,
    Game      = 7,
    AI        = 8,
    Assets    = 9,
    Procedural = 10,
    Animation = 11,
    Build     = 12,
    Player    = 13,
    Network   = 14,
    Count
};

// Ring buffer entry for crash handler context
struct LogEntry {
    char message[512];
    LogLevel level;
    LogCategory category;
};

// Callback fired after each log message is formatted (still under mutex)
using LogCallback = void(*)(LogLevel level, LogCategory category, const char* formatted);

class ENJIN_API Logger {
public:
    static Logger& Get();

    void Initialize(const std::string& logFile = "enjin.log");
    void Shutdown();

    void SetLogLevel(LogLevel level);
    void SetCategoryEnabled(LogCategory category, bool enabled);

    void Log(LogLevel level, LogCategory category, const char* file, u32 line, const char* function, const char* format, ...);

    // Convenience methods
    void Trace(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...);
    void Debug(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...);
    void Info(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...);
    void Warn(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...);
    void Error(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...);
    void Fatal(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...);

    // Log callback for editor console wiring
    void SetLogCallback(LogCallback callback);

    // Ring buffer access (for crash handler — reads without locking, process is stopped)
    const LogEntry* GetRingBuffer(usize& outCount, usize& outHead) const;

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    const char* GetLogLevelString(LogLevel level) const;
    const char* GetCategoryString(LogCategory category) const;
    void FormatTimestamp(char* buf, usize bufSize) const;

    static constexpr usize MAX_LOG_FILE_SIZE = 10 * 1024 * 1024; // 10 MB
    static constexpr usize RING_BUFFER_SIZE = 128;

    LogLevel m_MinLogLevel = LogLevel::Info;
    bool m_CategoryEnabled[static_cast<usize>(LogCategory::Count)] = { true };
    std::mutex m_Mutex;
    std::unique_ptr<std::ofstream> m_LogFile;
    std::string m_LogFilePath;
    usize m_CurrentFileSize = 0;
    bool m_Initialized = false;

    // Ring buffer for crash context
    LogEntry m_RingBuffer[RING_BUFFER_SIZE] = {};
    usize m_RingHead = 0;
    usize m_RingCount = 0;

    // External callback (e.g. editor console)
    LogCallback m_LogCallback = nullptr;

    void RotateLogFile();
};

} // namespace Enjin

// Macros for logging
// Uses ::Enjin:: to avoid namespace conflicts when called from within Enjin sub-namespaces
#define ENJIN_LOG_TRACE(category, ...) \
    ::Enjin::Logger::Get().Trace(::Enjin::LogCategory::category, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define ENJIN_LOG_DEBUG(category, ...) \
    ::Enjin::Logger::Get().Debug(::Enjin::LogCategory::category, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define ENJIN_LOG_INFO(category, ...) \
    ::Enjin::Logger::Get().Info(::Enjin::LogCategory::category, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define ENJIN_LOG_WARN(category, ...) \
    ::Enjin::Logger::Get().Warn(::Enjin::LogCategory::category, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define ENJIN_LOG_ERROR(category, ...) \
    ::Enjin::Logger::Get().Error(::Enjin::LogCategory::category, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define ENJIN_LOG_FATAL(category, ...) \
    ::Enjin::Logger::Get().Fatal(::Enjin::LogCategory::category, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
