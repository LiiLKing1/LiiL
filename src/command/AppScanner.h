#pragma once
#include <string>
#include <vector>

namespace liil {
namespace command {

// O'rnatilgan dastur haqida ma'lumot
struct AppInfo {
    std::string name;       // Ko'rinish nomi (masalan: "Telegram")
    std::string path;       // To'liq yo'l (.lnk yoki .exe)
    std::string searchKey;  // Kichik harflar, probelsiz (masalan: "telegram")
};

class AppScanner {
public:
    // Barcha manbalarni skaner qilish (Start Menu, Desktop, Apps/)
    static void ScanAll();

    // Topilgan dasturlar ro'yxatini qaytarish
    static const std::vector<AppInfo>& GetApps();

    // Nom bo'yicha izlash — yo'lni qaytaradi, topilmasa bo'sh satr
    static std::string FindApp(const std::string& name);

    // Dastur .exe nomini izlash (CloseApp uchun)
    static std::string FindExeName(const std::string& name);

    // Barcha dasturlarni log ga chiqarish
    static void PrintAppList();

private:
    // Start Menu papkasini rekursiv skanerlash
    static void ScanDirectory(const std::wstring& dirPath);

    // Bir faylni ro'yxatga qo'shish
    static void AddEntry(const std::wstring& filePath);

    // Nom normalizatsiyasi: kichik harf, probelsiz
    static std::string Normalize(const std::string& s);

    static std::vector<AppInfo> s_apps;
    static bool s_scanned;
};

} // namespace command
} // namespace liil
