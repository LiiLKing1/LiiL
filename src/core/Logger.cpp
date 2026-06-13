#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace liil {
namespace core {

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

void Logger::Initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    
    m_fileStream.open(logFilePath, std::ios::app | std::ios::out);
}

void Logger::SetUICallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_uiCallback = callback;
}

void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string timeStr = GetTimestamp();
    std::string levelStr = LevelToString(level);
    
    std::ostringstream formattedMessage;
    formattedMessage << "[" << timeStr << "] [" << levelStr << "] " << message << "\n";
    
    std::string finalStr = formattedMessage.str();
    
    // Write to file
    if (m_fileStream.is_open()) {
        m_fileStream << finalStr;
        m_fileStream.flush();
    }
    
    // Write to UI if callback is set
    if (m_uiCallback) {
        m_uiCallback(finalStr);
    }
}

std::string Logger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    struct tm buf;
#ifdef _WIN32
    localtime_s(&buf, &time);
#else
    localtime_r(&time, &buf);
#endif
    ss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string Logger::LevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace core
} // namespace liil
