@echo off
cd /d %~dp0
set RELEASE_DIR=..\MCTool_Build
if not exist %RELEASE_DIR% mkdir %RELEASE_DIR%

echo Building MCTool...

REM 编译资源文件
windres resource.rc -O coff -o resource.res
if errorlevel 1 (
    echo Failed to compile resource.
    pause
    exit /b 1
)

REM 编译主程序（明确指定 include 路径）
g++ -std=c++17 -Wall -Wextra -g3 -Iinclude -I. src/*.cpp resource.res -o %RELEASE_DIR%\YMCtoolkit.exe -lws2_32 -lwininet -lgdi32 -lcomctl32 -lgdiplus -lcrypt32 -lole32 -lstdc++fs -mwindows

if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo Copying required files...
copy config.json %RELEASE_DIR%\ >nul 2>&1
copy prompt_template.json %RELEASE_DIR%\ >nul 2>&1
copy start_game.bat %RELEASE_DIR%\ >nul 2>&1

echo Build completed. Output in %RELEASE_DIR%\
pause
