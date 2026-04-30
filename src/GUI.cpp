#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include "../include/Lexer.hpp"
#include "../include/Parser.hpp"
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")

using namespace Gdiplus;

#include <richedit.h>
#include <tom.h>

// Global variables for the UI
HWND hEditInput, hOutputArea, hVisualizerArea, hLineGutter;
HFONT hFont, hGutterFont;
ULONG_PTR gdiplusToken;
HINSTANCE hRichEditLib;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void UpdateLineNumbers() {
    int lineCount = SendMessage(hEditInput, EM_GETLINECOUNT, 0, 0);
    std::string numbers = "";
    for (int i = 1; i <= lineCount; ++i) {
        numbers += std::to_string(i) + "\r\n";
    }
    SetWindowTextA(hLineGutter, numbers.c_str());
    
    // Sync scrolling
    int firstLine = SendMessage(hEditInput, EM_GETFIRSTVISIBLELINE, 0, 0);
    SendMessage(hLineGutter, EM_LINESCROLL, 0, firstLine - SendMessage(hLineGutter, EM_GETFIRSTVISIBLELINE, 0, 0));
}

void ApplyHighlight(int start, int length, COLORREF color) {
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    
    // Select the range
    SendMessage(hEditInput, EM_SETSEL, start, (length == -1) ? -1 : (start + length));
    // Apply formatting to selection
    SendMessage(hEditInput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

void HighlightText(const std::string& input) {
    if (hEditInput == NULL) return;

    // Save current selection to restore it later
    CHARRANGE cr;
    SendMessage(hEditInput, EM_EXGETSEL, 0, (LPARAM)&cr);

    // Turn off redrawing to prevent flickering
    SendMessage(hEditInput, WM_SETREDRAW, FALSE, 0);

    // 1. Reset everything to default color first (Efficiently)
    ApplyHighlight(0, -1, RGB(220, 220, 220));

    try {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();

        for (const auto& t : tokens) {
            COLORREF color = RGB(220, 220, 220); // Default Gray
            
            if (t.type == WToken::INT || t.type == WToken::RETURN || t.type == WToken::MAIN) {
                color = RGB(86, 156, 214); // VS Blue
            } else if (t.type == WToken::NUMBER) {
                color = RGB(181, 206, 168); // VS Green-Yellow
            } else if (t.type == WToken::HASH || t.value == "include") {
                color = RGB(197, 134, 192); // VS Pink
            }

            if (color != RGB(220, 220, 220)) {
                ApplyHighlight(t.start, t.length, color);
            }
        }
    } catch (...) {}

    // Restore selection and turn redrawing back on
    SendMessage(hEditInput, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessage(hEditInput, WM_SETREDRAW, TRUE, 0);
    
    // Force a smooth redraw of only the edited area
    InvalidateRect(hEditInput, NULL, FALSE);
    UpdateWindow(hEditInput);
}

void UpdateUI(const std::string& input) {
    HighlightText(input);
    UpdateLineNumbers();

    if (input.empty()) return;
    try {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();
        // ... rest of visualizer code

        std::string tokenStr = "--- TOKENS ---\r\n";
        for (const auto& t : tokens) {
            if (t.type == WToken::END_OF_FILE) break;
            tokenStr += "[" + t.value + "] ";
        }

        Parser parser(tokens);
        auto ast = parser.parse();
        std::string astStr = "\r\n\r\n--- AST TREE ---\r\n" + ast->toString();
        
        SetWindowTextA(hVisualizerArea, (tokenStr + astStr).c_str());

        // 3. EVALUATE and Print to Shell
        double result = ast->evaluate();
        std::string shellOutput = "waffle-shell > Result: " + std::to_string((int)result);
        SetWindowTextA(hOutputArea, shellOutput.c_str());

    } catch (...) {
        // If there's a syntax error, show it in the shell
        SetWindowTextA(hOutputArea, "waffle-shell > [Parsing...] Waiting for complete code...");
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    hRichEditLib = LoadLibrary(L"Msftedit.dll");
    
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

    // Smooth Fade-In (0.5s)
    AnimateWindow(hwnd, 500, AW_BLEND | AW_ACTIVATE);
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
            hGutterFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 
                                DEFAULT_PITCH | FF_MODERN, L"Consolas");

            // Line Number Gutter (Moved closer to edge)
            hLineGutter = CreateWindowEx(0, L"EDIT", L"1", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_RIGHT,
                10, 80, 35, 300, hwnd, NULL, NULL, NULL);
            SendMessage(hLineGutter, WM_SETFONT, (WPARAM)hGutterFont, TRUE);

            // Main Input (Moved closer to gutter)
            hEditInput = CreateWindowEx(0, MSFTEDIT_CLASS, L"", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                45, 80, 580, 300, hwnd, NULL, NULL, NULL);
            SendMessage(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditInput, EM_SETBKGNDCOLOR, 0, RGB(35, 35, 35));

            // SYNC PADDING (Both start 10px from top)
            RECT rc;
            SetRect(&rc, 5, 10, 580, 300); // 10px top
            SendMessage(hEditInput, EM_SETRECT, 0, (LPARAM)&rc);
            
            RECT gutterRc;
            SetRect(&gutterRc, 0, 10, 35, 300); // 10px top for numbers
            SendMessage(hLineGutter, EM_SETRECT, 0, (LPARAM)&gutterRc);

            // Set DEFAULT text color to Light Gray so it's visible
            CHARFORMAT2 cf;
            ZeroMemory(&cf, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = RGB(220, 220, 220);
            SendMessage(hEditInput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            // ENABLE "Change" notifications for RichEdit
            SendMessage(hEditInput, EM_SETEVENTMASK, 0, ENM_CHANGE);

            // Add Padding (Left Margin) so text isn't against the edge
            SendMessage(hEditInput, EM_SETMARGINS, EC_LEFTMARGIN, MAKELONG(15, 0));

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

            int sidebarWidth = 50;
            int leftPanelWidth = (width * 60) / 100;
            int rightPanelWidth = (width * 30) / 100;
            int inputHeight = (height * 50) / 100;
            int outputHeight = (height * 35) / 100;

            // Shifted right by sidebarWidth
            MoveWindow(hLineGutter, sidebarWidth + 10, 80, 35, inputHeight, TRUE);
            MoveWindow(hEditInput, sidebarWidth + 45, 80, leftPanelWidth - 45, inputHeight, TRUE);
            
            RECT rc;
            SetRect(&rc, 5, 10, leftPanelWidth - 50, inputHeight);
            SendMessage(hEditInput, EM_SETRECT, 0, (LPARAM)&rc);

            RECT gutterRc;
            SetRect(&gutterRc, 0, 10, 35, inputHeight);
            SendMessage(hLineGutter, EM_SETRECT, 0, (LPARAM)&gutterRc);

            MoveWindow(hOutputArea, sidebarWidth + 25, 80 + inputHeight + 20, leftPanelWidth - 25, outputHeight, TRUE);
            MoveWindow(hVisualizerArea, sidebarWidth + leftPanelWidth + 20, 80, rightPanelWidth, height - 100, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            if (y >= 18 && y <= 30) {
                if (x >= 20 && x <= 32) {
                    // Smooth Fade-Out (0.5s)
                    AnimateWindow(hwnd, 500, AW_BLEND | AW_HIDE);
                    PostQuitMessage(0); 
                }
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
            if ((HWND)lParam == hLineGutter) {
                SetTextColor(hdcStatic, RGB(100, 100, 100)); // Dim gray for line numbers
            } else {
                SetTextColor(hdcStatic, RGB(220, 220, 220));
            }
            SetBkColor(hdcStatic, RGB(35, 35, 35));
            return (LRESULT)CreateSolidBrush(RGB(35, 35, 35));
        }

        case WM_COMMAND: {
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hEditInput) {
                GETTEXTEX gte;
                gte.cb = GetWindowTextLengthA(hEditInput) + 1;
                gte.flags = GT_DEFAULT;
                gte.codepage = CP_ACP;
                gte.lpDefaultChar = NULL;
                gte.lpUsedDefChar = NULL;

                char* buffer = new char[gte.cb];
                SendMessage(hEditInput, EM_GETTEXTEX, (WPARAM)&gte, (LPARAM)buffer);
                UpdateUI(buffer);
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

            // 1. Draw Left Sidebar (Activity Bar)
            RECT sidebarRect = {0, 50, 50, rect.bottom};
            HBRUSH sidebarBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &sidebarRect, sidebarBrush);
            DeleteObject(sidebarBrush);

            // 2. Draw Code Editor "Card"
            // We'll draw a slightly lighter area behind the gutter and editor
            int cardLeft = 60; // sidebarWidth + margin
            int cardTop = 80;
            int cardRight = (rect.right * 60 / 100) + 50;
            int cardBottom = 80 + (rect.bottom * 50 / 100);
            
            RECT cardRect = {cardLeft - 5, cardTop - 5, cardRight + 5, cardBottom + 5};
            HBRUSH cardBg = CreateSolidBrush(RGB(35, 35, 35)); // Slightly lighter than window bg
            HPEN cardBorder = CreatePen(PS_SOLID, 1, RGB(60, 60, 60)); // Subtle border
            
            SelectObject(hdc, cardBg);
            SelectObject(hdc, cardBorder);
            RoundRect(hdc, cardRect.left, cardRect.top, cardRect.right, cardRect.bottom, 10, 10);
            
            DeleteObject(cardBg);
            DeleteObject(cardBorder);

            Graphics g(hdc);
            Pen iconPen(Color(180, 180, 180), 2);

            // Real File Icon (file_icon.png)
            Image fileIcon(L"file_icon.png");
            if (fileIcon.GetLastStatus() == Ok) {
                g.DrawImage(&fileIcon, 12, 72, 25, 25);
            } else {
                // Fallback if image not found
                g.DrawRectangle(&iconPen, 15, 75, 20, 25);
            }

            // Search Icon (Magnifying glass) - Reduced 15%
            g.DrawEllipse(&iconPen, 18, 130, 13, 13);
            g.DrawLine(&iconPen, 28, 140, 34, 146);

            // Mac Buttons
            HBRUSH redBrush = CreateSolidBrush(RGB(255, 95, 87));
            HBRUSH yellowBrush = CreateSolidBrush(RGB(254, 188, 46));
            HBRUSH greenBrush = CreateSolidBrush(RGB(40, 200, 64));
            SelectObject(hdc, GetStockObject(NULL_PEN));

            SelectObject(hdc, redBrush); Ellipse(hdc, 20, 18, 32, 30);
            SelectObject(hdc, yellowBrush); Ellipse(hdc, 40, 18, 52, 30);
            SelectObject(hdc, greenBrush); Ellipse(hdc, 60, 18, 72, 30);

            DeleteObject(redBrush); DeleteObject(yellowBrush); DeleteObject(greenBrush);

            // Nav Menu Items
            SetTextColor(hdc, RGB(180, 180, 180));
            SetBkMode(hdc, TRANSPARENT);
            TextOut(hdc, 100, 15, L"File", 4);
            TextOut(hdc, 150, 15, L"Run", 3);
            TextOut(hdc, 200, 15, L"Terminal", 8);

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
