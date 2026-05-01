// ToolWindow.cpp
#include "ToolWindow.h"
#include "easy_UI.hpp"
#include "PopupWindow.h"
#include "resource.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <nlohmann/json.hpp>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace eui;

// 全局变量存储文本框内容（因为 easyUI::TextBox 的文本需要手动获取）
static std::wstring g_descText;
static std::wstring g_treeText;

ToolWindow::ToolWindow() {}
ToolWindow::~ToolWindow() {}

bool ToolWindow::Show(HWND hParent, HINSTANCE hInst) {
    m_hParent = hParent;
    m_hInst = hInst;

    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring exeDir = modulePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
    m_targetPath = exeDir;

    Application app;
    app.title = L"工具箱";
    app.width = 1500;
    app.height = 1100;
    app.OnInit = [this]() {
        easyUI.SetGlobalFont(L"Microsoft YaHei", 13);

        Theme t;
        t.bg = Color(45, 45, 48);
        t.fg = Color(63, 63, 70);
        t.accent = Color(0, 122, 204);
        t.text = Color(240, 240, 240);
        easyUI.SetTheme(t);

        const int ROW_H = 45;
        const int SPACE = 20;
        const int TEXT_AREA_H = 120;

        int y = 10;

       // ---------- DeepSeek 提示词生成器 ----------
easyUI.CreateLabel("lblPrompt", L"DeepSeek 提示词生成器", 38, y, 562, ROW_H);
y += ROW_H + SPACE;
easyUI.CreateLabel("lblDesc", L"项目描述:", 38, y, 281, ROW_H);
y += ROW_H + SPACE;

// 项目描述文本框，启用多行
easyUI.CreateTextBox("txtPromptDesc", 38, y, 1425, TEXT_AREA_H);
auto pDesc = dynamic_cast<eui::Controls::TextBox*>(eui::Controls::FindControl("txtPromptDesc"));
if (pDesc) pDesc->SetMultiline(true);
easyUI.Text("txtPromptDesc", g_descText);
y += TEXT_AREA_H + SPACE;

easyUI.CreateLabel("lblResult", L"生成的提示词:", 38, y, 281, ROW_H);
y += ROW_H + SPACE;

// 生成的提示词文本框，启用多行（通常较大且只读，但仍可设为多行以便查看和复制）
easyUI.CreateTextBox("txtPromptResult", 38, y, 1425, TEXT_AREA_H);
auto pResult = dynamic_cast<eui::Controls::TextBox*>(eui::Controls::FindControl("txtPromptResult"));
if (pResult) pResult->SetMultiline(true);
y += TEXT_AREA_H + SPACE;
easyUI.CreateButton("btnGenerate", L"生成提示词", 38, y, 262, ROW_H);
easyUI.CreateButton("btnExport", L"导出为 Markdown", 338, y, 300, ROW_H);
y += ROW_H + SPACE * 2;

// ---------- 项目结构生成器 ----------
easyUI.CreateLabel("lblStruct", L"项目结构生成器", 38, y, 562, ROW_H);
y += ROW_H + SPACE;
easyUI.CreateLabel("lblTree", L"粘贴目录树 (支持 tree /f 风格):", 38, y, 562, ROW_H);
y += ROW_H + SPACE;

// 目录树粘贴框，启用多行
easyUI.CreateTextBox("txtStructure", 38, y, 1425, 120);
auto pStruct = dynamic_cast<eui::Controls::TextBox*>(eui::Controls::FindControl("txtStructure"));
if (pStruct) pStruct->SetMultiline(true);
easyUI.Text("txtStructure", g_treeText);
y += 120 + SPACE;

easyUI.CreateButton("btnSelPath", L"选择生成目录", 38, y, 300, ROW_H);
easyUI.CreateButton("btnGenStruct", L"生成结构", 375, y, 281, ROW_H);
y += ROW_H + SPACE;
easyUI.CreateLabel("lblLog", L"日志:", 38, y, 112, ROW_H);
y += ROW_H + SPACE;

// 日志框，启用多行（仅用于展示，故设为多行便于滚动查看）
easyUI.CreateTextBox("txtLog", 38, y, 1425, 100);
auto pLog = dynamic_cast<eui::Controls::TextBox*>(eui::Controls::FindControl("txtLog"));
if (pLog) pLog->SetMultiline(true);

        // 事件绑定
        easyUI.OnClick("btnGenerate", [this]() {
            std::wstring desc = easyUI.Text("txtPromptDesc");
            g_descText = desc;
            if (desc.empty()) {
                MessageBoxW(detail::GS().hwnd, L"请输入项目描述。", L"提示", MB_OK);
                return;
            }
            std::string utf8Desc = PopupWindow::WideToUTF8(desc);
            std::string tmpl = LoadPromptTemplate();
            std::string prompt;
            if (!tmpl.empty()) {
                size_t p = tmpl.find("{{description}}");
                if (p != std::string::npos) {
                    prompt = tmpl;
                    prompt.replace(p, 15, utf8Desc);
                } else {
                    prompt = tmpl + "\n\n" + utf8Desc;
                }
            } else {
                prompt = "# DeepSeek 提示词模板\n\n## 项目背景\n" + utf8Desc +
                         "\n\n## 任务目标\n根据上述项目背景，完成以下任务：\n"
                         "1. 分析需求并给出技术选型\n2. 设计核心模块及接口\n3. 提供关键代码示例\n4. 指出难点和解决方案\n";
            }
            easyUI.Text("txtPromptResult", PopupWindow::UTF8ToWide(prompt));
        });

        easyUI.OnClick("btnExport", [this]() { OnExportPrompt(); });
        easyUI.OnClick("btnSelPath", [this]() { OnSelectPath(); });
        easyUI.OnClick("btnGenStruct", [this]() {
            std::wstring wtree = easyUI.Text("txtStructure");
            g_treeText = wtree;
            std::string treeText = PopupWindow::WideToUTF8(wtree);
            if (treeText.empty()) {
                MessageBoxW(detail::GS().hwnd, L"请粘贴目录树。", L"提示", MB_OK);
                return;
            }
            std::string log;
            bool ok = ParseAndGenerate(treeText, PopupWindow::WideToUTF8(m_targetPath), log);
            AppendLog(PopupWindow::UTF8ToWide(log));
            MessageBoxW(detail::GS().hwnd, ok ? L"项目结构生成成功" : L"生成过程出现错误，请查看日志", L"完成", MB_OK);
        });

        // 自动缩放
        easyUI.SetAutoScale(true, 1500, 1100);

        HWND hwnd = detail::GS().hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
        WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)ToolWndProc);
        SetPropW(hwnd, L"OLD_PROC", (HANDLE)oldProc);
    };

    easyUI.Run(app);
    return true;
}

