#pragma once
#include <windows.h>      // 必须包含，定义 HWND, HFONT 等
#include <string>
#include <vector>
#include "Config.h"

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent);
    ~SettingsWindow();

    void ShowModal();

private:
    HINSTANCE m_hInst;
    Config& m_config;
    HWND m_hParent;
    HWND m_hDlg;
    HWND m_hTab;

    // 各选项卡的子控件句柄
    HWND m_hServerList;
    HWND m_hShortcutList;
    HWND m_hPopupWidth, m_hPopupHeight, m_hEdgeThreshold;
    HWND m_hTimeEnabled, m_hTimeFormat;
    HWND m_hReminderEnabled, m_hReminderInterval, m_hReminderMessage;
    HWND m_hMonitorEnabled, m_hMonitorInterval, m_hMonitorMaxPoints;
    HWND m_hGameCommand;
    HWND m_hStartupEnabled;
    HWND m_hResetBtn, m_hBackupBtn, m_hRestoreBtn;

    void ShowTab(int index);
    void CreateServerTab(int x, int y, int w, int h, HFONT hFont);
    void CreateShortcutTab(int x, int y, int w, int h, HFONT hFont);
    void CreateUITab(int x, int y, int w, int h, HFONT hFont);
    void CreateTimeTab(int x, int y, int w, int h, HFONT hFont);
    void CreateReminderTab(int x, int y, int w, int h, HFONT hFont);
    void CreateMonitorTab(int x, int y, int w, int h, HFONT hFont);
    void CreateGameTab(int x, int y, int w, int h, HFONT hFont);
    void CreateOtherTab(int x, int y, int w, int h, HFONT hFont);
    void CreateAboutTab(int x, int y, int w, int h, HFONT hFont);

    void RefreshServerList();
    void RefreshShortcutList();
    void OnAddServer();
    void OnEditServer();
    void OnDeleteServer();
    void OnMoveUpServer();
    void OnMoveDownServer();
    void OnSetDefaultServer();
    void OnTestServer();
    void OnAddShortcut();
    void OnEditShortcut();
    void OnDeleteShortcut();
    void OnBrowseGameCommand();
    void OnToggleStartup(bool enable);
    void OnResetConfig();
    void OnBackupConfig();
    void OnRestoreConfig();
    void OnTestReminder();

    static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK TabProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
};