#include "ToolWindow.h"
#include "PopupWindow.h"
#include "DPIHelper.h"
#include "resource.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <cwctype>
#include <nlohmann/json.hpp>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

#define ID_PROMPT_EDIT    1001
#define ID_GENERATE_BTN   1002
#define ID_EXPORT_BTN     1003
#define ID_PROMPT_RESULT  1004
#define ID_STRUCTURE_EDIT 1005
#define ID_SELECT_PATH    1006
#define ID_GEN_STRUCT_BTN 1007
#define ID_LOG_EDIT       1008

const int BASE_DIALOG_WIDTH = 800;
const int BASE_DIALOG_HEIGHT = 720;

ToolWindow::ToolWindow() : m_hWnd(NULL),
    m_hPromptEdit(NULL), m_hGenerateBtn(NULL), m_hExportBtn(NULL), m_hPromptResultEdit(NULL),
    m_hStructureEdit(NULL), m_hSelectPathBtn(NULL), m_hGenerateStructureBtn(NULL), m_hLogEdit(NULL),
    m_hBkBrush(NULL), m_hNormalFont(NULL), m_hBoldFont(NULL), m_hHoverButton(NULL),
    m_hClearFont(NULL) {
}

ToolWindow::~ToolWindow() {
    if (m_hClearFont) DeleteObject(m_hClearFont);
    if (m_hWnd) DestroyWindow(m_hWnd);
}

