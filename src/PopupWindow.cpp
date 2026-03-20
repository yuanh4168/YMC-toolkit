#include "PopupWindow.h"
#include <shellapi.h>
#include <cstdio>

// 辅助函数：设置窗口圆角
static void SetRoundedCorners(HWND hWnd, int radius) {
    RECT rc;
    GetWindowRect(hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, radius, radius);
    SetWindowRgn(hWnd, hRgn, TRUE);
}

std::wstring PopupWindow::UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], len);
    return wstr;
}

PopupWindow::PopupWindow() 
    : m_hWnd(NULL), m_hServerStatic(NULL), m_hBkBrush(NULL), 
      m_hHoverButton(NULL), m_hNormalFont(NULL), m_hBoldFont(NULL),
      m_bTracking(false) {
    for (int i = 0; i < 4; ++i) m_hShortcutButtons[i] = NULL;
}

PopupWindow::~PopupWindow() {
    if (m_hWnd) DestroyWindow(m_hWnd);
    if (m_hBkBrush) DeleteObject(m_hBkBrush);
    if (m_hNormalFont) DeleteObject(m_hNormalFont);
    if (m_hBoldFont) DeleteObject(m_hBoldFont);
}

bool PopupWindow::Create(HWND hParent, HINSTANCE hInst, const Config& cfg) {
    m_config = cfg;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"PopupClass";
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"PopupClass", NULL,
        WS_POPUP | WS_BORDER,
        0, 0, cfg.popupWidth, cfg.popupHeight,
        hParent, NULL, hInst, this);
    if (!m_hWnd) return false;

    m_hBkBrush = CreateSolidBrush(RGB(32, 32, 32));

    // 创建字体
    LOGFONTW lf = {0};
    lf.lfHeight = -14;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, L"MS Shell Dlg");
    m_hNormalFont = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_BOLD;
    m_hBoldFont = CreateFontIndirectW(&lf);

    // 创建服务器状态标签
    CreateWindowW(L"STATIC", L"服务器状态", WS_CHILD | WS_VISIBLE,
        10, 10, 380, 20, m_hWnd, NULL, hInst, NULL);
    m_hServerStatic = CreateWindowW(L"STATIC", L"未知", WS_CHILD | WS_VISIBLE,
        10, 30, 380, 60, m_hWnd, (HMENU)IDC_SERVER_STATUS, hInst, NULL);
    SendMessageW(m_hServerStatic, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);

    // 创建四个快捷按钮
    int btnWidth = (cfg.popupWidth - 50) / 4;
    for (int i = 0; i < 4 && i < (int)cfg.shortcuts.size(); ++i) {
        m_hShortcutButtons[i] = CreateWindowW(
            L"BUTTON",
            UTF8ToWide(cfg.shortcuts[i].name).c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            10 + i * (btnWidth + 10), 100, btnWidth, 30,
            m_hWnd, (HMENU)(IDC_SHORTCUT1 + i), hInst, NULL);
        SendMessageW(m_hShortcutButtons[i], WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
    }

    // 创建启动游戏按钮
    HWND hLaunch = CreateWindowW(L"BUTTON", L"启动游戏", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        150, 150, 100, 30, m_hWnd, (HMENU)IDC_LAUNCH_BUTTON, hInst, NULL);
    SendMessageW(hLaunch, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);

    // 创建退出按钮
    HWND hExit = CreateWindowW(L"BUTTON", L"退出", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        cfg.popupWidth - 80, cfg.popupHeight - 40, 70, 30,
        m_hWnd, (HMENU)IDC_EXIT_BUTTON, hInst, NULL);
    SendMessageW(hExit, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);

    // 设置窗口圆角
    SetRoundedCorners(m_hWnd, 20);

    return true;
}

void PopupWindow::Show() {
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    SetWindowPos(m_hWnd, HWND_TOPMOST,
        (screenWidth - m_config.popupWidth) / 2, 0,
        m_config.popupWidth, m_config.popupHeight,
        SWP_SHOWWINDOW);
    m_hHoverButton = NULL;
    m_bTracking = false;
}

void PopupWindow::Hide() {
    ShowWindow(m_hWnd, SW_HIDE);
}

