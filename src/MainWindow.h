#pragma once
#include <windows.h>
#include <atomic>
#include <thread>
#include "Config.h"
#include "PopupWindow.h"
#include "ServerPinger.h"

#define WM_UPDATE_SERVER_STATUS (WM_USER + 1)
#define IDT_MOUSE_CHECK  1    // 鼠标检测定时器

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInst);
    void RunMessageLoop();
    HWND GetHWND() const { return m_hWnd; }
    const Config& GetConfig() const { return m_config; }
    bool IsPopupVisible() const { return m_popupVisible; }

private:
    HWND m_hWnd;
    HINSTANCE m_hInst;
    Config m_config;
    PopupWindow m_popup;
    bool m_popupVisible;

    std::atomic<bool> m_pingActive;
    std::thread m_pingThread;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CheckMouseEdge();
    void StartPingThread();
    void StopPingThread();
    void PingWorker();          // 工作线程函数
};