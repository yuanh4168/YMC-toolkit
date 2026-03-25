#include "ServerManageDlg.h"
#include "PopupWindow.h"
#include "ServerPinger.h"
#include <commctrl.h>
#include <vector>
#include <string>

#pragma comment(lib, "comctl32.lib")

#ifndef WM_CONFIG_UPDATED
#define WM_CONFIG_UPDATED (WM_USER + 300)
#endif

static Config* s_pConfig = nullptr;
static HWND s_hList = nullptr;
static std::vector<std::wstring> s_hostStrings;
static std::vector<std::wstring> s_portStrings;

// Simple input dialog using CreateWindow
static bool ShowInputDialog(HWND hParent, std::wstring& host, std::wstring& port, const wchar_t* title) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);
    
    // Register a temporary window class for the dialog
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"TempInputDlgClass";
    RegisterClassExW(&wc);
    
    HWND hDlg = CreateWindowExW(0, L"TempInputDlgClass", title,
        WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 150, hParent, NULL, hInst, NULL);
    if (!hDlg) return false;
    
    // Create controls
    CreateWindowW(L"STATIC", L"Server Address:", WS_CHILD | WS_VISIBLE, 10, 20, 100, 20, hDlg, NULL, hInst, NULL);
    HWND hEditHost = CreateWindowW(L"EDIT", host.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER, 120, 18, 160, 22, hDlg, (HMENU)1001, hInst, NULL);
    CreateWindowW(L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE, 10, 55, 100, 20, hDlg, NULL, hInst, NULL);
    HWND hEditPort = CreateWindowW(L"EDIT", port.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER, 120, 53, 80, 22, hDlg, (HMENU)1002, hInst, NULL);
    CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 140, 90, 60, 25, hDlg, (HMENU)IDOK, hInst, NULL);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 210, 90, 60, 25, hDlg, (HMENU)IDCANCEL, hInst, NULL);
    
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
    
    bool result = false;
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) {
            if (msg.message == WM_COMMAND && (LOWORD(msg.wParam) == IDOK || LOWORD(msg.wParam) == IDCANCEL)) {
                if (LOWORD(msg.wParam) == IDOK) {
                    wchar_t bufHost[256];
                    wchar_t bufPort[16];
                    GetWindowTextW(hEditHost, bufHost, 256);
                    GetWindowTextW(hEditPort, bufPort, 16);
                    host = bufHost;
                    port = bufPort;
                    result = true;
                }
                DestroyWindow(hDlg);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return result;
}

// Refresh ListView display from vectors
static void RefreshListView() {
    ListView_DeleteAllItems(s_hList);
    for (int i = 0; i < (int)s_hostStrings.size(); ++i) {
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = (LPWSTR)s_hostStrings[i].c_str();
        ListView_InsertItem(s_hList, &lvi);
        LVITEMW lviText = {0};
        lviText.iSubItem = 1;
        lviText.pszText = (LPWSTR)s_portStrings[i].c_str();
        SendMessageW(s_hList, LVM_SETITEMTEXTW, i, (LPARAM)&lviText);
    }
}

static LRESULT CALLBACK ManageDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* pCreate = (CREATESTRUCTW*)lParam;
            s_pConfig = (Config*)pCreate->lpCreateParams;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)s_pConfig);
            
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);
            
            // Create ListView
            s_hList = CreateWindowW(WC_LISTVIEWW, NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_EDITLABELS,
                10, 10, 400, 180, hDlg, (HMENU)1001, hInst, NULL);
            ListView_SetExtendedListViewStyle(s_hList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            
            // Add columns
            LVCOLUMNW lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH;
            lvc.cx = 300;
            lvc.pszText = L"Server Address";
            ListView_InsertColumn(s_hList, 0, &lvc);
            lvc.cx = 80;
            lvc.pszText = L"Port";
            ListView_InsertColumn(s_hList, 1, &lvc);
            
            // Load data
            s_hostStrings.clear();
            s_portStrings.clear();
            for (size_t i = 0; i < s_pConfig->servers.size(); ++i) {
                s_hostStrings.push_back(PopupWindow::UTF8ToWide(s_pConfig->servers[i].host));
                s_portStrings.push_back(std::to_wstring(s_pConfig->servers[i].port));
            }
            RefreshListView();
            
            // Buttons
            CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 200, 60, 25, hDlg, (HMENU)1002, hInst, NULL);
            CreateWindowW(L"BUTTON", L"Edit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                80, 200, 60, 25, hDlg, (HMENU)1003, hInst, NULL);
            CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                150, 200, 60, 25, hDlg, (HMENU)1004, hInst, NULL);
            CreateWindowW(L"BUTTON", L"Move Up", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                220, 200, 60, 25, hDlg, (HMENU)1005, hInst, NULL);
            CreateWindowW(L"BUTTON", L"Move Down", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                290, 200, 60, 25, hDlg, (HMENU)1006, hInst, NULL);
            CreateWindowW(L"BUTTON", L"Test", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 235, 100, 25, hDlg, (HMENU)1007, hInst, NULL);
            CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                310, 235, 70, 25, hDlg, (HMENU)IDOK, hInst, NULL);
            
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDOK) {
                // Save to config
                s_pConfig->servers.clear();
                for (int i = 0; i < (int)s_hostStrings.size(); ++i) {
                    ServerInfo si;
                    si.host = PopupWindow::WideToUTF8(s_hostStrings[i]);
                    si.port = _wtoi(s_portStrings[i].c_str());
                    if (!si.host.empty() && si.port > 0 && si.port <= 65535) {
                        s_pConfig->servers.push_back(si);
                    }
                }
                // Save to file
                wchar_t modulePath[MAX_PATH];
                GetModuleFileNameW(NULL, modulePath, MAX_PATH);
                std::wstring ws(modulePath);
                std::string exePath(ws.begin(), ws.end());
                size_t pos = exePath.find_last_of("\\");
                std::string configPath = exePath.substr(0, pos + 1) + "config.json";
                s_pConfig->Save(configPath);
                
                // Notify parent
                HWND hParent = GetParent(hDlg);
                if (hParent) {
                    PostMessage(hParent, WM_CONFIG_UPDATED, 0, 0);
                }
                DestroyWindow(hDlg);
                return TRUE;
            }
            else if (id == 1002) { // Add
                std::wstring host;
                std::wstring port = L"25565";
                if (ShowInputDialog(hDlg, host, port, L"Add Server")) {
                    if (!host.empty()) {
                        s_hostStrings.push_back(host);
                        s_portStrings.push_back(port);
                        RefreshListView();
                    }
                }
            }
            else if (id == 1003) { // Edit
                int sel = ListView_GetSelectionMark(s_hList);
                if (sel >= 0) {
                    std::wstring host = s_hostStrings[sel];
                    std::wstring port = s_portStrings[sel];
                    if (ShowInputDialog(hDlg, host, port, L"Edit Server")) {
                        s_hostStrings[sel] = host;
                        s_portStrings[sel] = port;
                        RefreshListView();
                    }
                } else {
                    MessageBoxW(hDlg, L"Please select a server first.", L"Info", MB_OK);
                }
            }
            else if (id == 1004) { // Delete
                int sel = ListView_GetSelectionMark(s_hList);
                if (sel >= 0) {
                    s_hostStrings.erase(s_hostStrings.begin() + sel);
                    s_portStrings.erase(s_portStrings.begin() + sel);
                    RefreshListView();
                } else {
                    MessageBoxW(hDlg, L"Please select a server first.", L"Info", MB_OK);
                }
            }
            else if (id == 1005) { // Move Up
                int sel = ListView_GetSelectionMark(s_hList);
                if (sel > 0) {
                    std::swap(s_hostStrings[sel], s_hostStrings[sel-1]);
                    std::swap(s_portStrings[sel], s_portStrings[sel-1]);
                    RefreshListView();
                    ListView_SetSelectionMark(s_hList, sel-1);
                }
            }
            else if (id == 1006) { // Move Down
                int sel = ListView_GetSelectionMark(s_hList);
                int count = (int)s_hostStrings.size();
                if (sel >= 0 && sel < count-1) {
                    std::swap(s_hostStrings[sel], s_hostStrings[sel+1]);
                    std::swap(s_portStrings[sel], s_portStrings[sel+1]);
                    RefreshListView();
                    ListView_SetSelectionMark(s_hList, sel+1);
                }
            }
            else if (id == 1007) { // Test
                int sel = ListView_GetSelectionMark(s_hList);
                if (sel >= 0) {
                    std::string host = PopupWindow::WideToUTF8(s_hostStrings[sel]);
                    int port = _wtoi(s_portStrings[sel].c_str());
                    ServerStatus status;
                    if (PingServer(host, port, status)) {
                        wchar_t msg[512];
                        swprintf(msg, 512, L"Online - Latency: %d ms\nPlayers: %d/%d\nVersion: %s",
                            status.latency, status.players, status.maxPlayers,
                            PopupWindow::UTF8ToWide(status.version).c_str());
                        MessageBoxW(hDlg, msg, L"Test Result", MB_OK);
                    } else {
                        MessageBoxW(hDlg, L"Connection failed.", L"Test Result", MB_OK);
                    }
                } else {
                    MessageBoxW(hDlg, L"Please select a server first.", L"Info", MB_OK);
                }
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hDlg);
            return TRUE;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

HWND CreateManageDialog(HWND hParent, Config& config) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hParent, GWLP_HINSTANCE);
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ManageDlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ManageDlgClass";
    RegisterClassExW(&wc);
    
    HWND hDlg = CreateWindowExW(0, L"ManageDlgClass", L"Server Manager",
        WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 440, 310, hParent, NULL, hInst, &config);
    if (hDlg) {
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);
    }
    return hDlg;
}