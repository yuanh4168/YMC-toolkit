#include "SettingsWindow.h"
#include "DPIHelper.h"
#include "PopupWindow.h"
#include "ReminderWindow.h"
#include "ServerPinger.h"
#include "resource.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

enum TabIndex {
    TAB_SERVER = 0,
    TAB_SHORTCUT,
    TAB_UI,
    TAB_TIME,
    TAB_REMINDER,
    TAB_MONITOR,
    TAB_GAME,
    TAB_OTHER,
    TAB_ABOUT,
    TAB_COUNT
};

static const wchar_t* TabNames[] = {
    L"服务器管理", L"快捷方式", L"界面外观", L"时间显示",
    L"休息提醒", L"服务器监控", L"游戏启动", L"其他", L"关于"
};

SettingsWindow::SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent)
    : m_hInst(hInst), m_config(cfg), m_hParent(hParent), m_hDlg(NULL), m_hTab(NULL),
      m_hServerList(NULL), m_hShortcutList(NULL), m_hPopupWidth(NULL), m_hPopupHeight(NULL), m_hEdgeThreshold(NULL),
      m_hTimeEnabled(NULL), m_hTimeFormat(NULL),
      m_hReminderEnabled(NULL), m_hReminderInterval(NULL), m_hReminderMessage(NULL),
      m_hMonitorEnabled(NULL), m_hMonitorInterval(NULL), m_hMonitorMaxPoints(NULL),
      m_hGameCommand(NULL), m_hStartupEnabled(NULL),
      m_hResetBtn(NULL), m_hBackupBtn(NULL), m_hRestoreBtn(NULL) {
}

SettingsWindow::~SettingsWindow() {
}

