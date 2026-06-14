#pragma once

#include <string>

namespace liil {
namespace core {

class SystemMonitor {
public:
    // Tizim parametrlarini xulosa qilib (matn ko'rinishida) AI prompti uchun qaytaradi
    static std::string GetSystemContext();

private:
    static std::string GetMemoryInfo();
    static std::string GetBatteryInfo();
    static std::string GetOSInfo();
    static std::string GetCurrentTimeStr();
};

} // namespace core
} // namespace liil
