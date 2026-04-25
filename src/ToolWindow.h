// ToolWindow.h
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
    HWND m_hParent;
    HINSTANCE m_hInst;
    std::wstring m_targetPath;

    static LRESULT CALLBACK ToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnGeneratePrompt();
    void OnExportPrompt();
    void OnSelectPath();
    void OnGenerateStructure();
    void AppendLog(const std::wstring& text);
    bool ParseAndGenerate(const std::string& treeText, const std::string& rootPath, std::string& log);
    std::string LoadPromptTemplate();
};