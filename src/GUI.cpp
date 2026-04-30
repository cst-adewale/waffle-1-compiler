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
#include <uxtheme.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Global variables for the UI
HWND hEditInput, hOutputArea, hTokenArea, hLineGutter, hASTArea;
HFONT hFont, hGutterFont, hNavFont;
ULONG_PTR gdiplusToken;
HINSTANCE hRichEditLib;
std::unique_ptr<ASTNode> globalAST; 

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void SetAppIcon(HWND hwnd) {
    Image image(L"logo.png");
    if (image.GetLastStatus() == Ok) {
        HICON hIcon;
        Bitmap* bitmap = static_cast<Bitmap*>(image.Clone());
        bitmap->GetHICON(&hIcon);
        
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        
        delete bitmap;
    }
}

void UpdateLineNumbers() {
    int lineCount = SendMessage(hEditInput, EM_GETLINECOUNT, 0, 0);
    std::string numbers = "";
    for (int i = 1; i <= lineCount; ++i) {
        numbers += std::to_string(i) + "\r\n";
    }
    SetWindowTextA(hLineGutter, numbers.c_str());

    // Pixel-Perfect Scroll Sync
    POINT pt;
    SendMessage(hEditInput, EM_GETSCROLLPOS, 0, (LPARAM)&pt);
    SendMessage(hLineGutter, EM_SETSCROLLPOS, 0, (LPARAM)&pt);
}

void DrawASTNode(Graphics& g, ASTNode* node, int x, int y, int xOffset, Font* font, SolidBrush* circleBrush, SolidBrush* textBrush, Pen* pen) {
    if (!node) return;

    // Draw branches
    if (auto bin = dynamic_cast<BinaryOpNode*>(node)) {
        int nextY = y + 70; // More vertical gap
        g.DrawLine(pen, x, y, x - xOffset, nextY);
        g.DrawLine(pen, x, y, x + xOffset, nextY);
        DrawASTNode(g, bin->left.get(), x - xOffset, nextY, xOffset / 2, font, circleBrush, textBrush, pen);
        DrawASTNode(g, bin->right.get(), x + xOffset, nextY, xOffset / 2, font, circleBrush, textBrush, pen);
    } else if (auto ret = dynamic_cast<ReturnNode*>(node)) {
        int nextY = y + 70;
        g.DrawLine(pen, x, y, x, nextY);
        DrawASTNode(g, ret->expression.get(), x, nextY, xOffset / 2, font, circleBrush, textBrush, pen);
    } else if (auto prog = dynamic_cast<ProgramNode*>(node)) {
         if (!prog->statements.empty()) {
            DrawASTNode(g, prog->statements[0].get(), x, y + 40, xOffset, font, circleBrush, textBrush, pen);
         }
    }

    // Draw the node circle
    g.FillEllipse(circleBrush, x - 20, y - 20, 40, 40); // Bigger circle
    g.DrawEllipse(pen, x - 20, y - 20, 40, 40);

    // Draw label in WHITE
    std::string label = "";
    if (auto bin = dynamic_cast<BinaryOpNode*>(node)) label = bin->op;
    else if (auto num = dynamic_cast<NumberNode*>(node)) {
        label = std::to_string((int)num->value);
        if (label.length() > 3) label = label.substr(0, 3);
    }
    else if (dynamic_cast<ReturnNode*>(node)) label = "RET";
    else label = "?";

    std::wstring wLabel(label.begin(), label.end());
    RectF textRect(x - 18, y - 8, 36, 16);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    g.DrawString(wLabel.c_str(), -1, font, textRect, &format, textBrush);
}

