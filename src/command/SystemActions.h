#pragma once
#include <string>

namespace liil {
namespace command {

class SystemActions {
public:
    static void TakeScreenshot();
    static void CopySelection();
    static void OpenFolder(const std::string& folderName);
    static void CloseApp(const std::string& appName);
};

} // namespace command
} // namespace liil
