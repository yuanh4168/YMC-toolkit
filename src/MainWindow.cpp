#include "MainWindow.h"
#include "GameLauncher.h"
#include <thread>

MainWindow::MainWindow() : m_hWnd(NULL), m_popupVisible(false) {}
MainWindow::~MainWindow() {}

bool MainWindow::Create(HINSTANCE hInst) {
    m_hInst = hInst;

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    std::string configPath = exePath.substr(0, pos + 1) + "config.json";
    if (!m_config.Load(configPath)) {
        MessageBoxA(NULL, "Failed to load config.json", "Error", MB_ICONERROR);
        return false;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MainHiddenClass";
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(0, L"MainHiddenClass", L"MCTool", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, NULL, NULL, hInst, this);
    if (!m_hWnd) return false;

    if (!m_popup.Create(m_hWnd, hInst, m_config)) return false;

    if (m_popup.GetLastX() == 0) {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int defaultX = (screenWidth - m_config.popupWidth) / 2;
        m_popup.SetLastX(defaultX);
    }

    // 显示初始服务器地址和“检测中...”
    m_popup.SetCurrentServerInfo();
    StartServerPing();

    SetTimer(m_hWnd, IDT_MOUSE_CHECK, 200, NULL);
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
    int popupX = m_popup.GetLastX();
    int popupWidth = m_config.popupWidth;
    if (pt.y < m_config.edgeThreshold && !m_popupVisible) {
        if (pt.x >= popupX && pt.x <= popupX + popupWidth) {
            m_popup.Show();
            m_popupVisible = true;
            StartServerPing();
            SetTimer(m_hWnd, IDT_SERVER_PING, 10000, NULL);
        }
    } else if (pt.y > m_config.popupHeight + 20 && m_popupVisible) {
        m_popup.Hide();
        m_popupVisible = false;
        KillTimer(m_hWnd, IDT_SERVER_PING);
    }
}

void MainWindow::StartServerPing() {
    std::thread([this]() {
        if (m_config.servers.empty()) return;
        int idx = m_config.currentServer;
        ServerStatus status;
        if (PingServer(m_config.servers[idx].host, m_config.servers[idx].port, status)) {
            PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(status));
        } else {
            ServerStatus offline;
            PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(offline));
        }
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
    m_popup.SyncCurrentServerIndex(m_config.currentServer);   // 同步弹窗的索引
    StartServerPing();                // 开始异步查询
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
                }
                break;
            case WM_COMMAND:
                if (LOWORD(wParam) == IDC_LAUNCH_BUTTON) {
                    LaunchGame();
                } else if (LOWORD(wParam) == IDC_SWITCH_BUTTON) {
                    pThis->SwitchToNextServer();
                }
                break;
            case WM_UPDATE_SERVER_STATUS:
            {
                ServerStatus* pStatus = (ServerStatus*)lParam;
                pThis->m_popup.UpdateServerStatus(*pStatus);
                delete pStatus;
                break;
            }
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}