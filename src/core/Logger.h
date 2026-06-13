#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <functional>

namespace liil {
namespace core {

enum class LogLevel {
    INFO,
    WARNING,
    ERR
};

class Logger {
public:
    static Logger& GetInstance();

    // Delete copy and move constructors
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void Initialize(const std::string& logFilePath);
    void SetUICallback(std::function<void(const std::string&)> callback);
    
    void Log(LogLevel level, const std::string& message);
    
    // Convenience methods
    static void Info(const std::string& message) { GetInstance().Log(LogLevel::INFO, message); }
    static void Warning(const std::string& message) { GetInstance().Log(LogLevel::WARNING, message); }
    static void Error(const std::string& message) { GetInstance().Log(LogLevel::ERR, message); }

private:
    Logger() = default;
    ~Logger();

    std::string GetTimestamp() const;
    std::string LevelToString(LogLevel level) const;

    std::ofstream m_fileStream;
    std::mutex m_mutex;
    std::function<void(const std::string&)> m_uiCallback;
};

} // namespace core
} // namespace liil
