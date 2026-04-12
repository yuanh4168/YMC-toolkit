#include "SettingsWindow.h"
#include "DPIHelper.h"
#include "PopupWindow.h"
#include "ServerPinger.h"
#include "ReminderWindow.h"
#include "resource.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define WM_CONFIG_UPDATED (WM_USER + 300)
#define WM_CLEAR_HISTORY  (WM_USER + 500)

// 替代 Button_SetCheck 和 Button_GetCheck 宏
#ifndef Button_SetCheck
#define Button_SetCheck(hwnd, check) SendMessage(hwnd, BM_SETCHECK, (check) ? BST_CHECKED : BST_UNCHECKED, 0)
#endif
#ifndef Button_GetCheck
#define Button_GetCheck(hwnd) (int)SendMessage(hwnd, BM_GETCHECK, 0, 0)
#endif

// 简单的输入对话框函数
static INT_PTR CALLBACK InputDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static std::pair<std::string, std::string>* pData = nullptr;
    switch (msg) {
        case WM_INITDIALOG:
            pData = (std::pair<std::string, std::string>*)lParam;
            SetWindowTextW(GetDlgItem(hDlg, 1001), L"名称/地址");
            SetWindowTextW(GetDlgItem(hDlg, 1002), L"值/端口");
            if (pData) {
                SetWindowTextW(GetDlgItem(hDlg, 1001), PopupWindow::UTF8ToWide(pData->first).c_str());
                SetWindowTextW(GetDlgItem(hDlg, 1002), PopupWindow::UTF8ToWide(pData->second).c_str());
            }
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t buf1[256] = {0}, buf2[256] = {0};
                GetDlgItemTextW(hDlg, 1001, buf1, 256);
                GetDlgItemTextW(hDlg, 1002, buf2, 256);
                if (pData) {
                    pData->first = PopupWindow::WideToUTF8(buf1);
                    pData->second = PopupWindow::WideToUTF8(buf2);
                }
                EndDialog(hDlg, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, IDCANCEL);
            }
            return TRUE;
    }
    return FALSE;
}

static bool ShowInputDialog(HWND hParent, const std::string& /*title*/, std::string& value1, std::string& value2)
{
    std::pair<std::string, std::string> data = {value1, value2};
    if (DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCEW(1000), hParent, InputDialogProc, (LPARAM)&data) == IDOK) {
        value1 = data.first;
        value2 = data.second;
        return true;
    }
    return false;
}

SettingsWindow::SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent)
    : m_config(cfg), m_hDlg(NULL), m_hInst(hInst), m_hParent(hParent), m_selectedTab(0),
      m_hClearFont(NULL)
{
    // 初始化所有控件句柄为 NULL
    m_hServerList = m_hAddServerBtn = m_hEditServerBtn = m_hDelServerBtn = NULL;
    m_hMoveUpBtn = m_hMoveDownBtn = m_hSetDefaultBtn = m_hTestServerBtn = NULL;
    m_hShortcutList = m_hAddShortcutBtn = m_hEditShortcutBtn = m_hDelShortcutBtn = NULL;
    m_hPopupWidth = m_hPopupHeight = m_hEdgeThreshold = NULL;
    m_hTimeEnabled = m_hTimeFormat = NULL;
    m_hReminderEnabled = m_hReminderInterval = m_hReminderMessage = m_hTestReminderBtn = NULL;
    m_hMonitorEnabled = m_hMonitorInterval = m_hMonitorMaxPoints = m_hClearHistoryBtn = NULL;
    m_hGameCommand = m_hBrowseGameBtn = NULL;
    m_hStartupEnabled = m_hResetConfigBtn = m_hBackupConfigBtn = m_hRestoreConfigBtn = m_hAboutText = NULL;
    m_hTabCtrl = NULL;
    m_hStaticPopupWidth = m_hStaticPopupHeight = m_hStaticEdgeThreshold = NULL;
    m_hStaticTimeFormat = NULL;
    m_hStaticReminderInterval = m_hStaticReminderMessage = NULL;
    m_hStaticMonitorInterval = m_hStaticMonitorMaxPoints = NULL;
    m_hStaticGameCommand = NULL;
}

