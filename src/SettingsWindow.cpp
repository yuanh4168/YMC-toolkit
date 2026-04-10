#include "SettingsWindow.h"
#include "DPIHelper.h"
#include "PopupWindow.h"
#include "MainWindow.h"          // 仍保留，以确保其他符号可用
#include "resource.h"
#include <commctrl.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

// 若 WM_CONFIG_UPDATED 未定义，则显式定义（确保编译通过）
#ifndef WM_CONFIG_UPDATED
#define WM_CONFIG_UPDATED (WM_USER + 1)
#endif

// 控件 ID 定义（避免与 resource.h 冲突）
#define IDC_EDIT_WIDTH      1001
#define IDC_EDIT_HEIGHT     1002
#define IDC_EDIT_THRESHOLD  1003
#define IDC_BTN_OK          1
#define IDC_BTN_CANCEL      2

// 窗口过程声明
static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

SettingsWindow::SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent)
    : m_config(cfg), m_hDlg(NULL), m_hInst(hInst), m_hParent(hParent)
{
}

SettingsWindow::~SettingsWindow()
{
    if (m_hDlg && IsWindow(m_hDlg))
        DestroyWindow(m_hDlg);
}

void SettingsWindow::Show()
{
    if (m_hDlg && IsWindow(m_hDlg)) {
        SetForegroundWindow(m_hDlg);
        return;
    }

    // 注册窗口类
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = SettingsWndProc;
    wc.hInstance     = m_hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"YMC_Settings_Class";
    wc.hIcon         = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wc.hIconSm       = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
    RegisterClassExW(&wc);

    double scale = GetDPIScale();
    int baseWidth  = 450;
    int baseHeight = 350;
    int width  = static_cast<int>(baseWidth * scale);
    int height = static_cast<int>(baseHeight * scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    m_hDlg = CreateWindowExW(0, wc.lpszClassName, L"设置",
                             WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                             x, y, width, height,
                             m_hParent, NULL, m_hInst, this);
    if (!m_hDlg) {
        MessageBoxW(m_hParent, L"创建设置窗口失败", L"错误", MB_ICONERROR);
        return;
    }

    ShowWindow(m_hDlg, SW_SHOW);
    UpdateWindow(m_hDlg);
}

// 窗口过程实现
static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SettingsWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = static_cast<SettingsWindow*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    pThis = reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (!pThis) return DefWindowProc(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE: {
            double scale = GetDPIScale();
            HINSTANCE hInst = pThis->m_hInst;
            Config& cfg = pThis->m_config;

            // 控件尺寸常量（基于 DPI 缩放）
            int margin     = static_cast<int>(15 * scale);
            int labelW     = static_cast<int>(100 * scale);
            int editW      = static_cast<int>(120 * scale);
            int rowH       = static_cast<int>(25 * scale);
            int btnW       = static_cast<int>(80 * scale);
            int btnH       = static_cast<int>(30 * scale);
            int yPos       = margin;

            // ---- 弹窗宽度 ----
            CreateWindowW(L"STATIC", L"弹窗宽度:", WS_CHILD | WS_VISIBLE,
                          margin, yPos, labelW, rowH, hWnd, NULL, hInst, NULL);
            HWND hEditWidth = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                            margin + labelW + 5, yPos, editW, rowH,
                                            hWnd, (HMENU)IDC_EDIT_WIDTH, hInst, NULL);
            wchar_t buf[32];
            swprintf(buf, 32, L"%d", cfg.popupWidth);
            SetWindowTextW(hEditWidth, buf);
            yPos += rowH + 10;

            // ---- 弹窗高度 ----
            CreateWindowW(L"STATIC", L"弹窗高度:", WS_CHILD | WS_VISIBLE,
                          margin, yPos, labelW, rowH, hWnd, NULL, hInst, NULL);
            HWND hEditHeight = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                             margin + labelW + 5, yPos, editW, rowH,
                                             hWnd, (HMENU)IDC_EDIT_HEIGHT, hInst, NULL);
            swprintf(buf, 32, L"%d", cfg.popupHeight);
            SetWindowTextW(hEditHeight, buf);
            yPos += rowH + 10;

            // ---- 触发阈值 ----
            CreateWindowW(L"STATIC", L"触发阈值(px):", WS_CHILD | WS_VISIBLE,
                          margin, yPos, labelW, rowH, hWnd, NULL, hInst, NULL);
            HWND hEditThreshold = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                                margin + labelW + 5, yPos, editW, rowH,
                                                hWnd, (HMENU)IDC_EDIT_THRESHOLD, hInst, NULL);
            swprintf(buf, 32, L"%d", cfg.edgeThreshold);
            SetWindowTextW(hEditThreshold, buf);
            yPos += rowH + 20;

            // ---- 按钮 ----
            int btnY = yPos;
            CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          margin + labelW + editW - btnW * 2 - 10, btnY, btnW, btnH,
                          hWnd, (HMENU)IDC_BTN_OK, hInst, NULL);
            CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          margin + labelW + editW - btnW, btnY, btnW, btnH,
                          hWnd, (HMENU)IDC_BTN_CANCEL, hInst, NULL);

            // 统一设置字体（使用系统默认 GUI 字体）
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            for (HWND hChild = GetWindow(hWnd, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
                SendMessage(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_OK: {
                    wchar_t buf[32];
                    // 读取编辑框内容
                    GetDlgItemTextW(hWnd, IDC_EDIT_WIDTH, buf, 32);
                    pThis->m_config.popupWidth = _wtoi(buf);
                    GetDlgItemTextW(hWnd, IDC_EDIT_HEIGHT, buf, 32);
                    pThis->m_config.popupHeight = _wtoi(buf);
                    GetDlgItemTextW(hWnd, IDC_EDIT_THRESHOLD, buf, 32);
                    pThis->m_config.edgeThreshold = _wtoi(buf);

                    // 保存到文件
                    pThis->m_config.Save("config.json");

                    // 通知主窗口配置已更新（WM_CONFIG_UPDATED 已显式定义）
                    PostMessage(pThis->m_hParent, WM_CONFIG_UPDATED, 0, 0);

                    DestroyWindow(hWnd);
                    break;
                }
                case IDC_BTN_CANCEL:
                    DestroyWindow(hWnd);
                    break;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            pThis->m_hDlg = NULL;
            break;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}