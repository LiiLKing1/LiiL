#include "CommandEngine.h"
#include "SystemCommands.h"
#include "SystemActions.h"
#include "AutomationEngine.h"
#include "AppScanner.h"
#include "AIEngine.h"
#include "../core/Logger.h"
#include "../core/MemoryManager.h"
#include <algorithm>
#include <regex>
#include <cctype>
#include <thread>
#include <windows.h>
#include <shellapi.h>

namespace liil {
namespace command {

CommandEngine& CommandEngine::GetInstance() {
    static CommandEngine instance;
    return instance;
}

void CommandEngine::Initialize() {
    // Barcha dasturlarni FONDA skanerlash — UI ni bloklamaydi
    std::thread([]() {
        AppScanner::ScanAll();
    }).detach();

    // Media commands
    RegisterCommand("ovozni oshir", SystemCommands::VolumeUp);
    RegisterCommand("ovozni pasaytir", SystemCommands::VolumeDown);
    RegisterCommand("jim", SystemCommands::VolumeMute);
    RegisterCommand("mute", SystemCommands::VolumeMute);
    
    // Brightness commands
    RegisterCommand("yorqinlikni oshir", SystemCommands::BrightnessUp);
    RegisterCommand("yorqinlikni pasaytir", SystemCommands::BrightnessDown);
    
    // Direct App opening matching
    RegisterCommand("yandexni och", [](){ SystemCommands::OpenApp("yandex"); });
    RegisterCommand("robloxni och", [](){ SystemCommands::OpenApp("roblox"); });
    RegisterCommand("telegramni och", [](){ SystemCommands::OpenApp("telegram"); });
    RegisterCommand("steamni och", [](){ SystemCommands::OpenApp("steam"); });
    
    // System Actions
    RegisterCommand("screenshot", SystemActions::TakeScreenshot);
    RegisterCommand("take screenshot", SystemActions::TakeScreenshot);
    RegisterCommand("nusxa ol", SystemActions::CopySelection);
    
    // Automation Modes
    RegisterCommand("coding mode", [](){ AutomationEngine::ExecuteMode("coding"); });
    RegisterCommand("gaming mode", [](){ AutomationEngine::ExecuteMode("gaming"); });

    // Dasturlar ro'yxati
    RegisterCommand("dasturlar", AppScanner::PrintAppList);
    RegisterCommand("ilovalar", AppScanner::PrintAppList);
    RegisterCommand("programlar", AppScanner::PrintAppList);
    
    core::Logger::Info("Command Engine initsializatsiya qilindi. Ovoz, yorqinlik, dasturlar, amallar va rejimlar tayyor.");
}

void CommandEngine::RegisterCommand(const std::string& trigger, std::function<void()> action) {
    m_commands[NormalizeInput(trigger)] = action;
}

std::string CommandEngine::NormalizeInput(const std::string& input) {
    std::string result = input;
    
    // To lower
    std::transform(result.begin(), result.end(), result.begin(), 
        [](unsigned char c){ return std::tolower(c); });
    
    // Remove punctuation
    result.erase(std::remove_if(result.begin(), result.end(), 
        [](unsigned char c){ return std::ispunct(c); }), result.end());
        
    // Remove extra spaces (trim and normalize internal spaces)
    std::regex extraSpaces(R"(^\s+|\s+$|(\s)+)");
    result = std::regex_replace(result, extraSpaces, "$1");
    
    return result;
}

void CommandEngine::Execute(const std::string& input) {
    std::string normalized = NormalizeInput(input);
    if (normalized.empty()) return;
    
    auto it = m_commands.find(normalized);
    if (it != m_commands.end()) {
        it->second();
    } else {
        std::smatch match;
        
        // Match "...ni och"
        std::regex openRegex(R"(^(.+?)(?:ni)? och$)");
        
        // Match "...dan chiq"
        std::regex closeRegex(R"(^(.+?)dan chiq$)");
        
        // Match "...ga kir"
        std::regex folderRegex(R"(^(.+?)ga kir$)");

        if (std::regex_match(normalized, match, openRegex) && match.size() > 1) {
            std::string target = match[1].str();
            target = std::regex_replace(target, std::regex(R"(\s+$)"), "");
            
            if (target == "desktop" || target == "ish stoli" || target == "downloads" || target == "hujjatlar") {
                SystemActions::OpenFolder(target);
            } else {
                SystemCommands::OpenApp(target);
            }
        } 
        else if (std::regex_match(normalized, match, closeRegex) && match.size() > 1) {
            std::string target = match[1].str();
            target = std::regex_replace(target, std::regex(R"(\s+$)"), "");
            SystemActions::CloseApp(target);
        }
        else if (std::regex_match(normalized, match, folderRegex) && match.size() > 1) {
            std::string target = match[1].str();
            target = std::regex_replace(target, std::regex(R"(\s+$)"), "");
            SystemActions::OpenFolder(target);
        }
        else {
            // "...qayerda" — dastur yo'lini ko'rsatish
            std::regex whereRegex(R"(^(.+?)\s+qayerda$)");
            if (std::regex_match(normalized, match, whereRegex) && match.size() > 1) {
                std::string target = match[1].str();
                target = std::regex_replace(target, std::regex(R"(\s+$)"), "");
                std::string path = AppScanner::FindApp(target);
                if (!path.empty()) {
                    core::Logger::Info("[" + target + "] manzili: " + path);
                } else {
                    core::Logger::Warning("'" + target + "' dasturi topilmadi.");
                }
            } else {
                // Agar qayerda ham bo'lmasa, AI ga topshiramiz
                AIEngine::ProcessCommand(input);
            }
        }
    }
}

void CommandEngine::ExecuteAction(const std::string& action, const std::string& target) {
    if (action == "chat") {
        core::Logger::Info("AI: " + target);
    } else if (action == "open_app") {
        SystemCommands::OpenApp(target);
    } else if (action == "close_app") {
        SystemActions::CloseApp(target);
    } else if (action == "open_folder") {
        SystemActions::OpenFolder(target);
    } else if (action == "volume_up") {
        SystemCommands::VolumeUp();
    } else if (action == "volume_down") {
        SystemCommands::VolumeDown();
    } else if (action == "volume_mute") {
        SystemCommands::VolumeMute();
    } else if (action == "brightness_up") {
        SystemCommands::BrightnessUp();
    } else if (action == "brightness_down") {
        SystemCommands::BrightnessDown();
    } else if (action == "take_screenshot") {
        SystemActions::TakeScreenshot();
    } else if (action == "coding_mode") {
        AutomationEngine::ExecuteMode("coding");
    } else if (action == "gaming_mode") {
        AutomationEngine::ExecuteMode("gaming");
    } else if (action == "view_logs") {
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::string currentPath(buffer);
        currentPath = currentPath.substr(0, currentPath.find_last_of("\\/"));
        
        std::string htmlPath = "file:///" + currentPath + "/logs.html";
        // replace all backslashes with forward slashes for URL format
        std::replace(htmlPath.begin(), htmlPath.end(), '\\', '/');

        if (!target.empty()) {
            htmlPath += "?date=" + target;
        }
        
        // Use ShellExecute directly for the URL format
        ShellExecuteA(NULL, "open", htmlPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        core::Logger::Info("Loglar oynasi ochilmoqda...");
    } else if (action == "update_memory" || action == "save_memory") {
        core::MemoryManager::GetInstance().ProcessFactJSON(target);
        core::Logger::Info("Xotira tahrirlandi: " + target);
    } else {
        core::Logger::Warning("AI jo'natgan noma'lum harakat: " + action);
    }
}

} // namespace command
} // namespace liil
