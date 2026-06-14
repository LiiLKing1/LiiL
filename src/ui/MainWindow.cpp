#include "MainWindow.h"
#include "../core/Logger.h"
#include "../command/CommandEngine.h"
#include "../voice/VoiceManager.h"
#include <commctrl.h>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "msimg32.lib")

#define IDC_MIC_BTN 101
#define IDC_PAUSE_BTN 102
#define IDC_CANCEL_BTN 103

#define WM_UPDATE_EQ (WM_USER + 1)
#define WM_DICT_RESULT (WM_USER + 2)

namespace liil {
namespace ui {

// Helper struct for cross-thread text passing
struct StringWrapper {
    std::string text;
};

MainWindow::MainWindow() : m_hwnd(NULL), m_hLogArea(NULL), m_hInputBox(NULL), 
    m_hMicBtn(NULL), m_hPauseBtn(NULL), m_hCancelBtn(NULL), m_hFont(NULL), m_amplitude(0) {
    m_eqHistory.resize(60, 0);
}

MainWindow::~MainWindow() {
    if (m_hFont) DeleteObject(m_hFont);
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

    m_hwnd = CreateWindowEx(0, CLASS_NAME, L"LiiL Assistant", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 850, 650, NULL, NULL, hInstance, this);

    if (m_hwnd == NULL) return false;

    m_hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    m_hLogArea = CreateWindowEx(0, L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, 0, 0, m_hwnd, NULL, hInstance, NULL);
    SendMessage(m_hLogArea, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_hInputBox = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 0, 0, m_hwnd, NULL, hInstance, NULL);
    SendMessage(m_hInputBox, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    SetWindowSubclass(m_hInputBox, InputEditProc, 1, (DWORD_PTR)this);

    // Voice UI Buttons
    m_hMicBtn = CreateWindowEx(0, L"BUTTON", L"Mic", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, m_hwnd, (HMENU)IDC_MIC_BTN, hInstance, NULL);
    SendMessage(m_hMicBtn, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_hPauseBtn = CreateWindowEx(0, L"BUTTON", L"Pause", WS_CHILD | BS_PUSHBUTTON,
        0, 0, 0, 0, m_hwnd, (HMENU)IDC_PAUSE_BTN, hInstance, NULL);
    SendMessage(m_hPauseBtn, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    m_hCancelBtn = CreateWindowEx(0, L"BUTTON", L"Cancel", WS_CHILD | BS_PUSHBUTTON,
        0, 0, 0, 0, m_hwnd, (HMENU)IDC_CANCEL_BTN, hInstance, NULL);
    SendMessage(m_hCancelBtn, WM_SETFONT, (WPARAM)m_hFont, TRUE);

    // Set callback for Equalizer
    voice::VoiceManager::GetInstance().SetAmplitudeCallback([this](int amp) {
        PostMessage(m_hwnd, WM_UPDATE_EQ, amp, 0);
    });

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
    int wlen = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, NULL, 0);
    std::wstring wmsg(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, &wmsg[0], wlen);

    int len = GetWindowTextLength(m_hLogArea);
    SendMessage(m_hLogArea, EM_SETSEL, (WPARAM)len, (LPARAM)len);
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

    if (pThis) return pThis->HandleMessage(hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            OnResize(width, height);
            return 0;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_MIC_BTN) {
                ToggleDictation();
            } else if (wmId == IDC_PAUSE_BTN) {
                auto& vm = voice::VoiceManager::GetInstance();
                if (vm.IsDictating()) {
                    vm.PauseDictation();
                    SetWindowText(m_hPauseBtn, L"Resume");
                } else {
                    vm.ResumeDictation();
                    SetWindowText(m_hPauseBtn, L"Pause");
                }
            } else if (wmId == IDC_CANCEL_BTN) {
                voice::VoiceManager::GetInstance().CancelDictation();
                SetWindowText(m_hMicBtn, L"Mic");
                ShowWindow(m_hPauseBtn, SW_HIDE);
                ShowWindow(m_hCancelBtn, SW_HIDE);
                m_amplitude = 0;
                std::fill(m_eqHistory.begin(), m_eqHistory.end(), 0);
                InvalidateRect(m_hwnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // Chizish hududi: Input qutisining ustida
            rect.bottom -= 40; 
            rect.top = rect.bottom - 60;
            rect.left += 10;
            rect.right -= 10;
            
            DrawEqualizer(hdc, rect);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1; // Flicckering oldini olish uchun (GDI qismi bilan to'ldiramiz)
            
        case WM_UPDATE_EQ: {
            int amp = static_cast<int>(wParam);
            UpdateEqualizer(amp);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            rect.bottom -= 40; 
            rect.top = rect.bottom - 60;
            InvalidateRect(hwnd, &rect, FALSE);
            return 0;
        }
        case WM_DICT_RESULT: {
            StringWrapper* wrap = reinterpret_cast<StringWrapper*>(lParam);
            HandleDictationResult(wrap->text);
            delete wrap;
            
            SetWindowText(m_hMicBtn, L"Mic");
            ShowWindow(m_hPauseBtn, SW_HIDE);
            ShowWindow(m_hCancelBtn, SW_HIDE);
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
        int btnWidth = 100;
        
        // LogArea endi sal teparoqda tugaydi (Equalizer uchun joy qolishi uchun)
        MoveWindow(m_hLogArea, padding, padding, width - 2 * padding, height - inputHeight - 80, TRUE);
        
        MoveWindow(m_hInputBox, padding, height - inputHeight - padding, width - 3 * padding - btnWidth * 3, inputHeight, TRUE);
        
        // Buttons
        MoveWindow(m_hMicBtn, width - padding - btnWidth * 3, height - inputHeight - padding, btnWidth, inputHeight, TRUE);
        MoveWindow(m_hPauseBtn, width - padding - btnWidth * 2, height - inputHeight - padding, btnWidth, inputHeight, TRUE);
        MoveWindow(m_hCancelBtn, width - padding - btnWidth, height - inputHeight - padding, btnWidth, inputHeight, TRUE);
    }
}

void MainWindow::UpdateEqualizer(int amp) {
    m_amplitude = amp;
    m_eqHistory.erase(m_eqHistory.begin());
    m_eqHistory.push_back(amp);
}

void MainWindow::DrawEqualizer(HDC hdc, RECT rect) {
    HBRUSH bgBrush = CreateSolidBrush(RGB(5, 10, 15)); // Dark blue/black background
    FillRect(hdc, &rect, bgBrush);
    DeleteObject(bgBrush);

    if (!voice::VoiceManager::GetInstance().IsDictating()) return;

    // Frequencies matching the image
    const wchar_t* freqs[] = {L"31", L"62", L"125", L"250", L"500", L"1K", L"2K", L"4K", L"8K", L"16K"};
    int numBars = 10;
    int totalWidth = rect.right - rect.left;
    int barSpacing = 10;
    int barWidth = (totalWidth / numBars) - barSpacing;
    if (barWidth < 5) barWidth = 5;

    int textHeight = 20;
    int eqTop = rect.top + 5;
    int eqBottom = rect.bottom - textHeight - 5;
    int maxHeight = eqBottom - eqTop;

    int blockHeight = 4;
    int blockSpacing = 2;
    int maxBlocks = maxHeight / (blockHeight + blockSpacing);
    if (maxBlocks < 1) maxBlocks = 1;

    // m_amplitude is a 0-100 value. We create a pyramid shape centered around the 4th/5th bars.
    // Base height is always 1 block.
    int centerIndex = 4; 
    
    // Create text font if needed or just use current
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(200, 255, 200));

    for (int i = 0; i < numBars; ++i) {
        // Calculate pyramid shape distribution
        int dist = std::abs(i - centerIndex);
        double multiplier = 1.0 - (dist * 0.15); // The further from center, the lower
        if (multiplier < 0.1) multiplier = 0.1;

        int targetBlocks = (int)((m_amplitude * maxBlocks / 100.0) * multiplier);
        if (targetBlocks < 1) targetBlocks = 1; // Always show bottom block

        int xPos = rect.left + (i * (barWidth + barSpacing)) + (barSpacing / 2);

        // Draw frequency text at the bottom
        RECT textRect = {xPos - 5, eqBottom + 2, xPos + barWidth + 5, rect.bottom};
        DrawTextW(hdc, freqs[i], -1, &textRect, DT_CENTER | DT_SINGLELINE);

        // Draw blocks for this bar
        for (int b = 0; b < targetBlocks; ++b) {
            // Colors: Cyan-green matrix look
            int r = 0;
            int g = 255;
            int blue = 150 - (b * (150/maxBlocks)); // more blue at bottom
            if (blue < 0) blue = 0;

            // Top blocks can be slightly more yellowish/bright
            if (b > maxBlocks * 0.7) {
                r = 200;
                blue = 0;
            }

            HBRUSH blockBrush = CreateSolidBrush(RGB(r, g, blue));
            
            RECT blockRect;
            blockRect.left = xPos;
            blockRect.right = xPos + barWidth;
            blockRect.bottom = eqBottom - (b * (blockHeight + blockSpacing));
            blockRect.top = blockRect.bottom - blockHeight;
            
            FillRect(hdc, &blockRect, blockBrush);
            DeleteObject(blockBrush);
        }
    }
}

void MainWindow::ToggleDictation() {
    auto& vm = voice::VoiceManager::GetInstance();
    if (vm.IsDictating()) {
        SetWindowText(m_hMicBtn, L"Tarjima...");
        vm.StopAndTranscribe([this](const std::string& text) {
            StringWrapper* wrap = new StringWrapper{text};
            PostMessage(m_hwnd, WM_DICT_RESULT, 0, reinterpret_cast<LPARAM>(wrap));
        });
    } else {
        if (vm.StartDictation()) {
            SetWindowText(m_hMicBtn, L"Stop");
            ShowWindow(m_hPauseBtn, SW_SHOW);
            ShowWindow(m_hCancelBtn, SW_SHOW);
        }
    }
}

void MainWindow::HandleDictationResult(const std::string& text) {
    if (text.empty()) {
        core::Logger::Warning("Ovozdan matn olinmadi yoki xato yuz berdi.");
        return;
    }
    
    // Set text to input box and trigger Enter
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    std::wstring wmsg(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wmsg[0], wlen);
    
    SetWindowText(m_hInputBox, wmsg.c_str());
    OnInputReady(); // Simulate Enter press
}

void MainWindow::OnInputReady() {
    int len = GetWindowTextLength(m_hInputBox);
    if (len == 0) return;

    std::vector<wchar_t> buffer(len + 1);
    GetWindowText(m_hInputBox, &buffer[0], len + 1);

    int nlen = WideCharToMultiByte(CP_UTF8, 0, &buffer[0], -1, NULL, 0, NULL, NULL);
    std::string msg(nlen - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, &buffer[0], -1, &msg[0], nlen, NULL, NULL);

    SetWindowText(m_hInputBox, L"");
    core::Logger::Info("User: " + msg);
    command::CommandEngine::GetInstance().Execute(msg);
}

LRESULT CALLBACK MainWindow::InputEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    MainWindow* pThis = (MainWindow*)dwRefData;
    switch (uMsg) {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                pThis->OnInputReady();
                return 0;
            }
            break;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

} // namespace ui
} // namespace liil