SettingsWindow::~SettingsWindow()
{
    if (m_hClearFont) DeleteObject(m_hClearFont);
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
    int width = (int)(650 * scale);
    int height = (int)(500 * scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    m_hDlg = CreateWindowExW(0, L"SettingsDialogClass", L"Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        m_hParent, NULL, m_hInst, this);
    if (!m_hDlg) {
        MessageBoxW(m_hParent, L"Failed to create settings window", L"Error", MB_ICONERROR);
        return;
    }

    InitControls();
    LoadDataToUI();

    ShowWindow(m_hDlg, SW_SHOW);
    UpdateWindow(m_hDlg);
}

void SettingsWindow::InitControls()
{
    // 初始化通用控件库（确保 Tab 控件正常工作）
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_TAB_CLASSES };
    InitCommonControlsEx(&icex);

    double scale = GetDPIScale();
    int margin = (int)(10 * scale);
    int tabHeight = (int)(25 * scale);
    int btnWidth = (int)(80 * scale);
    int btnHeight = (int)(25 * scale);
    int listWidth = (int)(400 * scale);
    int listHeight = (int)(250 * scale);

    RECT rcClient;
    GetClientRect(m_hDlg, &rcClient);
    int tabAreaTop = margin + tabHeight + margin;

    // 创建支持中文的字体（必须在创建任何控件前创建）
    if (!m_hClearFont) {
        HDC hdc = GetDC(m_hDlg);
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(m_hDlg, hdc);
        int fontSize = -MulDiv(12, dpiX, 96);
        LOGFONTW lf = {};
        lf.lfHeight = fontSize;
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
        m_hClearFont = CreateFontIndirectW(&lf);
    }

    // 创建选项卡控件
    m_hTabCtrl = CreateWindowW(WC_TABCONTROLW, NULL,
        WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
        margin, margin, rcClient.right - 2 * margin, tabHeight,
        m_hDlg, (HMENU)IDC_SETTINGS_TAB, m_hInst, NULL);
    SendMessage(m_hTabCtrl, WM_SETFONT, (WPARAM)m_hClearFont, TRUE);

    // 插入标签页（英文）
    TCITEMW tie = {};
    tie.mask = TCIF_TEXT;
    std::wstring tabs[] = {L"Servers", L"Shortcuts", L"UI", L"Time", L"Reminder", L"Monitor", L"Game", L"Other"};
    for (int i = 0; i < 8; ++i) {
        tie.pszText = const_cast<wchar_t*>(tabs[i].c_str());
        TabCtrl_InsertItem(m_hTabCtrl, i, &tie);
    }

    // ---------- 服务器选项卡 ----------
    m_hServerList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        margin, tabAreaTop, listWidth, listHeight,
        m_hDlg, (HMENU)IDC_SERVER_LIST, m_hInst, NULL);
    
    int btnX = margin + listWidth + margin;
    int btnY = tabAreaTop;
    m_hAddServerBtn = CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_ADD_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hEditServerBtn = CreateWindowW(L"BUTTON", L"Edit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_EDIT_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hDelServerBtn = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_DEL_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hMoveUpBtn = CreateWindowW(L"BUTTON", L"Move Up", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_MOVE_UP_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hMoveDownBtn = CreateWindowW(L"BUTTON", L"Move Down", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_MOVE_DOWN_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hSetDefaultBtn = CreateWindowW(L"BUTTON", L"Set Default", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_SET_DEFAULT_SERVER, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hTestServerBtn = CreateWindowW(L"BUTTON", L"Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_TEST_SERVER, m_hInst, NULL);

    // ---------- 快捷方式选项卡 ----------
    m_hShortcutList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        margin, tabAreaTop, listWidth, listHeight,
        m_hDlg, (HMENU)IDC_SHORTCUT_LIST, m_hInst, NULL);
    btnX = margin + listWidth + margin;
    btnY = tabAreaTop;
    m_hAddShortcutBtn = CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_ADD_SHORTCUT, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hEditShortcutBtn = CreateWindowW(L"BUTTON", L"Edit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_EDIT_SHORTCUT, m_hInst, NULL);
    btnY += btnHeight + margin;
    m_hDelShortcutBtn = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_DEL_SHORTCUT, m_hInst, NULL);

    // ---------- 界面选项卡 ----------
    int labelW = (int)(100 * scale);
    int editW = (int)(100 * scale);
    int lineH = (int)(25 * scale);
    int yPos = tabAreaTop;
    m_hStaticPopupWidth = CreateWindowW(L"STATIC", L"Popup Width:", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_POPUP_WIDTH, m_hInst, NULL);
    m_hPopupWidth = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        margin + labelW + margin, yPos, editW, lineH, m_hDlg, (HMENU)IDC_POPUP_WIDTH, m_hInst, NULL);
    yPos += lineH + margin;
    m_hStaticPopupHeight = CreateWindowW(L"STATIC", L"Popup Height:", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_POPUP_HEIGHT, m_hInst, NULL);
    m_hPopupHeight = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        margin + labelW + margin, yPos, editW, lineH, m_hDlg, (HMENU)IDC_POPUP_HEIGHT, m_hInst, NULL);
    yPos += lineH + margin;
    m_hStaticEdgeThreshold = CreateWindowW(L"STATIC", L"Edge Threshold(px):", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_EDGE_THRESHOLD, m_hInst, NULL);
    m_hEdgeThreshold = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        margin + labelW + margin, yPos, editW, lineH, m_hDlg, (HMENU)IDC_EDGE_THRESHOLD, m_hInst, NULL);

    // ---------- 时间选项卡 ----------
    yPos = tabAreaTop;
    m_hTimeEnabled = CreateWindowW(L"BUTTON", L"Enable Time Display", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        margin, yPos, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_TIME_ENABLED, m_hInst, NULL);
    yPos += lineH + margin;
    m_hStaticTimeFormat = CreateWindowW(L"STATIC", L"Time Format:", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_TIME_FORMAT, m_hInst, NULL);
    m_hTimeFormat = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        margin + labelW + margin, yPos, editW, lineH * 5, m_hDlg, (HMENU)IDC_TIME_FORMAT, m_hInst, NULL);
    SendMessageW(m_hTimeFormat, CB_ADDSTRING, 0, (LPARAM)L"HH:mm:ss");
    SendMessageW(m_hTimeFormat, CB_ADDSTRING, 0, (LPARAM)L"HH:mm");

    // ---------- 休息提醒选项卡 ----------
    yPos = tabAreaTop;
    m_hReminderEnabled = CreateWindowW(L"BUTTON", L"Enable Reminder", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        margin, yPos, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_REMINDER_ENABLED, m_hInst, NULL);
    yPos += lineH + margin;
    m_hStaticReminderInterval = CreateWindowW(L"STATIC", L"Interval(min):", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_REMINDER_INTERVAL, m_hInst, NULL);
    m_hReminderInterval = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        margin + labelW + margin, yPos, editW, lineH, m_hDlg, (HMENU)IDC_REMINDER_INTERVAL, m_hInst, NULL);
    yPos += lineH + margin;
    m_hStaticReminderMessage = CreateWindowW(L"STATIC", L"Message:", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_REMINDER_MESSAGE, m_hInst, NULL);
    m_hReminderMessage = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        margin + labelW + margin, yPos, (int)(300 * scale), lineH, m_hDlg, (HMENU)IDC_REMINDER_MESSAGE, m_hInst, NULL);
    yPos += lineH + margin;
    m_hTestReminderBtn = CreateWindowW(L"BUTTON", L"Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin, yPos, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_TEST_REMINDER, m_hInst, NULL);

    // ---------- 监控选项卡 ----------
    yPos = tabAreaTop;
    m_hMonitorEnabled = CreateWindowW(L"BUTTON", L"Enable Background Monitor", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        margin, yPos, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_MONITOR_ENABLED, m_hInst, NULL);
    yPos += lineH + margin;
    m_hStaticMonitorInterval = CreateWindowW(L"STATIC", L"Interval(sec):", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_MONITOR_INTERVAL, m_hInst, NULL);
    m_hMonitorInterval = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        margin + labelW + margin, yPos, editW, lineH, m_hDlg, (HMENU)IDC_MONITOR_INTERVAL, m_hInst, NULL);
    yPos += lineH + margin;
    m_hStaticMonitorMaxPoints = CreateWindowW(L"STATIC", L"Max Data Points:", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_MONITOR_MAXPOINTS, m_hInst, NULL);
    m_hMonitorMaxPoints = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        margin + labelW + margin, yPos, editW, lineH, m_hDlg, (HMENU)IDC_MONITOR_MAXPOINTS, m_hInst, NULL);
    yPos += lineH + margin;
    m_hClearHistoryBtn = CreateWindowW(L"BUTTON", L"Clear History", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin, yPos, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_CLEAR_HISTORY, m_hInst, NULL);

    // ---------- 游戏选项卡 ----------
    yPos = tabAreaTop;
    m_hStaticGameCommand = CreateWindowW(L"STATIC", L"Launch Command:", WS_CHILD | WS_VISIBLE,
        margin, yPos, labelW, lineH, m_hDlg, (HMENU)IDC_STATIC_GAME_COMMAND, m_hInst, NULL);
    m_hGameCommand = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        margin + labelW + margin, yPos, (int)(300 * scale), lineH, m_hDlg, (HMENU)IDC_GAME_COMMAND, m_hInst, NULL);
    m_hBrowseGameBtn = CreateWindowW(L"BUTTON", L"Browse...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin + labelW + margin + (int)(300 * scale) + margin, yPos, btnWidth, btnHeight,
        m_hDlg, (HMENU)IDC_BROWSE_GAME, m_hInst, NULL);

    // ---------- 其他选项卡 ----------
    yPos = tabAreaTop;
    m_hStartupEnabled = CreateWindowW(L"BUTTON", L"Run on Startup", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        margin, yPos, (int)(150 * scale), lineH, m_hDlg, (HMENU)IDC_STARTUP_ENABLED, m_hInst, NULL);
    yPos += lineH + margin;
    m_hResetConfigBtn = CreateWindowW(L"BUTTON", L"Reset Config", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin, yPos, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_RESET_CONFIG, m_hInst, NULL);
    yPos += btnHeight + margin;
    m_hBackupConfigBtn = CreateWindowW(L"BUTTON", L"Backup Config", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin, yPos, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_BACKUP_CONFIG, m_hInst, NULL);
    yPos += btnHeight + margin;
    m_hRestoreConfigBtn = CreateWindowW(L"BUTTON", L"Restore Config", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        margin, yPos, btnWidth, btnHeight, m_hDlg, (HMENU)IDC_RESTORE_CONFIG, m_hInst, NULL);
    yPos += btnHeight + margin;
    m_hAboutText = CreateWindowW(L"STATIC", L"YMC-toolkit v1.0\nA Minecraft server status monitor",
        WS_CHILD | WS_VISIBLE,
        margin, yPos, (int)(400 * scale), (int)(60 * scale), m_hDlg, (HMENU)IDC_ABOUT_TEXT, m_hInst, NULL);

    // 为所有子控件设置字体（解决中文乱码）
    EnumChildWindows(m_hDlg, [](HWND hWnd, LPARAM lParam) -> BOOL {
        SendMessage(hWnd, WM_SETFONT, (WPARAM)lParam, TRUE);
        return TRUE;
    }, (LPARAM)m_hClearFont);

    // 强制刷新 Tab 控件
    if (m_hTabCtrl) {
        InvalidateRect(m_hTabCtrl, NULL, TRUE);
        UpdateWindow(m_hTabCtrl);
    }

    // 默认显示服务器选项卡
    ShowTab(TAB_SERVER);
}

