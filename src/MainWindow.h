#pragma once
#include <windows.h>
#include "Config.h"
#include "PopupWindow.h"
#include "ServerPinger.h"

#define WM_UPDATE_SERVER_STATUS (WM_USER + 1)
#define IDT_MOUSE_CHECK  1
#define IDT_SERVER_PING  2

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInst);
    void RunMessageLoop();
    HWND GetHWND() const { return m_hWnd; }
    const Config& GetConfig() const { return m_config; }

private:
    HWND m_hWnd;
    HINSTANCE m_hInst;
    Config m_config;
    PopupWindow m_popup;
    bool m_popupVisible;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CheckMouseEdge();
    void StartServerPing();
    void OnPingTimer();
    void SwitchToNextServer();
    void UpdateConfigAndSave();
};