void SettingsWindow::ShowModal() {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefDlgProcW;
    wc.hInstance = m_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SettingsDialogClass";
    RegisterClassExW(&wc);

    double scale = GetDPIScale();
    int width = (int)(900 * scale);
    int height = (int)(600 * scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    m_hDlg = CreateWindowExW(0, L"SettingsDialogClass", L"设置",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
        x, y, width, height, m_hParent, NULL, m_hInst, this);
    if (!m_hDlg) return;

    // 创建 Tab 控件
    m_hTab = CreateWindowW(WC_TABCONTROLW, NULL,
        WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
        0, 0, width, height, m_hDlg, (HMENU)IDC_SETTINGS_TAB, m_hInst, NULL);
    SetWindowSubclass(m_hTab, TabProc, 0, (DWORD_PTR)this);

    TCITEMW tie = { TCIF_TEXT };
    for (int i = 0; i < TAB_COUNT; ++i) {
        tie.pszText = (LPWSTR)TabNames[i];
        TabCtrl_InsertItem(m_hTab, i, &tie);
    }

    ShowTab(0);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(m_hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
}

void SettingsWindow::ShowTab(int index) {
    // 销毁当前所有子控件（除了 Tab 控件本身）
    HWND hChild = GetWindow(m_hDlg, GW_CHILD);
    while (hChild) {
        HWND next = GetWindow(hChild, GW_HWNDNEXT);
        if (hChild != m_hTab) {
            DestroyWindow(hChild);
        }
        hChild = next;
    }

    double scale = GetDPIScale();
    RECT rcClient;
    GetClientRect(m_hDlg, &rcClient);
    SetWindowPos(m_hTab, NULL, 0, 0, rcClient.right, rcClient.bottom, SWP_NOZORDER);
    RECT rcTab;
    TabCtrl_AdjustRect(m_hTab, FALSE, &rcTab);
    int x0 = rcTab.left + (int)(10 * scale);
    int y0 = rcTab.top + (int)(10 * scale);
    int w = rcTab.right - rcTab.left - (int)(20 * scale);
    int h = rcTab.bottom - rcTab.top - (int)(20 * scale);

    HDC hdc = GetDC(m_hDlg);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(m_hDlg, hdc);
    int fontSize = -MulDiv(11, dpiX, 96);
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");

    switch (index) {
        case TAB_SERVER:   CreateServerTab(x0, y0, w, h, hFont); break;
        case TAB_SHORTCUT: CreateShortcutTab(x0, y0, w, h, hFont); break;
        case TAB_UI:       CreateUITab(x0, y0, w, h, hFont); break;
        case TAB_TIME:     CreateTimeTab(x0, y0, w, h, hFont); break;
        case TAB_REMINDER: CreateReminderTab(x0, y0, w, h, hFont); break;
        case TAB_MONITOR:  CreateMonitorTab(x0, y0, w, h, hFont); break;
        case TAB_GAME:     CreateGameTab(x0, y0, w, h, hFont); break;
        case TAB_OTHER:    CreateOtherTab(x0, y0, w, h, hFont); break;
        case TAB_ABOUT:    CreateAboutTab(x0, y0, w, h, hFont); break;
    }
    DeleteObject(hFont);
}

void SettingsWindow::CreateServerTab(int x, int y, int w, int h, HFONT hFont) {
    m_hServerList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        x, y, w - 150, h - 80, m_hDlg, (HMENU)IDC_SERVER_LIST, m_hInst, NULL);
    SendMessage(m_hServerList, WM_SETFONT, (WPARAM)hFont, TRUE);
    RefreshServerList();

    int btnX = x + w - 130;
    int btnY = y;
    int btnW = 120;
    int btnH = (int)(25 * GetDPIScale());

    CreateWindowW(L"BUTTON", L"添加", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_ADD_SERVER, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"编辑", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_EDIT_SERVER, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"删除", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_DEL_SERVER, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"上移", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_MOVE_UP_SERVER, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"下移", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_MOVE_DOWN_SERVER, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"设为默认", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_SET_DEFAULT_SERVER, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"测试连接", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_TEST_SERVER, m_hInst, NULL);
}

void SettingsWindow::RefreshServerList() {
    SendMessage(m_hServerList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.servers.size(); ++i) {
        std::wstring entry = PopupWindow::UTF8ToWide(m_config.servers[i].host + ":" + std::to_string(m_config.servers[i].port));
        SendMessage(m_hServerList, LB_ADDSTRING, 0, (LPARAM)entry.c_str());
    }
    SendMessage(m_hServerList, LB_SETCURSEL, m_config.currentServer, 0);
}

void SettingsWindow::CreateShortcutTab(int x, int y, int w, int h, HFONT hFont) {
    m_hShortcutList = CreateWindowW(L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
        x, y, w - 150, h - 80, m_hDlg, (HMENU)IDC_SHORTCUT_LIST, m_hInst, NULL);
    SendMessage(m_hShortcutList, WM_SETFONT, (WPARAM)hFont, TRUE);
    RefreshShortcutList();

    int btnX = x + w - 130;
    int btnY = y;
    int btnW = 120;
    int btnH = (int)(25 * GetDPIScale());

    CreateWindowW(L"BUTTON", L"添加快捷方式", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_ADD_SHORTCUT, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"编辑", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_EDIT_SHORTCUT, m_hInst, NULL);
    btnY += btnH + 5;
    CreateWindowW(L"BUTTON", L"删除", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnW, btnH, m_hDlg, (HMENU)IDC_DEL_SHORTCUT, m_hInst, NULL);
}

void SettingsWindow::RefreshShortcutList() {
    SendMessage(m_hShortcutList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < m_config.shortcuts.size(); ++i) {
        std::wstring entry = PopupWindow::UTF8ToWide(m_config.shortcuts[i].name + " -> " + m_config.shortcuts[i].url);
        SendMessage(m_hShortcutList, LB_ADDSTRING, 0, (LPARAM)entry.c_str());
    }
}

void SettingsWindow::CreateUITab(int x, int y, int w, int h, HFONT hFont) {
    int labelW = 120;
    int editW = 80;
    int yOff = y;
    int step = 35;

    CreateWindowW(L"STATIC", L"弹窗宽度:", WS_CHILD | WS_VISIBLE,
        x, yOff, labelW, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hPopupWidth = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + labelW, yOff, editW, 25, m_hDlg, (HMENU)IDC_POPUP_WIDTH, m_hInst, NULL);
    SendMessage(m_hPopupWidth, WM_SETFONT, (WPARAM)hFont, TRUE);
    yOff += step;

    CreateWindowW(L"STATIC", L"弹窗高度:", WS_CHILD | WS_VISIBLE,
        x, yOff, labelW, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hPopupHeight = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + labelW, yOff, editW, 25, m_hDlg, (HMENU)IDC_POPUP_HEIGHT, m_hInst, NULL);
    SendMessage(m_hPopupHeight, WM_SETFONT, (WPARAM)hFont, TRUE);
    yOff += step;

    CreateWindowW(L"STATIC", L"触发阈值(px):", WS_CHILD | WS_VISIBLE,
        x, yOff, labelW, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hEdgeThreshold = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + labelW, yOff, editW, 25, m_hDlg, (HMENU)IDC_EDGE_THRESHOLD, m_hInst, NULL);
    SendMessage(m_hEdgeThreshold, WM_SETFONT, (WPARAM)hFont, TRUE);

    // 加载当前值
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", m_config.popupWidth);
    SetWindowTextW(m_hPopupWidth, buf);
    swprintf(buf, 32, L"%d", m_config.popupHeight);
    SetWindowTextW(m_hPopupHeight, buf);
    swprintf(buf, 32, L"%d", m_config.edgeThreshold);
    SetWindowTextW(m_hEdgeThreshold, buf);
}

void SettingsWindow::CreateTimeTab(int x, int y, int w, int h, HFONT hFont) {
    m_hTimeEnabled = CreateWindowW(L"BUTTON", L"启用时间显示", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, 150, 25, m_hDlg, (HMENU)IDC_TIME_ENABLED, m_hInst, NULL);
    SendMessage(m_hTimeEnabled, WM_SETFONT, (WPARAM)hFont, TRUE);

    SendMessage(m_hTimeEnabled, BM_SETCHECK, m_config.timeDisplay.enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateWindowW(L"STATIC", L"格式:", WS_CHILD | WS_VISIBLE,
        x, y + 35, 50, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hTimeFormat = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        x + 50, y + 35, 120, 100, m_hDlg, (HMENU)IDC_TIME_FORMAT, m_hInst, NULL);
    SendMessage(m_hTimeFormat, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hTimeFormat, CB_ADDSTRING, 0, (LPARAM)L"HH:mm:ss");
    SendMessage(m_hTimeFormat, CB_ADDSTRING, 0, (LPARAM)L"HH:mm");
    if (m_config.timeDisplay.format == "HH:mm:ss")
        SendMessage(m_hTimeFormat, CB_SETCURSEL, 0, 0);
    else
        SendMessage(m_hTimeFormat, CB_SETCURSEL, 1, 0);
}

void SettingsWindow::CreateReminderTab(int x, int y, int w, int h, HFONT hFont) {
    m_hReminderEnabled = CreateWindowW(L"BUTTON", L"启用休息提醒", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, 150, 25, m_hDlg, (HMENU)IDC_REMINDER_ENABLED, m_hInst, NULL);
    SendMessage(m_hReminderEnabled, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hReminderEnabled, BM_SETCHECK, m_config.reminder.enabled ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateWindowW(L"STATIC", L"间隔(分钟):", WS_CHILD | WS_VISIBLE,
        x, y + 35, 100, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hReminderInterval = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + 100, y + 35, 80, 25, m_hDlg, (HMENU)IDC_REMINDER_INTERVAL, m_hInst, NULL);
    SendMessage(m_hReminderInterval, WM_SETFONT, (WPARAM)hFont, TRUE);
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", m_config.reminder.intervalMinutes);
    SetWindowTextW(m_hReminderInterval, buf);

    CreateWindowW(L"STATIC", L"提醒消息:", WS_CHILD | WS_VISIBLE,
        x, y + 70, 100, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hReminderMessage = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE,
        x, y + 95, w - 50, 80, m_hDlg, (HMENU)IDC_REMINDER_MESSAGE, m_hInst, NULL);
    SendMessage(m_hReminderMessage, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowTextW(m_hReminderMessage, PopupWindow::UTF8ToWide(m_config.reminder.message).c_str());

    CreateWindowW(L"BUTTON", L"测试提醒", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y + 190, 100, 30, m_hDlg, (HMENU)IDC_TEST_REMINDER, m_hInst, NULL);
}

void SettingsWindow::CreateMonitorTab(int x, int y, int w, int h, HFONT hFont) {
    m_hMonitorEnabled = CreateWindowW(L"BUTTON", L"启用后台监控", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, 150, 25, m_hDlg, (HMENU)IDC_MONITOR_ENABLED, m_hInst, NULL);
    SendMessage(m_hMonitorEnabled, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hMonitorEnabled, BM_SETCHECK, m_config.serverMonitor.backgroundEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    CreateWindowW(L"STATIC", L"刷新间隔(秒):", WS_CHILD | WS_VISIBLE,
        x, y + 35, 120, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hMonitorInterval = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + 120, y + 35, 80, 25, m_hDlg, (HMENU)IDC_MONITOR_INTERVAL, m_hInst, NULL);
    SendMessage(m_hMonitorInterval, WM_SETFONT, (WPARAM)hFont, TRUE);
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", m_config.serverMonitor.intervalSeconds);
    SetWindowTextW(m_hMonitorInterval, buf);

    CreateWindowW(L"STATIC", L"最大数据点数:", WS_CHILD | WS_VISIBLE,
        x, y + 70, 120, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hMonitorMaxPoints = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        x + 120, y + 70, 80, 25, m_hDlg, (HMENU)IDC_MONITOR_MAXPOINTS, m_hInst, NULL);
    SendMessage(m_hMonitorMaxPoints, WM_SETFONT, (WPARAM)hFont, TRUE);
    swprintf(buf, 32, L"%d", m_config.serverMonitor.maxDataPoints);
    SetWindowTextW(m_hMonitorMaxPoints, buf);

    CreateWindowW(L"BUTTON", L"清除历史数据", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y + 110, 120, 30, m_hDlg, (HMENU)IDC_CLEAR_HISTORY, m_hInst, NULL);
}

void SettingsWindow::CreateGameTab(int x, int y, int w, int h, HFONT hFont) {
    CreateWindowW(L"STATIC", L"启动命令/脚本路径:", WS_CHILD | WS_VISIBLE,
        x, y, 150, 25, m_hDlg, NULL, m_hInst, NULL);
    m_hGameCommand = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        x, y + 25, w - 100, 25, m_hDlg, (HMENU)IDC_GAME_COMMAND, m_hInst, NULL);
    SendMessage(m_hGameCommand, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowTextW(m_hGameCommand, PopupWindow::UTF8ToWide(m_config.gameCommand).c_str());

    CreateWindowW(L"BUTTON", L"浏览...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + w - 90, y + 25, 80, 25, m_hDlg, (HMENU)IDC_BROWSE_GAME, m_hInst, NULL);
}

void SettingsWindow::CreateOtherTab(int x, int y, int w, int h, HFONT hFont) {
    m_hStartupEnabled = CreateWindowW(L"BUTTON", L"开机自启动", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, 150, 25, m_hDlg, (HMENU)IDC_STARTUP_ENABLED, m_hInst, NULL);
    SendMessage(m_hStartupEnabled, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindowW(L"BUTTON", L"重置所有设置为默认", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y + 40, 180, 30, m_hDlg, (HMENU)IDC_RESET_CONFIG, m_hInst, NULL);
    CreateWindowW(L"BUTTON", L"备份配置", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y + 80, 180, 30, m_hDlg, (HMENU)IDC_BACKUP_CONFIG, m_hInst, NULL);
    CreateWindowW(L"BUTTON", L"恢复配置", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y + 120, 180, 30, m_hDlg, (HMENU)IDC_RESTORE_CONFIG, m_hInst, NULL);
}

void SettingsWindow::CreateAboutTab(int x, int y, int w, int h, HFONT hFont) {
    wchar_t text[1024];
    swprintf(text, 1024,
        L"YMC-toolkit 版本 1.0\n\n"
        L"Minecraft 服务器状态监控与快捷启动工具\n\n"
        L"作者: yuanh4148\n"
        L"使用 GDI+ 绘图，支持 Minecraft 现代/旧版协议\n\n"
        L"第三方库:\n"
        L"  - nlohmann/json (MIT License)\n"
        L"  - GDI+ (Windows SDK)\n\n"
        L"感谢使用！");
    CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, m_hDlg, (HMENU)IDC_ABOUT_TEXT, m_hInst, NULL);
    SendMessage(GetDlgItem(m_hDlg, IDC_ABOUT_TEXT), WM_SETFONT, (WPARAM)hFont, TRUE);
}

void SettingsWindow::OnAddServer() {
    // 示例：添加一个测试服务器
    ServerInfo newServer;
    newServer.host = "mc.hypixel.net";
    newServer.port = 25565;
    m_config.servers.push_back(newServer);
    m_config.Save("config.json");
    RefreshServerList();
}

void SettingsWindow::OnEditServer() {
    int sel = SendMessage(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR && sel < (int)m_config.servers.size()) {
        MessageBoxW(m_hDlg, L"编辑功能未完全实现，请手动修改配置文件。", L"提示", MB_OK);
    }
}

void SettingsWindow::OnDeleteServer() {
    int sel = SendMessage(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR && sel < (int)m_config.servers.size()) {
        m_config.servers.erase(m_config.servers.begin() + sel);
        if (m_config.currentServer >= sel && m_config.currentServer > 0)
            m_config.currentServer--;
        m_config.Save("config.json");
        RefreshServerList();
    }
}

void SettingsWindow::OnMoveUpServer() {
    int sel = SendMessage(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel > 0 && sel < (int)m_config.servers.size()) {
        std::swap(m_config.servers[sel], m_config.servers[sel-1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel-1;
        else if (m_config.currentServer == sel-1) m_config.currentServer = sel;
        m_config.Save("config.json");
        RefreshServerList();
        SendMessage(m_hServerList, LB_SETCURSEL, sel-1, 0);
    }
}

void SettingsWindow::OnMoveDownServer() {
    int sel = SendMessage(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < (int)m_config.servers.size()-1) {
        std::swap(m_config.servers[sel], m_config.servers[sel+1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel+1;
        else if (m_config.currentServer == sel+1) m_config.currentServer = sel;
        m_config.Save("config.json");
        RefreshServerList();
        SendMessage(m_hServerList, LB_SETCURSEL, sel+1, 0);
    }
}

void SettingsWindow::OnSetDefaultServer() {
    int sel = SendMessage(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR) {
        m_config.currentServer = sel;
        m_config.Save("config.json");
        RefreshServerList();
    }
}

void SettingsWindow::OnTestServer() {
    int sel = SendMessage(m_hServerList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR && sel < (int)m_config.servers.size()) {
        ServerStatus status;
        if (PingServer(m_config.servers[sel].host, m_config.servers[sel].port, status)) {
            std::wstring msg = L"在线 - " + PopupWindow::UTF8ToWide(status.motd) + L"\n玩家: " +
                std::to_wstring(status.players) + L"/" + std::to_wstring(status.maxPlayers) +
                L"\n延迟: " + std::to_wstring(status.latency) + L" ms";
            MessageBoxW(m_hDlg, msg.c_str(), L"测试结果", MB_OK);
        } else {
            MessageBoxW(m_hDlg, L"服务器离线或无法连接。", L"测试结果", MB_OK);
        }
    }
}

void SettingsWindow::OnAddShortcut() {
    Shortcut sc;
    sc.name = "新快捷方式";
    sc.url = "https://example.com";
    m_config.shortcuts.push_back(sc);
    m_config.Save("config.json");
    RefreshShortcutList();
}

void SettingsWindow::OnEditShortcut() {
    int sel = SendMessage(m_hShortcutList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR && sel < (int)m_config.shortcuts.size()) {
        MessageBoxW(m_hDlg, L"编辑功能未完全实现，请手动修改配置文件。", L"提示", MB_OK);
    }
}

void SettingsWindow::OnDeleteShortcut() {
    int sel = SendMessage(m_hShortcutList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR && sel < (int)m_config.shortcuts.size()) {
        m_config.shortcuts.erase(m_config.shortcuts.begin() + sel);
        m_config.Save("config.json");
        RefreshShortcutList();
    }
}

void SettingsWindow::OnBrowseGameCommand() {
    OPENFILENAMEW ofn = {};
    wchar_t file[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hDlg;
    ofn.lpstrFilter = L"可执行文件\0*.exe;*.bat\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        SetWindowTextW(m_hGameCommand, file);
        m_config.gameCommand = PopupWindow::WideToUTF8(file);
        m_config.Save("config.json");
    }
}

void SettingsWindow::OnToggleStartup(bool enable) {
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            RegSetValueExW(hKey, L"YMC-toolkit", 0, REG_SZ, (BYTE*)modulePath, (wcslen(modulePath)+1)*sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"YMC-toolkit");
        }
        RegCloseKey(hKey);
    }
}

void SettingsWindow::OnResetConfig() {
    if (MessageBoxW(m_hDlg, L"重置所有设置到默认值？当前配置将丢失。", L"确认", MB_YESNO) == IDYES) {
        DeleteFileW(L"config.json");
        m_config.Load("config.json");
        ShowTab(TabCtrl_GetCurSel(m_hTab));
    }
}

void SettingsWindow::OnBackupConfig() {
    OPENFILENAMEW ofn = {};
    wchar_t file[MAX_PATH] = L"config_backup.json";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hDlg;
    ofn.lpstrFilter = L"JSON文件\0*.json\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        CopyFileW(L"config.json", file, FALSE);
        MessageBoxW(m_hDlg, L"备份成功。", L"提示", MB_OK);
    }
}

void SettingsWindow::OnRestoreConfig() {
    OPENFILENAMEW ofn = {};
    wchar_t file[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hDlg;
    ofn.lpstrFilter = L"JSON文件\0*.json\0所有文件\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        CopyFileW(file, L"config.json", FALSE);
        m_config.Load("config.json");
        ShowTab(TabCtrl_GetCurSel(m_hTab));
        MessageBoxW(m_hDlg, L"恢复成功，部分设置可能需要重启程序生效。", L"提示", MB_OK);
    }
}

void SettingsWindow::OnTestReminder() {
    wchar_t msg[256];
    GetWindowTextW(m_hReminderMessage, msg, 256);
    ReminderWindow rw;
    rw.Show(msg);
}

INT_PTR CALLBACK SettingsWindow::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* pThis = nullptr;
    if (msg == WM_INITDIALOG) {
        pThis = (SettingsWindow*)lParam;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (SettingsWindow*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
    }
    if (!pThis) return FALSE;

    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_ADD_SERVER: pThis->OnAddServer(); break;
                case IDC_EDIT_SERVER: pThis->OnEditServer(); break;
                case IDC_DEL_SERVER: pThis->OnDeleteServer(); break;
                case IDC_MOVE_UP_SERVER: pThis->OnMoveUpServer(); break;
                case IDC_MOVE_DOWN_SERVER: pThis->OnMoveDownServer(); break;
                case IDC_SET_DEFAULT_SERVER: pThis->OnSetDefaultServer(); break;
                case IDC_TEST_SERVER: pThis->OnTestServer(); break;
                case IDC_ADD_SHORTCUT: pThis->OnAddShortcut(); break;
                case IDC_EDIT_SHORTCUT: pThis->OnEditShortcut(); break;
                case IDC_DEL_SHORTCUT: pThis->OnDeleteShortcut(); break;
                case IDC_BROWSE_GAME: pThis->OnBrowseGameCommand(); break;
                case IDC_STARTUP_ENABLED:
                    pThis->OnToggleStartup(IsDlgButtonChecked(hDlg, IDC_STARTUP_ENABLED) == BST_CHECKED);
                    break;
                case IDC_RESET_CONFIG: pThis->OnResetConfig(); break;
                case IDC_BACKUP_CONFIG: pThis->OnBackupConfig(); break;
                case IDC_RESTORE_CONFIG: pThis->OnRestoreConfig(); break;
                case IDC_TEST_REMINDER: pThis->OnTestReminder(); break;
                case IDOK:
                case IDCANCEL:
                    DestroyWindow(hDlg);
                    break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hDlg);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return FALSE;
    }
    return TRUE;
}

LRESULT CALLBACK SettingsWindow::TabProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    SettingsWindow* pThis = (SettingsWindow*)dwRefData;
    if (msg == WM_NOTIFY) {
        NMHDR* pnm = (NMHDR*)lParam;
        if (pnm->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(hWnd);
            pThis->ShowTab(sel);
        }
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}