#include "SystemMonitor.h"
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace liil {
namespace core {

std::string SystemMonitor::GetSystemContext() {
    std::ostringstream ctx;
    ctx << "[SYSTEM CONTEXT: " << GetCurrentTimeStr() << "]\n";
    ctx << "OS: " << GetOSInfo() << "\n";
    ctx << "Memory: " << GetMemoryInfo() << "\n";
    ctx << "Battery: " << GetBatteryInfo() << "\n";
    return ctx.str();
}

std::string SystemMonitor::GetMemoryInfo() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
        
        double totalGB = static_cast<double>(totalPhysMem) / (1024 * 1024 * 1024);
        double usedGB = static_cast<double>(physMemUsed) / (1024 * 1024 * 1024);
        
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << usedGB << "GB ishlatilmoqda / jami " << totalGB << "GB (" << memInfo.dwMemoryLoad << "% band)";
        return ss.str();
    }
    return "Noma'lum";
}

std::string SystemMonitor::GetBatteryInfo() {
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        std::ostringstream ss;
        if (status.BatteryFlag == 128 || status.BatteryFlag == 255) {
            ss << "Batareya yo'q yoki holati noma'lum";
        } else {
            ss << (int)status.BatteryLifePercent << "%";
            if (status.ACLineStatus == 1) {
                ss << " (Quvvatga ulangan)";
            } else {
                ss << " (Batareyada ishlamoqda)";
            }
        }
        return ss.str();
    }
    return "Noma'lum";
}

std::string SystemMonitor::GetOSInfo() {
    // Windows 10/11 uchun oddiy nomlash
    return "Windows (x64)";
}

std::string SystemMonitor::GetCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    struct tm buf;
    localtime_s(&buf, &time);
    ss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace core
} // namespace liil
