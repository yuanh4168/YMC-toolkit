#pragma once
#include <windows.h>
#include <string>
#include "Config.h"
#include "ServerPinger.h"

#define IDC_SERVER_STATUS 1001
#define IDC_LAUNCH_BUTTON 1003
#define IDC_SHORTCUT1     1004
#define IDC_SHORTCUT2     1005
#define IDC_SHORTCUT3     1006
#define IDC_SHORTCUT4     1007
#define IDC_EXIT_BUTTON   1008
#define IDC_SWITCH_BUTTON 1009

// 自定义消息：通知父窗口按钮悬停变化
#define WM_UPDATE_HOVER   (WM_USER + 200)

class PopupWindow {
public:
    PopupWindow();
    ~PopupWindow();

    bool Create(HWND hParent, HINSTANCE hInst, const Config& cfg);
    void Show();
    void Hide();
    void UpdateServerStatus(const ServerStatus& status);
    void SetCurrentServerInfo();
    void SyncCurrentServerIndex(int idx);
    HWND GetHWND() const { return m_hWnd; }
    int GetLastX() const { return m_lastX; }
    void SetLastX(int x) { m_lastX = x; }

private:
    HWND m_hWnd;
    HWND m_hServerAddressStatic;
    HWND m_hServerStatusStatic;
    HWND m_hShortcutButtons[4];
    Config m_config;
    HBRUSH m_hBkBrush;
    HWND m_hHoverButton;
    HFONT m_hNormalFont;
    HFONT m_hBoldFont;
    HWND m_hExitButton;
    HWND m_hSwitchButton;
    int m_lastX;
    bool m_autoHideScheduled;

    static std::wstring UTF8ToWide(const std::string& utf8);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void SetHoverButton(HWND hBtn);
    bool IsButton(HWND hWnd);
    void ScheduleAutoHide();
    void CancelAutoHide();
    void OnAutoHideTimer();
    void AdhereToTop();
    void UpdateLastX();
};