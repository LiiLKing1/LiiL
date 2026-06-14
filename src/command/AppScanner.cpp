#include "AppScanner.h"
#include "../core/Logger.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <mutex>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace liil {
namespace command {

// Statik a'zolar
std::vector<AppInfo> AppScanner::s_apps;
bool AppScanner::s_scanned = false;
static std::mutex s_mutex;

// -----------------------------------------------------------------------
// Ichki yordamchi: string normalizatsiya
// -----------------------------------------------------------------------
std::string AppScanner::Normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (!std::isspace(c)) {
            out += static_cast<char>(std::tolower(c));
        }
    }
    return out;
}

// -----------------------------------------------------------------------
// Bitta faylni ro'yxatga qo'shish (.lnk yoki .exe)
// -----------------------------------------------------------------------
void AppScanner::AddEntry(const std::wstring& filePath) {
    namespace fs = std::filesystem;
    fs::path p(filePath);

    std::wstring ext = p.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    // Faqat .lnk va .exe fayllarni qo'shamiz
    if (ext != L".lnk" && ext != L".exe") return;

    // Nom: kengaytmasiz stem
    std::wstring wstem = p.stem().wstring();
    std::string name(wstem.begin(), wstem.end());

    // To'liq yo'l
    std::wstring wfull = p.wstring();
    std::string fullPath(wfull.begin(), wfull.end());

    // Takrorlanmaslik uchun tekshirish
    std::string key = Normalize(name);
    for (const auto& existing : s_apps) {
        if (existing.searchKey == key) return; // allaqachon bor
    }

    AppInfo info;
    info.name      = name;
    info.path      = fullPath;
    info.searchKey = key;
    s_apps.push_back(info);
}

// -----------------------------------------------------------------------
// Papkani rekursiv skanerlash
// -----------------------------------------------------------------------
void AppScanner::ScanDirectory(const std::wstring& dirPath) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) return;

    try {
        auto it = fs::recursive_directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec);
        auto end = fs::recursive_directory_iterator();

        while (it != end && !ec) {
            if (it->is_regular_file(ec)) {
                AddEntry(it->path().wstring());
            }
            
            // Xavfsiz keyingi qadamga o'tish (Exception tashlamaydi, ec ga yozadi)
            it.increment(ec);
            if (ec) {
                // Agar o'tishda xatolik bo'lsa (Access Denied), davom etishga harakat
                ec.clear();
            }
        }
    } catch (...) {
        // Unhandled C++ exception larni butunlay e'tiborsiz qoldirish
    }
}

// -----------------------------------------------------------------------
// Asosiy skanerlash: barcha manbalar
// -----------------------------------------------------------------------
void AppScanner::ScanAll() {
    std::lock_guard<std::mutex> lock(s_mutex);
    
    // COM subsystem for this thread (required for SHGetKnownFolderPath)
    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    s_apps.clear();
    s_scanned = true;

    // --- 1. Barcha foydalanuvchilar uchun Start Menu ---
    {
        PWSTR pPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms, 0, NULL, &pPath))) {
            ScanDirectory(pPath);
            CoTaskMemFree(pPath);
        }
    }

    // --- 2. Joriy foydalanuvchi Start Menu ---
    {
        PWSTR pPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, NULL, &pPath))) {
            ScanDirectory(pPath);
            CoTaskMemFree(pPath);
        }
    }

    // --- 3. Desktop (ish stoli) ---
    {
        PWSTR pPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &pPath))) {
            ScanDirectory(pPath);
            CoTaskMemFree(pPath);
        }
    }

    // --- 4. LiiL/Apps/ papkasi (qo'lda qo'shilganlar) ---
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::filesystem::path appsDir =
            std::filesystem::path(exePath).parent_path() / L"Apps";
        if (std::filesystem::exists(appsDir)) {
            ScanDirectory(appsDir.wstring());
        }
    }

    // core::Logger::Info("AppScanner: " + std::to_string(s_apps.size()) +
    //                   " ta dastur topildi.");

    if (SUCCEEDED(hrCom)) {
        CoUninitialize();
    }
}

// -----------------------------------------------------------------------
// Getter
// -----------------------------------------------------------------------
const std::vector<AppInfo>& AppScanner::GetApps() {
    if (!s_scanned) ScanAll();
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_apps;
}

