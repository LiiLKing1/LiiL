#include "MemoryManager.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <iostream>

namespace liil {
namespace core {

MemoryManager& MemoryManager::GetInstance() {
    static MemoryManager instance;
    return instance;
}

void MemoryManager::Initialize(const std::string& appDir) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_appDir = appDir;
    m_jsonPath = appDir + "\\memory.json";
    m_jsPath = appDir + "\\memory_data.js"; // This goes to public UI
    
    std::ifstream file(m_jsonPath);
    if (file.is_open()) {
        try {
            nlohmann::json memoryJson;
            file >> memoryJson;
            if (memoryJson.contains("facts") && memoryJson["facts"].is_array()) {
                for (const auto& item : memoryJson["facts"]) {
                    MemoryFact fact;
                    fact.title = item.value("title", "");
                    fact.value = item.value("value", "");
                    fact.is_private = item.value("is_private", false);
                    m_facts.push_back(fact);
                }
            }
        } catch (...) {}
        file.close();
    } else {
        SaveToFile();
    }
}

void MemoryManager::ProcessFactJSON(const std::string& factJsonStr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        auto item = nlohmann::json::parse(factJsonStr);
        std::string action = item.value("action", "save"); // save, edit, delete
        std::string title = item.value("title", "");
        
        if (action == "delete") {
            m_facts.erase(std::remove_if(m_facts.begin(), m_facts.end(),
                [&title](const MemoryFact& f) { return f.title == title; }), m_facts.end());
        } 
        else if (action == "edit" || action == "save") {
            bool found = false;
            for (auto& f : m_facts) {
                if (f.title == title || (item.contains("old_title") && f.title == item.value("old_title", ""))) {
                    f.title = item.value("title", f.title);
                    f.value = item.value("value", f.value);
                    if (item.contains("is_private")) f.is_private = item.value("is_private", false);
                    found = true;
                    break;
                }
            }
            if (!found && action == "save") {
                MemoryFact fact;
                fact.title = title;
                fact.value = item.value("value", "");
                fact.is_private = item.value("is_private", false);
                m_facts.push_back(fact);
            }
        }
        SaveToFile();
        PushToGithub();
    } catch (...) {
        std::cout << "[ERROR] Memory JSON parsing failed: " << factJsonStr << std::endl;
    }
}

std::string MemoryManager::GetAllFactsAsString() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_facts.empty()) return "Hech qanday xotira yo'q.";
    
    std::string result = "Saqlangan faktlar (Memory):\n";
    for (const auto& f : m_facts) {
        result += "- " + f.title + ": " + f.value + (f.is_private ? " (PRIVATE)" : " (PUBLIC)") + "\n";
    }
    return result;
}

void MemoryManager::SaveToFile() {
    nlohmann::json memoryJson;
    auto factsArr = nlohmann::json::array();
    for (const auto& f : m_facts) {
        factsArr.push_back({{"title", f.title}, {"value", f.value}, {"is_private", f.is_private}});
    }
    memoryJson["facts"] = factsArr;
    
    std::ofstream file(m_jsonPath);
    if (file.is_open()) {
        file << memoryJson.dump(4);
        file.close();
    }
    ExportToJs();
}

void MemoryManager::ExportToJs() {
    // Build/Release klasöründen bir yadam tepada proje köküne yaz
    // Vercel için: <proje>/public/memory_data.js
    std::string publicDir = m_appDir + "\\..\\..\\..\\public";
    std::string publicJsPath = publicDir + "\\memory_data.js";
    
    // Build/Release'daki memory_data.js
    std::ofstream jsFile(m_jsPath);
    if (jsFile.is_open()) {
        auto publicFacts = nlohmann::json::array();
        for (const auto& f : m_facts) {
            if (!f.is_private) {
                publicFacts.push_back({{"title", f.title}, {"value", f.value}});
            }
        }
        std::string jsContent = "var liil_memory = " + publicFacts.dump(4) + ";\n";
        jsContent += "if(typeof renderPublicMemory !== 'undefined') { renderPublicMemory(); }\n";
        jsContent += "if(typeof renderMemory !== 'undefined') { renderMemory(); }\n";
        jsFile << jsContent;
        jsFile.close();

        // Ayni zamanda public/ papkasiga ham yozamiz (Vercel uchun)
        std::ofstream publicJs(publicJsPath);
        if (publicJs.is_open()) {
            publicJs << jsContent;
            publicJs.close();
        }
    }
}

void MemoryManager::PushToGithub() {
    std::thread([this]() {
        // build/Release dan 3 darajada chiqib proje köküne yetamiz: build/Release -> build -> proje köki
        std::string repoRoot = m_appDir + "\\..\\..\\..";
        std::string cmd = "cd /d \"" + repoRoot + "\" && git add public/memory_data.js && git commit -m \"[LiiL Auto] Public memory updated\" && git push origin main 2>&1";
        system(cmd.c_str());
    }).detach();
}

} // namespace core
} // namespace liil
