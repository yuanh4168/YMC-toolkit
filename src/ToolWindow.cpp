#include "ToolWindow.h"
#include "PopupWindow.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;

// 控件ID
#define ID_PROMPT_EDIT    1001
#define ID_GENERATE_BTN   1002
#define ID_EXPORT_BTN     1003
#define ID_PROMPT_RESULT  1004
#define ID_STRUCTURE_EDIT 1005
#define ID_SELECT_PATH    1006
#define ID_GEN_STRUCT_BTN 1007
#define ID_LOG_EDIT       1008

// 窗口尺寸
const int DIALOG_WIDTH = 800;
const int DIALOG_HEIGHT = 680;

ToolWindow::ToolWindow() : m_hWnd(NULL),
    m_hPromptEdit(NULL), m_hGenerateBtn(NULL), m_hExportBtn(NULL), m_hPromptResultEdit(NULL),
    m_hStructureEdit(NULL), m_hSelectPathBtn(NULL), m_hGenerateStructureBtn(NULL), m_hLogEdit(NULL) {
}

ToolWindow::~ToolWindow() {
    if (m_hWnd) DestroyWindow(m_hWnd);
}

bool ToolWindow::Show(HWND hParent, HINSTANCE hInst) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DlgProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ToolWindowClass";
    RegisterClassExW(&wc);
    
    m_hWnd = CreateWindowExW(0, L"ToolWindowClass", L"Toolbox",
        WS_OVERLAPPEDWINDOW | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, DIALOG_WIDTH, DIALOG_HEIGHT,
        hParent, NULL, hInst, this);
    if (!m_hWnd) return false;
    
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
            
            // ========== 第一部分：DeepSeek 提示词生成器 ==========
            // 分组框（Group Box）
            CreateWindowW(L"BUTTON", L"DeepSeek Prompt Generator", 
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                10, 10, DIALOG_WIDTH - 30, 320, hDlg, NULL, hInst, NULL);
            
            // 标签：Project Description
            CreateWindowW(L"STATIC", L"Project Description:", WS_CHILD | WS_VISIBLE,
                20, 40, 150, 20, hDlg, NULL, hInst, NULL);
            // 输入框
            pThis->m_hPromptEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                20, 65, DIALOG_WIDTH - 50, 100, hDlg, (HMENU)ID_PROMPT_EDIT, hInst, NULL);
            
            // 标签：Generated Prompt
            CreateWindowW(L"STATIC", L"Generated Prompt:", WS_CHILD | WS_VISIBLE,
                20, 180, 150, 20, hDlg, NULL, hInst, NULL);
            // 结果框
            pThis->m_hPromptResultEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                20, 205, DIALOG_WIDTH - 50, 90, hDlg, (HMENU)ID_PROMPT_RESULT, hInst, NULL);
            
            // 按钮：生成提示词
            pThis->m_hGenerateBtn = CreateWindowW(L"BUTTON", L"Generate Prompt",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 305, 140, 25, hDlg, (HMENU)ID_GENERATE_BTN, hInst, NULL);
            // 按钮：导出
            pThis->m_hExportBtn = CreateWindowW(L"BUTTON", L"Export as Markdown",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                170, 305, 150, 25, hDlg, (HMENU)ID_EXPORT_BTN, hInst, NULL);
            
            // ========== 第二部分：项目结构生成器 ==========
            // 分组框
            CreateWindowW(L"BUTTON", L"Project Structure Generator",
                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                10, 340, DIALOG_WIDTH - 30, 290, hDlg, NULL, hInst, NULL);
            
            // 标签
            CreateWindowW(L"STATIC", L"Paste directory tree (tree /f style):", WS_CHILD | WS_VISIBLE,
                20, 370, 300, 20, hDlg, NULL, hInst, NULL);
            // 目录树输入框
            pThis->m_hStructureEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                20, 395, DIALOG_WIDTH - 50, 100, hDlg, (HMENU)ID_STRUCTURE_EDIT, hInst, NULL);
            // 选择目录按钮
            pThis->m_hSelectPathBtn = CreateWindowW(L"BUTTON", L"Select Target Directory",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                DIALOG_WIDTH - 180, 395, 160, 25, hDlg, (HMENU)ID_SELECT_PATH, hInst, NULL);
            
            // 标签：Log
            CreateWindowW(L"STATIC", L"Log:", WS_CHILD | WS_VISIBLE,
                20, 510, 50, 20, hDlg, NULL, hInst, NULL);
            // 日志框
            pThis->m_hLogEdit = CreateWindowW(L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
                20, 535, DIALOG_WIDTH - 50, 70, hDlg, (HMENU)ID_LOG_EDIT, hInst, NULL);
            // 生成按钮
            pThis->m_hGenerateStructureBtn = CreateWindowW(L"BUTTON", L"Generate Structure",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                DIALOG_WIDTH - 160, 610, 140, 25, hDlg, (HMENU)ID_GEN_STRUCT_BTN, hInst, NULL);
            
            // 默认目标路径为程序所在目录
            wchar_t modulePath[MAX_PATH];
            GetModuleFileNameW(NULL, modulePath, MAX_PATH);
            std::wstring exeDir = modulePath;
            exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
            pThis->m_targetPath = exeDir;
            
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_GENERATE_BTN:
                    pThis->OnGeneratePrompt();
                    break;
                case ID_EXPORT_BTN:
                    pThis->OnExportPrompt();
                    break;
                case ID_SELECT_PATH:
                    pThis->OnSelectPath();
                    break;
                case ID_GEN_STRUCT_BTN:
                    pThis->OnGenerateStructure();
                    break;
                case IDCANCEL:
                    DestroyWindow(hDlg);
                    break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hDlg);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