void SettingsWindow::ShowTab(int tab)
{
    // 隐藏所有可能出现在不同选项卡中的控件
    // 服务器选项卡
    ShowWindow(m_hServerList, SW_HIDE);
    ShowWindow(m_hAddServerBtn, SW_HIDE);
    ShowWindow(m_hEditServerBtn, SW_HIDE);
    ShowWindow(m_hDelServerBtn, SW_HIDE);
    ShowWindow(m_hMoveUpBtn, SW_HIDE);
    ShowWindow(m_hMoveDownBtn, SW_HIDE);
    ShowWindow(m_hSetDefaultBtn, SW_HIDE);
    ShowWindow(m_hTestServerBtn, SW_HIDE);
    // 快捷方式选项卡
    ShowWindow(m_hShortcutList, SW_HIDE);
    ShowWindow(m_hAddShortcutBtn, SW_HIDE);
    ShowWindow(m_hEditShortcutBtn, SW_HIDE);
    ShowWindow(m_hDelShortcutBtn, SW_HIDE);
    // 界面选项卡
    ShowWindow(m_hStaticPopupWidth, SW_HIDE);
    ShowWindow(m_hPopupWidth, SW_HIDE);
    ShowWindow(m_hStaticPopupHeight, SW_HIDE);
    ShowWindow(m_hPopupHeight, SW_HIDE);
    ShowWindow(m_hStaticEdgeThreshold, SW_HIDE);
    ShowWindow(m_hEdgeThreshold, SW_HIDE);
    // 时间选项卡
    ShowWindow(m_hTimeEnabled, SW_HIDE);
    ShowWindow(m_hStaticTimeFormat, SW_HIDE);
    ShowWindow(m_hTimeFormat, SW_HIDE);
    // 休息提醒选项卡
    ShowWindow(m_hReminderEnabled, SW_HIDE);
    ShowWindow(m_hStaticReminderInterval, SW_HIDE);
    ShowWindow(m_hReminderInterval, SW_HIDE);
    ShowWindow(m_hStaticReminderMessage, SW_HIDE);
    ShowWindow(m_hReminderMessage, SW_HIDE);
    ShowWindow(m_hTestReminderBtn, SW_HIDE);
    // 监控选项卡
    ShowWindow(m_hMonitorEnabled, SW_HIDE);
    ShowWindow(m_hStaticMonitorInterval, SW_HIDE);
    ShowWindow(m_hMonitorInterval, SW_HIDE);
    ShowWindow(m_hStaticMonitorMaxPoints, SW_HIDE);
    ShowWindow(m_hMonitorMaxPoints, SW_HIDE);
    ShowWindow(m_hClearHistoryBtn, SW_HIDE);
    // 游戏选项卡
    ShowWindow(m_hStaticGameCommand, SW_HIDE);
    ShowWindow(m_hGameCommand, SW_HIDE);
    ShowWindow(m_hBrowseGameBtn, SW_HIDE);
    // 其他选项卡
    ShowWindow(m_hStartupEnabled, SW_HIDE);
    ShowWindow(m_hResetConfigBtn, SW_HIDE);
    ShowWindow(m_hBackupConfigBtn, SW_HIDE);
    ShowWindow(m_hRestoreConfigBtn, SW_HIDE);
    ShowWindow(m_hAboutText, SW_HIDE);

    // 根据选中的选项卡显示对应的控件
    switch (tab) {
        case TAB_SERVER:
            ShowWindow(m_hServerList, SW_SHOW);
            ShowWindow(m_hAddServerBtn, SW_SHOW);
            ShowWindow(m_hEditServerBtn, SW_SHOW);
            ShowWindow(m_hDelServerBtn, SW_SHOW);
            ShowWindow(m_hMoveUpBtn, SW_SHOW);
            ShowWindow(m_hMoveDownBtn, SW_SHOW);
            ShowWindow(m_hSetDefaultBtn, SW_SHOW);
            ShowWindow(m_hTestServerBtn, SW_SHOW);
            break;
        case TAB_SHORTCUT:
            ShowWindow(m_hShortcutList, SW_SHOW);
            ShowWindow(m_hAddShortcutBtn, SW_SHOW);
            ShowWindow(m_hEditShortcutBtn, SW_SHOW);
            ShowWindow(m_hDelShortcutBtn, SW_SHOW);
            break;
        case TAB_UI:
            ShowWindow(m_hStaticPopupWidth, SW_SHOW);
            ShowWindow(m_hPopupWidth, SW_SHOW);
            ShowWindow(m_hStaticPopupHeight, SW_SHOW);
            ShowWindow(m_hPopupHeight, SW_SHOW);
            ShowWindow(m_hStaticEdgeThreshold, SW_SHOW);
            ShowWindow(m_hEdgeThreshold, SW_SHOW);
            break;
        case TAB_TIME:
            ShowWindow(m_hTimeEnabled, SW_SHOW);
            ShowWindow(m_hStaticTimeFormat, SW_SHOW);
            ShowWindow(m_hTimeFormat, SW_SHOW);
            break;
        case TAB_REMINDER:
            ShowWindow(m_hReminderEnabled, SW_SHOW);
            ShowWindow(m_hStaticReminderInterval, SW_SHOW);
            ShowWindow(m_hReminderInterval, SW_SHOW);
            ShowWindow(m_hStaticReminderMessage, SW_SHOW);
            ShowWindow(m_hReminderMessage, SW_SHOW);
            ShowWindow(m_hTestReminderBtn, SW_SHOW);
            break;
        case TAB_MONITOR:
            ShowWindow(m_hMonitorEnabled, SW_SHOW);
            ShowWindow(m_hStaticMonitorInterval, SW_SHOW);
            ShowWindow(m_hMonitorInterval, SW_SHOW);
            ShowWindow(m_hStaticMonitorMaxPoints, SW_SHOW);
            ShowWindow(m_hMonitorMaxPoints, SW_SHOW);
            ShowWindow(m_hClearHistoryBtn, SW_SHOW);
            break;
        case TAB_GAME:
            ShowWindow(m_hStaticGameCommand, SW_SHOW);
            ShowWindow(m_hGameCommand, SW_SHOW);
            ShowWindow(m_hBrowseGameBtn, SW_SHOW);
            break;
        case TAB_OTHER:
            ShowWindow(m_hStartupEnabled, SW_SHOW);
            ShowWindow(m_hResetConfigBtn, SW_SHOW);
            ShowWindow(m_hBackupConfigBtn, SW_SHOW);
            ShowWindow(m_hRestoreConfigBtn, SW_SHOW);
            ShowWindow(m_hAboutText, SW_SHOW);
            break;
    }
}

