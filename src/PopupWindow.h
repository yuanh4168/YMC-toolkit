#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <map>
#include "Config.h"
#include "ServerPinger.h"

// 控件 ID
#define IDC_SERVER_STATUS 1001
#define IDC_LAUNCH_BUTTON 1003
#define IDC_SHORTCUT1     1004
#define IDC_SHORTCUT2     1005
#define IDC_SHORTCUT3     1006
#define IDC_SHORTCUT4     1007
#define IDC_EXIT_BUTTON   1008
#define IDC_SWITCH_BUTTON 1009
#define IDC_TOOL_BUTTON   1011
#define IDC_TIME_STATIC   1012
#define IDC_STATS_BUTTON  1013
#define IDC_SETTINGS_BUTTON 1014

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
    void ReloadConfig(const Config& newCfg);
    HWND GetHWND() const { return m_hWnd; }
    int GetLastX() const { return m_lastX; }
    void SetLastX(int x) { m_lastX = x; }

    static std::wstring UTF8ToWide(const std::string& utf8);
    static std::string WideToUTF8(const std::wstring& wide);

private:
    HWND m_hWnd;
    HWND m_hParent;          // 父窗口句柄（保存以便重建）
    HINSTANCE m_hInst;       // 实例句柄（保存以便重建）
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
    HWND m_hToolButton;
    HWND m_hStatsButton;
    HWND m_hSettingsButton;
    HWND m_hTimeStatic;
    int m_lastX;
    bool m_autoHideScheduled;

    HWND m_hFaviconStatic;
    Gdiplus::Bitmap* m_pFaviconBitmap;
    ULONG_PTR m_gdiplusToken;

    Gdiplus::Font* m_pGdiNormalFont;
    Gdiplus::Font* m_pGdiBoldFont;

    std::map<HWND, int> m_buttonRadiusMap;

    Gdiplus::Bitmap* CreateBitmapFromData(const BYTE* data, size_t len);
    Gdiplus::Bitmap* Base64ToBitmap(const std::string& base64Data);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ButtonSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void SetHoverButton(HWND hBtn);
    bool IsButton(HWND hWnd);
    void ScheduleAutoHide();
    void CancelAutoHide();
    void OnAutoHideTimer();
    void AdhereToTop();
    void UpdateLastX();
    void UpdateTimeDisplay();

    void Recreate();   // 新增：重建窗口
};