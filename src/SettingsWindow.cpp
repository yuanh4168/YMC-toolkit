#include "SettingsWindow.h"
#include "DPIHelper.h"
#include "PopupWindow.h"
#include <windowsx.h>
#include <string>
#include <fstream>

#ifndef WM_CONFIG_UPDATED
#define WM_CONFIG_UPDATED (WM_USER + 1)
#endif

#define IDC_EDIT_WIDTH      1001
#define IDC_EDIT_HEIGHT     1002
#define IDC_EDIT_THRESHOLD  1003
#define IDC_BTN_OK          1004
#define IDC_BTN_CANCEL      1005

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

    // 获取配置文件绝对路径
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    m_configPath = exePath.substr(0, pos + 1) + "config.json";

    // 重新加载最新配置
    m_config.Load(m_configPath);

    // 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = m_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SettingsDialogClass";
    RegisterClassExW(&wc);

    double scale = GetDPIScale();
    int baseWidth = 300;
    int baseHeight = 200;
    int width = static_cast<int>(baseWidth * scale);
    int height = static_cast<int>(baseHeight * scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    m_hDlg = CreateWindowExW(0, L"SettingsDialogClass", L"设置",
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                             x, y, width, height,
                             m_hParent, NULL, m_hInst, this);
    if (!m_hDlg) {
        MessageBoxW(m_hParent, L"创建设置窗口失败", L"错误", MB_ICONERROR);
        return;
    }

    // 创建控件
    int margin = (int)(10 * scale);
    int labelW = (int)(80 * scale);
    int editW = (int)(120 * scale);
    int rowH = (int)(25 * scale);
    int yPos = margin;

    // 宽度
    CreateWindowW(L"STATIC", L"弹窗宽度:", WS_CHILD | WS_VISIBLE,
                  margin, yPos, labelW, rowH, m_hDlg, NULL, m_hInst, NULL);
    HWND hEditWidth = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                    margin + labelW + 5, yPos, editW, rowH,
                                    m_hDlg, (HMENU)IDC_EDIT_WIDTH, m_hInst, NULL);
    yPos += rowH + margin;

    // 高度
    CreateWindowW(L"STATIC", L"弹窗高度:", WS_CHILD | WS_VISIBLE,
                  margin, yPos, labelW, rowH, m_hDlg, NULL, m_hInst, NULL);
    HWND hEditHeight = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                     margin + labelW + 5, yPos, editW, rowH,
                                     m_hDlg, (HMENU)IDC_EDIT_HEIGHT, m_hInst, NULL);
    yPos += rowH + margin;

    // 阈值
    CreateWindowW(L"STATIC", L"触发阈值(px):", WS_CHILD | WS_VISIBLE,
                  margin, yPos, labelW, rowH, m_hDlg, NULL, m_hInst, NULL);
    HWND hEditThreshold = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                                        margin + labelW + 5, yPos, editW, rowH,
                                        m_hDlg, (HMENU)IDC_EDIT_THRESHOLD, m_hInst, NULL);
    yPos += rowH + margin * 2;

    // 按钮
    int btnW = (int)(80 * scale);
    int btnH = (int)(30 * scale);
    int btnX = width - btnW - margin;
    CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  btnX, yPos, btnW, btnH,
                  m_hDlg, (HMENU)IDC_BTN_CANCEL, m_hInst, NULL);
    CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  btnX - btnW - margin, yPos, btnW, btnH,
                  m_hDlg, (HMENU)IDC_BTN_OK, m_hInst, NULL);

    // 设置字体
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    EnumChildWindows(m_hDlg, [](HWND hChild, LPARAM lParam) -> BOOL {
        SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }, (LPARAM)hFont);

    // 填充当前值
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", m_config.popupWidth);
    SetWindowTextW(hEditWidth, buf);
    swprintf(buf, 32, L"%d", m_config.popupHeight);
    SetWindowTextW(hEditHeight, buf);
    swprintf(buf, 32, L"%d", m_config.edgeThreshold);
    SetWindowTextW(hEditThreshold, buf);

    ShowWindow(m_hDlg, SW_SHOW);
    UpdateWindow(m_hDlg);
}

void SettingsWindow::OnOK()
{
    int newWidth = GetDlgItemInt(m_hDlg, IDC_EDIT_WIDTH, NULL, FALSE);
    int newHeight = GetDlgItemInt(m_hDlg, IDC_EDIT_HEIGHT, NULL, FALSE);
    int newThreshold = GetDlgItemInt(m_hDlg, IDC_EDIT_THRESHOLD, NULL, FALSE);

    if (newWidth < 200 || newHeight < 200) {
        MessageBoxW(m_hDlg, L"弹窗宽度和高度不能小于200像素。", L"无效输入", MB_OK | MB_ICONWARNING);
        return;
    }

    m_config.popupWidth = newWidth;
    m_config.popupHeight = newHeight;
    m_config.edgeThreshold = newThreshold;

    if (!m_config.Save(m_configPath)) {
        MessageBoxW(m_hDlg, L"保存配置文件失败，请检查文件权限。", L"错误", MB_ICONERROR);
        return;
    }

    PostMessage(m_hParent, WM_CONFIG_UPDATED, 0, 0);
    DestroyWindow(m_hDlg);
}

LRESULT CALLBACK SettingsWindow::DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_OK:
                    pThis->OnOK();
                    return 0;
                case IDC_BTN_CANCEL:
                    DestroyWindow(hWnd);
                    return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            pThis->m_hDlg = NULL;
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}