void SettingsWindow::LoadDataToUI()
{
    UpdateServerListUI();
    UpdateShortcutListUI();

    wchar_t buf[32];
    swprintf(buf, 32, L"%d", m_config.popupWidth);
    SetWindowTextW(m_hPopupWidth, buf);
    swprintf(buf, 32, L"%d", m_config.popupHeight);
    SetWindowTextW(m_hPopupHeight, buf);
    swprintf(buf, 32, L"%d", m_config.edgeThreshold);
    SetWindowTextW(m_hEdgeThreshold, buf);

    Button_SetCheck(m_hTimeEnabled, m_config.timeDisplay.enabled ? BST_CHECKED : BST_UNCHECKED);
    int formatIdx = (m_config.timeDisplay.format == "HH:mm:ss") ? 0 : 1;
    SendMessageW(m_hTimeFormat, CB_SETCURSEL, formatIdx, 0);

    Button_SetCheck(m_hReminderEnabled, m_config.reminder.enabled ? BST_CHECKED : BST_UNCHECKED);
    swprintf(buf, 32, L"%d", m_config.reminder.intervalMinutes);
    SetWindowTextW(m_hReminderInterval, buf);
    SetWindowTextW(m_hReminderMessage, PopupWindow::UTF8ToWide(m_config.reminder.message).c_str());

    Button_SetCheck(m_hMonitorEnabled, m_config.serverMonitor.backgroundEnabled ? BST_CHECKED : BST_UNCHECKED);
    swprintf(buf, 32, L"%d", m_config.serverMonitor.intervalSeconds);
    SetWindowTextW(m_hMonitorInterval, buf);
    swprintf(buf, 32, L"%d", m_config.serverMonitor.maxDataPoints);
    SetWindowTextW(m_hMonitorMaxPoints, buf);

    SetWindowTextW(m_hGameCommand, PopupWindow::UTF8ToWide(m_config.gameCommand).c_str());

    // 检查开机启动状态（读取注册表）
    HKEY hKey;
    bool startup = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        startup = (RegQueryValueExW(hKey, L"YMC-toolkit", NULL, NULL, (BYTE*)path, &size) == ERROR_SUCCESS);
        RegCloseKey(hKey);
    }
    Button_SetCheck(m_hStartupEnabled, startup ? BST_CHECKED : BST_UNCHECKED);
}

