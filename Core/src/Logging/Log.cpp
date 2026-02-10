#include "Enjin/Logging/Log.h"
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
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

        m_LogFile = std::make_unique<std::ofstream>(logFile, std::ios::app);
        if (!m_LogFile->is_open()) {
            std::cerr << "Failed to open log file: " << logFile << " (continuing with console logging only)" << std::endl;
            m_LogFile.reset();
        }
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

    // Format log entry
    std::stringstream ss;
    ss << "[" << GetTimestamp() << "] "
       << "[" << GetLogLevelString(level) << "] "
       << "[" << GetCategoryString(category) << "] "
       << filename << ":" << line << " (" << function << ") "
       << buffer << std::endl;

    std::string logEntry = ss.str();

    // Output to console
    if (level >= LogLevel::Error) {
        std::cerr << logEntry;
    } else {
        std::cout << logEntry;
    }

    // Output to file
    if (m_LogFile && m_LogFile->is_open()) {
        *m_LogFile << logEntry;
        m_LogFile->flush();
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

std::string Logger::GetLogLevelString(LogLevel level) const {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetCategoryString(LogCategory category) const {
    switch (category) {
        case LogCategory::Core:     return "CORE  ";
        case LogCategory::Renderer: return "RENDER";
        case LogCategory::Physics:  return "PHYS  ";
        case LogCategory::Audio:    return "AUDIO ";
        case LogCategory::Asset:    return "ASSET ";
        case LogCategory::Script:   return "SCRIPT";
        case LogCategory::Editor:   return "EDITOR";
        case LogCategory::Game:     return "GAME  ";
        case LogCategory::Network:  return "NET   ";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetTimestamp() const {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    
    std::stringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace Enjin
