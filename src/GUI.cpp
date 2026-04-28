#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include "Lexer.hpp"
#include "Parser.hpp"
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

using namespace Gdiplus;

// Global variables for the UI
HWND hEditInput;
HWND hOutputArea;
HWND hVisualizerArea;
HFONT hFont;
ULONG_PTR gdiplusToken;

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
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    const wchar_t CLASS_NAME[] = L"WaffleStudioWindow";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30)); // Dark background

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Waffle Studio",
        WS_POPUP | WS_THICKFRAME | WS_VISIBLE,
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

    GdiplusShutdown(gdiplusToken);
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static POINT ptLastMouse;
    static bool bDragging = false;

    switch (uMsg) {
        case WM_CREATE: {
            hFont = CreateFont(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 
                               DEFAULT_PITCH | FF_MODERN, L"Consolas");

            hEditInput = CreateWindowEx(0, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                25, 80, 600, 300, hwnd, NULL, NULL, NULL);
            SendMessage(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);

            hOutputArea = CreateWindowEx(0, L"EDIT", L"waffle-shell > Welcome to Waffle Studio\r\nType your C++ math code above...", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                25, 400, 600, 250, hwnd, NULL, NULL, NULL);
            SendMessage(hOutputArea, WM_SETFONT, (WPARAM)hFont, TRUE);

            hVisualizerArea = CreateWindowEx(0, L"EDIT", L"--- COMPILER GUTS ---", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                650, 80, 310, 570, hwnd, NULL, NULL, NULL);
            SendMessage(hVisualizerArea, WM_SETFONT, (WPARAM)hFont, TRUE);
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            // Responsive Layout Calculation
            int leftPanelWidth = (width * 65) / 100;
            int rightPanelWidth = (width * 30) / 100;
            int inputHeight = (height * 50) / 100;
            int outputHeight = (height * 35) / 100;

            MoveWindow(hEditInput, 25, 80, leftPanelWidth - 25, inputHeight, TRUE);
            MoveWindow(hOutputArea, 25, 80 + inputHeight + 20, leftPanelWidth - 25, outputHeight, TRUE);
            MoveWindow(hVisualizerArea, leftPanelWidth + 20, 80, rightPanelWidth, height - 100, TRUE);
            
            InvalidateRect(hwnd, NULL, TRUE); // Redraw title bar
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            if (y >= 18 && y <= 30) {
                if (x >= 20 && x <= 32) PostQuitMessage(0);
                else if (x >= 40 && x <= 52) ShowWindow(hwnd, SW_MINIMIZE);
                else if (x >= 60 && x <= 72) {
                    if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);
                    else ShowWindow(hwnd, SW_MAXIMIZE);
                }
            }

            if (y < 50) {
                bDragging = true;
                GetCursorPos(&ptLastMouse);
                SetCapture(hwnd);
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (bDragging) {
                POINT ptCurrent;
                GetCursorPos(&ptCurrent);
                int dx = ptCurrent.x - ptLastMouse.x;
                int dy = ptCurrent.y - ptLastMouse.y;

                RECT rc;
                GetWindowRect(hwnd, &rc);
                MoveWindow(hwnd, rc.left + dx, rc.top + dy, rc.right - rc.left, rc.bottom - rc.top, TRUE);
                ptLastMouse = ptCurrent;
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (bDragging) {
                bDragging = false;
                ReleaseCapture();
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(220, 220, 220));
            SetBkColor(hdcStatic, RGB(30, 30, 30));
            return (LRESULT)CreateSolidBrush(RGB(30, 30, 30));
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
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            HBRUSH bgBrush = CreateSolidBrush(RGB(20, 20, 20));
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);

            RECT titleRect = {0, 0, rect.right, 50};
            HBRUSH titleBrush = CreateSolidBrush(RGB(35, 35, 35));
            FillRect(hdc, &titleRect, titleBrush);
            DeleteObject(titleBrush);

            HBRUSH redBrush = CreateSolidBrush(RGB(255, 95, 87));
            HBRUSH yellowBrush = CreateSolidBrush(RGB(254, 188, 46));
            HBRUSH greenBrush = CreateSolidBrush(RGB(40, 200, 64));
            SelectObject(hdc, GetStockObject(NULL_PEN));

            SelectObject(hdc, redBrush); Ellipse(hdc, 20, 18, 32, 30);
            SelectObject(hdc, yellowBrush); Ellipse(hdc, 40, 18, 52, 30);
            SelectObject(hdc, greenBrush); Ellipse(hdc, 60, 18, 72, 30);

            DeleteObject(redBrush); DeleteObject(yellowBrush); DeleteObject(greenBrush);

            // Centered Logo and Text
            SetTextColor(hdc, RGB(200, 200, 200));
            SetBkMode(hdc, TRANSPARENT);
            std::wstring title = L"Waffle Studio";
            int titleX = (rect.right / 2) - 50;
            TextOut(hdc, titleX, 15, title.c_str(), title.length());

            // Real Logo (logo.jpg)
            Graphics graphics(hdc);
            Image image(L"logo.jpg");
            if (image.GetLastStatus() == Ok) {
                graphics.DrawImage(&image, titleX - 45, 10, 30, 30);
            } else {
                // Fallback to Symbolic Logo if image not found
                HBRUSH logoBrush = CreateSolidBrush(RGB(255, 140, 0));
                SelectObject(hdc, logoBrush);
                Ellipse(hdc, titleX - 45, 10, titleX - 15, 40);
                DeleteObject(logoBrush);
            }

            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
