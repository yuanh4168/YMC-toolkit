#pragma once
#include <windows.h>
#include "Config.h"

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent);
    ~SettingsWindow();
    void Show();

private:
    Config& m_config;
    HWND m_hDlg;
    HINSTANCE m_hInst;
    HWND m_hParent;
    std::string m_configPath;

    static LRESULT CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnOK();
};