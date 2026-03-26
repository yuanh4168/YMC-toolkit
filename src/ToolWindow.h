#pragma once
#include <windows.h>
#include <string>
#include "Config.h"

class ToolWindow {
public:
    ToolWindow();
    ~ToolWindow();

    bool Show(HWND hParent, HINSTANCE hInst);

private:
    HWND m_hWnd;
    
    // 提示词生成器控件
    HWND m_hPromptEdit;
    HWND m_hGenerateBtn;
    HWND m_hExportBtn;
    HWND m_hPromptResultEdit;
    
    // 项目结构生成器控件
    HWND m_hStructureEdit;
    HWND m_hSelectPathBtn;
    HWND m_hGenerateStructureBtn;
    HWND m_hLogEdit;
    
    std::wstring m_targetPath;   // 生成目标路径

    static LRESULT CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void OnGeneratePrompt();
    void OnExportPrompt();
    void OnSelectPath();
    void OnGenerateStructure();
    void AppendLog(const std::wstring& text);
    void AppendLog(const std::string& text);
    
    bool ParseAndGenerate(const std::string& treeText, const std::string& rootPath, std::string& log);
    std::string LoadPromptTemplate();  // 从JSON加载模板
};