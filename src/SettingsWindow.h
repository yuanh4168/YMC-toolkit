#pragma once
#include <windows.h>
#include "Config.h"

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent);
    ~SettingsWindow();
    void Show();

    // 公共成员，方便窗口过程访问
    Config& m_config;
    HWND m_hDlg;
    HINSTANCE m_hInst;      // 原为 private，现改为 public
    HWND m_hParent;         // 原为 private，现改为 public
};