// 以下函数（LoadPromptTemplate, OnExportPrompt, OnSelectPath, AppendLog, ParseAndGenerate, ToolWndProc）
// 与之前完全相同，请从您原有的 ToolWindow.cpp 中复制。
// 注意：这些函数无需修改。
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
        if (!f.is_open()) {
            std::ofstream ofs(templatePath);
            if (ofs) {
                json j;
                j["template"] = "# DeepSeek 提示词模板\n\n## 项目描述\n{{description}}\n\n## 任务\n请根据项目描述生成详细的代码实现方案。";
                ofs << j.dump(4);
            }
            return "";
        }
        json j;
        f >> j;
        if (j.contains("template") && j["template"].is_string())
            return j["template"].get<std::string>();
    } catch (...) {}
    return "";
}

void ToolWindow::OnGeneratePrompt() {
    std::wstring desc = easyUI.Text("txtPromptDesc");
    std::string utf8Desc = PopupWindow::WideToUTF8(desc);
    if (utf8Desc.empty()) {
        MessageBoxW(detail::GS().hwnd, L"请输入项目描述。", L"提示", MB_OK);
        return;
    }
    std::string tmpl = LoadPromptTemplate();
    std::string prompt;
    if (!tmpl.empty()) {
        size_t p = tmpl.find("{{description}}");
        if (p != std::string::npos) {
            prompt = tmpl;
            prompt.replace(p, 15, utf8Desc);
        } else {
            prompt = tmpl + "\n\n" + utf8Desc;
        }
    } else {
        prompt = "# DeepSeek 提示词模板\n\n## 项目背景\n" + utf8Desc +
                 "\n\n## 任务目标\n根据上述项目背景，完成以下任务：\n"
                 "1. 分析需求并给出技术选型\n2. 设计核心模块及接口\n3. 提供关键代码示例\n4. 指出难点和解决方案\n";
    }
    easyUI.Text("txtPromptResult", PopupWindow::UTF8ToWide(prompt));
}

