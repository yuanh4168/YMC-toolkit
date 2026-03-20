#include "MainWindow.h"
#include "GameLauncher.h"
#include <chrono>
#include <thread>

MainWindow::MainWindow() : m_hWnd(NULL), m_popupVisible(false), m_pingActive(false) {}
MainWindow::~MainWindow() {
    StopPingThread();
}

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
    if (pt.y < m_config.edgeThreshold && !m_popupVisible) {
        m_popup.Show();
        m_popupVisible = true;
        StartPingThread();
    } else if (pt.y > m_config.popupHeight + 20 && m_popupVisible) {
        m_popup.Hide();
        m_popupVisible = false;
        StopPingThread();
    }
}

void MainWindow::StartPingThread() {
    if (m_pingThread.joinable()) return;
    m_pingActive = true;
    m_pingThread = std::thread(&MainWindow::PingWorker, this);
}

void MainWindow::StopPingThread() {
    m_pingActive = false;
    if (m_pingThread.joinable()) {
        m_pingThread.join();
    }
}

void MainWindow::PingWorker() {
    while (m_pingActive) {
        if (m_popupVisible) {
            ServerStatus status;
            if (PingServer(m_config.serverHost, m_config.serverPort, status)) {
                PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(status));
            } else {
                ServerStatus offline;
                PostMessage(m_hWnd, WM_UPDATE_SERVER_STATUS, 0, (LPARAM)new ServerStatus(offline));
            }
        }
        // 休眠 10 秒，可分小段以便快速响应停止信号
        for (int i = 0; i < 100 && m_pingActive; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
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
                }
                break;
            case WM_COMMAND:
                if (LOWORD(wParam) == IDC_LAUNCH_BUTTON) {
                    LaunchGame();
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