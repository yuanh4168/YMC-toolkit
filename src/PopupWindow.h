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

class PopupWindow {
public:
    PopupWindow();
    ~PopupWindow();

    bool Create(HWND hParent, HINSTANCE hInst, const Config& cfg);
    void Show();
    void Hide();
    void UpdateServerStatus(const ServerStatus& status);
    HWND GetHWND() const { return m_hWnd; }

private:
    HWND m_hWnd;
    HWND m_hServerStatic;
    HWND m_hShortcutButtons[4];
    Config m_config;
    HBRUSH m_hBkBrush;
    HWND m_hHoverButton;
    HFONT m_hNormalFont;
    HFONT m_hBoldFont;
    bool m_bTracking;   // 是否正在跟踪鼠标离开

    static std::wstring UTF8ToWide(const std::string& utf8);
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnMouseMove(WPARAM wParam, LPARAM lParam);
    void SetHoverButton(HWND hBtn);
    bool IsButton(HWND hWnd);
};