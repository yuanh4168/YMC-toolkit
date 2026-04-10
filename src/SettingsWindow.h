#pragma once
#include <windows.h>
#include "Config.h"

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent);
    ~SettingsWindow();
    void Show();

    Config& m_config;  // 改为 public 以便窗口过程访问
    HWND m_hDlg;

private:
    HINSTANCE m_hInst;
    HWND m_hParent;
};