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

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string GetLogLevelString(LogLevel level) const;
    std::string GetCategoryString(LogCategory category) const;
    std::string GetTimestamp() const;

    static constexpr usize MAX_LOG_FILE_SIZE = 10 * 1024 * 1024; // 10 MB

    LogLevel m_MinLogLevel = LogLevel::Info;
    bool m_CategoryEnabled[static_cast<usize>(LogCategory::Count)] = { true };
    std::mutex m_Mutex;
    std::unique_ptr<std::ofstream> m_LogFile;
    std::string m_LogFilePath;
    usize m_CurrentFileSize = 0;
    bool m_Initialized = false;

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
