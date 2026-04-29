#include "MainWindow.h"
#include "SettingsWindow.h"
#include "GameLauncher.h"
#include "ReminderWindow.h"
#include "StatsWindow.h"
#include "ServerPinger.h"
#include <thread>
#include <mutex>
#include <shellapi.h>

static std::mutex g_pingMutex;
static bool g_pingInProgress = false;

MainWindow::MainWindow() : m_hWnd(NULL), m_popupVisible(false), m_backgroundMonitoring(false), m_hMonitorThread(NULL), m_pSettingsWnd(nullptr) {}
MainWindow::~MainWindow() { StopBackgroundMonitoring(); delete m_pSettingsWnd; }

bool MainWindow::Create(HINSTANCE hInst, HICON hIcon) {
    m_hInst = hInst;

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    std::string configPath = exePath.substr(0, pos + 1) + "config.json";
    m_config.Load(configPath);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MainHiddenClass";
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(0, L"MainHiddenClass", L"YMC-toolkit", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInst, this);
    if (!m_hWnd) return false;

    if (!m_popup.Create(m_hWnd, hInst, m_config)) return false;

    m_popup.SetCurrentServerInfo();
    StartServerPing();

    SetTimer(m_hWnd, IDT_MOUSE_CHECK, 200, NULL);

    if (m_config.reminder.enabled) {
        UINT intervalMs = m_config.reminder.intervalMinutes * 60 * 1000;
        SetTimer(m_hWnd, IDT_REMINDER, intervalMs, NULL);
    }

    if (m_config.serverMonitor.backgroundEnabled) {
        StartBackgroundMonitoring();
    }

    return true;
}

void MainWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void MainWindow::CheckMouseEdge() {
    POINT pt;
    GetCursorPos(&pt);
    int edge = m_popup.GetDockedEdge();
    int popupW = m_config.popupWidth;
    int popupH = m_config.popupHeight;
    int threshold = m_config.edgeThreshold;
    RECT work;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);

    bool shouldShow = false;
    switch (edge) {
        case 0: // top
            if (pt.y < work.top + threshold && pt.x >= m_popup.GetEdgeOffset() && pt.x <= m_popup.GetEdgeOffset() + popupW)
                shouldShow = true;
            break;
        case 1: // right
            if (pt.x > work.right - threshold && pt.y >= m_popup.GetEdgeOffset() && pt.y <= m_popup.GetEdgeOffset() + popupH)
                shouldShow = true;
            break;
        case 2: // bottom
            if (pt.y > work.bottom - threshold && pt.x >= m_popup.GetEdgeOffset() && pt.x <= m_popup.GetEdgeOffset() + popupW)
                shouldShow = true;
            break;
        case 3: // left
            if (pt.x < work.left + threshold && pt.y >= m_popup.GetEdgeOffset() && pt.y <= m_popup.GetEdgeOffset() + popupH)
                shouldShow = true;
            break;
    }

    if (shouldShow && !m_popupVisible) {
        m_popup.Show();
        m_popupVisible = true;
        StartServerPing();
        SetTimer(m_hWnd, IDT_SERVER_PING, 10000, NULL);
    } else if (!shouldShow && m_popupVisible) {
        RECT rc;
        GetWindowRect(m_popup.GetHWND(), &rc);
        if (!PtInRect(&rc, pt)) {
            m_popup.Hide();
            m_popupVisible = false;
            KillTimer(m_hWnd, IDT_SERVER_PING);
        }
    }
}

void MainWindow::StartServerPing() {
    std::lock_guard<std::mutex> lock(g_pingMutex);
    if (g_pingInProgress) return;
    g_pingInProgress = true;

    std::thread([this]() {
        if (m_config.servers.empty()) {
            std::lock_guard<std::mutex> lock(g_pingMutex);
            g_pingInProgress = false;
            return;
        }
        int idx = m_config.currentServer;
        ServerStatus status;
        if (PingServer(m_config.servers[idx].host, m_config.servers[idx].port, status)) {
            PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(status));
        } else {
            ServerStatus offline;
            PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(offline));
        }
        std::lock_guard<std::mutex> lock(g_pingMutex);
        g_pingInProgress = false;
    }).detach();
}

void MainWindow::OnPingTimer() {
    if (m_popupVisible) {
        StartServerPing();
    }
}

void MainWindow::SwitchToNextServer() {
    if (m_config.servers.empty()) return;
    m_config.currentServer = (m_config.currentServer + 1) % m_config.servers.size();
    UpdateConfigAndSave();
    m_popup.SyncCurrentServerIndex(m_config.currentServer);
    StartServerPing();
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_latencyHistory.clear();
    }
}

void MainWindow::UpdateConfigAndSave() {
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    std::string configPath = exePath.substr(0, pos + 1) + "config.json";
    m_config.Save(configPath);
}

