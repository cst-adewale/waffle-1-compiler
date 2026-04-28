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
HWND hButtonRun;
HFONT hFont;
std::unique_ptr<ASTNode> currentAST; // Store the last successfully parsed AST

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void DrawNode(HDC hdc, ASTNode* node, int x, int y, int xOffset) {
    if (!node) return;

    // Draw the circle for the node
    HBRUSH brush = CreateSolidBrush(RGB(70, 70, 70));
    SelectObject(hdc, brush);
    Ellipse(hdc, x - 20, y - 20, x + 20, y + 20);
    DeleteObject(brush);

    // Draw the text inside the node
    std::string label = "?";
    if (auto n = dynamic_cast<NumberNode*>(node)) {
        label = std::to_string((int)n->value);
    } else if (auto b = dynamic_cast<BinaryOpNode*>(node)) {
        label = b->op;
        // Draw lines to children
        MoveToEx(hdc, x, y, NULL); LineTo(hdc, x - xOffset, y + 80);
        MoveToEx(hdc, x, y, NULL); LineTo(hdc, x + xOffset, y + 80);
        DrawNode(hdc, b->left.get(), x - xOffset, y + 80, xOffset / 2);
        DrawNode(hdc, b->right.get(), x + xOffset, y + 80, xOffset / 2);
    } else if (auto r = dynamic_cast<ReturnNode*>(node)) {
        label = "ret";
        MoveToEx(hdc, x, y, NULL); LineTo(hdc, x, y + 80);
        DrawNode(hdc, r->expression.get(), x, y + 80, xOffset);
    } else if (auto p = dynamic_cast<ProgramNode*>(node)) {
        label = "Prog";
        if (!p->statements.empty()) {
            MoveToEx(hdc, x, y, NULL); LineTo(hdc, x, y + 80);
            DrawNode(hdc, p->statements[0].get(), x, y + 80, xOffset);
        }
    }

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x - 8, y - 8, label.c_str(), label.length());
}

void UpdateVisualizer(HWND hwnd, const std::string& input) {
    if (input.empty()) return;
    try {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        currentAST = parser.parse();
        InvalidateRect(hwnd, NULL, TRUE); // Trigger a repaint to draw the tree
    } catch (...) {}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"WaffleStudioWindow";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Waffle Studio", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700, NULL, NULL, hInstance, NULL);
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
            hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");

            // 1. Input Area (Transparent-ish looking Edit)
            hEditInput = CreateWindowEx(0, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
                30, 80, 580, 260, hwnd, NULL, NULL, NULL);
            SendMessage(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 2. Output Area (Bottom)
            hOutputArea = CreateWindowEx(0, L"EDIT", L"SYSTEM ONLINE...", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                30, 400, 580, 230, hwnd, NULL, NULL, NULL);
            SendMessage(hOutputArea, WM_SETFONT, (WPARAM)hFont, TRUE);

            // 3. Minimalist RUN Button
            hButtonRun = CreateWindow(L"BUTTON", L"[ RUN ]", 
                WS_VISIBLE | WS_CHILD | BS_FLAT,
                480, 20, 100, 30, hwnd, (HMENU)1, NULL, NULL);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == 1) { // Run Button
                int length = GetWindowTextLengthA(hEditInput);
                char* buffer = new char[length + 1];
                GetWindowTextA(hEditInput, buffer, length + 1);
                try {
                    Lexer lexer(buffer);
                    auto tokens = lexer.tokenize();
                    Parser parser(tokens);
                    auto ast = parser.parse();
                    std::string resultStr = "> RESULT: " + std::to_string(ast->evaluate());
                    SetWindowTextA(hOutputArea, resultStr.c_str());
                } catch (const std::exception& e) {
                    SetWindowTextA(hOutputArea, e.what());
                }
                delete[] buffer;
            }
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hEditInput) {
                int length = GetWindowTextLengthA(hEditInput);
                char* buffer = new char[length + 1];
                GetWindowTextA(hEditInput, buffer, length + 1);
                UpdateVisualizer(hwnd, buffer);
                delete[] buffer;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Background
            RECT fullRect; GetClientRect(hwnd, &fullRect);
            HBRUSH bgBrush = CreateSolidBrush(RGB(15, 15, 15));
            FillRect(hdc, &fullRect, bgBrush);
            DeleteObject(bgBrush);

            // Title Bar Area
            HBRUSH barBrush = CreateSolidBrush(RGB(45, 45, 45));
            RECT barRect = { 0, 0, fullRect.right, 55 };
            FillRect(hdc, &barRect, barBrush);
            DeleteObject(barBrush);

            // Mac Buttons
            HBRUSH rB = CreateSolidBrush(RGB(255, 95, 87)); Ellipse(hdc, 15, 20, 27, 32); DeleteObject(rB);
            HBRUSH yB = CreateSolidBrush(RGB(254, 188, 46)); Ellipse(hdc, 35, 20, 47, 32); DeleteObject(yB);
            HBRUSH gB = CreateSolidBrush(RGB(40, 200, 64)); Ellipse(hdc, 55, 20, 67, 32); DeleteObject(gB);

            // Centered Title
            SetTextColor(hdc, RGB(200, 200, 200));
            SetBkMode(hdc, TRANSPARENT);
            TextOut(hdc, (fullRect.right / 2) - 50, 18, L"WAFFLE TERMINAL", 15);

            // Draw Grid Lines (Thin White Lines)
            HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
            SelectObject(hdc, gridPen);

            // Vertical line splitting editor/console from tree
            MoveToEx(hdc, 630, 55, NULL); LineTo(hdc, 630, fullRect.bottom);
            // Horizontal line splitting editor from console
            MoveToEx(hdc, 0, 360, NULL); LineTo(hdc, 630, 360);

            // Tree Visualization Header
            TextOut(hdc, 650, 75, L"COMPILER GUTS", 13);

            if (currentAST) {
                DrawNode(hdc, currentAST.get(), 810, 150, 70);
            }

            DeleteObject(gridPen);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_CTLCOLOREDIT: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(255, 150, 50)); // Orange Terminal Text
            SetBkColor(hdcStatic, RGB(15, 15, 15));
            return (LRESULT)CreateSolidBrush(RGB(15, 15, 15));
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
