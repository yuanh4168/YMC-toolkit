// SettingsWindow.cpp
#include "SettingsWindow.h"
#include "easy_UI.hpp"
#include "ServerPinger.h"
#include "ReminderWindow.h"
#include "PopupWindow.h"
#include "DPIHelper.h"
#include <sstream>
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <nlohmann/json.hpp>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

#define WM_CONFIG_UPDATED (WM_USER + 300)
#define WM_CLEAR_HISTORY  (WM_USER + 500)

using namespace eui;

// ===== 输入对话框（原封不动从原项目搬过来） =====
struct InputDialogData {
    std::string title, label1, label2, value1, value2;
    bool confirmed;
    HWND hDlg;
    HFONT hFont;
};

static LRESULT CALLBACK InputDialogWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    InputDialogData* pData = nullptr;
    if (msg == WM_NCCREATE) {
        pData = (InputDialogData*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pData);
        pData->hDlg = hWnd;
        return TRUE;
    }
    pData = (InputDialogData*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!pData) return DefWindowProc(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE: {
            double scale = GetDPIScale();
            int xMargin = (int)(15 * scale);
            int yMargin = (int)(15 * scale);
            int labelW = (int)(80 * scale);
            int editW = (int)(200 * scale);
            int btnW = (int)(80 * scale);
            int rowH = (int)(28 * scale);
            int y = yMargin;

            HDC hdc = GetDC(hWnd);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hWnd, hdc);
            int fontSize = -MulDiv(11, dpiX, 96);
            LOGFONTW lf = {0};
            lf.lfHeight = fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
            pData->hFont = CreateFontIndirectW(&lf);

            CreateWindowW(L"STATIC", PopupWindow::UTF8ToWide(pData->label1).c_str(),
                WS_CHILD | WS_VISIBLE, xMargin, y, labelW, rowH, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hEdit1 = CreateWindowW(L"EDIT", PopupWindow::UTF8ToWide(pData->value1).c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                xMargin + labelW + xMargin, y, editW, rowH, hWnd, (HMENU)101, GetModuleHandle(NULL), NULL);
            y += rowH + yMargin;

            CreateWindowW(L"STATIC", PopupWindow::UTF8ToWide(pData->label2).c_str(),
                WS_CHILD | WS_VISIBLE, xMargin, y, labelW, rowH, hWnd, NULL, GetModuleHandle(NULL), NULL);
            HWND hEdit2 = CreateWindowW(L"EDIT", PopupWindow::UTF8ToWide(pData->value2).c_str(),
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                xMargin + labelW + xMargin, y, editW, rowH, hWnd, (HMENU)102, GetModuleHandle(NULL), NULL);
            y += rowH + yMargin * 2;

            CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                xMargin + labelW + xMargin + editW - btnW * 2 - xMargin, y, btnW, rowH,
                hWnd, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
            CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                xMargin + labelW + xMargin + editW - btnW, y, btnW, rowH,
                hWnd, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

            for (HWND hChild = GetWindow(hWnd, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
                SendMessage(hChild, WM_SETFONT, (WPARAM)pData->hFont, TRUE);
            }
            SetFocus(hEdit1);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t buf1[256] = {0}, buf2[256] = {0};
                GetDlgItemTextW(hWnd, 101, buf1, 256);
                GetDlgItemTextW(hWnd, 102, buf2, 256);
                pData->value1 = PopupWindow::WideToUTF8(buf1);
                pData->value2 = PopupWindow::WideToUTF8(buf2);
                pData->confirmed = true;
                DestroyWindow(hWnd);
            } else if (LOWORD(wParam) == IDCANCEL) {
                pData->confirmed = false;
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_CLOSE:
            pData->confirmed = false;
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            if (pData->hFont) DeleteObject(pData->hFont);
            break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static bool ShowInputDialog(HWND hParent, const std::string& title,
                            const std::string& label1, const std::string& label2,
                            std::string& value1, std::string& value2) {
    HINSTANCE hInst = GetModuleHandle(NULL);
    double scale = GetDPIScale();
    int width = (int)(350 * scale);
    int height = (int)(150 * scale);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = InputDialogWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"InputDialogClass";
    RegisterClassExW(&wc);

    InputDialogData data;
    data.title = title;
    data.label1 = label1;
    data.label2 = label2;
    data.value1 = value1;
    data.value2 = value2;
    data.confirmed = false;
    data.hFont = NULL;

    HWND hDlg = CreateWindowExW(0, L"InputDialogClass", PopupWindow::UTF8ToWide(title).c_str(),
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - width) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - height) / 2,
        width, height, hParent, NULL, hInst, &data);
    if (!hDlg) return false;

    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (data.confirmed) {
        value1 = data.value1;
        value2 = data.value2;
        return true;
    }
    return false;
}

// ===== SettingsWindow 实现 =====
SettingsWindow::SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent)
    : m_config(cfg), m_hInst(hInst), m_hParent(hParent) {}

