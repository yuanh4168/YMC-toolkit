#include "SettingsWindow.h"
#include "DPIHelper.h"
#include "PopupWindow.h"
#include <commctrl.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

// 窗口过程函数声明
static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

SettingsWindow::SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent)
    : m_config(cfg), m_hDlg(NULL), m_hInst(hInst), m_hParent(hParent) {
}

SettingsWindow::~SettingsWindow() {
    if (m_hDlg) {
        DestroyWindow(m_hDlg);
        m_hDlg = NULL;
    }
}

void SettingsWindow::Show() {
    if (m_hDlg && IsWindow(m_hDlg)) {
        SetForegroundWindow(m_hDlg);
        return;
    }

    // 注册窗口类
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = m_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"YMC_Settings_Class";
    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            wchar_t msg[256];
            swprintf(msg, 256, L"注册窗口类失败，错误码: %lu", err);
            MessageBoxW(m_hParent, msg, L"错误", MB_OK);
            return;
        }
    }

    double scale = GetDPIScale();
    int width = (int)(500 * scale);
    int height = (int)(400 * scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    m_hDlg = CreateWindowExW(0, L"YMC_Settings_Class", L"设置",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, width, height, m_hParent, NULL, m_hInst, this);
    if (!m_hDlg) {
        DWORD err = GetLastError();
        wchar_t msg[256];
        swprintf(msg, 256, L"创建设置窗口失败，错误码: %lu", err);
        MessageBoxW(m_hParent, msg, L"错误", MB_OK);
        return;
    }

    // 创建控件
    int yOff = 20;
    int xOff = 20;
    int labelW = 100;
    int editW = 100;

    CreateWindowW(L"STATIC", L"弹窗宽度:", WS_CHILD | WS_VISIBLE,
        xOff, yOff, labelW, 25, m_hDlg, NULL, m_hInst, NULL);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        xOff + labelW + 10, yOff, editW, 25, m_hDlg, (HMENU)1001, m_hInst, NULL);
    yOff += 35;

    CreateWindowW(L"STATIC", L"弹窗高度:", WS_CHILD | WS_VISIBLE,
        xOff, yOff, labelW, 25, m_hDlg, NULL, m_hInst, NULL);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        xOff + labelW + 10, yOff, editW, 25, m_hDlg, (HMENU)1002, m_hInst, NULL);
    yOff += 35;

    CreateWindowW(L"STATIC", L"触发阈值(px):", WS_CHILD | WS_VISIBLE,
        xOff, yOff, labelW, 25, m_hDlg, NULL, m_hInst, NULL);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        xOff + labelW + 10, yOff, editW, 25, m_hDlg, (HMENU)1003, m_hInst, NULL);
    yOff += 40;

    // 确定和取消按钮
    CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        width - 100, height - 50, 80, 30, m_hDlg, (HMENU)1, m_hInst, NULL);
    CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        width - 190, height - 50, 80, 30, m_hDlg, (HMENU)2, m_hInst, NULL);

    // 加载当前值
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", m_config.popupWidth);
    SetDlgItemTextW(m_hDlg, 1001, buf);
    swprintf(buf, 32, L"%d", m_config.popupHeight);
    SetDlgItemTextW(m_hDlg, 1002, buf);
    swprintf(buf, 32, L"%d", m_config.edgeThreshold);
    SetDlgItemTextW(m_hDlg, 1003, buf);
}

// 窗口过程函数实现
static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (SettingsWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    pThis = (SettingsWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pThis) return DefWindowProc(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1: { // 确定按钮
                    wchar_t buf[32];
                    GetDlgItemTextW(hWnd, 1001, buf, 32);
                    pThis->m_config.popupWidth = _wtoi(buf);
                    GetDlgItemTextW(hWnd, 1002, buf, 32);
                    pThis->m_config.popupHeight = _wtoi(buf);
                    GetDlgItemTextW(hWnd, 1003, buf, 32);
                    pThis->m_config.edgeThreshold = _wtoi(buf);
                    pThis->m_config.Save("config.json");
                    DestroyWindow(hWnd);
                    break;
                }
                case 2: // 取消按钮
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