bool ToolWindow::Show(HWND hParent, HINSTANCE hInst) {
    double scale = GetDPIScale();
    int dialogWidth = (int)(BASE_DIALOG_WIDTH * scale);
    int dialogHeight = (int)(BASE_DIALOG_HEIGHT * scale);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ToolWindowClass";
    HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAIN_ICON));
    if (hIcon) {
        wc.hIcon = hIcon;
        wc.hIconSm = hIcon;
    }
    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(0, L"ToolWindowClass", L"工具箱",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, dialogWidth, dialogHeight,
        hParent, NULL, hInst, this);
    if (!m_hWnd) return false;

    if (hIcon) {
        SendMessage(m_hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(m_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);

    MSG msg;
    while (IsWindow(m_hWnd) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(m_hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return true;
}

LRESULT CALLBACK ToolWindow::DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    ToolWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        pThis = (ToolWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ToolWindow*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
    }
    if (!pThis) return DefWindowProcW(hDlg, msg, wParam, lParam);

    switch (msg) {
        case WM_CREATE: {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE);
            double scale = GetDPIScale();

            int x10 = (int)(10 * scale);
            int x20 = (int)(20 * scale);
            int x30 = (int)(30 * scale);
            int x50 = (int)(50 * scale);
            int x140 = (int)(140 * scale);
            int x150 = (int)(150 * scale);
            int x170 = (int)(170 * scale);
            int x300 = (int)(300 * scale);
            int dialogWidth = (int)(BASE_DIALOG_WIDTH * scale);
            int widthMinus30 = dialogWidth - (int)(30 * scale);
            int widthMinus50 = dialogWidth - (int)(50 * scale);

            int y10 = (int)(10 * scale);
            int y20 = (int)(20 * scale);
            int y40 = (int)(40 * scale);
            int y65 = (int)(65 * scale);
            int y100 = (int)(100 * scale);
            int y180 = (int)(180 * scale);
            int y205 = (int)(205 * scale);
            int y305 = (int)(305 * scale);
            int y340 = (int)(340 * scale);
            int y370 = (int)(370 * scale);
            int y395 = (int)(395 * scale);
            int y505 = (int)(505 * scale);
            int y540 = (int)(540 * scale);
            int y565 = (int)(565 * scale);
            int y645 = (int)(645 * scale);
            int height20 = (int)(20 * scale);
            int height25 = (int)(25 * scale);
            int height70 = (int)(70 * scale);
            int height90 = (int)(90 * scale);
            int height100 = (int)(100 * scale);
            int height320 = (int)(320 * scale);
            int height350 = (int)(350 * scale);

            HDC hdc = GetDC(hDlg);
            int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(hDlg, hdc);
            int fontSize = -MulDiv(12, dpiX, 96);
            LOGFONTW lf = {0};
            lf.lfHeight = fontSize;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, L"Microsoft YaHei");
            pThis->m_hClearFont = CreateFontIndirectW(&lf);

            CreateWindowW(L"BUTTON", L"DeepSeek 提示词生成器", 
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                x10, y10, widthMinus30, height320, hDlg, NULL, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"项目描述：", WS_CHILD | WS_VISIBLE,
                x20, y40, (int)(150 * scale), height20, hDlg, NULL, hInst, NULL);
            pThis->m_hPromptEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                x20, y65, widthMinus50, height100, hDlg, (HMENU)ID_PROMPT_EDIT, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"生成的提示词：", WS_CHILD | WS_VISIBLE,
                x20, y180, (int)(150 * scale), height20, hDlg, NULL, hInst, NULL);
            pThis->m_hPromptResultEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                x20, y205, widthMinus50, height90, hDlg, (HMENU)ID_PROMPT_RESULT, hInst, NULL);
            
            pThis->m_hGenerateBtn = CreateWindowW(L"BUTTON", L"生成提示词",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x20, y305, x140, height25, hDlg, (HMENU)ID_GENERATE_BTN, hInst, NULL);
            pThis->m_hExportBtn = CreateWindowW(L"BUTTON", L"导出为 Markdown",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x170, y305, x150, height25, hDlg, (HMENU)ID_EXPORT_BTN, hInst, NULL);
            
            CreateWindowW(L"BUTTON", L"项目结构生成器",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                x10, y340, widthMinus30, height350, hDlg, NULL, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"粘贴目录树（支持 tree /f 风格）：", WS_CHILD | WS_VISIBLE,
                x20, y370, x300, height20, hDlg, NULL, hInst, NULL);
            pThis->m_hStructureEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                x20, y395, widthMinus50, height100, hDlg, (HMENU)ID_STRUCTURE_EDIT, hInst, NULL);
            
            pThis->m_hSelectPathBtn = CreateWindowW(L"BUTTON", L"选择生成目录",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x20, y505, x140, height25, hDlg, (HMENU)ID_SELECT_PATH, hInst, NULL);
            
            CreateWindowW(L"STATIC", L"日志：", WS_CHILD | WS_VISIBLE,
                x20, y540, (int)(50 * scale), height20, hDlg, NULL, hInst, NULL);
            pThis->m_hLogEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                x20, y565, widthMinus50, height70, hDlg, (HMENU)ID_LOG_EDIT, hInst, NULL);
            
            pThis->m_hGenerateStructureBtn = CreateWindowW(L"BUTTON", L"生成结构",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                x20, y645, x140, height25, hDlg, (HMENU)ID_GEN_STRUCT_BTN, hInst, NULL);
            
            for (HWND hChild = GetWindow(hDlg, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT)) {
                SendMessage(hChild, WM_SETFONT, (WPARAM)pThis->m_hClearFont, TRUE);
            }
            
            wchar_t modulePath[MAX_PATH];
            GetModuleFileNameW(NULL, modulePath, MAX_PATH);
            std::wstring exeDir = modulePath;
            exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
            pThis->m_targetPath = exeDir;
            
            return 0;
        }
        case WM_SIZE:
            return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO* pInfo = (MINMAXINFO*)lParam;
            double scale = GetDPIScale();
            pInfo->ptMinTrackSize.x = (int)(BASE_DIALOG_WIDTH * scale);
            pInfo->ptMinTrackSize.y = (int)(BASE_DIALOG_HEIGHT * scale);
            pInfo->ptMaxTrackSize.x = (int)(BASE_DIALOG_WIDTH * scale);
            pInfo->ptMaxTrackSize.y = (int)(BASE_DIALOG_HEIGHT * scale);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_GENERATE_BTN: pThis->OnGeneratePrompt(); break;
                case ID_EXPORT_BTN:   pThis->OnExportPrompt(); break;
                case ID_SELECT_PATH:  pThis->OnSelectPath(); break;
                case ID_GEN_STRUCT_BTN: pThis->OnGenerateStructure(); break;
                case IDCANCEL:        DestroyWindow(hDlg); break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hDlg);
            break;
        case WM_DESTROY:
            break;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

// ------------------ 辅助函数 ------------------
std::string ToolWindow::LoadPromptTemplate() {
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    std::string configDir = exePath.substr(0, pos + 1);
    std::string templatePath = configDir + "prompt_template.json";
    try {
        std::ifstream f(templatePath);
        if (!f.is_open()) return "";
        json j;
        f >> j;
        if (j.contains("template") && j["template"].is_string())
            return j["template"].get<std::string>();
    } catch (...) {}
    return "";
}

