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
    m_hParent = hParent;
    m_hInst = hInst;
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
        0, 0, m_config.popupWidth, m_config.popupHeight,
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

    // 辅助函数：从配置中获取按钮矩形（逻辑像素），若不存在则返回 false
    auto getButtonRect = [&](const std::string& id, RECT& rect) -> bool {
        for (const auto& br : m_config.buttonRects) {
            if (br.id == id) {
                rect.left = br.left;
                rect.top = br.top;
                rect.right = br.right;
                rect.bottom = br.bottom;
                return true;
            }
        }
        return false;
    };

    // 通用按钮创建函数，优先使用配置坐标，否则使用默认坐标（默认坐标需由调用者提供）
    auto createButton = [&](const std::string& id, const std::wstring& text, int cmdId, 
                            const RECT& defaultRect, DWORD extraStyle = 0) -> HWND {
        RECT rect = defaultRect;
        if (getButtonRect(id, rect)) {
            // 使用配置坐标（逻辑像素），不进行任何适配检查
        }
        // 应用 DPI 缩放
        int x = (int)(rect.left * scale);
        int y = (int)(rect.top * scale);
        int w = (int)((rect.right - rect.left) * scale);
        int h = (int)((rect.bottom - rect.top) * scale);
        HWND hBtn = CreateWindowW(L"BUTTON", text.c_str(),
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | extraStyle,
            x, y, w, h,
            m_hWnd, (HMENU)cmdId, hInst, NULL);
        if (hBtn) {
            SendMessageW(hBtn, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
            SetWindowSubclass(hBtn, ButtonSubclassProc, 0, (DWORD_PTR)this);
        }
        return hBtn;
    };

    // 静态文本创建函数（同样支持配置坐标，但静态文本通常不需要，为了完整也支持）
    auto createStatic = [&](const std::string& id, const std::wstring& text, int cmdId,
                            const RECT& defaultRect) -> HWND {
        RECT rect = defaultRect;
        if (getButtonRect(id, rect)) {
            // 使用配置坐标
        }
        int x = (int)(rect.left * scale);
        int y = (int)(rect.top * scale);
        int w = (int)((rect.right - rect.left) * scale);
        int h = (int)((rect.bottom - rect.top) * scale);
        HWND hStatic = CreateWindowW(L"STATIC", text.c_str(),
            WS_CHILD | WS_VISIBLE,
            x, y, w, h,
            m_hWnd, (HMENU)cmdId, hInst, NULL);
        if (hStatic) {
            SendMessageW(hStatic, WM_SETFONT, (WPARAM)m_hNormalFont, TRUE);
        }
        return hStatic;
    };

    // ========== 创建控件，优先使用 config 中的坐标 ==========

    // 1. "当前服务器" 标签（默认位置：左上角）
    RECT defaultLabelRect = {10, 10, m_config.popupWidth - 10, 30};
    createStatic("label_current_server", L"当前服务器", 0, defaultLabelRect);

    // 2. 服务器地址（默认位置：标签下方）
    RECT defaultAddrRect = {10, 30, m_config.popupWidth - 10, 50};
    m_hServerAddressStatic = createStatic("server_address", L"未知", IDC_SERVER_STATUS, defaultAddrRect);

    // 3. "服务器状态" 标签
    RECT defaultStatusLabelRect = {10, 60, m_config.popupWidth - 10, 80};
    createStatic("label_server_status", L"服务器状态", 0, defaultStatusLabelRect);

    // 4. 服务器状态文本区域（默认位置：标签下方，预留高度 90）
    RECT defaultStatusRect = {10, 80, m_config.popupWidth - 10 - 64 - 10, 170};  // 右侧留出图标空间
    m_hServerStatusStatic = createStatic("server_status_text", L"检测中...", IDC_SERVER_STATUS + 1, defaultStatusRect);

    // 5. Favicon 图标（默认位置：右上角状态区域右侧）
    RECT defaultFaviconRect = {m_config.popupWidth - 64 - 10, 80, m_config.popupWidth - 10, 80 + 64};
    if (getButtonRect("favicon", defaultFaviconRect)) {
        // 如果配置中有 favicon 矩形，则使用
    }
    int fx = (int)(defaultFaviconRect.left * scale);
    int fy = (int)(defaultFaviconRect.top * scale);
    int fw = (int)((defaultFaviconRect.right - defaultFaviconRect.left) * scale);
    int fh = (int)((defaultFaviconRect.bottom - defaultFaviconRect.top) * scale);
    m_hFaviconStatic = CreateWindowW(L"STATIC", NULL, WS_CHILD | WS_VISIBLE | SS_BITMAP,
        fx, fy, fw, fh, m_hWnd, NULL, hInst, NULL);

    // 6. 统计按钮（默认位置：Favicon 下方）
    RECT defaultStatsRect = {m_config.popupWidth - 60 - 10, 80 + 64 + 10, m_config.popupWidth - 10, 80 + 64 + 10 + 30};
    m_hStatsButton = createButton("stats", L"统计", IDC_STATS_BUTTON, defaultStatsRect);

    // 7. 快捷按钮（最多4个，默认均匀分布，但优先使用配置中的 shortcut1~shortcut4）
    int shortcutCount = (int)m_config.shortcuts.size();
    if (shortcutCount > 4) shortcutCount = 4;
    int defaultShortcutWidth = (m_config.popupWidth - 50) / 4;  // 间距 10*5 = 50
    for (int i = 0; i < shortcutCount; ++i) {
        std::string id = "shortcut" + std::to_string(i + 1);
        RECT defaultRect = {10 + i * (defaultShortcutWidth + 10), 190,
                            10 + (i+1) * defaultShortcutWidth + i * 10, 190 + 30};
        m_hShortcutButtons[i] = createButton(id, UTF8ToWide(m_config.shortcuts[i].name),
                                             IDC_SHORTCUT1 + i, defaultRect);
    }

    // 8. 启动游戏按钮
    RECT defaultLaunchRect = {150, 230, 250, 260};
    HWND hLaunch = createButton("launch", L"启动游戏", IDC_LAUNCH_BUTTON, defaultLaunchRect);

    // 9. 切换服务器按钮
    RECT defaultSwitchRect = {260, 230, 360, 260};
    m_hSwitchButton = createButton("switch", L"切换服务器", IDC_SWITCH_BUTTON, defaultSwitchRect);

    // 10. 工具箱按钮
    RECT defaultToolRect = {40, 270, 140, 300};
    m_hToolButton = createButton("tool", L"工具箱", IDC_TOOL_BUTTON, defaultToolRect);

    // 11. 设置按钮
    RECT defaultSettingsRect = {150, 270, 250, 300};
    m_hSettingsButton = createButton("settings", L"设置", IDC_SETTINGS_BUTTON, defaultSettingsRect);

    // 12. 退出按钮（右上角）
    RECT defaultExitRect = {m_config.popupWidth - 30, 0, m_config.popupWidth, 30};
    m_hExitButton = createButton("exit", L"×", IDC_EXIT_BUTTON, defaultExitRect);

    // 13. 时间显示（默认位置：退出按钮左侧）
    if (m_config.timeDisplay.enabled) {
        RECT defaultTimeRect = {m_config.popupWidth - 100 - 30, 5, m_config.popupWidth - 30, 30};
        m_hTimeStatic = createStatic("time_display", L"", IDC_TIME_STATIC, defaultTimeRect);
        SetTimer(m_hWnd, 101, 1000, NULL);
        UpdateTimeDisplay();
    }

    // 统一字体（再次确保所有子控件都使用了字体）
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

void PopupWindow::ReloadConfig(const Config& newCfg) {
    bool sizeChanged = (newCfg.popupWidth != m_config.popupWidth || newCfg.popupHeight != m_config.popupHeight);
    m_config = newCfg;

    if (sizeChanged) {
        // 尺寸变化，需要重建窗口
        Recreate();
        return;
    }

    // 尺寸未变化，只需更新一些动态内容
    if (m_hTimeStatic) {
        if (m_config.timeDisplay.enabled) {
            ShowWindow(m_hTimeStatic, SW_SHOW);
            UpdateTimeDisplay();
        } else {
            ShowWindow(m_hTimeStatic, SW_HIDE);
        }
    }
    SetCurrentServerInfo();
}

void PopupWindow::Recreate() {
    if (!m_hWnd) return;
    // 保存当前位置
    UpdateLastX();
    // 销毁当前窗口
    DestroyWindow(m_hWnd);
    m_hWnd = NULL;
    // 重新创建
    Create(m_hParent, m_hInst, m_config);
    // 恢复位置（如果之前可见则显示）
    if (IsWindowVisible(m_hWnd)) {
        SetWindowPos(m_hWnd, HWND_TOPMOST, m_lastX, 0, m_config.popupWidth, m_config.popupHeight, SWP_NOZORDER);
        ShowWindow(m_hWnd, SW_SHOW);
    }
}