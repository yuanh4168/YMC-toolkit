#include <windows.h>
#include "MainWindow.h"
#include "resource.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    // 启用 DPI 感知
    SetProcessDPIAware();

    // 加载图标（如果失败则使用默认应用图标）
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    if (!hIcon) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }

    MainWindow mainWnd;
    if (!mainWnd.Create(hInstance, hIcon)) {
        return 1;
    }
    mainWnd.RunMessageLoop();
    return 0;
}