// ========== 从 JSON 文件加载提示词模板 ==========
std::string ToolWindow::LoadPromptTemplate() {
    // 获取程序所在目录
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring ws(modulePath);
    std::string exePath(ws.begin(), ws.end());
    size_t pos = exePath.find_last_of("\\");
    std::string configDir = exePath.substr(0, pos + 1);
    std::string templatePath = configDir + "prompt_template.json";
    
    try {
        std::ifstream f(templatePath);
        if (!f.is_open()) {
            // 文件不存在，返回默认模板
            return "";
        }
        json j;
        f >> j;
        if (j.contains("template") && j["template"].is_string()) {
            return j["template"].get<std::string>();
        }
    } catch (const std::exception& e) {
        // 解析失败，返回空
    }
    return "";
}

// ========== 生成提示词 ==========
void ToolWindow::OnGeneratePrompt() {
    // 获取用户输入
    int len = GetWindowTextLengthW(m_hPromptEdit);
    std::wstring desc(len + 1, L'\0');
    GetWindowTextW(m_hPromptEdit, &desc[0], len + 1);
    desc.resize(len);
    std::string description = PopupWindow::WideToUTF8(desc);
    
    if (description.empty()) {
        MessageBoxW(m_hWnd, L"Please enter a project description.", L"Info", MB_OK);
        return;
    }
    
    // 加载模板
    std::string templateStr = LoadPromptTemplate();
    std::string prompt;
    
    if (!templateStr.empty()) {
        // 使用 JSON 模板，替换占位符 {{description}}
        size_t pos = templateStr.find("{{description}}");
        if (pos != std::string::npos) {
            prompt = templateStr;
            prompt.replace(pos, 15, description); // 15 是 "{{description}}" 的长度
        } else {
            // 如果占位符不存在，则追加到模板末尾
            prompt = templateStr + "\n\n" + description;
        }
    } else {
        // 使用内置默认模板（与之前相同）
        prompt = "# DeepSeek Prompt Template\n\n";
        prompt += "## Project Background\n";
        prompt += description + "\n\n";
        prompt += "## Task Objectives\n";
        prompt += "Based on the above project background, complete the following tasks:\n\n";
        prompt += "1. Analyze project requirements and provide technology stack recommendations\n";
        prompt += "2. Design core functional modules and interfaces\n";
        prompt += "3. Provide key code examples\n";
        prompt += "4. Identify possible challenges and solutions\n\n";
        prompt += "## Output Format\n";
        prompt += "Please output in Markdown format with clear headings and code blocks.\n";
    }
    
    std::wstring wprompt = PopupWindow::UTF8ToWide(prompt);
    SetWindowTextW(m_hPromptResultEdit, wprompt.c_str());
}

void ToolWindow::OnExportPrompt() {
    int len = GetWindowTextLengthW(m_hPromptResultEdit);
    if (len == 0) {
        MessageBoxW(m_hWnd, L"No content to export. Please generate a prompt first.", L"Info", MB_OK);
        return;
    }
    
    std::wstring wprompt(len + 1, L'\0');
    GetWindowTextW(m_hPromptResultEdit, &wprompt[0], len + 1);
    wprompt.resize(len);
    std::string prompt = PopupWindow::WideToUTF8(wprompt);
    
    OPENFILENAMEW ofn = {0};
    wchar_t fileName[MAX_PATH] = L"prompt_template.md";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"Markdown Files\0*.md\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"md";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream file(fileName, std::ios::binary);
        if (file) {
            file << prompt;
            file.close();
            MessageBoxW(m_hWnd, L"Export successful.", L"Info", MB_OK);
        } else {
            MessageBoxW(m_hWnd, L"Failed to save file.", L"Error", MB_ICONERROR);
        }
    }
}

