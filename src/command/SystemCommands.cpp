#include "SystemCommands.h"
#include "AppScanner.h"
#include "../core/Logger.h"
#include "../config/ConfigManager.h"
#include <windows.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <algorithm>
#include <vector>
#include <cctype>
#include <filesystem>

#pragma comment(lib, "Dxva2.lib")

namespace liil {
namespace command {

void SystemCommands::SendVirtualKey(unsigned short key) {
    INPUT inputs[2] = {};
    ZeroMemory(inputs, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = key;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    if (uSent != ARRAYSIZE(inputs)) {
        core::Logger::Error("Klaviatura simulyatsiyasida xatolik: " + std::to_string(key));
    }
}

void SystemCommands::VolumeUp() {
    core::Logger::Info("Ovoz oshirilmoqda...");
    for (int i = 0; i < 2; ++i) { // +4% volume usually
        SendVirtualKey(VK_VOLUME_UP);
    }
}

void SystemCommands::VolumeDown() {
    core::Logger::Info("Ovoz pasaytirilmoqda...");
    for (int i = 0; i < 2; ++i) { // -4% volume usually
        SendVirtualKey(VK_VOLUME_DOWN);
    }
}

void SystemCommands::VolumeMute() {
    core::Logger::Info("Ovoz o'zgartirildi (Mute).");
    SendVirtualKey(VK_VOLUME_MUTE);
}

// Monitor brightness helper
struct MonitorEnumData {
    std::vector<HMONITOR> monitors;
};

static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    auto data = reinterpret_cast<MonitorEnumData*>(dwData);
    data->monitors.push_back(hMonitor);
    return TRUE;
}

void SystemCommands::ChangeBrightness(int delta) {
    MonitorEnumData data;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&data));

    if (data.monitors.empty()) {
        core::Logger::Error("Yorqinlikni o'zgartirish uchun monitor topilmadi.");
        return;
    }

    bool success = false;
    for (HMONITOR hMonitor : data.monitors) {
        DWORD numPhysicalMonitors = 0;
        if (GetNumberOfPhysicalMonitorsFromHMONITOR(hMonitor, &numPhysicalMonitors) && numPhysicalMonitors > 0) {
            std::vector<PHYSICAL_MONITOR> physicalMonitors(numPhysicalMonitors);
            if (GetPhysicalMonitorsFromHMONITOR(hMonitor, numPhysicalMonitors, physicalMonitors.data())) {
                for (DWORD i = 0; i < numPhysicalMonitors; ++i) {
                    DWORD minVal = 0, currentVal = 0, maxVal = 0;
                    if (GetMonitorBrightness(physicalMonitors[i].hPhysicalMonitor, &minVal, &currentVal, &maxVal)) {
                        int newVal = static_cast<int>(currentVal) + delta;
                        if (newVal < static_cast<int>(minVal)) newVal = minVal;
                        if (newVal > static_cast<int>(maxVal)) newVal = maxVal;
                        
                        SetMonitorBrightness(physicalMonitors[i].hPhysicalMonitor, newVal);
                        success = true;
                    }
                }
                DestroyPhysicalMonitors(numPhysicalMonitors, physicalMonitors.data());
            }
        }
    }

    if (success) {
        core::Logger::Info("Yorqinlik o'zgartirildi: " + std::to_string(delta > 0 ? 10 : -10) + "%");
    } else {
        core::Logger::Warning("Physical Monitor API orqali yorqinlikni o'zgartirib bo'lmadi. Powershell orqali urinib ko'ramiz...");
        // WMI fallback for laptops
        std::string psCmd = "powershell -WindowStyle Hidden -Command \"$b = Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightness; $level = $b.CurrentBrightness + " + std::to_string(delta) + "; if($level -lt 0) { $level=0 }; if($level -gt 100) { $level=100 }; Invoke-CimMethod -Namespace root/WMI -ClassName WmiMonitorBrightnessMethods -MethodName WmiSetBrightness -Arguments @{Timeout=0; Brightness=$level}\"";
        
        int wlen = MultiByteToWideChar(CP_UTF8, 0, psCmd.c_str(), -1, NULL, 0);
        std::wstring wcmd(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, psCmd.c_str(), -1, &wcmd[0], wlen);
        
        _wsystem(wcmd.c_str());
        core::Logger::Info("Yorqinlik komandasi yuborildi.");
    }
}

void SystemCommands::BrightnessUp() {
    ChangeBrightness(10);
}

void SystemCommands::BrightnessDown() {
    ChangeBrightness(-10);
}

void SystemCommands::OpenApp(const std::string& appName) {
    // 1. AppScanner orqali barcha manbalardan izlash
    //    (Start Menu, Desktop, Apps/ papkasi)
    std::string foundPath = AppScanner::FindApp(appName);

    if (!foundPath.empty()) {
        core::Logger::Info("Topildi va ochilmoqda: " + foundPath);
        int wlen = MultiByteToWideChar(CP_UTF8, 0, foundPath.c_str(), -1, NULL, 0);
        std::wstring wpath(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, foundPath.c_str(), -1, &wpath[0], wlen);
        HINSTANCE result = ShellExecuteW(NULL, L"open", wpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) > 32) return;
        core::Logger::Warning("ShellExecute muvaffaqiyatsiz, zaxira usulga o'tilmoqda...");
    }

    // 2. ConfigManager'dan maxsus yo'lni tekshirish
    auto& config = config::ConfigManager::GetInstance();
    std::string key = "app_" + appName;
    for (auto& c : key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string path = config.GetString(key, "");

    // 3. Oxirgi zaxira: mashhur dasturlar uchun hard-coded yo'llar
    if (path.empty()) {
        if (appName == "yandex") path = "browser.exe";
        else if (appName == "steam") path = "steam://";
        else if (appName == "telegram") path = "tg://";
        else if (appName == "roblox") path = "robloxplayer-launcher.exe";
        else path = appName + ".exe";
    }

    core::Logger::Info("Dastur ochilmoqda (zaxira): " + appName + " (" + path + ")");
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
    HINSTANCE result = ShellExecuteW(NULL, L"open", wpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        core::Logger::Error("Dastur topilmadi: '" + appName +
            "'. Start Menu'da yo'q bo'lishi mumkin.");
    }
}

} // namespace command
} // namespace liil
