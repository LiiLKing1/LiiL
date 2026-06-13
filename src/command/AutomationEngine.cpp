#include "AutomationEngine.h"
#include "SystemCommands.h"
#include "SystemActions.h"
#include "../core/Logger.h"

namespace liil {
namespace command {

void AutomationEngine::ExecuteMode(const std::string& modeName) {
    if (modeName == "coding") {
        core::Logger::Info("Coding Mode ishga tushirilmoqda...");
        SystemCommands::OpenApp("antigravity");
        SystemCommands::OpenApp("yandex");
        SystemCommands::OpenApp("yandexmusic");
    } 
    else if (modeName == "gaming") {
        core::Logger::Info("Gaming Mode ishga tushirilmoqda...");
        SystemCommands::OpenApp("steam");
        SystemCommands::OpenApp("telegram");
        SystemCommands::OpenApp("yandexmusic");
        
        SystemActions::CloseApp("yandex"); // CloseApp o'zi browser deb izlaydi yoki yorliq orqali
        SystemActions::CloseApp("antigravity");
    } 
    else {
        core::Logger::Warning("Noma'lum rejim: " + modeName);
    }
}

} // namespace command
} // namespace liil
