#include <windows.h>
#include "ui/MainWindow.h"
#include "core/Logger.h"
#include "config/ConfigManager.h"
#include <string>
#include <filesystem>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Avoid unused parameter warnings
    (void)hPrevInstance;
    (void)pCmdLine;

    // Set working directory to executable directory (optional, but good for local config/logs)
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::filesystem::path exePath(path);
    std::filesystem::current_path(exePath.parent_path());

    // Initialize ConfigManager
    auto& config = liil::config::ConfigManager::GetInstance();
    if (!config.Load("settings.ini")) {
        // If it doesn't exist, set some defaults and save
        config.SetString("app_name", "LiiL");
        config.SetString("version", "0.1.0");
        config.Save();
    }

    // Initialize UI
    liil::ui::MainWindow mainWindow;
    
    // Initialize Logger and attach UI callback
    auto& logger = liil::core::Logger::GetInstance();
    logger.Initialize("liil.log");
    
    // Setup lambda to append logs to the UI safely
    logger.SetUICallback([&mainWindow](const std::string& msg) {
        mainWindow.AppendLog(msg);
    });

    liil::core::Logger::Info("LiiL starting up...");
    liil::core::Logger::Info("Version: " + config.GetString("version"));

    if (!mainWindow.Initialize(hInstance, nCmdShow)) {
        MessageBoxW(NULL, L"Failed to create main window!", L"Error", MB_ICONERROR);
        return 0;
    }

    liil::core::Logger::Info("LiiL started successfully. Waiting for commands...");

    // Run Message Loop
    mainWindow.RunMessageLoop();

    liil::core::Logger::Info("LiiL shutting down...");

    return 0;
}
