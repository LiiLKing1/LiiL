#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace liil {
namespace core {

struct MemoryFact {
    std::string title;
    std::string value;
    bool is_private;
};

class MemoryManager {
public:
    static MemoryManager& GetInstance();

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    void Initialize(const std::string& appDir);
    
    // AI orqali factlarni saqlash yoki yangilash
    void ProcessFactJSON(const std::string& factJson);
    
    std::string GetAllFactsAsString();

private:
    MemoryManager() = default;
    
    void SaveToFile();
    void ExportToJs();
    void PushToGithub();

    std::vector<MemoryFact> m_facts;
    std::string m_jsonPath;
    std::string m_jsPath;
    std::string m_appDir;
    std::mutex m_mutex;
};

} // namespace core
} // namespace liil
