#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "Config.h"

// 选项卡索引枚举
enum TabIndex {
    TAB_SERVER = 0,
    TAB_SHORTCUT,
    TAB_UI,
    TAB_TIME,
    TAB_REMINDER,
    TAB_MONITOR,
    TAB_GAME,
    TAB_OTHER
};

// 静态文本控件 ID（用于在切换选项卡时隐藏/显示）
#define IDC_STATIC_POPUP_WIDTH     2100
#define IDC_STATIC_POPUP_HEIGHT    2101
#define IDC_STATIC_EDGE_THRESHOLD  2102
#define IDC_STATIC_TIME_FORMAT     2103
#define IDC_STATIC_REMINDER_INTERVAL 2104
#define IDC_STATIC_REMINDER_MESSAGE  2105
#define IDC_STATIC_MONITOR_INTERVAL  2106
#define IDC_STATIC_MONITOR_MAXPOINTS 2107
#define IDC_STATIC_GAME_COMMAND      2108

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent);
    ~SettingsWindow();
    void Show();

private:
    Config& m_config;
    HWND m_hDlg;
    HINSTANCE m_hInst;
    HWND m_hParent;
    std::string m_configPath;
    int m_selectedTab;

    // 子控件句柄
    HWND m_hServerList;
    HWND m_hAddServerBtn, m_hEditServerBtn, m_hDelServerBtn;
    HWND m_hMoveUpBtn, m_hMoveDownBtn, m_hSetDefaultBtn, m_hTestServerBtn;
    HWND m_hShortcutList;
    HWND m_hAddShortcutBtn, m_hEditShortcutBtn, m_hDelShortcutBtn;
    HWND m_hPopupWidth, m_hPopupHeight, m_hEdgeThreshold;
    HWND m_hTimeEnabled, m_hTimeFormat;
    HWND m_hReminderEnabled, m_hReminderInterval, m_hReminderMessage, m_hTestReminderBtn;
    HWND m_hMonitorEnabled, m_hMonitorInterval, m_hMonitorMaxPoints, m_hClearHistoryBtn;
    HWND m_hGameCommand, m_hBrowseGameBtn;
    HWND m_hStartupEnabled, m_hResetConfigBtn, m_hBackupConfigBtn, m_hRestoreConfigBtn, m_hAboutText;
    HWND m_hTabCtrl;

    // 用于切换选项卡时隐藏/显示的静态文本控件句柄
    HWND m_hStaticPopupWidth, m_hStaticPopupHeight, m_hStaticEdgeThreshold;
    HWND m_hStaticTimeFormat;
    HWND m_hStaticReminderInterval, m_hStaticReminderMessage;
    HWND m_hStaticMonitorInterval, m_hStaticMonitorMaxPoints;
    HWND m_hStaticGameCommand;

    HFONT m_hClearFont;   // 支持中文的字体

    static LRESULT CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void InitControls();
    void ShowTab(int tab);
    void LoadDataToUI();
    void SaveUItoConfig();
    void UpdateServerListUI();
    void UpdateShortcutListUI();

    void AddServer();
    void EditServer();
    void DeleteServer();
    void MoveServerUp();
    void MoveServerDown();
    void SetDefaultServer();
    void TestServer();

    void AddShortcut();
    void EditShortcut();
    void DeleteShortcut();

    void BrowseGameCommand();
    void TestReminder();
    void ClearHistory();
    void ResetConfig();
    void BackupConfig();
    void RestoreConfig();
    void SetStartup(bool enable);
};