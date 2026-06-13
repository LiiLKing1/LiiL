#include "SystemActions.h"
#include "../core/Logger.h"
#include "../config/ConfigManager.h"
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cwctype>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace liil {
namespace command {

void SystemActions::TakeScreenshot() {
    int screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, screenX, screenY, SRCCOPY);
    SelectObject(hMemoryDC, hOldBitmap);

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBitmap);
        CloseClipboard();
        core::Logger::Info("Screenshot xotiraga (clipboard) saqlandi.");
    } else {
        core::Logger::Error("Clipboardni ochishda xatolik yuz berdi.");
        DeleteObject(hBitmap);
    }

    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}

void SystemActions::CopySelection() {
    core::Logger::Info("Nusxa olinmoqda (Ctrl+C)...");

    INPUT inputs[4] = {};
    ZeroMemory(inputs, sizeof(inputs));

    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'C';

    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'C';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    UINT uSent = SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    if (uSent != ARRAYSIZE(inputs)) {
        core::Logger::Error("Nusxa olishda (Ctrl+C) xatolik.");
    }
}

void SystemActions::OpenFolder(const std::string& folderName) {
    KNOWNFOLDERID folderId = FOLDERID_Desktop;
    bool known = false;

    if (folderName == "downloads" || folderName == "yuklanmalar") {
        folderId = FOLDERID_Downloads;
        known = true;
    } else if (folderName == "documents" || folderName == "hujjatlar") {
        folderId = FOLDERID_Documents;
        known = true;
    } else if (folderName == "desktop" || folderName == "ish stoli") {
        folderId = FOLDERID_Desktop;
        known = true;
    } else if (folderName == "pictures" || folderName == "rasmlar") {
        folderId = FOLDERID_Pictures;
        known = true;
    } else if (folderName == "music" || folderName == "musiqa") {
        folderId = FOLDERID_Music;
        known = true;
    } else if (folderName == "videos" || folderName == "videolar") {
        folderId = FOLDERID_Videos;
        known = true;
    }

    if (known) {
        PWSTR path = NULL;
        if (SUCCEEDED(SHGetKnownFolderPath(folderId, 0, NULL, &path))) {
            core::Logger::Info("Papka ochilmoqda: " + folderName);
            ShellExecuteW(NULL, L"explore", path, NULL, NULL, SW_SHOWNORMAL);
            CoTaskMemFree(path);
        } else {
            core::Logger::Error("Papkani topib bo'lmadi: " + folderName);
        }
    } else {
        // Just try to open it as a generic path if it's not a known special folder
        core::Logger::Info("Yo'lak ochilmoqda: " + folderName);
        int wlen = MultiByteToWideChar(CP_UTF8, 0, folderName.c_str(), -1, NULL, 0);
        std::wstring wpath(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, folderName.c_str(), -1, &wpath[0], wlen);
        ShellExecuteW(NULL, L"explore", wpath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

void SystemActions::CloseApp(const std::string& appName) {
    core::Logger::Info("Dasturni yopishga harakat qilinmoqda: " + appName);
    
    // Map common names to exe names
    std::string exeName = appName;
    if (appName == "yandex") exeName = "browser.exe";
    else if (appName == "telegram") exeName = "Telegram.exe";
    else if (appName == "steam") exeName = "steam.exe";
    else if (appName == "roblox") exeName = "RobloxPlayerBeta.exe";
    else if (appName == "yandexmusic") exeName = "YandexMusic.exe";
    else exeName += ".exe";

    std::wstring wexeName;
    wexeName.assign(exeName.begin(), exeName.end());
    
    std::wstring searchLower = wexeName;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), std::towlower);
    
    std::wstring appNameW;
    appNameW.assign(appName.begin(), appName.end());
    std::transform(appNameW.begin(), appNameW.end(), appNameW.begin(), std::towlower);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        core::Logger::Error("Jarayonlarni o'qib bo'lmadi.");
        return;
    }

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);

    bool found = false;
    if (Process32FirstW(hSnap, &pe32)) {
        do {
            std::wstring currentExe = pe32.szExeFile;
            std::wstring currentExeLower = currentExe;
            std::transform(currentExeLower.begin(), currentExeLower.end(), currentExeLower.begin(), std::towlower);

            std::wstring noSpaceAppName = appNameW;
            noSpaceAppName.erase(std::remove(noSpaceAppName.begin(), noSpaceAppName.end(), L' '), noSpaceAppName.end());

            std::wstring noSpaceExe = currentExeLower;
            noSpaceExe.erase(std::remove(noSpaceExe.begin(), noSpaceExe.end(), L' '), noSpaceExe.end());

            bool match = false;
            if (currentExeLower == searchLower) {
                match = true;
            } else if (!noSpaceAppName.empty() && noSpaceExe.find(noSpaceAppName) != std::wstring::npos) {
                match = true;
            }

            if (match) {
                found = true;
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess != NULL) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    std::wstring wexe(pe32.szExeFile);
                    std::string narrowExe(wexe.begin(), wexe.end());
                    core::Logger::Info(narrowExe + " yopildi.");
                } else {
                    std::wstring wexe(pe32.szExeFile);
                    std::string narrowExe(wexe.begin(), wexe.end());
                    core::Logger::Error(narrowExe + " ni yopish uchun huquq yetarli emas.");
                }
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    
    CloseHandle(hSnap);

    if (!found) {
        core::Logger::Warning(exeName + " nomli ishlayotgan jarayon topilmadi.");
    }
}

} // namespace command
} // namespace liil
