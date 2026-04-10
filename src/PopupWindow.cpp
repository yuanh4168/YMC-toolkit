#include "PopupWindow.h"
#include "ToolWindow.h"
#include "SettingsWindow.h"
#include "StatsWindow.h"
#include "DPIHelper.h"
#include <shellapi.h>
#include <cstdio>
#include <commctrl.h>
#include <gdiplus.h>
#include <wincrypt.h>
#include <map>
#include <ctime>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Gdiplus;

static Color ColorFromCOLORREF(COLORREF cr) {
    return Color(GetRValue(cr), GetGValue(cr), GetBValue(cr));
}

std::wstring PopupWindow::UTF8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    if (len == 0) return std::wstring();
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wstr[0], len);
    return wstr;
}

std::string PopupWindow::WideToUTF8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), NULL, 0, NULL, NULL);
    if (len == 0) return std::string();
    std::string utf8(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &utf8[0], len, NULL, NULL);
    return utf8;
}

LRESULT CALLBACK PopupWindow::ButtonSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData) {
    PopupWindow* pThis = reinterpret_cast<PopupWindow*>(dwRefData);
    if (!pThis) return DefSubclassProc(hWnd, msg, wParam, lParam);
    switch (msg) {
        case WM_MOUSEMOVE:
            PostMessage(pThis->m_hWnd, WM_UPDATE_HOVER, (WPARAM)hWnd, 0);
            {
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tme);
            }
            break;
        case WM_MOUSELEAVE:
            PostMessage(pThis->m_hWnd, WM_UPDATE_HOVER, 0, 0);
            break;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

PopupWindow::PopupWindow() 
    : m_hWnd(NULL), m_hServerAddressStatic(NULL), m_hServerStatusStatic(NULL),
      m_hBkBrush(NULL), m_hHoverButton(NULL), m_hNormalFont(NULL), m_hBoldFont(NULL),
      m_hExitButton(NULL), m_hSwitchButton(NULL), m_hToolButton(NULL), m_hStatsButton(NULL),
      m_hSettingsButton(NULL), m_hTimeStatic(NULL), m_lastX(0), m_autoHideScheduled(false),
      m_hFaviconStatic(NULL), m_pFaviconBitmap(NULL), m_gdiplusToken(0),
      m_pGdiNormalFont(NULL), m_pGdiBoldFont(NULL) {
    for (int i = 0; i < 4; ++i) m_hShortcutButtons[i] = NULL;
}

PopupWindow::~PopupWindow() {
    if (m_pFaviconBitmap) delete m_pFaviconBitmap;
    if (m_pGdiNormalFont) delete m_pGdiNormalFont;
    if (m_pGdiBoldFont) delete m_pGdiBoldFont;
    if (m_hWnd) DestroyWindow(m_hWnd);
    if (m_hBkBrush) DeleteObject(m_hBkBrush);
    if (m_hNormalFont) DeleteObject(m_hNormalFont);
    if (m_hBoldFont) DeleteObject(m_hBoldFont);
    if (m_gdiplusToken) GdiplusShutdown(m_gdiplusToken);
}

bool PopupWindow::Create(HWND hParent, HINSTANCE hInst, const Config& cfg) {
    m_config = cfg;
    double scale = GetDPIScale();

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
        WS_POPUP,
        0, 0, cfg.popupWidth, cfg.popupHeight,
        hParent, NULL, hInst, this);
    if (!m_hWnd) return false;

    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    m_hBkBrush = CreateSolidBrush(RGB(32, 32, 32));

    HDC hdc = GetDC(NULL);
    int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    int fontSize = -MulDiv(14, dpiX, 96);
    LOGFONTW lf = {0};
    lf.lfHeight = fontSize;
    lf.lfWeight = FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
    m_hNormalFont = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_BOLD;
    m_hBoldFont = CreateFontIndirectW(&lf);
    m_pGdiNormalFont = new Font(hdc, m_hNormalFont);
    m_pGdiBoldFont   = new Font(hdc, m_hBoldFont);

    int x10 = (int)(10 * scale);
    int x40 = (int)(40 * scale);
    int x150 = (int)(150 * scale);
    int x260 = (int)(260 * scale);
    int x320 = (int)(320 * scale);
    int width380 = (int)(380 * scale);
    int width310 = (int)(310 * scale);
    int width32 = (int)(64 * scale);
    int height20 = (int)(20 * scale);
    int height30 = (int)(30 * scale);
    int height90 = (int)(90 * scale);
    int y10 = (int)(10 * scale);
    int y30 = (int)(30 * scale);
    int y60 = (int)(60 * scale);
    int y80 = (int)(80 * scale);
    int y190 = (int)(190 * scale);
    int y230 = (int)(230 * scale);
    int y270 = (int)(270 * scale);

    CreateWindowW(L"STATIC", L"当前服务器", WS_CHILD | WS_VISIBLE,
        x10, y10, width380, height20, m_hWnd, NULL, hInst, NULL);
    m_hServerAddressStatic = CreateWindowW(L"STATIC", L"未知", WS_CHILD | WS_VISIBLE,
        x10, y30, width380, height20, m_hWnd, (HMENU)IDC_SERVER_STATUS, hInst, NULL);
    SendMessageW(m_hServerAddressStatic, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);

    CreateWindowW(L"STATIC", L"服务器状态", WS_CHILD | WS_VISIBLE,
        x10, y60, width380, height20, m_hWnd, NULL, hInst, NULL);
    m_hServerStatusStatic = CreateWindowW(L"STATIC", L"检测中...", WS_CHILD | WS_VISIBLE,
        x10, y80, width310, height90, m_hWnd, (HMENU)(IDC_SERVER_STATUS + 1), hInst, NULL);
    SendMessageW(m_hServerStatusStatic, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);

    m_hFaviconStatic = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP,
        x320, y80, width32, width32, m_hWnd, NULL, hInst, NULL);
    SendMessageW(m_hFaviconStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

    // ========== 根据配置创建按钮 ==========
    auto getRect = [&](const std::string& id, RECT& rc) -> bool {
        for (const auto& br : m_config.buttonRects) {
            if (br.id == id) {
                rc.left = br.left;
                rc.top = br.top;
                rc.right = br.right;
                rc.bottom = br.bottom;
                return true;
            }
        }
        return false;
    };

    // 快捷按钮
    for (int i = 0; i < 4 && i < (int)m_config.shortcuts.size(); ++i) {
        std::string id = "shortcut" + std::to_string(i + 1);
        RECT rc = {0,0,0,0};
        if (!getRect(id, rc)) {
            int btnWidth = (m_config.popupWidth - (int)(50 * scale)) / 4;
            rc.left = x10 + i * (btnWidth + x10);
            rc.top = y190;
            rc.right = rc.left + btnWidth;
            rc.bottom = rc.top + height30;
        }
        m_hShortcutButtons[i] = CreateWindowW(
            L"BUTTON",
            UTF8ToWide(m_config.shortcuts[i].name).c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)(IDC_SHORTCUT1 + i), hInst, NULL);
        SendMessageW(m_hShortcutButtons[i], WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(m_hShortcutButtons[i], ButtonSubclassProc, 0, (DWORD_PTR)this);
    }

    // 启动游戏按钮
    {
        RECT rc = {0,0,0,0};
        if (!getRect("launch", rc)) {
            rc.left = x150;
            rc.top = y230;
            rc.right = rc.left + (int)(100 * scale);
            rc.bottom = rc.top + height30;
        }
        HWND hLaunch = CreateWindowW(L"BUTTON", L"启动游戏", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)IDC_LAUNCH_BUTTON, hInst, NULL);
        SendMessageW(hLaunch, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(hLaunch, ButtonSubclassProc, 0, (DWORD_PTR)this);
    }

    // 切换服务器按钮
    {
        RECT rc = {0,0,0,0};
        if (!getRect("switch", rc)) {
            rc.left = x260;
            rc.top = y230;
            rc.right = rc.left + (int)(100 * scale);
            rc.bottom = rc.top + height30;
        }
        m_hSwitchButton = CreateWindowW(L"BUTTON", L"切换服务器", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)IDC_SWITCH_BUTTON, hInst, NULL);
        SendMessageW(m_hSwitchButton, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(m_hSwitchButton, ButtonSubclassProc, 0, (DWORD_PTR)this);
    }

    // 工具箱按钮
    {
        RECT rc = {0,0,0,0};
        if (!getRect("tool", rc)) {
            rc.left = x40;
            rc.top = y270;
            rc.right = rc.left + (int)(100 * scale);
            rc.bottom = rc.top + height30;
        }
        m_hToolButton = CreateWindowW(L"BUTTON", L"工具箱", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)IDC_TOOL_BUTTON, hInst, NULL);
        SendMessageW(m_hToolButton, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(m_hToolButton, ButtonSubclassProc, 0, (DWORD_PTR)this);
    }

    // 设置按钮（新增）
    {
        RECT rc = {0,0,0,0};
        if (!getRect("settings", rc)) {
            rc.left = x150;
            rc.top = y270;
            rc.right = rc.left + (int)(100 * scale);
            rc.bottom = rc.top + height30;
        }
        m_hSettingsButton = CreateWindowW(L"BUTTON", L"设置", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)IDC_SETTINGS_BUTTON, hInst, NULL);
        SendMessageW(m_hSettingsButton, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(m_hSettingsButton, ButtonSubclassProc, 0, (DWORD_PTR)this);
    }

    // 退出按钮
    {
        RECT rc = {0,0,0,0};
        if (!getRect("exit", rc)) {
            rc.left = m_config.popupWidth - (int)(30 * scale);
            rc.top = 0;
            rc.right = m_config.popupWidth;
            rc.bottom = (int)(30 * scale);
        }
        m_hExitButton = CreateWindowW(L"BUTTON", L"×", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)IDC_EXIT_BUTTON, hInst, NULL);
        SendMessageW(m_hExitButton, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(m_hExitButton, ButtonSubclassProc, 0, (DWORD_PTR)this);
    }

    // 统计按钮
    {
        RECT rc = {0,0,0,0};
        if (!getRect("stats", rc)) {
            rc.left = x320 + width32 + (int)(10 * scale);
            rc.top = y80 + (int)(10 * scale);
            rc.right = rc.left + (int)(60 * scale);
            rc.bottom = rc.top + height30;
        }
        m_hStatsButton = CreateWindowW(L"BUTTON", L"统计", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)IDC_STATS_BUTTON, hInst, NULL);
        SendMessageW(m_hStatsButton, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(m_hStatsButton, ButtonSubclassProc, 0, (DWORD_PTR)this);
    }

    // 时间显示
    if (m_config.timeDisplay.enabled) {
        RECT rc = {0,0,0,0};
        if (!getRect("time_display", rc)) {
            rc.left = m_config.popupWidth - (int)(100 * scale);
            rc.top = (int)(5 * scale);
            rc.right = rc.left + (int)(80 * scale);
            rc.bottom = rc.top + (int)(25 * scale);
        }
        m_hTimeStatic = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            m_hWnd, (HMENU)IDC_TIME_STATIC, hInst, NULL);
        SendMessageW(m_hTimeStatic, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        SetWindowSubclass(m_hTimeStatic, ButtonSubclassProc, 0, (DWORD_PTR)this);
        SetTimer(m_hWnd, 101, 1000, NULL);
        UpdateTimeDisplay();
    }

    // 统一字体
    for (HWND hChild = GetWindow(m_hWnd, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
        SendMessage(hChild, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
    }

    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
    TrackMouseEvent(&tme);
    return true;
}

void PopupWindow::UpdateTimeDisplay() {
    if (!m_hTimeStatic) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    if (m_config.timeDisplay.format == "HH:mm:ss")
        swprintf(buf, 64, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    else if (m_config.timeDisplay.format == "HH:mm")
        swprintf(buf, 64, L"%02d:%02d", st.wHour, st.wMinute);
    else
        swprintf(buf, 64, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    SetWindowTextW(m_hTimeStatic, buf);
}

void PopupWindow::Show() {
    if (IsWindowVisible(m_hWnd)) return;
    int x = (m_lastX != 0) ? m_lastX : (GetSystemMetrics(SM_CXSCREEN) - m_config.popupWidth) / 2;
    SetWindowPos(m_hWnd, HWND_TOPMOST, x, 0, m_config.popupWidth, m_config.popupHeight, SWP_NOZORDER);
    AnimateWindow(m_hWnd, 200, AW_SLIDE | AW_VER_POSITIVE | AW_ACTIVATE);
    m_hHoverButton = NULL;
    CancelAutoHide();
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
    TrackMouseEvent(&tme);
}

void PopupWindow::Hide() {
    if (!IsWindowVisible(m_hWnd)) return;
    UpdateLastX();
    AnimateWindow(m_hWnd, 200, AW_SLIDE | AW_VER_NEGATIVE | AW_HIDE);
    CancelAutoHide();
}

void PopupWindow::UpdateLastX() {
    RECT rc;
    GetWindowRect(m_hWnd, &rc);
    m_lastX = rc.left;
}

void PopupWindow::AdhereToTop() {
    if (!IsWindowVisible(m_hWnd)) return;
    RECT rc;
    GetWindowRect(m_hWnd, &rc);
    int x = rc.left;
    SetWindowPos(m_hWnd, HWND_TOPMOST, x, 0, m_config.popupWidth, m_config.popupHeight, SWP_NOZORDER);
    UpdateLastX();
    ScheduleAutoHide();
}

void PopupWindow::ScheduleAutoHide() {
    if (m_autoHideScheduled) return;
    SetTimer(m_hWnd, 100, 2000, NULL);
    m_autoHideScheduled = true;
}

void PopupWindow::CancelAutoHide() {
    if (m_autoHideScheduled) {
        KillTimer(m_hWnd, 100);
        m_autoHideScheduled = false;
    }
}

void PopupWindow::OnAutoHideTimer() {
    KillTimer(m_hWnd, 100);
    m_autoHideScheduled = false;
    if (IsWindowVisible(m_hWnd)) {
        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(m_hWnd, &rc);
        if (!PtInRect(&rc, pt)) {
            Hide();
        }
    }
}

void PopupWindow::SetCurrentServerInfo() {
    if (!m_hServerAddressStatic || !m_hServerStatusStatic) return;
    if (!m_config.servers.empty()) {
        int idx = m_config.currentServer;
        std::string addr = m_config.servers[idx].host + ":" + std::to_string(m_config.servers[idx].port);
        SetWindowTextW(m_hServerAddressStatic, UTF8ToWide(addr).c_str());
        SetWindowTextW(m_hServerStatusStatic, L"检测中...");
    } else {
        SetWindowTextW(m_hServerAddressStatic, L"未配置服务器");
        SetWindowTextW(m_hServerStatusStatic, L"");
    }
}

void PopupWindow::UpdateServerStatus(const ServerStatus& status) {
    if (m_hServerStatusStatic) {
        std::wstring text;
        if (status.online) {
            wchar_t buf[2048];
            swprintf(buf, 2048, L"在线 - %s\n%d/%d 玩家\n版本: %s\n延迟: %d ms",
                UTF8ToWide(status.motd).c_str(),
                status.players, status.maxPlayers,
                UTF8ToWide(status.version).c_str(),
                status.latency);
            text = buf;
            if (!status.mods.empty()) {
                text += L"\n模组: ";
                for (size_t i = 0; i < status.mods.size() && i < 3; ++i) {
                    if (i > 0) text += L", ";
                    text += UTF8ToWide(status.mods[i].modid);
                }
                if (status.mods.size() > 3) text += L" ...";
            }
        } else {
            text = L"离线";
        }
        SetWindowTextW(m_hServerStatusStatic, text.c_str());
    }

    if (m_hFaviconStatic) {
        if (m_pFaviconBitmap) {
            delete m_pFaviconBitmap;
            m_pFaviconBitmap = NULL;
        }
        if (!status.favicon_base64.empty()) {
            m_pFaviconBitmap = Base64ToBitmap(status.favicon_base64);
            if (m_pFaviconBitmap) {
                HBITMAP hBitmap = NULL;
                m_pFaviconBitmap->GetHBITMAP(Color(0,0,0), &hBitmap);
                if (hBitmap) {
                    SendMessageW(m_hFaviconStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);
                }
            }
        } else {
            SendMessageW(m_hFaviconStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
        }
    }
}

void PopupWindow::SyncCurrentServerIndex(int idx) {
    if (idx >= 0 && idx < (int)m_config.servers.size()) {
        m_config.currentServer = idx;
        SetCurrentServerInfo();
    }
}

bool PopupWindow::IsButton(HWND hWnd) {
    for (int i = 0; i < 4; ++i) {
        if (hWnd == m_hShortcutButtons[i]) return true;
    }
    HWND hLaunch = GetDlgItem(m_hWnd, IDC_LAUNCH_BUTTON);
    return (hWnd == hLaunch || hWnd == m_hExitButton || hWnd == m_hSwitchButton || 
            hWnd == m_hToolButton || hWnd == m_hStatsButton || hWnd == m_hSettingsButton);
}

void PopupWindow::SetHoverButton(HWND hBtn) {
    if (m_hHoverButton == hBtn) return;
    HWND oldHover = m_hHoverButton;
    m_hHoverButton = hBtn;
    if (oldHover) InvalidateRect(oldHover, NULL, TRUE);
    if (m_hHoverButton) InvalidateRect(m_hHoverButton, NULL, TRUE);
}

Gdiplus::Bitmap* PopupWindow::CreateBitmapFromData(const BYTE* data, size_t len) {
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hGlobal) return NULL;
    void* pMem = GlobalLock(hGlobal);
    if (!pMem) {
        GlobalFree(hGlobal);
        return NULL;
    }
    memcpy(pMem, data, len);
    GlobalUnlock(hGlobal);
    IStream* pStream = NULL;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) != S_OK) {
        GlobalFree(hGlobal);
        return NULL;
    }
    Gdiplus::Bitmap* pBitmap = Gdiplus::Bitmap::FromStream(pStream);
    pStream->Release();
    return pBitmap;
}

Gdiplus::Bitmap* PopupWindow::Base64ToBitmap(const std::string& base64Data) {
    size_t commaPos = base64Data.find(',');
    std::string data = (commaPos != std::string::npos) ? base64Data.substr(commaPos + 1) : base64Data;
    DWORD decodedLen = 0;
    if (!CryptStringToBinaryA(data.c_str(), data.size(), CRYPT_STRING_BASE64, NULL, &decodedLen, NULL, NULL)) {
        return NULL;
    }
    std::vector<BYTE> decoded(decodedLen);
    if (!CryptStringToBinaryA(data.c_str(), data.size(), CRYPT_STRING_BASE64, decoded.data(), &decodedLen, NULL, NULL)) {
        return NULL;
    }
    return CreateBitmapFromData(decoded.data(), decodedLen);
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
        case WM_UPDATE_HOVER: {
            HWND hBtn = (HWND)wParam;
            pThis->SetHoverButton(hBtn);
            return 0;
        }
        case WM_NCHITTEST: {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            ScreenToClient(hWnd, &pt);
            double scale = GetDPIScale();
            if (pt.y < (int)(30 * scale)) {
                return HTCAPTION;
            }
            break;
        }
        case WM_MOVING: {
            RECT* pRect = (RECT*)lParam;
            int height = pRect->bottom - pRect->top;
            pRect->top = 0;
            pRect->bottom = height;
            return TRUE;
        }
        case WM_EXITSIZEMOVE: {
            pThis->UpdateLastX();
            break;
        }
        case WM_MOUSELEAVE: {
            pThis->SetHoverButton(NULL);
            pThis->AdhereToTop();
            break;
        }
        case WM_TIMER:
            if (wParam == 100) {
                pThis->OnAutoHideTimer();
            } else if (wParam == 101) {
                pThis->UpdateTimeDisplay();
            }
            break;
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
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpDIS = (LPDRAWITEMSTRUCT)lParam;
            if (lpDIS->CtlType == ODT_BUTTON) {
                HDC hdc = lpDIS->hDC;
                wchar_t text[256];
                GetWindowTextW(lpDIS->hwndItem, text, 256);

                bool bHover = (lpDIS->hwndItem == pThis->m_hHoverButton);
                bool bPressed = (lpDIS->itemState & ODS_SELECTED);

                COLORREF bgColor;
                if (lpDIS->hwndItem == pThis->m_hExitButton) {
                    if (bPressed) bgColor = RGB(40, 20, 20);
                    else if (bHover) bgColor = RGB(60, 30, 30);
                    else bgColor = RGB(80, 40, 40);
                } else {
                    if (bPressed) bgColor = RGB(30, 30, 30);
                    else if (bHover) bgColor = RGB(40, 40, 40);
                    else bgColor = RGB(60, 60, 60);
                }

                RECT rcItem = lpDIS->rcItem;
                int width = rcItem.right - rcItem.left;
                int height = rcItem.bottom - rcItem.top;

                HBRUSH hBgBrush = CreateSolidBrush(bgColor);
                FillRect(hdc, &rcItem, hBgBrush);
                DeleteObject(hBgBrush);

                HPEN hBorderPen = CreatePen(PS_SOLID, 1, bHover ? RGB(200, 200, 200) : RGB(80, 80, 80));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hBorderPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, rcItem.left, rcItem.top, rcItem.right, rcItem.bottom);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hBorderPen);

                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, bHover ? RGB(255, 220, 100) : RGB(220, 220, 220));
                HFONT hOldFont = (HFONT)SelectObject(hdc, bHover ? pThis->m_hBoldFont : pThis->m_hNormalFont);

                SIZE textSize;
                GetTextExtentPoint32W(hdc, text, wcslen(text), &textSize);
                int x = rcItem.left + (width - textSize.cx) / 2;
                int y = rcItem.top + (height - textSize.cy) / 2;
                if (x < rcItem.left) x = rcItem.left;
                if (y < rcItem.top) y = rcItem.top;
                ExtTextOutW(hdc, x, y, ETO_CLIPPED, &rcItem, text, wcslen(text), NULL);

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
            } else if (id == IDC_LAUNCH_BUTTON) {
                SendMessage(GetParent(hWnd), WM_COMMAND, IDC_LAUNCH_BUTTON, 0);
            } else if (id == IDC_EXIT_BUTTON) {
                PostQuitMessage(0);
            } else if (id == IDC_SWITCH_BUTTON) {
                SendMessage(GetParent(hWnd), WM_COMMAND, IDC_SWITCH_BUTTON, 0);
            } else if (id == IDC_TOOL_BUTTON) {
                ToolWindow toolWnd;
                toolWnd.Show(pThis->m_hWnd, (HINSTANCE)GetWindowLongPtr(pThis->m_hWnd, GWLP_HINSTANCE));
            } else if (id == IDC_STATS_BUTTON) {
                SendMessage(GetParent(hWnd), WM_COMMAND, IDC_STATS_BUTTON, 0);
            } else if (id == IDC_SETTINGS_BUTTON) {
                SendMessage(GetParent(hWnd), WM_COMMAND, IDC_SETTINGS_BUTTON, 0);
            }
            break;
        }
        case WM_DESTROY:
            break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
void PopupWindow::ReloadConfig(const Config& newCfg)
{
    m_config = newCfg;  // 更新内部配置副本

    // 如果时间显示启用状态或格式变化，刷新显示
    if (m_hTimeStatic) {
        if (m_config.timeDisplay.enabled) {
            ShowWindow(m_hTimeStatic, SW_SHOW);
            UpdateTimeDisplay();
        } else {
            ShowWindow(m_hTimeStatic, SW_HIDE);
        }
    }

    // 注意：弹窗尺寸（popupWidth/Height）变化需要销毁重建才能完全生效，
    // 为简化，此处仅做配置同步。用户可通过重启程序使新尺寸生效。
    SetCurrentServerInfo();  // 确保服务器地址显示正确
}