#pragma once
#include <string>

namespace liil {
namespace command {

class SystemCommands {
public:
    static void VolumeUp();
    static void VolumeDown();
    static void VolumeMute();
    
    static void BrightnessUp();
    static void BrightnessDown();
    
    static void OpenApp(const std::string& appName);
    
private:
    static void SendVirtualKey(unsigned short key);
    static void ChangeBrightness(int delta);
};

} // namespace command
} // namespace liil
