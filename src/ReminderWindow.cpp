#include "ReminderWindow.h"
#include "resource.h"
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")

// 全局或静态变量管理托盘图标
static HWND g_hReminderWnd = NULL;
static NOTIFYICONDATAW g_nid = {};
static bool g_initialized = false;
static HICON g_hIcon = NULL;

// 隐藏窗口过程
static LRESULT CALLBACK HiddenWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            if (!g_initialized) {
                // 加载程序图标
                g_hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MAIN_ICON));
                if (!g_hIcon) {
                    g_hIcon = LoadIcon(NULL, IDI_APPLICATION);
                }
                // 初始化 NOTIFYICONDATA
                memset(&g_nid, 0, sizeof(g_nid));
                g_nid.cbSize = sizeof(NOTIFYICONDATAW);
                g_nid.hWnd = hWnd;
                g_nid.uID = 1;
                g_nid.uFlags = NIF_INFO | NIF_ICON;  // 必须包含 NIF_ICON
                g_nid.hIcon = g_hIcon;
                g_nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
                wcscpy_s(g_nid.szInfoTitle, L"YMC-toolkit 提醒");
                // 添加托盘图标
                Shell_NotifyIconW(NIM_ADD, &g_nid);
                g_initialized = true;
            }
            break;

        case WM_USER_SHOW_BALLOON: {
            const wchar_t* msgText = (const wchar_t*)lParam;
            if (msgText && g_initialized) {
                wcscpy_s(g_nid.szInfo, msgText);
                // 修改图标标志，显示气泡
                Shell_NotifyIconW(NIM_MODIFY, &g_nid);
                delete[] msgText;   // 释放内存
            }
            break;
        }

        case WM_DESTROY:
            if (g_initialized) {
                Shell_NotifyIconW(NIM_DELETE, &g_nid);
                memset(&g_nid, 0, sizeof(g_nid));
                if (g_hIcon) {
                    DestroyIcon(g_hIcon);
                    g_hIcon = NULL;
                }
                g_initialized = false;
            }
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

ReminderWindow::ReminderWindow() {}
ReminderWindow::~ReminderWindow() {}

void ReminderWindow::Show(const std::wstring& message, int /*displaySeconds*/) {
    // 创建隐藏窗口（如果尚未创建）
    if (!g_hReminderWnd) {
        HINSTANCE hInst = GetModuleHandle(NULL);
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = HiddenWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"ReminderHiddenClass";
        RegisterClassExW(&wc);
        g_hReminderWnd = CreateWindowExW(0, L"ReminderHiddenClass", NULL, WS_POPUP,
            0, 0, 0, 0, NULL, NULL, hInst, NULL);
    }

    if (g_hReminderWnd && g_initialized) {
        // 复制消息字符串到堆内存
        wchar_t* msgCopy = new wchar_t[message.length() + 1];
        wcscpy(msgCopy, message.c_str());
        PostMessage(g_hReminderWnd, WM_USER_SHOW_BALLOON, 0, (LPARAM)msgCopy);
    }
}