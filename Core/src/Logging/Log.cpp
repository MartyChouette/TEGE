#include "Enjin/Logging/Log.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <cstring>

namespace Enjin {

Logger& Logger::Get() {
    static Logger instance;
    return instance;
}

void Logger::Initialize(const std::string& logFile) {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        if (m_Initialized) {
            return;
        }

        // Default: enable all categories so logs actually show up.
        for (usize i = 0; i < static_cast<usize>(LogCategory::Count); ++i) {
            m_CategoryEnabled[i] = true;
        }

        // Always initialize console logging, even if file logging is unavailable.
        m_Initialized = true;

        m_LogFilePath = logFile;

        // Truncate on startup — fresh log each session
        m_LogFile = std::make_unique<std::ofstream>(logFile, std::ios::trunc);
        if (!m_LogFile->is_open()) {
            std::cerr << "Failed to open log file: " << logFile << " (continuing with console logging only)" << std::endl;
            m_LogFile.reset();
        }
        m_CurrentFileSize = 0;
    }

    // IMPORTANT: do not call Info() while holding m_Mutex (it locks internally).
    Info(LogCategory::Core, __FILE__, __LINE__, __FUNCTION__, "Logger initialized");
}

void Logger::Shutdown() {
    // IMPORTANT: do not call Info() while holding m_Mutex (it locks internally).
    bool shouldLog = false;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        shouldLog = m_Initialized;
    }

    if (shouldLog) {
        Info(LogCategory::Core, __FILE__, __LINE__, __FUNCTION__, "Logger shutting down");
    }

    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (!m_Initialized) {
            return;
        }
        if (m_LogFile) {
            m_LogFile->close();
            m_LogFile.reset();
        }
        m_Initialized = false;
    }
}

void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_MinLogLevel = level;
}

void Logger::SetCategoryEnabled(LogCategory category, bool enabled) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CategoryEnabled[static_cast<usize>(category)] = enabled;
}

void Logger::Log(LogLevel level, LogCategory category, const char* file, u32 line, const char* function, const char* format, ...) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!m_Initialized || level < m_MinLogLevel || !m_CategoryEnabled[static_cast<usize>(category)]) {
        return;
    }

    // Format the message
    va_list args;
    va_start(args, format);
    
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    // Get filename from path
    const char* filename = file;
    const char* lastSlash = strrchr(file, '/');
    if (!lastSlash) {
        lastSlash = strrchr(file, '\\');
    }
    if (lastSlash) {
        filename = lastSlash + 1;
    }

    // Format log entry into stack buffer (no heap allocation)
    char timestamp[32];
    FormatTimestamp(timestamp, sizeof(timestamp));

    char logEntry[4608];
    int len = snprintf(logEntry, sizeof(logEntry), "[%s] [%s] [%s] %s:%u (%s) %s\n",
        timestamp, GetLogLevelString(level), GetCategoryString(category),
        filename, line, function, buffer);
    if (len < 0) len = 0;
    if (len >= static_cast<int>(sizeof(logEntry))) len = static_cast<int>(sizeof(logEntry)) - 1;

    // Output to console
    if (level >= LogLevel::Error) {
        fwrite(logEntry, 1, static_cast<size_t>(len), stderr);
    } else {
        fwrite(logEntry, 1, static_cast<size_t>(len), stdout);
    }

    // Output to file
    if (m_LogFile && m_LogFile->is_open()) {
        m_LogFile->write(logEntry, len);
        m_LogFile->flush();
        m_CurrentFileSize += static_cast<usize>(len);
        if (m_CurrentFileSize >= MAX_LOG_FILE_SIZE) {
            RotateLogFile();
        }
    }
}

void Logger::Trace(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(LogLevel::Trace, category, file, line, function, "%s", buffer);
}

void Logger::Debug(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(LogLevel::Debug, category, file, line, function, "%s", buffer);
}

void Logger::Info(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(LogLevel::Info, category, file, line, function, "%s", buffer);
}

void Logger::Warn(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(LogLevel::Warn, category, file, line, function, "%s", buffer);
}

void Logger::Error(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(LogLevel::Error, category, file, line, function, "%s", buffer);
}

void Logger::Fatal(LogCategory category, const char* file, u32 line, const char* function, const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(LogLevel::Fatal, category, file, line, function, "%s", buffer);
}

const char* Logger::GetLogLevelString(LogLevel level) const {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "?????";
    }
}

const char* Logger::GetCategoryString(LogCategory category) const {
    switch (category) {
        case LogCategory::Core:       return "CORE  ";
        case LogCategory::Renderer:   return "RENDER";
        case LogCategory::Physics:    return "PHYS  ";
        case LogCategory::Audio:      return "AUDIO ";
        case LogCategory::Asset:      return "ASSET ";
        case LogCategory::Script:     return "SCRIPT";
        case LogCategory::Editor:     return "EDITOR";
        case LogCategory::Game:       return "GAME  ";
        case LogCategory::AI:         return "AI    ";
        case LogCategory::Assets:     return "ASSETS";
        case LogCategory::Procedural: return "PROCGN";
        case LogCategory::Animation:  return "ANIM  ";
        case LogCategory::Build:      return "BUILD ";
        case LogCategory::Player:     return "PLAYER";
        case LogCategory::Network:    return "NET   ";
        default: return "??????";
    }
}

void Logger::FormatTimestamp(char* buf, usize bufSize) const {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    if (tm) {
        std::strftime(buf, bufSize, "%Y-%m-%d %H:%M:%S", tm);
    } else {
        snprintf(buf, bufSize, "0000-00-00 00:00:00");
    }
}

void Logger::RotateLogFile() {
    if (!m_LogFile || m_LogFilePath.empty()) return;

    m_LogFile->close();

    // Rename current log to .old (overwrite any previous .old)
    std::string oldPath = m_LogFilePath + ".old";
    std::remove(oldPath.c_str());
    std::rename(m_LogFilePath.c_str(), oldPath.c_str());

    // Open a fresh log
    m_LogFile = std::make_unique<std::ofstream>(m_LogFilePath, std::ios::trunc);
    m_CurrentFileSize = 0;

    if (m_LogFile->is_open()) {
        char ts[32];
        FormatTimestamp(ts, sizeof(ts));
        char msg[256];
        int n = snprintf(msg, sizeof(msg), "[%s] [INFO ] [CORE  ] Log rotated (previous log saved as %s)\n", ts, oldPath.c_str());
        if (n > 0) m_LogFile->write(msg, n);
    }
}

} // namespace Enjin