DWORD WINAPI MainWindow::MonitorThreadProc(LPVOID lpParam) {
    MainWindow* pThis = (MainWindow*)lpParam;
    Config& cfg = pThis->m_config;
    while (pThis->m_backgroundMonitoring) {
        int idx = cfg.currentServer;
        if (idx >= 0 && idx < (int)cfg.servers.size()) {
            ServerStatus status;
            long long latency = -1;
            bool ok = PingServer(cfg.servers[idx].host, cfg.servers[idx].port, status);
            if (ok) latency = status.latency;
            else latency = -1;
            PostMessage(pThis->GetHWND(), WM_BACKGROUND_PING_RESULT, 0, (LPARAM)latency);
        }
        Sleep(cfg.serverMonitor.intervalSeconds * 1000);
    }
    return 0;
}

void MainWindow::StartBackgroundMonitoring() {
    if (m_backgroundMonitoring) return;
    m_backgroundMonitoring = true;
    m_hMonitorThread = CreateThread(NULL, 0, MonitorThreadProc, this, 0, NULL);
}

void MainWindow::StopBackgroundMonitoring() {
    if (!m_backgroundMonitoring) return;
    m_backgroundMonitoring = false;
    if (m_hMonitorThread) {
        CloseHandle(m_hMonitorThread);
        m_hMonitorThread = NULL;
    }
}

void MainWindow::AddLatencyRecord(int latency) {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    LatencyRecord rec;
    rec.timestamp = time(NULL);
    rec.latency = latency;
    m_latencyHistory.push_back(rec);
    while ((int)m_latencyHistory.size() > m_config.serverMonitor.maxDataPoints) {
        m_latencyHistory.erase(m_latencyHistory.begin());
    }
}

std::vector<MainWindow::LatencyRecord> MainWindow::GetLatencyHistoryCopy() const {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    return m_latencyHistory;
}

void MainWindow::ShowStatsWindow() {
    auto history = GetLatencyHistoryCopy();
    if (history.empty()) {
        MessageBoxW(m_hWnd, L"暂无延迟数据，请等待后台监控收集。", L"提示", MB_OK);
        return;
    }
    StatsWindow statsWnd(m_hInst, history);
    statsWnd.Show();
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (MainWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (MainWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        switch (msg) {
            case WM_TIMER:
                if (wParam == IDT_MOUSE_CHECK) {
                    pThis->CheckMouseEdge();
                } else if (wParam == IDT_SERVER_PING) {
                    pThis->OnPingTimer();
                } else if (wParam == IDT_REMINDER) {
                    ReminderWindow reminder;
                    reminder.Show(PopupWindow::UTF8ToWide(pThis->m_config.reminder.message));
                }
                break;
            case WM_COMMAND:
                if (LOWORD(wParam) == IDC_LAUNCH_BUTTON) {
                    LaunchGame();
                } else if (LOWORD(wParam) == IDC_SWITCH_BUTTON) {
                    pThis->SwitchToNextServer();
                } else if (LOWORD(wParam) == IDC_STATS_BUTTON) {
                    pThis->ShowStatsWindow();
                } else if (LOWORD(wParam) == IDC_SETTINGS_BUTTON) {
                    if (!pThis->m_pSettingsWnd) {
                        pThis->m_pSettingsWnd = new SettingsWindow(pThis->m_hInst, pThis->m_config, pThis->GetHWND());
                    }
                    pThis->m_pSettingsWnd->Show();
                }
                break;
            case WM_UPDATE_SERVER_STATUS: {
                ServerStatus* pStatus = (ServerStatus*)lParam;
                pThis->m_popup.UpdateServerStatus(*pStatus);
                delete pStatus;
                break;
            }
            case WM_BACKGROUND_PING_RESULT: {
                int latency = (int)lParam;
                pThis->AddLatencyRecord(latency);
                break;
            }
            case WM_CONFIG_UPDATED: {
                // 从 PopupWindow 获取最新停靠信息
                int edge = pThis->m_popup.GetDockedEdge();
                int offset = pThis->m_popup.GetEdgeOffset();
                if (edge != pThis->m_config.dockedEdge || offset != pThis->m_config.edgeOffset) {
                    pThis->m_config.dockedEdge = edge;
                    pThis->m_config.edgeOffset = offset;
                    pThis->UpdateConfigAndSave();
                }
                pThis->m_popup.ReloadConfig(pThis->m_config);
                pThis->m_popup.SetCurrentServerInfo();
                pThis->StartServerPing();

                if (pThis->m_config.reminder.enabled) {
                    KillTimer(pThis->m_hWnd, IDT_REMINDER);
                    UINT intervalMs = pThis->m_config.reminder.intervalMinutes * 60 * 1000;
                    SetTimer(pThis->m_hWnd, IDT_REMINDER, intervalMs, NULL);
                } else {
                    KillTimer(pThis->m_hWnd, IDT_REMINDER);
                }

                if (pThis->m_config.serverMonitor.backgroundEnabled) {
                    pThis->StopBackgroundMonitoring();
                    pThis->StartBackgroundMonitoring();
                } else {
                    pThis->StopBackgroundMonitoring();
                }
                break;
            }
            case WM_CLEAR_HISTORY: {
                std::lock_guard<std::mutex> lock(pThis->m_historyMutex);
                pThis->m_latencyHistory.clear();
                break;
            }
            case WM_DESTROY:
                pThis->m_popup.Hide();
                pThis->StopBackgroundMonitoring();
                PostQuitMessage(0);
                break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}