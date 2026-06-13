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

    // Generate filename based on current time
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    struct tm buf;
#ifdef _WIN32
    localtime_s(&buf, &time);
#else
    localtime_r(&time, &buf);
#endif
    ss << "screenshot_" << std::put_time(&buf, "%Y%m%d_%H%M%S") << ".bmp";
    std::string filename = ss.str();

    // Get executable directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::filesystem::path fullPath = std::filesystem::path(exePath).parent_path() / filename;

    // Save Bitmap to File
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPFILEHEADER bmfHeader;
    BITMAPINFOHEADER bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    DWORD dwBmpSize = ((bmp.bmWidth * bi.biBitCount + 31) / 32) * 4 * bmp.bmHeight;
    HANDLE hDIB = GlobalAlloc(GHND, dwBmpSize);
    char* lpbitmap = (char*)GlobalLock(hDIB);

    GetDIBits(hScreenDC, hBitmap, 0, (UINT)bmp.bmHeight, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
        bmfHeader.bfSize = dwSizeofDIB;
        bmfHeader.bfType = 0x4D42; // BM
        bmfHeader.bfReserved1 = 0;
        bmfHeader.bfReserved2 = 0;

        DWORD dwBytesWritten = 0;
        WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
        WriteFile(hFile, (LPSTR)&bi, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
        WriteFile(hFile, (LPSTR)lpbitmap, dwBmpSize, &dwBytesWritten, NULL);
        CloseHandle(hFile);

        core::Logger::Info("Screenshot saqlandi: " + filename);
    } else {
        core::Logger::Error("Screenshot saqlashda xatolik yuz berdi.");
    }

    GlobalUnlock(hDIB);
    GlobalFree(hDIB);
    DeleteObject(hBitmap);
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
    else exeName += ".exe";

    std::wstring wexeName;
    wexeName.assign(exeName.begin(), exeName.end());

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
            // Case insensitive comparison
            bool match = true;
            if (currentExe.length() == wexeName.length()) {
                for (size_t i = 0; i < currentExe.length(); ++i) {
                    if (std::tolower(currentExe[i]) != std::tolower(wexeName[i])) {
                        match = false;
                        break;
                    }
                }
            } else {
                match = false;
            }

            if (match) {
                found = true;
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                if (hProcess != NULL) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    core::Logger::Info(exeName + " yopildi.");
                } else {
                    core::Logger::Error(exeName + " ni yopish uchun huquq yetarli emas.");
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
