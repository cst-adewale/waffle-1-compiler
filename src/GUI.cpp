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
#include <shobjidl.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Global variables for the UI
HWND hOutputArea, hTokenArea, hASTArea;
HFONT hFont, hGutterFont, hNavFont;
ULONG_PTR gdiplusToken;
HINSTANCE hRichEditLib;
std::unique_ptr<ASTNode> globalAST; 

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void SetAppIcon(HWND hwnd) {
    Image image(L"vlogo.png");
    if (image.GetLastStatus() == Ok) {
        HICON hIcon;
        Bitmap* bitmap = static_cast<Bitmap*>(image.Clone());
        bitmap->GetHICON(&hIcon);
        
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        
        delete bitmap;
    }
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

void OpenFolderPicker(HWND hwnd) {
    IFileOpenDialog *pFileOpen;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, 
                                  IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    if (SUCCEEDED(hr)) {
        DWORD dwOptions;
        if (SUCCEEDED(pFileOpen->GetOptions(&dwOptions))) {
            pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        }
        hr = pFileOpen->Show(hwnd);
        if (SUCCEEDED(hr)) {
            IShellItem *pItem;
            hr = pFileOpen->GetResult(&pItem);
            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr)) {
                    int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, NULL, 0, NULL, NULL);
                    std::string narrowPath(size_needed, 0);
                    WideCharToMultiByte(CP_UTF8, 0, pszFilePath, -1, &narrowPath[0], size_needed, NULL, NULL);
                    if(!narrowPath.empty() && narrowPath.back() == '\0') narrowPath.pop_back();

                    char shellBuf[8192];
                    GetWindowTextA(hOutputArea, shellBuf, 8192);
                    std::string shellOutput = std::string(shellBuf) + "\r\nvanilla-shell >> Selected folder: " + narrowPath + "\r\nvanilla-shell >> ";
                    SetWindowTextA(hOutputArea, shellOutput.c_str());
                    SendMessage(hOutputArea, EM_SETSEL, -1, -1);
                    SendMessage(hOutputArea, EM_SCROLLCARET, 0, 0);

                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }
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

    // Highlight Yellow Commands
    std::vector<std::string> commands = {"clear", "help", "run", "exit", "ast", "tokens", "solve"};
    for (const auto& cmd : commands) {
        size_t pos = text.find(cmd);
        while (pos != std::string::npos) {
            cf.crTextColor = RGB(255, 255, 153); // Light Yellow
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
    int lineCount = SendMessage(hOutputArea, EM_GETLINECOUNT, 0, 0);
    if (lineCount <= 0) return;
    int lastLineIndex = lineCount - 1;
    char lineBuf[2048] = {0};
    *(WORD*)lineBuf = sizeof(lineBuf) - 1;
    int len = SendMessage(hOutputArea, EM_GETLINE, lastLineIndex, (LPARAM)lineBuf);
    lineBuf[len] = '\0';
    std::string input = lineBuf;

    size_t pos = input.find("vanilla-shell >> ");
    if (pos != std::string::npos) {
        input = input.substr(pos + 17);
    }

    input.erase(0, input.find_first_not_of(" \t\r\n"));

    if (input.empty()) {
        char shellBuf[4096];
        GetWindowTextA(hOutputArea, shellBuf, 4096);
        std::string shellOutput = std::string(shellBuf) + "\r\nvanilla-shell >> ";
        SetWindowTextA(hOutputArea, shellOutput.c_str());
        SendMessage(hOutputArea, EM_SETSEL, -1, -1);
        SendMessage(hOutputArea, EM_SCROLLCARET, 0, 0);
        return;
    }

    if (input.length() < 5 || input.substr(0, 5) != "solve") {
        char shellBuf[4096];
        GetWindowTextA(hOutputArea, shellBuf, 4096);
        std::string shellOutput = std::string(shellBuf) + "\r\nvanilla-shell >> [Error] You forgot to add the 'solve' keyword!\r\nvanilla-shell >> ";
        SetWindowTextA(hOutputArea, shellOutput.c_str());
        HighlightShell();
        
        SetWindowTextA(hTokenArea, "--- TOKENS ---\r\n");
        globalAST.reset();
        InvalidateRect(hASTArea, NULL, TRUE);
        
        SendMessage(hOutputArea, EM_SETSEL, -1, -1);
        SendMessage(hOutputArea, EM_SCROLLCARET, 0, 0);
        return;
    }

    input = input.substr(5);
    input.erase(0, input.find_first_not_of(" \t\r\n"));

    try {
        Lexer lexer(input);
        auto tokens = lexer.tokenize();

        std::string tokenStr = "--- TOKENS ---\r\n";
        for (const auto& t : tokens) {
            if (t.type == WToken::END_OF_FILE) break;
            tokenStr += "[" + t.value + "] ";
        }
        SetWindowTextA(hTokenArea, tokenStr.c_str());

        Parser parser(tokens);
        globalAST = parser.parse();
        
        RECT astRc; GetClientRect(hASTArea, &astRc);
        int treeWidth = 500;
        ShowScrollBar(hASTArea, SB_HORZ, treeWidth > (astRc.right - astRc.left));
        
        InvalidateRect(hASTArea, NULL, TRUE);

        double result = globalAST->evaluate();
        char shellBuf[4096];
        GetWindowTextA(hOutputArea, shellBuf, 4096);
        std::string shellOutput = std::string(shellBuf) + "\r\nvanilla-shell >> Result: " + std::to_string(result) + "\r\nvanilla-shell >> ";
        SetWindowTextA(hOutputArea, shellOutput.c_str());
        HighlightShell();

        SendMessage(hOutputArea, EM_SETSEL, -1, -1);
        SendMessage(hOutputArea, EM_SCROLLCARET, 0, 0);
    } catch (...) {
        char shellBuf[4096];
        GetWindowTextA(hOutputArea, shellBuf, 4096);
        std::string shellOutput = std::string(shellBuf) + "\r\nvanilla-shell >> [Error] Syntax Error in expression!\r\nvanilla-shell >> ";
        SetWindowTextA(hOutputArea, shellOutput.c_str());
        HighlightShell();
        SendMessage(hOutputArea, EM_SETSEL, -1, -1);
        SendMessage(hOutputArea, EM_SCROLLCARET, 0, 0);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hRichEditLib = LoadLibrary(L"Msftedit.dll");
    
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    const wchar_t CLASS_NAME[] = L"VanillaCalculatorWindow";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));

    RegisterClass(&wc);

    WNDCLASS twc = { };
    twc.lpfnWndProc = TreeWndProc;
    twc.hInstance = hInstance;
    twc.lpszClassName = L"WaffleTreeVisualizer";
    twc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&twc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Vanilla Calculator",
        WS_POPUP | WS_THICKFRAME | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    AnimateWindow(hwnd, 500, AW_BLEND | AW_ACTIVATE);
    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return 0;
}

WNDPROC oldShellProc;
LRESULT CALLBACK ShellProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_BACK || wParam == VK_LEFT) {
            long startChar, endChar;
            SendMessage(hwnd, EM_GETSEL, (WPARAM)&startChar, (LPARAM)&endChar);
            long lineIndex = SendMessage(hwnd, EM_EXLINEFROMCHAR, 0, startChar);
            long lineStartChar = SendMessage(hwnd, EM_LINEINDEX, lineIndex, 0);
            
            if (startChar == endChar && startChar - lineStartChar <= 17) {
                return 0; // block backspace and left arrow
            }
        }
        else if (wParam == VK_HOME) {
            long startChar, endChar;
            SendMessage(hwnd, EM_GETSEL, (WPARAM)&startChar, (LPARAM)&endChar);
            long lineIndex = SendMessage(hwnd, EM_EXLINEFROMCHAR, 0, startChar);
            long lineStartChar = SendMessage(hwnd, EM_LINEINDEX, lineIndex, 0);
            SendMessage(hwnd, EM_SETSEL, lineStartChar + 17, lineStartChar + 17);
            return 0;
        }
        else if (wParam == VK_RETURN) {
            ExecuteCode(GetParent(hwnd));
            return 0;
        }
    }
    if (uMsg == WM_CHAR) {
        if (wParam == VK_BACK) {
            long startChar, endChar;
            SendMessage(hwnd, EM_GETSEL, (WPARAM)&startChar, (LPARAM)&endChar);
            long lineIndex = SendMessage(hwnd, EM_EXLINEFROMCHAR, 0, startChar);
            long lineStartChar = SendMessage(hwnd, EM_LINEINDEX, lineIndex, 0);
            
            if (startChar == endChar && startChar - lineStartChar <= 17) {
                return 0; // block backspace
            }
        }
        else if (wParam == VK_RETURN) {
            return 0;
        }
    }
    return CallWindowProc(oldShellProc, hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static POINT ptLastMouse;
    static bool bDragging = false;

    switch (uMsg) {
        case WM_CREATE: {
            SetAppIcon(hwnd);

            hOutputArea = CreateWindowEx(0, MSFTEDIT_CLASS, L"vanilla-shell >> enter your mathematical expression bellow\r\nvanilla-shell >> ", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL,
                50, 60, 600, 600, hwnd, NULL, NULL, NULL);
            SendMessage(hOutputArea, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hOutputArea, EM_SETBKGNDCOLOR, 0, RGB(35, 35, 35));
            SendMessage(hOutputArea, EM_SETEVENTMASK, 0, ENM_CHANGE);
            
            oldShellProc = (WNDPROC)SetWindowLongPtr(hOutputArea, GWLP_WNDPROC, (LONG_PTR)ShellProc);

            hTokenArea = CreateWindowEx(0, MSFTEDIT_CLASS, L"--- TOKENS ---", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                650, 80, 310, 280, hwnd, NULL, NULL, NULL);
            SendMessage(hTokenArea, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hTokenArea, EM_SETBKGNDCOLOR, 0, RGB(35, 35, 35));

            hASTArea = CreateWindowEx(0, L"WaffleTreeVisualizer", NULL, 
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                650, 370, 310, 280, hwnd, NULL, NULL, NULL);
            SetWindowTheme(hASTArea, L"DarkMode_Explorer", NULL);

            CHARFORMAT2 cfAll;
            ZeroMemory(&cfAll, sizeof(cfAll));
            cfAll.cbSize = sizeof(cfAll);
            cfAll.dwMask = CFM_COLOR;
            cfAll.crTextColor = RGB(220, 220, 220);
            SendMessage(hOutputArea, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfAll);
            SendMessage(hTokenArea, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfAll);

            SetWindowTheme(hOutputArea, L"DarkMode_Explorer", NULL);
            SetWindowTheme(hTokenArea, L"DarkMode_Explorer", NULL);

            BOOL useDarkMode = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
            HighlightShell();
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

            MoveWindow(hOutputArea, SIDEBAR_WIDTH, TOP_OFFSET, leftPanelWidth, height - TOP_OFFSET - GAP, TRUE);
            RECT shellRc; SetRect(&shellRc, 10, 10, leftPanelWidth - 10, height - TOP_OFFSET - GAP);
            SendMessage(hOutputArea, EM_SETRECT, 0, (LPARAM)&shellRc);

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

            if (y >= 18 && y <= 30) {
                if (x >= 20 && x <= 32) { AnimateWindow(hwnd, 500, AW_BLEND | AW_HIDE); PostQuitMessage(0); }
                else if (x >= 40 && x <= 52) ShowWindow(hwnd, SW_MINIMIZE);
                else if (x >= 60 && x <= 72) { if (IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE); else ShowWindow(hwnd, SW_MAXIMIZE); }
            }
            
            if (y >= 10 && y <= 40) {
                if (x >= 140 && x <= 180) {
                    ExecuteCode(hwnd);
                }
            }

            if (x >= 12 && x <= 37 && y >= 72 && y <= 97) {
                OpenFolderPicker(hwnd);
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

        case WM_COMMAND: {
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == hOutputArea) {
                SendMessage(hOutputArea, EM_SETEVENTMASK, 0, 0);
                HighlightShell();
                SendMessage(hOutputArea, EM_SETEVENTMASK, 0, ENM_CHANGE);
            }
            break;
        }

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(220, 220, 220));
            SetBkColor(hdcStatic, RGB(35, 35, 35));
            return (LRESULT)CreateSolidBrush(RGB(35, 35, 35));
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

            RECT sidebarRect = {0, 50, 50, rect.bottom};
            HBRUSH sidebarBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &sidebarRect, sidebarBrush);
            DeleteObject(sidebarBrush);

            const int GAP = 5;
            const int TOP_O = 50 + GAP;
            const int SIDE_W = 50 + GAP;

            HBRUSH cardBg = CreateSolidBrush(RGB(35, 35, 35));
            HPEN cardBorder = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
            SelectObject(hdc, cardBg); SelectObject(hdc, cardBorder);

            int leftW = (rect.right * 65) / 100;
            int rightW = rect.right - leftW - SIDE_W - GAP;

            RoundRect(hdc, SIDE_W - 5, TOP_O - 5, SIDE_W + leftW + 5, rect.bottom - GAP + 5, 10, 10);

            int gutsH = (rect.bottom - TOP_O - GAP) / 2;
            RoundRect(hdc, SIDE_W + leftW + GAP - 5, TOP_O - 5, rect.right - GAP + 5, TOP_O + gutsH - GAP + 5, 10, 10);
            RoundRect(hdc, SIDE_W + leftW + GAP - 5, TOP_O + gutsH - 5, rect.right - GAP + 5, rect.bottom - GAP + 5, 10, 10);

            DeleteObject(cardBg);
            DeleteObject(cardBorder);
            
            Graphics g(hdc);
            
            if (globalAST) {
                Font astFont(L"Arial", 10);
                SolidBrush nodeBrush(Color(255, 86, 156, 214));
                SolidBrush whiteBrush(Color(255, 255, 255, 255));
                Pen branchPen(Color(255, 100, 100, 100), 2);
                
                int treeCenterX = SIDE_W + leftW + GAP + (rightW / 2);
                int treeTopY = TOP_O + gutsH + 40;
                
                DrawASTNode(g, globalAST.get(), treeCenterX, treeTopY, rightW / 3, &astFont, &nodeBrush, &whiteBrush, &branchPen);
            }

            Pen iconPen(Color(180, 180, 180), 2);

            Image fileIcon(L"file_icon.png");
            if (fileIcon.GetLastStatus() == Ok) {
                g.DrawImage(&fileIcon, 12, 72, 25, 25);
            } else {
                g.DrawRectangle(&iconPen, 15, 75, 20, 25);
            }

            g.DrawEllipse(&iconPen, 18, 130, 13, 13);
            g.DrawLine(&iconPen, 28, 140, 34, 146);

            HBRUSH redBrush = CreateSolidBrush(RGB(255, 95, 87));
            HBRUSH yellowBrush = CreateSolidBrush(RGB(254, 188, 46));
            HBRUSH greenBrush = CreateSolidBrush(RGB(40, 200, 64));
            SelectObject(hdc, GetStockObject(NULL_PEN));

            SelectObject(hdc, redBrush); Ellipse(hdc, 20, 18, 32, 30);
            SelectObject(hdc, yellowBrush); Ellipse(hdc, 40, 18, 52, 30);
            SelectObject(hdc, greenBrush); Ellipse(hdc, 60, 18, 72, 30);

            DeleteObject(redBrush); DeleteObject(yellowBrush); DeleteObject(greenBrush);

            SetTextColor(hdc, RGB(180, 180, 180));
            SetBkMode(hdc, TRANSPARENT);
            TextOut(hdc, 100, 15, L"File", 4);
            TextOut(hdc, 150, 15, L"Run", 3);
            TextOut(hdc, 200, 15, L"Terminal", 8);

            SetTextColor(hdc, RGB(200, 200, 200));
            SetBkMode(hdc, TRANSPARENT);
            std::wstring title = L"Vanilla Calculator";
            int titleX = (rect.right / 2) - 60;
            TextOut(hdc, titleX, 15, title.c_str(), title.length());

            Graphics graphics(hdc);
            Image image(L"vlogo.png");
            if (image.GetLastStatus() == Ok) {
                graphics.DrawImage(&image, titleX - 45, 10, 30, 30);
            } else {
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