void ApplyHighlight(int start, int length, COLORREF color) {
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessage(hEditInput, EM_SETSEL, start, (length == -1) ? -1 : (start + length));
    SendMessage(hEditInput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}

void HighlightText(const std::string& input) {
    if (hEditInput == NULL) return;
    CHARRANGE cr;
    SendMessage(hEditInput, EM_EXGETSEL, 0, (LPARAM)&cr);
    SendMessage(hEditInput, WM_SETREDRAW, FALSE, 0);
    ApplyHighlight(0, -1, RGB(220, 220, 220));

    try {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();
        for (const auto& t : tokens) {
            COLORREF color = RGB(220, 220, 220);
            if (t.type == WToken::INT || t.type == WToken::RETURN || t.type == WToken::MAIN) color = RGB(86, 156, 214);
            else if (t.type == WToken::NUMBER) color = RGB(181, 206, 168);
            else if (t.type == WToken::HASH || t.value == "include") color = RGB(197, 134, 192);
            if (color != RGB(220, 220, 220)) ApplyHighlight(t.start, t.length, color);
        }
    } catch (...) {}

    SendMessage(hEditInput, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessage(hEditInput, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hEditInput, NULL, FALSE);
    UpdateWindow(hEditInput);
}

static int scrollPosX = 0, scrollPosY = 0;

LRESULT CALLBACK TreeWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rect; GetClientRect(hwnd, &rect);
            HBRUSH bg = CreateSolidBrush(RGB(35, 35, 35));
            FillRect(hdc, &rect, bg);
            DeleteObject(bg);
            Graphics g(hdc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            
            // Apply Scroll Offsets
            g.TranslateTransform((REAL)-scrollPosX, (REAL)-scrollPosY);

            if (globalAST) {
                Font astFont(L"Arial", 10);
                SolidBrush nodeBrush(Color(255, 86, 156, 214)); 
                SolidBrush whiteBrush(Color(255, 255, 255, 255));
                Pen branchPen(Color(255, 100, 100, 100), 2);
                DrawASTNode(g, globalAST.get(), rect.right / 2 + scrollPosX, 40, rect.right / 3, &astFont, &nodeBrush, &whiteBrush, &branchPen);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            scrollPosY -= (delta / 2);
            if (scrollPosY < 0) scrollPosY = 0;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_VSCROLL: {
            SCROLLINFO si = { sizeof(si), SIF_ALL };
            GetScrollInfo(hwnd, SB_VERT, &si);
            int oldPos = si.nPos;
            switch (LOWORD(wParam)) {
                case SB_LINEUP: si.nPos -= 20; break;
                case SB_LINEDOWN: si.nPos += 20; break;
                case SB_THUMBTRACK: si.nPos = HIWORD(wParam); break;
            }
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            scrollPosY = si.nPos;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_HSCROLL: {
            SCROLLINFO si = { sizeof(si), SIF_ALL };
            GetScrollInfo(hwnd, SB_HORZ, &si);
            switch (LOWORD(wParam)) {
                case SB_LINELEFT: si.nPos -= 20; break;
                case SB_LINERIGHT: si.nPos += 20; break;
                case SB_THUMBTRACK: si.nPos = HIWORD(wParam); break;
            }
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
            scrollPosX = si.nPos;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        default: return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void HighlightShell() {
    GETTEXTEX gte = { (DWORD)SendMessage(hOutputArea, WM_GETTEXTLENGTH, 0, 0) + 1, GT_DEFAULT, 0, NULL, NULL };
    char* buffer = new char[gte.cb];
    SendMessage(hOutputArea, EM_GETTEXTEX, (WPARAM)&gte, (LPARAM)buffer);
    std::string text = buffer;
    delete[] buffer;

    // Save selection
    CHARRANGE cr; SendMessage(hOutputArea, EM_EXGETSEL, 0, (LPARAM)&cr);
    SendMessage(hOutputArea, WM_SETREDRAW, FALSE, 0);

    // Default Gray
    CHARFORMAT2 cf; ZeroMemory(&cf, sizeof(cf)); cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR; cf.crTextColor = RGB(220, 220, 220);
    SendMessage(hOutputArea, EM_SETSEL, 0, -1);
    SendMessage(hOutputArea, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    // Highlight Yellow Commands (Library)
    std::vector<std::string> commands = {"clear", "help", "run", "exit", "ast", "tokens"};
    for (const auto& cmd : commands) {
        size_t pos = text.find(cmd);
        while (pos != std::string::npos) {
            cf.crTextColor = RGB(255, 215, 0); // Golden Yellow
            SendMessage(hOutputArea, EM_SETSEL, pos, pos + cmd.length());
            SendMessage(hOutputArea, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            pos = text.find(cmd, pos + 1);
        }
    }

    SendMessage(hOutputArea, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessage(hOutputArea, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hOutputArea, NULL, FALSE);
}

void ExecuteCode(HWND hwnd) {
    GETTEXTEX gte = { (DWORD)SendMessage(hEditInput, WM_GETTEXTLENGTH, 0, 0) + 1, GT_DEFAULT, 0, NULL, NULL };
    char* buffer = new char[gte.cb];
    SendMessage(hEditInput, EM_GETTEXTEX, (WPARAM)&gte, (LPARAM)buffer);
    std::string input = buffer;
    delete[] buffer;

    if (input.empty()) return;
    try {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();

        // 1. Update Tokens
        std::string tokenStr = "--- TOKENS ---\r\n";
        for (const auto& t : tokens) {
            if (t.type == WToken::END_OF_FILE) break;
            tokenStr += "[" + t.value + "] ";
        }
        SetWindowTextA(hTokenArea, tokenStr.c_str());

        // 2. Parse and Redraw Tree
        Parser parser(tokens);
        globalAST = parser.parse();
        
        // Smart Scrollbar Check
        RECT astRc; GetClientRect(hASTArea, &astRc);
        int treeWidth = 500; // Estimated for now
        ShowScrollBar(hASTArea, SB_HORZ, treeWidth > (astRc.right - astRc.left));
        
        InvalidateRect(hASTArea, NULL, TRUE);

        // 3. Evaluate to Shell
        double result = globalAST->evaluate();
        std::string currentShellText = "";
        char shellBuf[2048];
        GetWindowTextA(hOutputArea, shellBuf, 2048);
        std::string shellOutput = std::string(shellBuf) + "\r\nwaffle-shell > Result: " + std::to_string((int)result);
        SetWindowTextA(hOutputArea, shellOutput.c_str());
        HighlightShell();

    } catch (...) {
        SetWindowTextA(hOutputArea, "waffle-shell > [Error] Syntax Error in code!");
    }
}

void UpdateUI(HWND hwnd, const std::string& input) {
    HighlightText(input);
    UpdateLineNumbers();
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

    WNDCLASS twc = { };
    twc.lpfnWndProc = TreeWndProc;
    twc.hInstance = hInstance;
    twc.lpszClassName = L"WaffleTreeVisualizer";
    twc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&twc);

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
            SetAppIcon(hwnd);
            // Line Number Gutter (Upgraded to RichEdit)
            hLineGutter = CreateWindowEx(0, MSFTEDIT_CLASS, L"1", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_RIGHT | ES_NOHIDESEL,
                10, 80, 35, 300, hwnd, NULL, NULL, NULL);
            SendMessage(hLineGutter, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hLineGutter, EM_SETBKGNDCOLOR, 0, RGB(35, 35, 35));
            SendMessage(hLineGutter, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
            SetWindowTheme(hLineGutter, L"DarkMode_Explorer", NULL);

            // Main Input
            hEditInput = CreateWindowEx(0, MSFTEDIT_CLASS, L"", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                45, 80, 580, 300, hwnd, NULL, NULL, NULL);
            SendMessage(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hEditInput, EM_SETBKGNDCOLOR, 0, RGB(35, 35, 35));
            SendMessage(hEditInput, EM_SETTARGETDEVICE, 0, 1); // Disable Word Wrap
            SendMessage(hEditInput, EM_SETMARGINS, EC_LEFTMARGIN, 0);

            // SYNC PADDING (Exactly 10px top)
            RECT rc; SetRect(&rc, 0, 10, 580, 300); 
            SendMessage(hEditInput, EM_SETRECT, 0, (LPARAM)&rc);
            
            RECT gutterRc; SetRect(&gutterRc, 0, 10, 35, 300); 
            SendMessage(hLineGutter, EM_SETRECT, 0, (LPARAM)&gutterRc);

            // Set DEFAULT text color to Light Gray so it's visible
            CHARFORMAT2 cf;
            ZeroMemory(&cf, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR;
            cf.crTextColor = RGB(220, 220, 220);
            SendMessage(hEditInput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            // ENABLE "Change" and "Scroll" notifications for RichEdit
            SendMessage(hEditInput, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SCROLL);

            // Add Padding (Left Margin) so text isn't against the edge
            SendMessage(hEditInput, EM_SETMARGINS, EC_LEFTMARGIN, MAKELONG(15, 0));

            SendMessage(hLineGutter, WM_SETFONT, (WPARAM)hFont, TRUE);

            hOutputArea = CreateWindowEx(0, MSFTEDIT_CLASS, L"waffle-shell > Welcome to Waffle Studio", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
                75, 400, 600, 250, hwnd, NULL, NULL, NULL);
            SendMessage(hOutputArea, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hOutputArea, EM_SETBKGNDCOLOR, 0, RGB(35, 35, 35));
            SendMessage(hOutputArea, EM_SETEVENTMASK, 0, ENM_CHANGE);

            hTokenArea = CreateWindowEx(0, MSFTEDIT_CLASS, L"--- TOKENS ---", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                650, 80, 310, 280, hwnd, NULL, NULL, NULL);
            SendMessage(hTokenArea, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hTokenArea, EM_SETBKGNDCOLOR, 0, RGB(35, 35, 35));

            hASTArea = CreateWindowEx(0, L"WaffleTreeVisualizer", NULL, 
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                650, 370, 310, 280, hwnd, NULL, NULL, NULL);
            SetWindowTheme(hASTArea, L"DarkMode_Explorer", NULL);

            // SET ALL TEXT (Including Gutter) TO LIGHT GRAY
            CHARFORMAT2 cfAll;
            ZeroMemory(&cfAll, sizeof(cfAll));
            cfAll.cbSize = sizeof(cfAll);
            cfAll.dwMask = CFM_COLOR;
            cfAll.crTextColor = RGB(220, 220, 220);
            SendMessage(hEditInput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfAll);
            SendMessage(hOutputArea, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfAll);
            SendMessage(hTokenArea, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfAll);
            SendMessage(hLineGutter, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfAll);

            // Apply Aggressive Dark Scrollbars
            SetWindowTheme(hEditInput, L"DarkMode_Explorer", NULL);
            SetWindowTheme(hOutputArea, L"DarkMode_Explorer", NULL);
            SetWindowTheme(hTokenArea, L"DarkMode_Explorer", NULL);
            SetWindowTheme(hLineGutter, L"DarkMode_Explorer", NULL);

            BOOL useDarkMode = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
            break;
        }

        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            const int GAP = 5;
            const int TOP_OFFSET = 50 + GAP;
            const int SIDEBAR_WIDTH = 50 + GAP;

            int leftPanelWidth = (width * 65) / 100;
            int rightPanelWidth = width - leftPanelWidth - SIDEBAR_WIDTH - GAP;
            int inputHeight = (height * 60) / 100;
            int outputHeight = height - inputHeight - TOP_OFFSET - GAP;

            // 1. Gutter and Editor (Aligned with Left Nav and Top Nav)
            MoveWindow(hLineGutter, SIDEBAR_WIDTH, TOP_OFFSET, 35, inputHeight, TRUE);
            MoveWindow(hEditInput, SIDEBAR_WIDTH + 35, TOP_OFFSET, leftPanelWidth - 35, inputHeight, TRUE);
            
            RECT rc; SetRect(&rc, 5, 10, leftPanelWidth - 40, inputHeight);
            SendMessage(hEditInput, EM_SETRECT, 0, (LPARAM)&rc);

            RECT gutterRc; SetRect(&gutterRc, 0, 10, 35, inputHeight);
            SendMessage(hLineGutter, EM_SETRECT, 0, (LPARAM)&gutterRc);

            // 2. Shell (Below Editor, Aligned with Editor Left/Right)
            MoveWindow(hOutputArea, SIDEBAR_WIDTH, TOP_OFFSET + inputHeight + GAP, leftPanelWidth, outputHeight - GAP, TRUE);
            RECT shellRc; SetRect(&shellRc, 10, 10, leftPanelWidth - 10, outputHeight);
            SendMessage(hOutputArea, EM_SETRECT, 0, (LPARAM)&shellRc);

            // 3. Guts (Right side, Split into Top/Bottom)
            int gutsHeight = (height - TOP_OFFSET - GAP) / 2;
            MoveWindow(hTokenArea, SIDEBAR_WIDTH + leftPanelWidth + GAP, TOP_OFFSET, rightPanelWidth - GAP, gutsHeight - GAP, TRUE);
            RECT tokenRc; SetRect(&tokenRc, 10, 10, rightPanelWidth - 20, gutsHeight - 20);
            SendMessage(hTokenArea, EM_SETRECT, 0, (LPARAM)&tokenRc);

            MoveWindow(hASTArea, SIDEBAR_WIDTH + leftPanelWidth + GAP, TOP_OFFSET + gutsHeight, rightPanelWidth - GAP, gutsHeight, TRUE);

            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // Close, Minimize, Maximize
            if (y >= 18 && y <= 30) {
                if (x >= 20 && x <= 32) { AnimateWindow(hwnd, 500, AW_BLEND | AW_HIDE); PostQuitMessage(0); }
                else if (x >= 40 && x <= 52) ShowWindow(hwnd, SW_MINIMIZE);
                else if (x >= 60 && x <= 72) { if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE); else ShowWindow(hwnd, SW_MAXIMIZE); }
            }
            
            // "Run" Button in Navbar
            if (y >= 10 && y <= 40) {
                if (x >= 140 && x <= 180) {
                    ExecuteCode(hwnd);
                }
            }

            bDragging = true;
            ptLastMouse.x = x;
            ptLastMouse.y = y;
            SetCapture(hwnd);
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
                UpdateUI(hwnd, buffer);
                delete[] buffer;
            }
            if (HIWORD(wParam) == EN_VSCROLL && (HWND)lParam == hEditInput) {
                UpdateLineNumbers();
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

            // 2. Draw Cards based on Grid
            const int GAP = 5;
            const int TOP_O = 50 + GAP;
            const int SIDE_W = 50 + GAP;

            HBRUSH cardBg = CreateSolidBrush(RGB(35, 35, 35));
            HPEN cardBorder = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
            SelectObject(hdc, cardBg); SelectObject(hdc, cardBorder);

            // Calculate Grid Areas (Same as WM_SIZE)
            int leftW = (rect.right * 65) / 100;
            int rightW = rect.right - leftW - SIDE_W - GAP;
            int inH = (rect.bottom * 60) / 100;
            int outH = rect.bottom - inH - TOP_O - GAP;

            // Main Editor Card
            RoundRect(hdc, SIDE_W - 5, TOP_O - 5, SIDE_W + leftW + 5, TOP_O + inH + 5, 10, 10);
            
            // Shell Card
            RoundRect(hdc, SIDE_W - 5, TOP_O + inH + GAP - 5, SIDE_W + leftW + 5, rect.bottom - GAP + 5, 10, 10);

            // Guts Cards (Split)
            int gutsH = (rect.bottom - TOP_O - GAP) / 2;
            // Token Card
            RoundRect(hdc, SIDE_W + leftW + GAP - 5, TOP_O - 5, rect.right - GAP + 5, TOP_O + gutsH - GAP + 5, 10, 10);
            // AST Card (Visual Tree Canvas)
            RoundRect(hdc, SIDE_W + leftW + GAP - 5, TOP_O + gutsH - 5, rect.right - GAP + 5, rect.bottom - GAP + 5, 10, 10);

            DeleteObject(cardBg);
            DeleteObject(cardBorder);
            
            Graphics g(hdc);
            
            // Draw Visual AST Tree
            if (globalAST) {
                Font astFont(L"Arial", 10);
                SolidBrush nodeBrush(Color(255, 86, 156, 214)); // VS Blue for nodes
                SolidBrush whiteBrush(Color(255, 255, 255, 255)); // White for text
                Pen branchPen(Color(255, 100, 100, 100), 2);
                
                int treeCenterX = SIDE_W + leftW + GAP + (rightW / 2);
                int treeTopY = TOP_O + gutsH + 40;
                
                DrawASTNode(g, globalAST.get(), treeCenterX, treeTopY, rightW / 3, &astFont, &nodeBrush, &whiteBrush, &branchPen);
            }

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

            // Real Logo (logo.png)
            Graphics graphics(hdc);
            Image image(L"logo.png");
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
