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
    if (m_jsStream.is_open()) {
        m_jsStream.close();
    }
}

void Logger::Initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    if (m_jsStream.is_open()) {
        m_jsStream.close();
    }
    
    m_fileStream.open(logFilePath, std::ios::app | std::ios::out);
    
    // JS fayli uchun
    std::string jsPath = logFilePath;
    if (jsPath.find(".log") != std::string::npos) {
        jsPath.replace(jsPath.find(".log"), 4, "_data.js");
    } else {
        jsPath += "_data.js";
    }
    
    bool jsExists = false;
    std::ifstream checkJs(jsPath);
    if (checkJs.is_open()) {
        jsExists = true;
        checkJs.close();
    }
    
    m_jsStream.open(jsPath, std::ios::app | std::ios::out);
    if (!jsExists && m_jsStream.is_open()) {
        m_jsStream << "var liil_logs = [];\nfunction addLog(entry) { liil_logs.push(entry); if(typeof renderLogs !== 'undefined') { renderLogs(); } }\n";
        m_jsStream.flush();
    }
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
    
    // Write to JS file
    if (m_jsStream.is_open()) {
        std::string safeMsg = message;
        // escape quotes and slashes
        size_t pos = 0;
        while((pos = safeMsg.find('\\', pos)) != std::string::npos) { safeMsg.replace(pos, 1, "\\\\"); pos += 2; }
        pos = 0;
        while((pos = safeMsg.find('\"', pos)) != std::string::npos) { safeMsg.replace(pos, 1, "\\\""); pos += 2; }
        pos = 0;
        while((pos = safeMsg.find('\n', pos)) != std::string::npos) { safeMsg.replace(pos, 1, "\\n"); pos += 2; }
        pos = 0;
        while((pos = safeMsg.find('\r', pos)) != std::string::npos) { safeMsg.replace(pos, 1, ""); }
        
        m_jsStream << "addLog({\"time\": \"" << timeStr << "\", \"level\": \"" << levelStr << "\", \"msg\": \"" << safeMsg << "\"});\n";
        m_jsStream.flush();
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