void SettingsWindow::SaveUItoConfig()
{
    BOOL translated;
    int val = GetDlgItemInt(m_hDlg, IDC_POPUP_WIDTH, &translated, FALSE);
    if (translated && val >= 200) m_config.popupWidth = val;
    val = GetDlgItemInt(m_hDlg, IDC_POPUP_HEIGHT, &translated, FALSE);
    if (translated && val >= 200) m_config.popupHeight = val;
    val = GetDlgItemInt(m_hDlg, IDC_EDGE_THRESHOLD, &translated, FALSE);
    if (translated) m_config.edgeThreshold = val;

    m_config.timeDisplay.enabled = (Button_GetCheck(m_hTimeEnabled) == BST_CHECKED);
    int formatIdx = (int)SendMessageW(m_hTimeFormat, CB_GETCURSEL, 0, 0);
    m_config.timeDisplay.format = (formatIdx == 0) ? "HH:mm:ss" : "HH:mm";

    m_config.reminder.enabled = (Button_GetCheck(m_hReminderEnabled) == BST_CHECKED);
    val = GetDlgItemInt(m_hDlg, IDC_REMINDER_INTERVAL, &translated, FALSE);
    if (translated && val > 0) m_config.reminder.intervalMinutes = val;
    wchar_t msgBuf[256];
    GetWindowTextW(m_hReminderMessage, msgBuf, 256);
    m_config.reminder.message = PopupWindow::WideToUTF8(msgBuf);

    m_config.serverMonitor.backgroundEnabled = (Button_GetCheck(m_hMonitorEnabled) == BST_CHECKED);
    val = GetDlgItemInt(m_hDlg, IDC_MONITOR_INTERVAL, &translated, FALSE);
    if (translated && val > 0) m_config.serverMonitor.intervalSeconds = val;
    val = GetDlgItemInt(m_hDlg, IDC_MONITOR_MAXPOINTS, &translated, FALSE);
    if (translated && val > 0) m_config.serverMonitor.maxDataPoints = val;

    wchar_t cmdBuf[MAX_PATH];
    GetWindowTextW(m_hGameCommand, cmdBuf, MAX_PATH);
    m_config.gameCommand = PopupWindow::WideToUTF8(cmdBuf);

    m_config.Save(m_configPath);
}