void ToolWindow::OnExportPrompt() {
    std::wstring wprompt = easyUI.Text("txtPromptResult");
    if (wprompt.empty()) {
        MessageBoxW(detail::GS().hwnd, L"没有可导出的内容。", L"提示", MB_OK);
        return;
    }
    std::string prompt = PopupWindow::WideToUTF8(wprompt);
    OPENFILENAMEW ofn = {};
    wchar_t fileName[MAX_PATH] = L"prompt_template.md";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = detail::GS().hwnd;
    ofn.lpstrFilter = L"Markdown 文件\0*.md\0所有文件\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"md";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        std::ofstream file(fileName, std::ios::binary);
        if (file) {
            file << prompt;
            MessageBoxW(detail::GS().hwnd, L"导出成功。", L"提示", MB_OK);
        } else {
            MessageBoxW(detail::GS().hwnd, L"保存失败。", L"错误", MB_ICONERROR);
        }
    }
}

void ToolWindow::OnSelectPath() {
    BROWSEINFOW bi = {};
    bi.hwndOwner = detail::GS().hwnd;
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
    std::wstring wtree = easyUI.Text("txtStructure");
    std::string treeText = PopupWindow::WideToUTF8(wtree);
    if (treeText.empty()) {
        MessageBoxW(detail::GS().hwnd, L"请粘贴目录树。", L"提示", MB_OK);
        return;
    }
    std::string log;
    bool ok = ParseAndGenerate(treeText, PopupWindow::WideToUTF8(m_targetPath), log);
    AppendLog(PopupWindow::UTF8ToWide(log));
    MessageBoxW(detail::GS().hwnd, ok ? L"项目结构生成成功" : L"生成过程出现错误，请查看日志", L"完成", MB_OK);
}

void ToolWindow::AppendLog(const std::wstring& text) {
    std::wstring current = easyUI.Text("txtLog");
    if (!current.empty()) current += L"\r\n";
    current += text;
    easyUI.Text("txtLog", current);
    try {
        wchar_t modulePath[MAX_PATH];
        GetModuleFileNameW(NULL, modulePath, MAX_PATH);
        std::wstring exeDir = modulePath;
        exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));
        std::wstring logPath = exeDir + L"\\tool_log.log";
        std::ofstream logFile(PopupWindow::WideToUTF8(logPath), std::ios::binary | std::ios::app);
        if (logFile) {
            logFile << PopupWindow::WideToUTF8(text) << std::endl;
        }
    } catch (...) {}
}

