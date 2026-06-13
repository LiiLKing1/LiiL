#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace liil {
namespace config {

class ConfigManager {
public:
    static ConfigManager& GetInstance();

    // Delete copy and move constructors
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

    bool Load(const std::string& filePath);
    bool Save();

    std::string GetString(const std::string& key, const std::string& defaultValue = "") const;
    int GetInt(const std::string& key, int defaultValue = 0) const;
    bool GetBool(const std::string& key, bool defaultValue = false) const;

    void SetString(const std::string& key, const std::string& value);
    void SetInt(const std::string& key, int value);
    void SetBool(const std::string& key, bool value);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    std::string m_filePath;
    std::unordered_map<std::string, std::string> m_settings;
    mutable std::mutex m_mutex;
    
    std::string Trim(const std::string& str) const;
};

} // namespace config
} // namespace liil
