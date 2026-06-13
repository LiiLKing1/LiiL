#include "CommandEngine.h"
#include "SystemCommands.h"
#include "SystemActions.h"
#include "../core/Logger.h"
#include <algorithm>
#include <regex>
#include <cctype>

namespace liil {
namespace command {

CommandEngine& CommandEngine::GetInstance() {
    static CommandEngine instance;
    return instance;
}

void CommandEngine::Initialize() {
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
    
    core::Logger::Info("Command Engine initsializatsiya qilindi. Ovoz, yorqinlik, dasturlar va tizim amallari tayyor.");
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
            core::Logger::Warning("Noma'lum buyruq: " + input);
        }
    }
}

} // namespace command
} // namespace liil