void SettingsWindow::UpdateServerListUI()
{
    SendMessageW(m_hServerList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.servers.size(); ++i) {
        std::string entry = m_config.servers[i].host + ":" + std::to_string(m_config.servers[i].port);
        std::wstring wentry = PopupWindow::UTF8ToWide(entry);
        SendMessageW(m_hServerList, LB_ADDSTRING, 0, (LPARAM)wentry.c_str());
    }
    if (m_config.currentServer >= 0 && m_config.currentServer < (int)m_config.servers.size()) {
        SendMessageW(m_hServerList, LB_SETCURSEL, m_config.currentServer, 0);
    }
}

void SettingsWindow::UpdateShortcutListUI()
{
    SendMessageW(m_hShortcutList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.shortcuts.size(); ++i) {
        std::string entry = m_config.shortcuts[i].name + " -> " + m_config.shortcuts[i].url;
        std::wstring wentry = PopupWindow::UTF8ToWide(entry);
        SendMessageW(m_hShortcutList, LB_ADDSTRING, 0, (LPARAM)wentry.c_str());
    }
}

void SettingsWindow::AddServer()
{
    std::string host = "localhost";
    std::string port = "25565";
    if (ShowInputDialog(m_hDlg, "Add Server", host, port)) {
        if (!host.empty()) {
            ServerInfo sv;
            sv.host = host;
            sv.port = std::stoi(port);
            m_config.servers.push_back(sv);
            UpdateServerListUI();
        }
    }
}