SettingsWindow::~SettingsWindow() {}

void SettingsWindow::Show() {
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    m_configPath = exePath.substr(0, pos + 1) + "config.json";
    m_config.Load(m_configPath);

    Application app;
    app.title = L"设置";
    app.width = 800;
    app.height = 950;
    app.OnInit = [this]() {
        using namespace eui;
        easyUI.SetGlobalFont(L"Microsoft YaHei", 14);
        Theme t;
        t.bg = Color(45, 45, 48);
        t.fg = Color(63, 63, 70);
        t.accent = Color(0, 122, 204);
        t.text = Color(240, 240, 240);
        easyUI.SetTheme(t);

        const int ROW_H = 28;
        const int SPACE = 12;            // 行间距，使布局不拥挤

        int y = 10;

        // ---------- 服务器管理 ----------
        easyUI.CreateLabel("lblServers", L"服务器管理", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateComboBox("cmbServers", 20, y, 400, ROW_H);
        easyUI.CreateButton("btnAddSrv", L"添加", 430, y, 80, ROW_H);
        easyUI.CreateButton("btnEditSrv", L"编辑", 520, y, 60, ROW_H);
        easyUI.CreateButton("btnDelSrv", L"删除", 590, y, 60, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("btnUpSrv", L"上移", 430, y, 60, ROW_H);
        easyUI.CreateButton("btnDownSrv", L"下移", 500, y, 60, ROW_H);
        easyUI.CreateButton("btnDefSrv", L"设为默认", 570, y, 80, ROW_H);
        easyUI.CreateButton("btnTestSrv", L"测试连接", 660, y, 80, ROW_H);
        y += ROW_H + SPACE * 2;

        // ---------- 快捷方式管理 ----------
        easyUI.CreateLabel("lblShortcuts", L"快捷方式管理", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateComboBox("cmbShortcuts", 20, y, 400, ROW_H);
        easyUI.CreateButton("btnAddShc", L"添加", 430, y, 80, ROW_H);
        easyUI.CreateButton("btnEditShc", L"编辑", 520, y, 60, ROW_H);
        easyUI.CreateButton("btnDelShc", L"删除", 590, y, 60, ROW_H);
        y += ROW_H + SPACE * 2;

        // ---------- 界面设置 ----------
        easyUI.CreateLabel("lblUI", L"界面设置", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateLabel("lblPW", L"弹窗宽度:", 20, y, 100, ROW_H);
        easyUI.CreateTextBox("txtPopWidth", 130, y, 100, ROW_H);
        easyUI.CreateLabel("lblPH", L"弹窗高度:", 260, y, 100, ROW_H);
        easyUI.CreateTextBox("txtPopHeight", 370, y, 100, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateLabel("lblEdge", L"边缘阈值:", 20, y, 100, ROW_H);
        easyUI.CreateTextBox("txtEdge", 130, y, 100, ROW_H);
        y += ROW_H + SPACE * 2;

        // ---------- 时间显示 ----------
        easyUI.CreateLabel("lblTime", L"时间显示", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("chkTimeEn", L"[ ] 启用时间显示", 20, y, 160, ROW_H);
        easyUI.CreateLabel("lblFmt", L"格式:", 200, y, 50, ROW_H);
        easyUI.CreateComboBox("cmbTimeFmt", 260, y, 150, ROW_H);
        y += ROW_H + SPACE * 2;

        // ---------- 休息提醒 ----------
        easyUI.CreateLabel("lblRem", L"休息提醒", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("chkRemEn", L"[ ] 启用休息提醒", 20, y, 160, ROW_H);
        easyUI.CreateLabel("lblRemInt", L"间隔(分钟):", 200, y, 100, ROW_H);
        easyUI.CreateTextBox("txtRemInterval", 310, y, 80, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateLabel("lblRemMsg", L"消息:", 20, y, 60, ROW_H);
        easyUI.CreateTextBox("txtRemMsg", 90, y, 400, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("btnTestRem", L"测试提醒", 20, y, 100, ROW_H);
        y += ROW_H + SPACE * 2;

        // ---------- 后台监控 ----------
        easyUI.CreateLabel("lblMon", L"后台监控", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("chkMonEn", L"[ ] 启用后台监控", 20, y, 160, ROW_H);
        easyUI.CreateLabel("lblMonInt", L"间隔(秒):", 200, y, 100, ROW_H);
        easyUI.CreateTextBox("txtMonInterval", 310, y, 80, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateLabel("lblMonMax", L"最大点数:", 20, y, 100, ROW_H);
        easyUI.CreateTextBox("txtMonMaxPts", 130, y, 80, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("btnClearHist", L"清除历史", 20, y, 100, ROW_H);
        y += ROW_H + SPACE * 2;

        // ---------- 游戏启动 ----------
        easyUI.CreateLabel("lblGame", L"游戏启动", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateLabel("lblCmd", L"命令行:", 20, y, 80, ROW_H);
        easyUI.CreateTextBox("txtGameCmd", 110, y, 420, ROW_H);
        easyUI.CreateButton("btnBrowse", L"浏览", 540, y, 70, ROW_H);
        y += ROW_H + SPACE * 2;

        // ---------- 其他 ----------
        easyUI.CreateLabel("lblOther", L"其他", 20, y, 200, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("chkStartup", L"[ ] 开机启动", 20, y, 120, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateButton("btnReset", L"重置配置", 20, y, 100, ROW_H);
        easyUI.CreateButton("btnBackup", L"备份配置", 140, y, 100, ROW_H);
        easyUI.CreateButton("btnRestore", L"恢复配置", 260, y, 100, ROW_H);
        y += ROW_H + SPACE;
        easyUI.CreateLabel("lblAbout", L"YMC-toolkit v1.0  |  Minecraft 服务器监控工具", 20, y, 450, ROW_H);

        // ---------- 加载数据 ----------
        LoadDataToUI();

        // ---------- 事件绑定 ----------
        easyUI.OnClick("btnAddSrv", [this]() { AddServer(); });
        easyUI.OnClick("btnEditSrv", [this]() { EditServer(); });
        easyUI.OnClick("btnDelSrv", [this]() { DeleteServer(); });
        easyUI.OnClick("btnUpSrv", [this]() { MoveServerUp(); });
        easyUI.OnClick("btnDownSrv", [this]() { MoveServerDown(); });
        easyUI.OnClick("btnDefSrv", [this]() { SetDefaultServer(); });
        easyUI.OnClick("btnTestSrv", [this]() { TestServer(); });
        easyUI.OnClick("btnAddShc", [this]() { AddShortcut(); });
        easyUI.OnClick("btnEditShc", [this]() { EditShortcut(); });
        easyUI.OnClick("btnDelShc", [this]() { DeleteShortcut(); });
        easyUI.OnClick("btnBrowse", [this]() { BrowseGameCommand(); });
        easyUI.OnClick("btnTestRem", [this]() { TestReminder(); });
        easyUI.OnClick("btnClearHist", [this]() { ClearHistory(); });
        easyUI.OnClick("btnReset", [this]() { ResetConfig(); });
        easyUI.OnClick("btnBackup", [this]() { BackupConfig(); });
        easyUI.OnClick("btnRestore", [this]() { RestoreConfig(); });

        // 复选框切换
        auto ToggleChk = [this](const std::string& name, bool& configBool) {
            configBool = !configBool;
            std::wstring txt = easyUI.Text(name);
            size_t pos = txt.find(L"] ");
            if (pos != std::wstring::npos)
                txt = (configBool ? L"[√] " : L"[ ] ") + txt.substr(pos + 2);
            easyUI.Text(name, txt);
        };

        easyUI.OnClick("chkTimeEn", [this, &ToggleChk]() { ToggleChk("chkTimeEn", m_config.timeDisplay.enabled); });
        easyUI.OnClick("chkRemEn", [this, &ToggleChk]() { ToggleChk("chkRemEn", m_config.reminder.enabled); });
        easyUI.OnClick("chkMonEn", [this, &ToggleChk]() { ToggleChk("chkMonEn", m_config.serverMonitor.backgroundEnabled); });

        easyUI.OnClick("chkStartup", [this]() {
            bool cur = easyUI.Text("chkStartup").find(L"[√]") != std::wstring::npos;
            SetStartup(!cur);
            std::wstring txt = easyUI.Text("chkStartup");
            size_t pos = txt.find(L"] ");
            if (pos != std::wstring::npos)
                txt = ((!cur) ? L"[√] " : L"[ ] ") + txt.substr(pos + 2);
            easyUI.Text("chkStartup", txt);
        });

        // ---------- 窗口关闭处理 ----------
        HWND hwnd = eui::detail::GS().hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
        WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)SettingsWndProc);
        SetPropW(hwnd, L"OLD_PROC", (HANDLE)oldProc);
    };

    easyUI.Run(app);
    SaveUItoConfig();
    PostMessage(m_hParent, WM_CONFIG_UPDATED, 0, 0);
}

void SettingsWindow::LoadDataToUI() {
    // 清空并重新填充服务器列表
    auto* cmbSrv = dynamic_cast<Controls::ComboBox*>(Controls::FindControl("cmbServers"));
    if (cmbSrv) cmbSrv->items.clear();
    for (auto& sv : m_config.servers)
        easyUI.AddComboItem("cmbServers", PopupWindow::UTF8ToWide(sv.host + ":" + std::to_string(sv.port)));

    // 清空并重新填充快捷方式列表
    auto* cmbShc = dynamic_cast<Controls::ComboBox*>(Controls::FindControl("cmbShortcuts"));
    if (cmbShc) cmbShc->items.clear();
    for (auto& sc : m_config.shortcuts)
        easyUI.AddComboItem("cmbShortcuts", PopupWindow::UTF8ToWide(sc.name + " -> " + sc.url));

    // 界面数值
    easyUI.Text("txtPopWidth", std::to_wstring(m_config.popupWidth));
    easyUI.Text("txtPopHeight", std::to_wstring(m_config.popupHeight));
    easyUI.Text("txtEdge", std::to_wstring(m_config.edgeThreshold));

    // 时间显示
    SetCheckButtonState("chkTimeEn", m_config.timeDisplay.enabled);
    easyUI.Text("chkTimeEn", (m_config.timeDisplay.enabled ? L"[√] " : L"[ ] ") + std::wstring(L"启用时间显示"));
    auto* cmbFmt = dynamic_cast<Controls::ComboBox*>(Controls::FindControl("cmbTimeFmt"));
    if (cmbFmt) {
        cmbFmt->items.clear();
        cmbFmt->items.push_back(L"HH:mm:ss");
        cmbFmt->items.push_back(L"HH:mm");
        cmbFmt->selectedIndex = (m_config.timeDisplay.format == "HH:mm") ? 1 : 0;
    }

    // 休息提醒
    SetCheckButtonState("chkRemEn", m_config.reminder.enabled);
    easyUI.Text("chkRemEn", (m_config.reminder.enabled ? L"[√] " : L"[ ] ") + std::wstring(L"启用休息提醒"));
    easyUI.Text("txtRemInterval", std::to_wstring(m_config.reminder.intervalMinutes));
    easyUI.Text("txtRemMsg", PopupWindow::UTF8ToWide(m_config.reminder.message));

    // 后台监控
    SetCheckButtonState("chkMonEn", m_config.serverMonitor.backgroundEnabled);
    easyUI.Text("chkMonEn", (m_config.serverMonitor.backgroundEnabled ? L"[√] " : L"[ ] ") + std::wstring(L"启用后台监控"));
    easyUI.Text("txtMonInterval", std::to_wstring(m_config.serverMonitor.intervalSeconds));
    easyUI.Text("txtMonMaxPts", std::to_wstring(m_config.serverMonitor.maxDataPoints));

    // 游戏命令
    easyUI.Text("txtGameCmd", PopupWindow::UTF8ToWide(m_config.gameCommand));

    // 开机启动状态
    HKEY hKey;
    bool startup = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t path[MAX_PATH];
        DWORD size = sizeof(path);
        startup = (RegQueryValueExW(hKey, L"YMC-toolkit", NULL, NULL, (BYTE*)path, &size) == ERROR_SUCCESS);
        RegCloseKey(hKey);
    }
    SetCheckButtonState("chkStartup", startup);
    easyUI.Text("chkStartup", (startup ? L"[√] " : L"[ ] ") + std::wstring(L"开机启动"));
}

void SettingsWindow::SaveUItoConfig() {
    auto GetInt = [](const std::string& name) -> int {
        std::wstring str = easyUI.Text(name);
        try { return std::stoi(str); } catch (...) { return 0; }
    };

    m_config.popupWidth = GetInt("txtPopWidth");
    m_config.popupHeight = GetInt("txtPopHeight");
    m_config.edgeThreshold = GetInt("txtEdge");
    m_config.reminder.intervalMinutes = GetInt("txtRemInterval");
    m_config.reminder.message = PopupWindow::WideToUTF8(easyUI.Text("txtRemMsg"));
    m_config.serverMonitor.intervalSeconds = GetInt("txtMonInterval");
    m_config.serverMonitor.maxDataPoints = GetInt("txtMonMaxPts");
    m_config.gameCommand = PopupWindow::WideToUTF8(easyUI.Text("txtGameCmd"));

    auto* cmbFmt = dynamic_cast<Controls::ComboBox*>(Controls::FindControl("cmbTimeFmt"));
    if (cmbFmt && cmbFmt->selectedIndex >= 0 && cmbFmt->selectedIndex < (int)cmbFmt->items.size())
        m_config.timeDisplay.format = (cmbFmt->selectedIndex == 1) ? "HH:mm" : "HH:mm:ss";

    m_config.Save(m_configPath);
}

void SettingsWindow::SetCheckButtonState(const std::string& name, bool checked) {
    BtnStyle chkOn, chkOff;
    chkOn.bgNormal = Color(70, 130, 180);  chkOn.bgHover = Color(90, 150, 200);
    chkOn.border = Color(60, 100, 150);     chkOn.textColor = Color(255, 255, 255);
    chkOff.bgNormal = Color(80, 80, 80);   chkOff.bgHover = Color(100, 100, 100);
    chkOff.border = Color(100, 100, 100);   chkOff.textColor = Color(180, 180, 180);
    easyUI.SetBtnStyle(name, checked ? chkOn : chkOff);
}

// ---------- 服务器操作 ----------
void SettingsWindow::AddServer() {
    std::string host = "localhost", port = "25565";
    if (ShowInputDialog(detail::GS().hwnd, "添加服务器", "主机名/IP", "端口", host, port)) {
        m_config.servers.push_back({host, std::stoi(port)});
        LoadDataToUI();
    }
}
void SettingsWindow::EditServer() {
    int sel = easyUI.GetComboIndex("cmbServers");
    if (sel < 0 || sel >= (int)m_config.servers.size()) return;
    ServerInfo& sv = m_config.servers[sel];
    std::string host = sv.host, port = std::to_string(sv.port);
    if (ShowInputDialog(detail::GS().hwnd, "编辑服务器", "主机名/IP", "端口", host, port)) {
        sv.host = host;
        sv.port = std::stoi(port);
        LoadDataToUI();
    }
}
void SettingsWindow::DeleteServer() {
    int sel = easyUI.GetComboIndex("cmbServers");
    if (sel >= 0 && sel < (int)m_config.servers.size()) {
        m_config.servers.erase(m_config.servers.begin() + sel);
        if (m_config.currentServer >= (int)m_config.servers.size())
            m_config.currentServer = (int)m_config.servers.size() - 1;
        LoadDataToUI();
    }
}
void SettingsWindow::MoveServerUp() {
    int sel = easyUI.GetComboIndex("cmbServers");
    if (sel > 0) {
        std::swap(m_config.servers[sel], m_config.servers[sel - 1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel - 1;
        LoadDataToUI();
    }
}
void SettingsWindow::MoveServerDown() {
    int sel = easyUI.GetComboIndex("cmbServers");
    if (sel >= 0 && sel < (int)m_config.servers.size() - 1) {
        std::swap(m_config.servers[sel], m_config.servers[sel + 1]);
        if (m_config.currentServer == sel) m_config.currentServer = sel + 1;
        LoadDataToUI();
    }
}
void SettingsWindow::SetDefaultServer() {
    int sel = easyUI.GetComboIndex("cmbServers");
    if (sel >= 0) { m_config.currentServer = sel; LoadDataToUI(); }
}
void SettingsWindow::TestServer() {
    int sel = easyUI.GetComboIndex("cmbServers");
    if (sel < 0) return;
    ServerInfo& sv = m_config.servers[sel];
    ServerStatus status;
    if (PingServer(sv.host, sv.port, status)) {
        wchar_t msg[1024];
        swprintf(msg, 1024, L"在线\n玩家: %d/%d\n版本: %s\n延迟: %d ms",
            status.players, status.maxPlayers, PopupWindow::UTF8ToWide(status.version).c_str(), status.latency);
        MessageBoxW(detail::GS().hwnd, msg, L"测试结果", MB_OK);
    } else {
        MessageBoxW(detail::GS().hwnd, L"离线或无法连接", L"测试结果", MB_OK);
    }
}

// ---------- 快捷方式操作 ----------
void SettingsWindow::AddShortcut() {
    std::string name = "新快捷方式", url = "https://";
    if (ShowInputDialog(detail::GS().hwnd, "添加快捷方式", "名称", "URL", name, url)) {
        m_config.shortcuts.push_back({name, url});
        LoadDataToUI();
    }
}
void SettingsWindow::EditShortcut() {
    int sel = easyUI.GetComboIndex("cmbShortcuts");
    if (sel < 0) return;
    Shortcut& sc = m_config.shortcuts[sel];
    std::string name = sc.name, url = sc.url;
    if (ShowInputDialog(detail::GS().hwnd, "编辑快捷方式", "名称", "URL", name, url)) {
        sc.name = name; sc.url = url;
        LoadDataToUI();
    }
}
void SettingsWindow::DeleteShortcut() {
    int sel = easyUI.GetComboIndex("cmbShortcuts");
    if (sel >= 0) {
        m_config.shortcuts.erase(m_config.shortcuts.begin() + sel);
        LoadDataToUI();
    }
}
void SettingsWindow::BrowseGameCommand() {
    OPENFILENAMEW ofn = {};
    wchar_t fileName[MAX_PATH] = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = detail::GS().hwnd;
    ofn.lpstrFilter = L"可执行文件\0*.exe\0所有文件\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn))
        easyUI.Text("txtGameCmd", fileName);
}
void SettingsWindow::TestReminder() {
    ReminderWindow reminder;
    reminder.Show(PopupWindow::UTF8ToWide(m_config.reminder.message));
}
void SettingsWindow::ClearHistory() {
    PostMessage(m_hParent, WM_CLEAR_HISTORY, 0, 0);
    MessageBoxW(detail::GS().hwnd, L"历史数据已清除", L"提示", MB_OK);
}
void SettingsWindow::ResetConfig() {
    if (MessageBoxW(detail::GS().hwnd, L"重置所有配置到默认值？当前配置将丢失。", L"确认", MB_YESNO) == IDYES) {
        m_config = Config::FromJson(nlohmann::json::object());
        m_config.Save(m_configPath);
        LoadDataToUI();
    }
}
void SettingsWindow::BackupConfig() {
    wchar_t backupPath[MAX_PATH];
    GetModuleFileNameW(NULL, backupPath, MAX_PATH);
    wcscat_s(backupPath, L".backup.json");
    if (CopyFileW(PopupWindow::UTF8ToWide(m_configPath).c_str(), backupPath, FALSE))
        MessageBoxW(detail::GS().hwnd, L"备份成功", L"提示", MB_OK);
    else
        MessageBoxW(detail::GS().hwnd, L"备份失败", L"错误", MB_ICONERROR);
}
void SettingsWindow::RestoreConfig() {
    wchar_t backupPath[MAX_PATH];
    GetModuleFileNameW(NULL, backupPath, MAX_PATH);
    wcscat_s(backupPath, L".backup.json");
    if (CopyFileW(backupPath, PopupWindow::UTF8ToWide(m_configPath).c_str(), FALSE)) {
        m_config.Load(m_configPath);
        LoadDataToUI();
        MessageBoxW(detail::GS().hwnd, L"恢复成功", L"提示", MB_OK);
    } else {
        MessageBoxW(detail::GS().hwnd, L"未找到备份文件", L"错误", MB_ICONERROR);
    }
}
void SettingsWindow::SetStartup(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (enable)
            RegSetValueExW(hKey, L"YMC-toolkit", 0, REG_SZ, (BYTE*)exePath, (wcslen(exePath) + 1) * sizeof(wchar_t));
        else
            RegDeleteValueW(hKey, L"YMC-toolkit");
        RegCloseKey(hKey);
    }
}

LRESULT CALLBACK SettingsWindow::SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* pThis = (SettingsWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    WNDPROC oldProc = (WNDPROC)GetPropW(hwnd, L"OLD_PROC");
    if (msg == WM_CLOSE) {
        if (pThis) pThis->SaveUItoConfig();
        PostMessage(pThis ? pThis->m_hParent : NULL, WM_CONFIG_UPDATED, 0, 0);
    }
    return CallWindowProc(oldProc, hwnd, msg, wParam, lParam);
}