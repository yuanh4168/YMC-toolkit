// SettingsWindow.h
#pragma once
#include <windows.h>
#include <string>
#include "Config.h"

class SettingsWindow {
public:
    SettingsWindow(HINSTANCE hInst, Config& cfg, HWND hParent);
    ~SettingsWindow();
    void Show();

private:
    Config& m_config;
    HINSTANCE m_hInst;
    HWND m_hParent;
    std::string m_configPath;

    // 辅助方法
    void LoadDataToUI();
    void SaveUItoConfig();
    void SetCheckButtonState(const std::string& name, bool checked);
    
    // 回调中将使用的简易方法
    void AddServer();
    void EditServer();
    void DeleteServer();
    void MoveServerUp();
    void MoveServerDown();
    void SetDefaultServer();
    void TestServer();
    void AddShortcut();
    void EditShortcut();
    void DeleteShortcut();
    void BrowseGameCommand();
    void TestReminder();
    void ClearHistory();
    void ResetConfig();
    void BackupConfig();
    void RestoreConfig();
    void SetStartup(bool enable);

    static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};