void PopupWindow::UpdateServerStatus(const ServerStatus& status) {
    if (!m_hServerStatic) return;
    std::wstring text;
    if (status.online) {
        wchar_t buf[1024];
        swprintf(buf, 1024, L"在线 - %s\n%d/%d 玩家\n版本: %s\n延迟: %d ms",
            UTF8ToWide(status.motd).c_str(),
            status.players, status.maxPlayers,
            UTF8ToWide(status.version).c_str(),
            status.latency);
        text = buf;
    } else {
        text = L"离线";
    }
    SetWindowTextW(m_hServerStatic, text.c_str());
}

bool PopupWindow::IsButton(HWND hWnd) {
    for (int i = 0; i < 4; ++i) {
        if (hWnd == m_hShortcutButtons[i]) return true;
    }
    HWND hLaunch = GetDlgItem(m_hWnd, IDC_LAUNCH_BUTTON);
    HWND hExit = GetDlgItem(m_hWnd, IDC_EXIT_BUTTON);
    return (hWnd == hLaunch || hWnd == hExit);
}

void PopupWindow::OnMouseMove(WPARAM wParam, LPARAM lParam) {
    POINT pt = { LOWORD(lParam), HIWORD(lParam) };
    HWND hChild = ChildWindowFromPoint(m_hWnd, pt);
    if (IsButton(hChild)) {
        SetHoverButton(hChild);
        if (!m_bTracking) {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hWnd, 0 };
            TrackMouseEvent(&tme);
            m_bTracking = true;
        }
    } else {
        SetHoverButton(NULL);
    }
}

void PopupWindow::SetHoverButton(HWND hBtn) {
    if (m_hHoverButton == hBtn) return;
    HWND oldHover = m_hHoverButton;
    m_hHoverButton = hBtn;
    if (oldHover) InvalidateRect(oldHover, NULL, TRUE);
    if (m_hHoverButton) InvalidateRect(m_hHoverButton, NULL, TRUE);
}

LRESULT CALLBACK PopupWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PopupWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (PopupWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (PopupWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (!pThis) return DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg) {
        case WM_ERASEBKGND: {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hWnd, &rc);
            FillRect(hdc, &rc, pThis->m_hBkBrush);
            return TRUE;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, RGB(240, 240, 240));
            SetBkColor(hdcStatic, RGB(32, 32, 32));
            return (LRESULT)pThis->m_hBkBrush;
        }
        case WM_MOUSEMOVE:
            pThis->OnMouseMove(wParam, lParam);
            break;
        case WM_MOUSELEAVE:
            pThis->SetHoverButton(NULL);
            pThis->m_bTracking = false;
            break;
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
            if (lpDIS->CtlType == ODT_BUTTON) {
                wchar_t text[256];
                GetWindowTextW(lpDIS->hwndItem, text, 256);
                HDC hdc = lpDIS->hDC;
                SetBkMode(hdc, TRANSPARENT);

                bool bHover = (lpDIS->hwndItem == pThis->m_hHoverButton);
                bool bPressed = (lpDIS->itemState & ODS_SELECTED);

                COLORREF bgColor = bPressed ? RGB(60, 60, 60) : RGB(45, 45, 45);
                HBRUSH bgBrush = CreateSolidBrush(bgColor);
                FillRect(hdc, &lpDIS->rcItem, bgBrush);
                DeleteObject(bgBrush);

                COLORREF borderColor = bHover ? RGB(200, 200, 200) : RGB(80, 80, 80);
                HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
                SelectObject(hdc, pen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                // 绘制圆角矩形
                RoundRect(hdc, lpDIS->rcItem.left, lpDIS->rcItem.top,
                          lpDIS->rcItem.right, lpDIS->rcItem.bottom, 10, 10);
                DeleteObject(pen);

                COLORREF textColor = bHover ? RGB(255, 200, 0) : RGB(220, 220, 220);
                SetTextColor(hdc, textColor);

                HFONT hOldFont = (HFONT)SelectObject(hdc, bHover ? pThis->m_hBoldFont : pThis->m_hNormalFont);
                DrawTextW(hdc, text, -1, &lpDIS->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, hOldFont);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= IDC_SHORTCUT1 && id <= IDC_SHORTCUT4) {
                int index = id - IDC_SHORTCUT1;
                if (index < (int)pThis->m_config.shortcuts.size()) {
                    const std::string& url = pThis->m_config.shortcuts[index].url;
                    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                }
                break;
            } else if (id == IDC_LAUNCH_BUTTON) {
                SendMessage(GetParent(hWnd), WM_COMMAND, IDC_LAUNCH_BUTTON, 0);
            } else if (id == IDC_EXIT_BUTTON) {
                PostQuitMessage(0);
            }
            break;
        }
        case WM_DESTROY:
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}