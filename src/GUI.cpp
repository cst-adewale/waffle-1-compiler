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

            hEditInput = CreateWindowEx(0, L"EDIT", L"", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL,
                20, 60, 600, 250, hwnd, NULL, NULL, NULL);
            SendMessage(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);

            hButtonRun = CreateWindow(L"BUTTON", L"RUN CODE", 
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                20, 320, 100, 40, hwnd, (HMENU)1, NULL, NULL);

            hOutputArea = CreateWindowEx(0, L"EDIT", L"Console Ready...", 
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                20, 380, 600, 250, hwnd, NULL, NULL, NULL);
            SendMessage(hOutputArea, WM_SETFONT, (WPARAM)hFont, TRUE);
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
                    double result = ast->evaluate();
                    std::string out = "Result: " + std::to_string(result);
                    SetWindowTextA(hOutputArea, out.c_str());
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
            
            // Draw Mac Buttons
            HBRUSH rB = CreateSolidBrush(RGB(255, 95, 87)); Ellipse(hdc, 20, 20, 35, 35); DeleteObject(rB);
            HBRUSH yB = CreateSolidBrush(RGB(254, 188, 46)); Ellipse(hdc, 45, 20, 60, 35); DeleteObject(yB);
            HBRUSH gB = CreateSolidBrush(RGB(40, 200, 64)); Ellipse(hdc, 70, 20, 85, 35); DeleteObject(gB);

            // Draw Graphical Tree Area
            RECT treeRect = { 640, 60, 960, 630 };
            HBRUSH bg = CreateSolidBrush(RGB(20, 20, 20));
            FillRect(hdc, &treeRect, bg);
            DeleteObject(bg);

            if (currentAST) {
                DrawNode(hdc, currentAST.get(), 800, 150, 100);
            }

            EndPaint(hwnd, &ps);
            break;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
