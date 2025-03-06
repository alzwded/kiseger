/*
BSD 2-Clause License

Copyright (c) 2016, Vlad Mesco
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef UNICODE
# define UNICODE
#endif
#ifndef _UNICODE
# define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#include <tchar.h>
#include <Shellapi.h>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <map>
#include <utility>
#include <functional>
#include <string>
#include <sstream>

// ==========================================================
// Global State
// ==========================================================

#define STATE_RESET -30
int g_state = 0;
size_t g_statistics = 0;
bool g_paused = false;
bool g_debug = false;
bool g_wroteToLogFile = false;
HFONT g_hFont = NULL;
TCHAR g_txt[MAX_PATH+1] = {(TCHAR)0};
HWND hWnd = NULL;

#define IDT_TIMER1 1001
#define IDT_TIMER2 1002

// ==========================================================
// Utilities
// ==========================================================

std::wstring operator "" _ws(const wchar_t* s, size_t n)
{
    return std::wstring(s);
}

static void Help(LPWSTR);
static std::map<wchar_t, std::tuple<bool, std::wstring, std::function<void(LPWSTR)>>> g_commandArgs {
    {L'?', std::make_tuple(false, L"Shows this message"_ws, std::function<void(LPWSTR)>(Help))},
    {L'D', std::make_tuple(false, L"Breaks for debugger"_ws, [](LPWSTR) { g_debug = true; })}
};

static FILE* GetLogFile()
{
    g_wroteToLogFile = true;
    //GetTempFileName(_T("."), _T("jak"), 0, g_txt);
    //_tcscpy(g_txt, _T("winapp.log"));

    return _tfopen(g_txt, _T("w"));
}

void Log(LPTSTR format, ...)
{
    static FILE* f = GetLogFile();

    if(!f) return;

    va_list args;
    va_start(args, format);
    _vftprintf(f, format, args);
    fflush(f);
    va_end(args);
}

static void Help(LPWSTR)
{
    std::wstringstream ss;
    for(auto&& i : g_commandArgs)
    {
        if(std::get<0>(i.second)) {
            Log(L"/%carg\t%s\n", i.first, std::get<1>(i.second).c_str());
            ss << L'/' << i.first << L"arg\t" << std::get<1>(i.second).c_str() << std::endl;
        } else {
            Log(L"/%c\t%s\n", i.first, std::get<1>(i.second).c_str());
            ss << L'/' << i.first << L"\t" << std::get<1>(i.second).c_str() << std::endl;
        }
    }
    MessageBox(hWnd, ss.str().c_str(), L"WinApp", MB_OK);
    exit(2);
}

static void ParseArgs()
{
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if(!argv)
    {
        Log(L"Could not get command line arguments: %d\n", GetLastError());
        return;
    }

    for(size_t i = 1; i < argc; ++i)
    {
        auto&& arg = argv[i];
        if(arg[0] != L'/' || !arg[1])
        {
            Log(L"invalid argument %d: %s\n", i, arg);
            continue;
        }
        auto&& found = g_commandArgs.find(arg[1]);
        std::get<2>(found->second)(arg);
    }
}

// ==========================================================
// WinApi
// ==========================================================

LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam);

int WINAPI wWinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPWSTR lpCmdLine,
        int nCmdShow)
{
    // initialize log path; the happy path doesn't actually write out that file
    GetTempFileName(_T("."), _T("jak"), 0, g_txt);
    SetProcessDPIAware();
    //Log(_T("Launched with: %s\n"), lpCmdLine);

    ParseArgs();

    if(g_debug) __debugbreak();

    const wchar_t CLASS_NAME[] = _T("Form1");
    WNDCLASS wc = {};

    HCURSOR crsr = LoadCursor(NULL, IDC_ARROW);

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = crsr;

    RegisterClass(&wc);

    TCHAR windowText[256] = _T("\0");
    _tcscat(windowText, _T("Log file is in: "));
    _tcsncat(windowText, g_txt, 100);
    windowText[116] = L'\0';

    HWND hwnd = CreateWindowEx(
            0, // styles
            CLASS_NAME,
            windowText, // text
            WS_OVERLAPPEDWINDOW, // style
            CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT, // size & pos
            NULL, // parent
            NULL, // menu
            hInstance, // instance handle
            NULL); // application data

    if(hwnd == NULL)
    {
        Log(_T("Failed to create window %d\n"), GetLastError());
    }

    hWnd = hwnd;

    ShowWindow(hwnd, nCmdShow);

    // Message pump
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT uMsg,
        WPARAM wParam,
        LPARAM lParam)
{
    INPUT inp;
    switch(uMsg)
    {
        case WM_CREATE:
            g_paused = false;
            g_statistics = 0;
            g_state = STATE_RESET;
            SetTimer(hwnd,
                    IDT_TIMER1,
                    1000,
                    (TIMERPROC) NULL);
            g_hFont = CreateFont(24, FW_DONTCARE, FW_DONTCARE, FW_DONTCARE, FW_BOLD,
                                        FALSE, FALSE, FALSE, ANSI_CHARSET,
                                        OUT_DEVICE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH,
                                        _T("Courier New"));

            return 0;
        case WM_DESTROY:
            KillTimer(hWnd, IDT_TIMER1);
            if(g_state) KillTimer(hWnd, IDT_TIMER2);
            DeleteObject(g_hFont);
            if(!g_wroteToLogFile && *g_txt)
            {
                DeleteFile(g_txt);
            }
            FreeConsole();
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        case WM_KEYUP:
            switch(wParam)
            {
                default:
                    break;
                case VK_SPACE:
                    /*fallthrough*/(void)wParam;
            }
        case WM_LBUTTONUP:
            g_paused = !g_paused;
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        case WM_PAINT:
            {
                std::wstringstream text;
                text << (g_paused ? _T("PAUSED ") : _T("")) << _T("state: ") << g_state << _T(" statistic(k)s: ") << g_statistics;
                TCHAR* t = (TCHAR*)malloc((text.str().size() + 5) * sizeof(TCHAR));
                memset(t, 0, (text.str().size() + 5) * sizeof(CHAR));
                _tcscpy(t, text.str().c_str());

                // Begining to pain
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW+1));

                auto hOldFont = (HFONT) SelectObject(hdc, g_hFont); // <-- add this
                //SetTextColor(hdc, RGB(255, 0, 0));

                RECT r;
                BOOL b = GetClientRect(hWnd, &r);
                DrawText(hdc,
                        t,
                        -1,
                        &r,
                        DT_CENTER|DT_VCENTER|DT_SINGLELINE);

                SelectObject(hdc, hOldFont);

                EndPaint(hwnd, &ps);
                // Stopped painging

                free(t);
            }
            return 0;
        case WM_TIMER:
            switch(wParam)
            {
                case IDT_TIMER1:
                    if(!g_paused)
                    {
                        ++g_state;
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    if(g_state == 0)
                    {
                        ++g_statistics;
                        g_state = 1;
                        SetTimer(hwnd,
                                IDT_TIMER2,
                                50,
                                (TIMERPROC) NULL);
                    }
                    return 0;
                case IDT_TIMER2:
                    switch(g_state)
                    {
                        case 1:
                            ++g_state;
                            memset(&inp, 0, sizeof(INPUT));
                            inp.type = INPUT_MOUSE;
                            inp.mi.dx = 10;
                            inp.mi.dy = 10;
                            inp.mi.dwFlags = MOUSEEVENTF_MOVE;
                            SendInput(1, &inp, sizeof(INPUT));
                            return 0;
                        case 2:
                            ++g_state;
                            memset(&inp, 0, sizeof(INPUT));
                            inp.type = INPUT_MOUSE;
                            inp.mi.dx = 10;
                            inp.mi.dy = -10;
                            inp.mi.dwFlags = MOUSEEVENTF_MOVE;
                            SendInput(1, &inp, sizeof(INPUT));
                            return 0;
                        case 3:
                            ++g_state;
                            memset(&inp, 0, sizeof(INPUT));
                            inp.type = INPUT_MOUSE;
                            inp.mi.dx = -10;
                            inp.mi.dy = -10;
                            inp.mi.dwFlags = MOUSEEVENTF_MOVE;
                            SendInput(1, &inp, sizeof(INPUT));
                            return 0;
                        case 4:
                            ++g_state;
                            memset(&inp, 0, sizeof(INPUT));
                            inp.type = INPUT_MOUSE;
                            inp.mi.dx = -10;
                            inp.mi.dy = 10;
                            inp.mi.dwFlags = MOUSEEVENTF_MOVE;
                            SendInput(1, &inp, sizeof(INPUT));
                            return 0;
                        case 5:
                            ++g_state;
                            memset(&inp, 0, sizeof(INPUT));
                            inp.type = INPUT_KEYBOARD;
                            inp.ki.wVk = VK_LCONTROL;
                            SendInput(1, &inp, sizeof(INPUT));
                            return 0;
                        case 6:
                            ++g_state;
                            memset(&inp, 0, sizeof(INPUT));
                            inp.type = INPUT_KEYBOARD;
                            inp.ki.wVk = VK_LCONTROL;
                            inp.ki.dwFlags = KEYEVENTF_KEYUP;
                            SendInput(1, &inp, sizeof(INPUT));
                            return 0;
                        default:
                            g_state = STATE_RESET;
                            KillTimer(hWnd, IDT_TIMER2);
                            InvalidateRect(hWnd, NULL, FALSE);
                            return 0;
                    }
            }
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