void ToolWindow::OnSelectPath() {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = L"Select target directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) {
            m_targetPath = path;
            AppendLog(L"Target directory set to: " + m_targetPath);
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
        MessageBoxW(m_hWnd, L"Please paste a directory tree.", L"Info", MB_OK);
        return;
    }
    
    std::string rootPath = PopupWindow::WideToUTF8(m_targetPath);
    std::string log;
    bool success = ParseAndGenerate(treeText, rootPath, log);
    
    AppendLog(PopupWindow::UTF8ToWide(log));
    if (success) {
        MessageBoxW(m_hWnd, L"Project structure generated successfully. Check the log.", L"Done", MB_OK);
    } else {
        MessageBoxW(m_hWnd, L"Errors occurred during generation. Check the log.", L"Error", MB_ICONERROR);
    }
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
}

void ToolWindow::AppendLog(const std::string& text) {
    AppendLog(PopupWindow::UTF8ToWide(text));
}

bool ToolWindow::ParseAndGenerate(const std::string& treeText, const std::string& rootPath, std::string& log) {
    // 与原实现相同，此处省略（保持原有功能）
    // ... 略（与之前版本相同）
    // 为了完整性，这里保留原实现（从之前的代码复制即可）
    log.clear();
    std::istringstream iss(treeText);
    std::string line;
    
    struct Entry {
        int level;
        std::string name;
        bool isDirectory;
        std::string path;
    };
    std::vector<Entry> entries;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        
        int level = 0;
        for (char c : line) {
            if (c == ' ') level++;
            else break;
        }
        level /= 2;
        std::string name = line.substr(level * 2);
        
        size_t pos = name.find_first_not_of("│├─└ ");
        if (pos != std::string::npos) name = name.substr(pos);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        
        bool isDir = false;
        if (!name.empty() && name.back() == '/') {
            isDir = true;
            name.pop_back();
        } else if (name.find('.') == std::string::npos && name.find("\\") == std::string::npos) {
            isDir = true;
        }
        
        const std::string treeChars = "│├─└ ";
        name.erase(std::remove_if(name.begin(), name.end(), [&](char c) {
            return treeChars.find(c) != std::string::npos;
        }), name.end());
        
        if (name.empty()) continue;
        entries.push_back({level, name, isDir, ""});
    }
    
    if (entries.empty()) {
        log = "No valid directory tree entries found.";
        return false;
    }
    
    std::vector<std::string> pathStack;
    bool allSuccess = true;
    
    for (size_t i = 0; i < entries.size(); ++i) {
        Entry& e = entries[i];
        while ((int)pathStack.size() > e.level) pathStack.pop_back();
        if (e.level == 0 && pathStack.empty()) {
            pathStack.push_back(e.name);
        } else {
            pathStack.push_back(e.name);
        }
        std::string fullPath = rootPath;
        for (const auto& seg : pathStack) {
            fullPath += "\\" + seg;
        }
        e.path = fullPath;
        
        if (e.isDirectory) {
            try {
                fs::create_directories(e.path);
                log += "✓ Created directory: " + e.path + "\n";
            } catch (const std::exception& ex) {
                log += "✗ Failed to create directory: " + e.path + " - " + ex.what() + "\n";
                allSuccess = false;
            }
        } else {
            fs::path parent = fs::path(e.path).parent_path();
            if (!parent.empty() && !fs::exists(parent)) {
                try {
                    fs::create_directories(parent);
                    log += "✓ Created parent directory: " + parent.string() + "\n";
                } catch (...) {
                    log += "✗ Failed to create parent directory: " + parent.string() + "\n";
                    allSuccess = false;
                    continue;
                }
            }
            if (!fs::exists(e.path)) {
                std::ofstream file(e.path, std::ios::binary);
                if (file) {
                    file.close();
                    log += "✓ Created file: " + e.path + "\n";
                } else {
                    log += "✗ Failed to create file: " + e.path + "\n";
                    allSuccess = false;
                }
            } else {
                log += "○ File already exists, skipped: " + e.path + "\n";
            }
        }
    }
    return allSuccess;
}