void ToolWindow::OnGeneratePrompt() {
    int len = GetWindowTextLengthW(m_hPromptEdit);
    std::wstring wdesc(len + 1, L'\0');
    GetWindowTextW(m_hPromptEdit, &wdesc[0], len + 1);
    wdesc.resize(len);
    std::string description = PopupWindow::WideToUTF8(wdesc);
    if (description.empty()) {
        MessageBoxW(m_hWnd, L"请输入项目描述。", L"提示", MB_OK);
        return;
    }
    std::string templateStr = LoadPromptTemplate();
    std::string prompt;
    if (!templateStr.empty()) {
        size_t pos = templateStr.find("{{description}}");
        if (pos != std::string::npos) {
            prompt = templateStr;
            prompt.replace(pos, 15, description);
        } else {
            prompt = templateStr + "\n\n" + description;
        }
    } else {
        prompt = "# DeepSeek 提示词模板\n\n## 项目背景\n" + description + "\n\n## 任务目标\n"
                + "根据上述项目背景，完成以下任务：\n\n1. 分析项目需求，给出技术选型建议\n"
                + "2. 设计核心功能模块及接口\n3. 提供关键代码示例\n4. 指出可能的难点和解决方案\n\n"
                + "## 输出格式\n请以 Markdown 格式输出，包含清晰的标题和代码块。\n";
    }
    std::wstring wprompt = PopupWindow::UTF8ToWide(prompt);
    SetWindowTextW(m_hPromptResultEdit, wprompt.c_str());
}

void ToolWindow::OnExportPrompt() {
    int len = GetWindowTextLengthW(m_hPromptResultEdit);
    if (len == 0) {
        MessageBoxW(m_hWnd, L"没有可导出的内容，请先生成提示词。", L"提示", MB_OK);
        return;
    }
    std::wstring wprompt(len + 1, L'\0');
    GetWindowTextW(m_hPromptResultEdit, &wprompt[0], len + 1);
    wprompt.resize(len);
    std::string prompt = PopupWindow::WideToUTF8(wprompt);
    OPENFILENAMEW ofn = {};
    wchar_t fileName[MAX_PATH] = L"prompt_template.md";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"Markdown 文件\0*.md\0所有文件\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"md";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream file(fileName, std::ios::binary);
        if (file) {
            file << prompt;
            file.close();
            MessageBoxW(m_hWnd, L"导出成功。", L"提示", MB_OK);
        } else {
            MessageBoxW(m_hWnd, L"保存文件失败。", L"错误", MB_ICONERROR);
        }
    }
}

void ToolWindow::OnSelectPath() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = L"选择生成目录";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            m_targetPath = path;
            AppendLog(L"目标目录已设置为: " + m_targetPath);
        }
        CoTaskMemFree(pidl);
    }
}

void ToolWindow::OnGenerateStructure() {
    int len = GetWindowTextLengthW(m_hStructureEdit);
    std::wstring wtree(len + 1, L'\0');
    GetWindowTextW(m_hStructureEdit, &wtree[0], len + 1);
    wtree.resize(len);
    std::string treeText = PopupWindow::WideToUTF8(wtree);
    if (treeText.empty()) {
        MessageBoxW(m_hWnd, L"请粘贴目录树。", L"提示", MB_OK);
        return;
    }
    std::string rootPath = PopupWindow::WideToUTF8(m_targetPath);
    std::string log;
    bool success = ParseAndGenerate(treeText, rootPath, log);
    AppendLog(PopupWindow::UTF8ToWide(log));
    if (success)
        MessageBoxW(m_hWnd, L"项目结构生成成功。请查看日志窗口。", L"完成", MB_OK);
    else
        MessageBoxW(m_hWnd, L"生成过程中出现错误，请查看日志窗口。", L"错误", MB_ICONERROR);
}

void ToolWindow::AppendLog(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_hLogEdit);
    std::wstring current(len + 1, L'\0');
    GetWindowTextW(m_hLogEdit, &current[0], len + 1);
    current.resize(len);
    if (!current.empty()) current += L"\r\n";
    current += text;
    SetWindowTextW(m_hLogEdit, current.c_str());
    SendMessageW(m_hLogEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(m_hLogEdit, EM_SCROLLCARET, 0, 0);

    try {
        wchar_t modulePath[MAX_PATH];
        GetModuleFileNameW(NULL, modulePath, MAX_PATH);
        std::wstring exeDir = modulePath;
        exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
        std::wstring logPath = exeDir + L"\\tool_log.log";

        std::string narrowPath = PopupWindow::WideToUTF8(logPath);
        std::ofstream logFile(narrowPath, std::ios::binary | std::ios::app);
        if (logFile) {
            std::string utf8Text = PopupWindow::WideToUTF8(text);
            logFile << utf8Text << std::endl;
            logFile.close();
        }
    } catch (...) {}
}

void ToolWindow::AppendLog(const std::string& text) {
    AppendLog(PopupWindow::UTF8ToWide(text));
}

