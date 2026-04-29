#pragma once
#include <windows.h>
#include <atomic>
#include <vector>
#include <mutex>
#include "Config.h"
#include "PopupWindow.h"
#include "ServerPinger.h"
#include "SettingsWindow.h"

#define WM_UPDATE_SERVER_STATUS (WM_USER + 1)
#define WM_BACKGROUND_PING_RESULT (WM_USER + 2)
#define WM_CLEAR_HISTORY (WM_USER + 500)
#define WM_CONFIG_UPDATED (WM_USER + 300)
#define IDT_MOUSE_CHECK  1
#define IDT_SERVER_PING  2
#define IDT_REMINDER     3

class MainWindow {
public:
    struct LatencyRecord {
        time_t timestamp;
        int latency;
    };

    MainWindow();
    ~MainWindow();

    bool Create(HINSTANCE hInst, HICON hIcon);
    void RunMessageLoop();
    HWND GetHWND() const { return m_hWnd; }
    const Config& GetConfig() const { return m_config; }
    std::vector<LatencyRecord> GetLatencyHistoryCopy() const;

private:
    HWND m_hWnd;
    HINSTANCE m_hInst;
    Config m_config;
    PopupWindow m_popup;
    bool m_popupVisible;

    std::atomic<bool> m_backgroundMonitoring;
    HANDLE m_hMonitorThread;
    std::vector<LatencyRecord> m_latencyHistory;
    mutable std::mutex m_historyMutex;

    SettingsWindow* m_pSettingsWnd;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CheckMouseEdge();
    void StartServerPing();
    void OnPingTimer();
    void SwitchToNextServer();
    void UpdateConfigAndSave();

    void StartBackgroundMonitoring();
    void StopBackgroundMonitoring();
    static DWORD WINAPI MonitorThreadProc(LPVOID lpParam);
    void AddLatencyRecord(int latency);
    void ShowStatsWindow();
};