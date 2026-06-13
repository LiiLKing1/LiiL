#pragma once

#include <windows.h>
#include <string>

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
    
    HFONT m_hFont;
};

} // namespace ui
} // namespace liil
