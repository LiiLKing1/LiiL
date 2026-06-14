#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace liil {
namespace ui {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    void RunMessageLoop();
    void AppendLog(const std::string& message);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK InputEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnResize(int width, int height);
    void OnInputReady();

    HWND m_hwnd;
    HWND m_hLogArea;
    HWND m_hInputBox;
    HWND m_hMicBtn;
    HWND m_hPauseBtn;
    HWND m_hCancelBtn;
    
    HFONT m_hFont;
    
    // Equalizer va Voice
    int m_amplitude;
    std::vector<int> m_eqHistory;
    
    void UpdateEqualizer(int amp);
    void DrawEqualizer(HDC hdc, RECT rect);
    void ToggleDictation();
    void HandleDictationResult(const std::string& text);
};

} // namespace ui
} // namespace liil
