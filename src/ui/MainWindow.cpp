#include "MainWindow.h"
#include "../core/Logger.h"
#include <commctrl.h>
#include <vector>

#pragma comment(lib, "comctl32.lib")

namespace liil {
namespace ui {

MainWindow::MainWindow() : m_hwnd(NULL), m_hLogArea(NULL), m_hInputBox(NULL), m_hFont(NULL) {
}

MainWindow::~MainWindow() {
    if (m_hFont) {
        DeleteObject(m_hFont);
    }
}

bool MainWindow::Initialize(HINSTANCE hInstance, int nCmdShow) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"LiiLMainWindowClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    m_hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"LiiL Assistant",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (m_hwnd == NULL) {
        return false;
    }

    // Create Font
    m_hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    // Create Log Area (Read-only Edit Control)
    m_hLogArea = CreateWindowEx(
        0, L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        10, 10, 760, 500,
        m_hwnd, NULL, hInstance, NULL);

    SendMessage(m_hLogArea, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Create Input Box
    m_hInputBox = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 520, 760, 30,
        m_hwnd, NULL, hInstance, NULL);

    SendMessage(m_hInputBox, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Subclass InputBox to catch Enter key
    SetWindowSubclass(m_hInputBox, InputEditProc, 1, (DWORD_PTR)this);

    ShowWindow(m_hwnd, nCmdShow);
    
    return true;
}

void MainWindow::RunMessageLoop() {
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void MainWindow::AppendLog(const std::string& message) {
    if (!m_hLogArea) return;
    
    // Convert to wide string
    int wlen = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
    std::wstring wmsg(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wmsg[0], wlen);

    // Get current text length
    int len = GetWindowTextLength(m_hLogArea);
    
    // Move selection to end
    SendMessage(m_hLogArea, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    
    // Append text
    SendMessage(m_hLogArea, EM_REPLACESEL, FALSE, (LPARAM)wmsg.c_str());
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = NULL;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (MainWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (MainWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            OnResize(width, height);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SETFOCUS:
            SetFocus(m_hInputBox);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void MainWindow::OnResize(int width, int height) {
    if (m_hLogArea && m_hInputBox) {
        int inputHeight = 30;
        int padding = 10;
        
        MoveWindow(m_hLogArea, padding, padding, width - 2 * padding, height - inputHeight - 3 * padding, TRUE);
        MoveWindow(m_hInputBox, padding, height - inputHeight - padding, width - 2 * padding, inputHeight, TRUE);
    }
}

void MainWindow::OnInputReady() {
    int len = GetWindowTextLength(m_hInputBox);
    if (len == 0) return;

    std::vector<wchar_t> buffer(len + 1);
    GetWindowText(m_hInputBox, &buffer[0], len + 1);

    // Convert to narrow string
    int nlen = WideCharToMultiByte(CP_UTF8, 0, &buffer[0], -1, NULL, 0, NULL, NULL);
    std::string msg(nlen - 1, 0); // -1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, &buffer[0], -1, &msg[0], nlen, NULL, NULL);

    // Clear input
    SetWindowText(m_hInputBox, L"");

    // Log the input
    core::Logger::Info("User: " + msg);
}

LRESULT CALLBACK MainWindow::InputEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    MainWindow* pThis = (MainWindow*)dwRefData;
    
    switch (uMsg) {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                pThis->OnInputReady();
                return 0; // Handled
            }
            break;
    }
    
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

} // namespace ui
} // namespace liil