void SettingsWindow::EditServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    ServerInfo& sv = m_config.servers[sel];
    std::string host = sv.host;
    std::string port = std::to_string(sv.port);
    if (ShowInputDialog(m_hDlg, "Edit Server", host, port)) {
        sv.host = host;
        sv.port = std::stoi(port);
        UpdateServerListUI();
    }
}

void SettingsWindow::DeleteServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        m_config.servers.erase(m_config.servers.begin() + sel);
        if (m_config.currentServer >= (int)m_config.servers.size())
            m_config.currentServer = (int)m_config.servers.size() - 1;
        if (m_config.currentServer < 0) m_config.currentServer = 0;
        UpdateServerListUI();
    }
}

void SettingsWindow::MoveServerUp()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel > 0) {
        std::swap(m_config.servers[sel], m_config.servers[sel-1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel-1;
        else if (m_config.currentServer == sel-1) m_config.currentServer = sel;
        UpdateServerListUI();
        SendMessageW(m_hServerList, LB_SETCURSEL, sel-1, 0);
    }
}

void SettingsWindow::MoveServerDown()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < (int)m_config.servers.size() - 1) {
        std::swap(m_config.servers[sel], m_config.servers[sel+1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel+1;
        else if (m_config.currentServer == sel+1) m_config.currentServer = sel;
        UpdateServerListUI();
        SendMessageW(m_hServerList, LB_SETCURSEL, sel+1, 0);
    }
}

void SettingsWindow::SetDefaultServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        m_config.currentServer = sel;
        UpdateServerListUI();
    }
}