bool ToolWindow::ParseAndGenerate(const std::string& treeText, const std::string& rootPath, std::string& log) {
    log.clear();

    std::string text = treeText;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
        text = text.substr(3);

    std::wstring wTreeText = PopupWindow::UTF8ToWide(text);
    std::wistringstream iss(wTreeText);
    std::wstring line;

    struct Entry { int level; std::wstring nameW; bool isDir; std::string fullPath; };
    std::vector<Entry> entries;
    const std::wstring treeSymbols = L"│├─└─┐┌┘┼┤┴┬";
    const int INDENT_UNIT = 4;

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        for (wchar_t ch : treeSymbols)
            std::replace(line.begin(), line.end(), ch, L' ');

        size_t leadingSpaces = 0;
        while (leadingSpaces < line.size() && line[leadingSpaces] == L' ') ++leadingSpaces;
        if (leadingSpaces == line.size()) continue;

        int level = (int)(leadingSpaces / INDENT_UNIT);
        std::wstring nameW = line.substr(leadingSpaces);
        nameW.erase(0, nameW.find_first_not_of(L" \t"));
        nameW.erase(nameW.find_last_not_of(L" \t") + 1);
        if (nameW.empty()) continue;

        bool isDir = false;
        if (!nameW.empty() && nameW.back() == L'/') { isDir = true; nameW.pop_back(); }
        else {
            size_t dot = nameW.find(L'.');
            if (dot != std::wstring::npos && dot != nameW.size() - 1) isDir = false;
            else isDir = true;
        }
        std::wstring cleanNameW;
        for (wchar_t c : nameW) {
            if (iswalnum(c) || c == L'.' || c == L'_' || c == L'-' || c == L' ' || c > 0x7F)
                cleanNameW += c;
            else
                cleanNameW += L'_';
        }
        if (cleanNameW.empty()) continue;
        entries.push_back({level, cleanNameW, isDir, ""});
    }

    if (entries.empty()) { log = "未找到有效的目录树条目。"; return false; }

    std::vector<std::string> pathStackUTF8;
    std::vector<std::wstring> pathStackW;
    bool allSuccess = true;

    for (size_t i = 0; i < entries.size(); ++i) {
        Entry& e = entries[i];
        while ((int)pathStackW.size() > e.level) { pathStackW.pop_back(); pathStackUTF8.pop_back(); }
        if ((int)pathStackW.size() < e.level) {
            log += "✗ 缺失父目录，跳过: " + PopupWindow::WideToUTF8(e.nameW) + "\n";
            allSuccess = false; continue;
        }
        pathStackW.push_back(e.nameW);
        pathStackUTF8.push_back(PopupWindow::WideToUTF8(e.nameW));
        fs::path full = fs::path(rootPath);
        for (const auto& seg : pathStackUTF8) full /= seg;
        e.fullPath = full.string();

        if (e.isDir) {
            try {
                if (fs::create_directories(e.fullPath)) log += "✓ 创建目录: " + e.fullPath + "\n";
                else log += "○ 目录已存在: " + e.fullPath + "\n";
            } catch (const std::exception& ex) {
                log += "✗ 创建目录失败: " + e.fullPath + " - " + ex.what() + "\n";
                allSuccess = false;
            }
        } else {
            fs::path parent = full.parent_path();
            if (!parent.empty() && !fs::exists(parent)) {
                try { fs::create_directories(parent); log += "✓ 创建父目录: " + parent.string() + "\n"; }
                catch (...) { log += "✗ 创建父目录失败: " + parent.string() + "\n"; allSuccess = false; continue; }
            }
            if (!fs::exists(e.fullPath)) {
                try {
                    std::ofstream file(e.fullPath, std::ios::binary);
                    if (file) { file.close(); log += "✓ 创建文件: " + e.fullPath + "\n"; }
                    else { log += "✗ 创建文件失败: " + e.fullPath + "\n"; allSuccess = false; }
                } catch (...) { log += "✗ 创建文件异常: " + e.fullPath + "\n"; allSuccess = false; }
            } else log += "○ 文件已存在，跳过: " + e.fullPath + "\n";
        }
    }
    return allSuccess;
}

LRESULT CALLBACK ToolWindow::ToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ToolWindow* pThis = (ToolWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    WNDPROC oldProc = (WNDPROC)GetPropW(hwnd, L"OLD_PROC");
    (void)pThis;
    return CallWindowProc(oldProc, hwnd, msg, wParam, lParam);
}