bool ToolWindow::ParseAndGenerate(const std::string& treeText, const std::string& rootPath, std::string& log) {
    log.clear();

    std::string text = treeText;
    if (text.size() >= 3 && text[0] == '\xEF' && text[1] == '\xBB' && text[2] == '\xBF')
        text = text.substr(3);

    std::wstring wTreeText = PopupWindow::UTF8ToWide(text);
    std::wistringstream iss(wTreeText);
    std::wstring line;

    struct Entry {
        int level;
        std::wstring nameW;
        bool isDir;
        std::string fullPath;
    };
    std::vector<Entry> entries;

    const std::wstring treeSymbols = L"│├─└─┐┌┘┼┤┴┬";
    const int INDENT_UNIT = 4;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == L'\r') line.pop_back();

        for (wchar_t ch : treeSymbols) {
            std::replace(line.begin(), line.end(), ch, L' ');
        }

        size_t leadingSpaces = 0;
        while (leadingSpaces < line.size() && line[leadingSpaces] == L' ') {
            ++leadingSpaces;
        }

        if (leadingSpaces == line.size()) continue;

        int level = (int)(leadingSpaces / INDENT_UNIT);
        std::wstring nameW = line.substr(leadingSpaces);
        nameW.erase(0, nameW.find_first_not_of(L" \t"));
        nameW.erase(nameW.find_last_not_of(L" \t") + 1);
        if (nameW.empty()) continue;

        bool isDir = false;
        if (!nameW.empty() && nameW.back() == L'/') {
            isDir = true;
            nameW.pop_back();
        } else {
            size_t dot = nameW.find(L'.');
            if (dot != std::wstring::npos && dot != nameW.size() - 1) {
                isDir = false;
            } else {
                isDir = true;
            }
        }

        std::wstring cleanNameW;
        for (wchar_t c : nameW) {
            if (iswalnum(c) || c == L'.' || c == L'_' || c == L'-' || c == L' ' || c > 0x7F) {
                cleanNameW += c;
            } else {
                cleanNameW += L'_';
            }
        }
        if (cleanNameW.empty()) continue;

        entries.push_back({level, cleanNameW, isDir, ""});
    }

    if (entries.empty()) {
        log = "未找到有效的目录树条目。";
        return false;
    }

    std::vector<std::string> pathStackUTF8;
    std::vector<std::wstring> pathStackW;
    bool allSuccess = true;

    for (size_t i = 0; i < entries.size(); ++i) {
        Entry& e = entries[i];

        while ((int)pathStackW.size() > e.level) {
            pathStackW.pop_back();
            pathStackUTF8.pop_back();
        }

        if ((int)pathStackW.size() < e.level) {
            std::wstring msg = L"✗ 缺失父目录，跳过: " + e.nameW + L" (层级 " + std::to_wstring(e.level) +
                               L"，当前栈深度 " + std::to_wstring(pathStackW.size()) + L")\n";
            log += PopupWindow::WideToUTF8(msg);
            allSuccess = false;
            continue;
        }

        pathStackW.push_back(e.nameW);
        pathStackUTF8.push_back(PopupWindow::WideToUTF8(e.nameW));

        fs::path full = fs::path(rootPath);
        for (const auto& seg : pathStackUTF8) {
            full /= seg;
        }
        e.fullPath = full.string();

        if (e.isDir) {
            try {
                if (fs::create_directories(e.fullPath)) {
                    log += "✓ 创建目录: " + e.fullPath + "\n";
                } else {
                    log += "○ 目录已存在: " + e.fullPath + "\n";
                }
            } catch (const std::exception& ex) {
                log += "✗ 创建目录失败: " + e.fullPath + " - " + ex.what() + "\n";
                allSuccess = false;
            } catch (...) {
                log += "✗ 创建目录失败（未知异常）: " + e.fullPath + "\n";
                allSuccess = false;
            }
        } else {
            fs::path parent = full.parent_path();
            if (!parent.empty() && !fs::exists(parent)) {
                try {
                    fs::create_directories(parent);
                    log += "✓ 创建父目录: " + parent.string() + "\n";
                } catch (const std::exception& ex) {
                    log += "✗ 创建父目录失败: " + parent.string() + " - " + ex.what() + "\n";
                    allSuccess = false;
                    continue;
                } catch (...) {
                    log += "✗ 创建父目录失败（未知异常）: " + parent.string() + "\n";
                    allSuccess = false;
                    continue;
                }
            }

            if (!fs::exists(e.fullPath)) {
                try {
                    std::ofstream file(e.fullPath, std::ios::binary);
                    if (file) {
                        file.close();
                        log += "✓ 创建文件: " + e.fullPath + "\n";
                    } else {
                        log += "✗ 创建文件失败（无法打开）: " + e.fullPath + "\n";
                        allSuccess = false;
                    }
                } catch (const std::exception& ex) {
                    log += "✗ 创建文件失败: " + e.fullPath + " - " + ex.what() + "\n";
                    allSuccess = false;
                } catch (...) {
                    log += "✗ 创建文件失败（未知异常）: " + e.fullPath + "\n";
                    allSuccess = false;
                }
            } else {
                log += "○ 文件已存在，跳过: " + e.fullPath + "\n";
            }
        }
    }

    return allSuccess;
}