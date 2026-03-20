#include "GameLauncher.h"
#include <windows.h>
#include <string>
#include <filesystem>

bool LaunchGame() {
    // 获取当前 exe 所在目录
    wchar_t modulePath[MAX_PATH];
    GetModuleFileNameW(NULL, modulePath, MAX_PATH);
    std::wstring exeDir = modulePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\"));

    // 拼接 bat 文件路径（固定名称 start_game.bat）
    std::wstring batPath = exeDir + L"\\start_game.bat";

    // 检查文件是否存在
    if (!std::filesystem::exists(batPath)) {
        MessageBoxW(NULL, L"未找到启动脚本 start_game.bat，请将其放在程序目录下。", L"错误", MB_ICONERROR);
        return false;
    }

    // 使用 ShellExecute 执行 bat
    HINSTANCE h = ShellExecuteW(NULL, L"open", batPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    // 返回值大于 32 表示成功（Windows 约定）
    return reinterpret_cast<intptr_t>(h) > 32;
}