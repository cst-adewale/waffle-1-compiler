#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include "Lexer.hpp"
#include "Parser.hpp"

// Global variables for the UI
HWND hEditInput;
HWND hOutputArea;
HWND hVisualizerArea;
HFONT hFont;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void UpdateVisualizer(const std::string& input) {
    if (input.empty()) return;

    try {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();

        std::string tokenStr = "--- TOKENS ---\r\n";
        for (const auto& t : tokens) {
            if (t.type == WToken::END_OF_FILE) break;
            tokenStr += "[" + t.value + "] ";
        }

        Parser parser(tokens);
        auto ast = parser.parse();
        
        std::string astStr = "\r\n\r\n--- AST TREE ---\r\n" + ast->toString();
        
        SetWindowTextA(hVisualizerArea, (tokenStr + astStr).c_str());
    } catch (...) {
        // Silently ignore errors during live typing
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"WaffleStudioWindow";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30)); // Dark background

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Waffle Studio - Waffle 1.0",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            hFont = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                               DEFAULT_PITCH | FF_MODERN, L"Consolas");

            // Input Area (Top Left)
            hEditInput = CreateWindowEx(0, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
                20, 60, 600, 300, hwnd, NULL, NULL, NULL);
            SendMessage(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Output Area (Bottom)
            hOutputArea = CreateWindowEx(0, L"EDIT", L"Waffle Studio Console\r\nWelcome! Type math above...", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                20, 380, 600, 250, hwnd, NULL, NULL, NULL);
            SendMessage(hOutputArea, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Visualizer Area (Right)
            hVisualizerArea = CreateWindowEx(0, L"EDIT", L"--- GUTS ---", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                640, 60, 320, 570, hwnd, NULL, NULL, NULL);
            SendMessage(hVisualizerArea, WM_SETFONT, (WPARAM)hFont, TRUE);

            break;
        }

        case WM_COMMAND: {
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hEditInput) {
                int length = GetWindowTextLengthA(hEditInput);
                char* buffer = new char[length + 1];
                GetWindowTextA(hEditInput, buffer, length + 1);
                UpdateVisualizer(buffer);
                delete[] buffer;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Draw "Mac Style" Buttons
            HBRUSH redBrush = CreateSolidBrush(RGB(255, 95, 87));
            HBRUSH yellowBrush = CreateSolidBrush(RGB(254, 188, 46));
            HBRUSH greenBrush = CreateSolidBrush(RGB(40, 200, 64));

            SelectObject(hdc, redBrush);
            Ellipse(hdc, 20, 20, 35, 35);
            SelectObject(hdc, yellowBrush);
            Ellipse(hdc, 45, 20, 60, 35);
            SelectObject(hdc, greenBrush);
            Ellipse(hdc, 70, 20, 85, 35);

            DeleteObject(redBrush);
            DeleteObject(yellowBrush);
            DeleteObject(greenBrush);

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