// -----------------------------------------------------------------------
// Nom bo'yicha izlash — to'liq yo'lni qaytaradi
// -----------------------------------------------------------------------
std::string AppScanner::FindApp(const std::string& name) {
    if (!s_scanned) ScanAll();
    std::lock_guard<std::mutex> lock(s_mutex);

    std::string query = Normalize(name);
    if (query.empty()) return "";

    // 1. To'liq mos kelish
    for (const auto& app : s_apps) {
        if (app.searchKey == query) return app.path;
    }

    // 2. Qisman mos kelish (query app nomida topilsa)
    for (const auto& app : s_apps) {
        if (app.searchKey.find(query) != std::string::npos) return app.path;
    }

    // 3. App nomi query ichida topilsa (teskari)
    for (const auto& app : s_apps) {
        if (query.find(app.searchKey) != std::string::npos) return app.path;
    }

    return "";
}

// -----------------------------------------------------------------------
// .exe nomini izlash (CloseApp uchun)
// -----------------------------------------------------------------------
std::string AppScanner::FindExeName(const std::string& name) {
    if (!s_scanned) ScanAll();
    std::lock_guard<std::mutex> lock(s_mutex);

    std::string query = Normalize(name);
    if (query.empty()) return "";

    // Initialize COM for IShellLink if not already initialized on this thread
    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    auto extractExe = [](const std::string& pathStr) {
        std::filesystem::path p(pathStr);
        std::wstring ext = p.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        
        if (ext == L".exe") {
            return p.filename().string();
        }
        
        // If it's a shortcut (.lnk), read its real target executable
        if (ext == L".lnk") {
            IShellLinkW* psl;
            std::string exeName = p.stem().string() + ".exe"; // Fallback
            
            if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl))) {
                IPersistFile* ppf;
                if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
                    std::wstring wpathStr = p.wstring();
                    if (SUCCEEDED(ppf->Load(wpathStr.c_str(), STGM_READ))) {
                        wchar_t szGotPath[MAX_PATH];
                        if (SUCCEEDED(psl->GetPath(szGotPath, MAX_PATH, NULL, SLGP_UNCPRIORITY))) {
                            std::filesystem::path targetP(szGotPath);
                            std::wstring targetExt = targetP.extension().wstring();
                            std::transform(targetExt.begin(), targetExt.end(), targetExt.begin(), ::towlower);
                            if (targetExt == L".exe") {
                                std::wstring wname = targetP.filename().wstring();
                                int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, NULL, 0, NULL, NULL);
                                if (len > 0) {
                                    std::string s(len - 1, 0);
                                    WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &s[0], len, NULL, NULL);
                                    exeName = s;
                                }
                            }
                        }
                    }
                    ppf->Release();
                }
                psl->Release();
            }
            return exeName;
        }
        return p.stem().string() + ".exe";
    };

    // 1. To'liq mos kelish
    for (const auto& app : s_apps) {
        if (app.searchKey == query) {
            std::string res = extractExe(app.path);
            if (SUCCEEDED(hrCom)) CoUninitialize();
            return res;
        }
    }

    // 2. Qisman mos kelish (query app nomida topilsa)
    for (const auto& app : s_apps) {
        if (app.searchKey.find(query) != std::string::npos) {
            std::string res = extractExe(app.path);
            if (SUCCEEDED(hrCom)) CoUninitialize();
            return res;
        }
    }

    // 3. Teskari qisman (app nomi query ichida topilsa)
    for (const auto& app : s_apps) {
        if (query.find(app.searchKey) != std::string::npos) {
            std::string res = extractExe(app.path);
            if (SUCCEEDED(hrCom)) CoUninitialize();
            return res;
        }
    }
    
    if (SUCCEEDED(hrCom)) CoUninitialize();
    return "";
}

// -----------------------------------------------------------------------
// Barcha dasturlarni log ga chiqarish
// -----------------------------------------------------------------------
void AppScanner::PrintAppList() {
    if (!s_scanned) ScanAll();
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_apps.empty()) {
        core::Logger::Info("Hech qanday dastur topilmadi.");
        return;
    }

    core::Logger::Info("=== O'rnatilgan dasturlar (" +
                       std::to_string(s_apps.size()) + " ta) ===");
    for (const auto& app : s_apps) {
        core::Logger::Info("  [" + app.name + "]  ->  " + app.path);
    }
    core::Logger::Info("=== Ro'yxat tugadi ===");
}

} // namespace command
} // namespace liil