void SettingsWindow::TestServer()
{
    int sel = (int)SendMessageW(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    ServerInfo& sv = m_config.servers[sel];
    ServerStatus status;
    if (PingServer(sv.host, sv.port, status)) {
        wchar_t msg[1024];
        swprintf(msg, 1024, L"Online\nPlayers: %d/%d\nVersion: %s\nLatency: %d ms",
            status.players, status.maxPlayers,
            PopupWindow::UTF8ToWide(status.version).c_str(),
            status.latency);
        MessageBoxW(m_hDlg, msg, L"Test Result", MB_OK);
    } else {
        MessageBoxW(m_hDlg, L"Offline or unreachable", L"Test Result", MB_OK);
    }
}

void SettingsWindow::AddShortcut()
{
    std::string name = "New Shortcut";
    std::string url = "https://";
    if (ShowInputDialog(m_hDlg, "Add Shortcut", name, url)) {
        if (!name.empty() && !url.empty()) {
            Shortcut sc;
            sc.name = name;
            sc.url = url;
            m_config.shortcuts.push_back(sc);
            UpdateShortcutListUI();
        }
    }
}

void SettingsWindow::EditShortcut()
{
    int sel = (int)SendMessageW(m_hShortcutList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    Shortcut& sc = m_config.shortcuts[sel];
    std::string name = sc.name;
    std::string url = sc.url;
    if (ShowInputDialog(m_hDlg, "Edit Shortcut", name, url)) {
        sc.name = name;
        sc.url = url;
        UpdateShortcutListUI();
    }
}

void SettingsWindow::DeleteShortcut()
{
    int sel = (int)SendMessageW(m_hShortcutList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        m_config.shortcuts.erase(m_config.shortcuts.begin() + sel);
        UpdateShortcutListUI();
    }
}

void SettingsWindow::BrowseGameCommand()
{
    OPENFILENAMEW ofn = {};
    wchar_t fileName[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hDlg;
    ofn.lpstrFilter = L"Executable Files\0*.exe\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        SetWindowTextW(m_hGameCommand, fileName);
    }
}

void SettingsWindow::TestReminder()
{
    ReminderWindow reminder;
    reminder.Show(PopupWindow::UTF8ToWide(m_config.reminder.message));
}

void SettingsWindow::ClearHistory()
{
    PostMessage(m_hParent, WM_CLEAR_HISTORY, 0, 0);
    MessageBoxW(m_hDlg, L"History cleared", L"Info", MB_OK);
}

void SettingsWindow::ResetConfig()
{
    if (MessageBoxW(m_hDlg, L"Reset all settings to default? Current settings will be lost.", L"Confirm", MB_YESNO) == IDYES) {
        m_config = Config::FromJson(nlohmann::json::object());
        m_config.Save(m_configPath);
        LoadDataToUI();
        PostMessage(m_hParent, WM_CONFIG_UPDATED, 0, 0);
    }
}

void SettingsWindow::BackupConfig()
{
    wchar_t backupPath[MAX_PATH];
    GetModuleFileNameW(NULL, backupPath, MAX_PATH);
    wcscat_s(backupPath, L".backup.json");
    if (CopyFileW(PopupWindow::UTF8ToWide(m_configPath).c_str(), backupPath, FALSE)) {
        MessageBoxW(m_hDlg, L"Backup successful", L"Info", MB_OK);
    } else {
        MessageBoxW(m_hDlg, L"Backup failed", L"Error", MB_ICONERROR);
    }
}

void SettingsWindow::RestoreConfig()
{
    wchar_t backupPath[MAX_PATH];
    GetModuleFileNameW(NULL, backupPath, MAX_PATH);
    wcscat_s(backupPath, L".backup.json");
    if (CopyFileW(backupPath, PopupWindow::UTF8ToWide(m_configPath).c_str(), FALSE)) {
        m_config.Load(m_configPath);
        LoadDataToUI();
        PostMessage(m_hParent, WM_CONFIG_UPDATED, 0, 0);
        MessageBoxW(m_hDlg, L"Restore successful", L"Info", MB_OK);
    } else {
        MessageBoxW(m_hDlg, L"Backup file not found", L"Error", MB_ICONERROR);
    }
}

void SettingsWindow::SetStartup(bool enable)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (enable) {
            RegSetValueExW(hKey, L"YMC-toolkit", 0, REG_SZ, (BYTE*)exePath, (wcslen(exePath)+1)*sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"YMC-toolkit");
        }
        RegCloseKey(hKey);
    }
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
                case IDC_ADD_SERVER: pThis->AddServer(); break;
                case IDC_EDIT_SERVER: pThis->EditServer(); break;
                case IDC_DEL_SERVER: pThis->DeleteServer(); break;
                case IDC_MOVE_UP_SERVER: pThis->MoveServerUp(); break;
                case IDC_MOVE_DOWN_SERVER: pThis->MoveServerDown(); break;
                case IDC_SET_DEFAULT_SERVER: pThis->SetDefaultServer(); break;
                case IDC_TEST_SERVER: pThis->TestServer(); break;
                case IDC_ADD_SHORTCUT: pThis->AddShortcut(); break;
                case IDC_EDIT_SHORTCUT: pThis->EditShortcut(); break;
                case IDC_DEL_SHORTCUT: pThis->DeleteShortcut(); break;
                case IDC_BROWSE_GAME: pThis->BrowseGameCommand(); break;
                case IDC_TEST_REMINDER: pThis->TestReminder(); break;
                case IDC_CLEAR_HISTORY: pThis->ClearHistory(); break;
                case IDC_RESET_CONFIG: pThis->ResetConfig(); break;
                case IDC_BACKUP_CONFIG: pThis->BackupConfig(); break;
                case IDC_RESTORE_CONFIG: pThis->RestoreConfig(); break;
                case IDC_STARTUP_ENABLED:
                    pThis->SetStartup(Button_GetCheck(pThis->m_hStartupEnabled) == BST_CHECKED);
                    break;
                case IDCANCEL:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;
        case WM_NOTIFY: {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (pnmh->hwndFrom == pThis->m_hTabCtrl && pnmh->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(pThis->m_hTabCtrl);
                pThis->ShowTab(sel);
            }
            return 0;
        }
        case WM_CLOSE:
            pThis->SaveUItoConfig();
            PostMessage(pThis->m_hParent, WM_CONFIG_UPDATED, 0, 0);
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            pThis->m_hDlg = NULL;
            return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}