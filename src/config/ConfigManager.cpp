#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace liil {
namespace config {

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::Load(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_filePath = filePath;
    m_settings.clear();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue; // Skip empty lines and comments
        }

        auto delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = Trim(line.substr(0, delimiterPos));
            std::string value = Trim(line.substr(delimiterPos + 1));
            m_settings[key] = value;
        }
    }

    return true;
}

bool ConfigManager::Save() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_filePath.empty()) {
        return false;
    }

    std::ofstream file(m_filePath);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& pair : m_settings) {
        file << pair.first << "=" << pair.second << "\n";
    }

    return true;
}

std::string ConfigManager::GetString(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_settings.find(key);
    if (it != m_settings.end()) {
        return it->second;
    }
    return defaultValue;
}

int ConfigManager::GetInt(const std::string& key, int defaultValue) const {
    std::string valStr = GetString(key);
    if (valStr.empty()) return defaultValue;
    try {
        return std::stoi(valStr);
    } catch (...) {
        return defaultValue;
    }
}

bool ConfigManager::GetBool(const std::string& key, bool defaultValue) const {
    std::string valStr = GetString(key);
    if (valStr.empty()) return defaultValue;
    
    std::string lowerVal = valStr;
    std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), 
        [](unsigned char c){ return std::tolower(c); });
    
    if (lowerVal == "true" || lowerVal == "1" || lowerVal == "yes") return true;
    if (lowerVal == "false" || lowerVal == "0" || lowerVal == "no") return false;
    
    return defaultValue;
}

void ConfigManager::SetString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_settings[key] = value;
}

void ConfigManager::SetInt(const std::string& key, int value) {
    SetString(key, std::to_string(value));
}

void ConfigManager::SetBool(const std::string& key, bool value) {
    SetString(key, value ? "true" : "false");
}

std::string ConfigManager::Trim(const std::string& str) const {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

} // namespace config
} // namespace liil
