下面提供更新后的 README.md 以及 NSIS 安装脚本 (installer.nsi)，并说明运行 NSIS 脚本所需的环境。

---

📄 更新后的 README.md

# MCTool – Minecraft 服务器状态监控与快捷启动工具

MCTool 是一个轻量级 Windows 桌面工具，专注于 **Minecraft 服务器状态监控**、**游戏快速启动** 以及 **开发辅助功能**。它将弹窗隐藏在屏幕顶部边缘，鼠标移入时滑出，显示服务器信息、快捷入口，并支持一键启动游戏。

## ✨ 主要功能

- **服务器状态监控**  
  支持现代（1.7+）及旧版 Minecraft 服务器协议，显示：
  - 在线 / 离线状态
  - 玩家数量（在线/最大）
  - 服务器 MOTD（支持 JSON 格式）
  - 服务器版本
  - 延迟（ping 值）
  - 服务器图标（favicon，现代协议）
  - 模组列表（Forge/Fabric 格式，现代协议）

- **多服务器管理**  
  内置服务器列表，可通过 `config.json` 添加、编辑、删除、排序服务器，并在弹窗中一键切换。

- **智能弹窗**  
  - 鼠标移至屏幕顶部边缘时自动滑出，移出弹窗区域后自动收回。
  - 可拖动弹窗顶部标题栏移动位置，下次弹出时记住位置。
  - 自绘按钮，带悬停效果（文字加粗、颜色变化）。

- **快捷网页入口**  
  可配置常用网站（如 DeepSeek、GitHub、B站、Minecraft 官网），点击按钮直接打开。

- **一键启动游戏**  
  点击“启动游戏”按钮执行同目录下的 `start_game.bat` 脚本（用户需自行放置）。方便整合任意启动器或自定义启动命令。

- **工具箱**（新增）  
  包含两个实用工具：
  - **DeepSeek 提示词生成器**：根据项目描述自动生成格式化的 Markdown 提示词，支持导出为 `.md` 文件。
  - **项目结构生成器**：粘贴 `tree /f` 风格目录树，自动在指定目录下创建对应的文件/文件夹结构，支持日志记录。

## 🖥️ 系统要求

- Windows 7 及以上（x86/x64）
- 已安装 [MinGW-w64](https://www.mingw-w64.org/) 或支持 `g++` 的编译环境（用于编译）
- 运行时需要系统自带 DLL：`gdiplus.dll`, `ws2_32.dll`, `comctl32.dll`, `crypt32.dll`, `ole32.dll`
- 若使用 MinGW 编译且未静态链接，可能需要 `libgcc_s_seh-1.dll`, `libstdc++-6.dll` 等运行时库（安装包会提供或请自行安装）
- 如需使用游戏启动，请自行准备 `start_game.bat` 放在程序同目录下

## 📦 编译与运行

### 编译（使用 MinGW）
项目根目录下提供了 `编译.bat`，内容如下：

```batch
@echo off
set RELEASE_DIR=..\MCTool_Build
if not exist %RELEASE_DIR% mkdir %RELEASE_DIR%

echo Building MCTool...
g++ -std=c++17 -Wall -Wextra -g3 -I./include src/*.cpp -o %RELEASE_DIR%\MCTool.exe -lws2_32 -lwininet -lgdi32 -lcomctl32 -lgdiplus -lcrypt32 -lole32 -lstdc++fs -mwindows
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
```

直接双击运行即可生成 MCTool.exe 至上级目录的 MCTool_Build 文件夹中。
如果未安装 MinGW 或需要调整，请修改脚本中的 g++ 路径。

从上级目录编译（可选）

若希望从项目父目录执行编译，可编写一个 build_mctool.bat 放在父目录中，自动进入项目目录并调用 编译.bat（示例见原 README）。

运行

1. 确保 config.json、prompt_template.json 与 MCTool.exe 在同一目录（若文件不存在程序会报错）。
2. 运行 MCTool.exe（不会显示主窗口，只在后台检测鼠标位置）。
3. 将鼠标移至屏幕顶部边缘，弹窗会自动滑出。
4. 可拖动弹窗顶部的标题栏移动位置。

⚙️ 配置文件说明

config.json（必须）

项目启动时会读取同目录下的 config.json，以下是一个完整示例：

```json
{
    "current_server": 2,
    "servers": [
        { "host": "3c3u.org", "port": 25565 },
        { "host": "mc.hypixel.net", "port": 25565 },
        { "host": "mcrhy.online", "port": 25565 }
    ],
    "shortcuts": [
        { "name": "deepseek", "url": "chat.deepseek.com" },
        { "name": "GitHub", "url": "https://github.com" },
        { "name": "B站", "url": "https://bilibili.com" },
        { "name": "Minecraft官网", "url": "https://www.minecraft.net" }
    ],
    "ui": {
        "edge_threshold": 10,
        "popup_width": 400,
        "popup_height": 320
    },
    "buttons": [
        { "id": "launch", "left": 150, "top": 230, "right": 250, "bottom": 260, "radius": 8 },
        { "id": "switch", "left": 260, "top": 230, "right": 360, "bottom": 260, "radius": 8 },
        { "id": "tool", "left": 40, "top": 270, "right": 140, "bottom": 300, "radius": 8 },
        { "id": "exit", "left": 370, "top": 0, "right": 400, "bottom": 30, "radius": 8 }
    ]
}
```

字段 说明
servers 服务器列表，每个对象包含 host 和 port。
current_server 当前选中服务器的索引（从 0 开始）。
shortcuts 快捷按钮列表，每个包含 name（显示文字）和 url（点击打开的网址）。
ui 界面配置：edge_threshold 触发弹窗的屏幕顶部距离（像素），popup_width/height 弹窗尺寸（逻辑像素，程序会自动根据 DPI 缩放）。
buttons 可选，自定义按钮位置和大小（单位为逻辑像素，会自动缩放）。每个按钮包含 id（固定取值：launch, switch, tool, exit, shortcut1...shortcut4）和矩形坐标。若不提供则使用默认布局。

prompt_template.json（可选，用于工具箱）

如果存在该文件，DeepSeek 提示词生成器将使用其中的 template 字段作为模板，模板中 {{description}} 会被用户输入的项目描述替换。
示例：

```json
{
    "template": "请根据以下项目描述生成代码：\n\n{{description}}\n\n要求：\n- 使用C++\n- 包含注释"
}
```

若文件不存在或格式错误，程序将使用内置默认模板。

🎮 使用说明

服务器状态查看

· 弹窗顶部显示当前服务器地址和端口，下方显示状态信息（在线/离线、玩家数、版本、延迟等）。
· 若服务器支持，会显示右侧的图标（favicon）和模组信息。

切换服务器

· 点击“切换服务器”按钮可依次切换到列表中的下一个服务器，当前索引会保存到配置文件。

启动游戏

· 点击“启动游戏”按钮，程序会执行同目录下的 start_game.bat 脚本。
    注意：你需要自己创建该脚本，并写入实际的游戏启动命令（如启动 Minecraft 客户端）。脚本示例已提供 start_game.bat，可根据自身环境修改路径。

快捷网页

· 四个快捷按钮（如果配置了足够条目）可快速打开常用网站。

工具箱

· 点击“工具箱”按钮打开辅助工具窗口：
  · DeepSeek 提示词生成器：在“项目描述”框输入内容，点击“生成提示词”得到 Markdown 格式的提示词，可导出为 .md 文件。
  · 项目结构生成器：粘贴 Windows tree /f 命令输出的目录树（例如 tree /f > structure.txt），选择目标目录，点击“生成结构”即可自动创建文件和文件夹。生成日志会显示在日志窗口并自动保存到 tool_log.log。

关闭程序

· 点击弹窗右上角的 × 按钮退出程序。

📁 项目结构

```
MCTool/
├── src/                     # 源代码
│   ├── Config.cpp/h
│   ├── DPIHelper.h
│   ├── GameLauncher.cpp/h
│   ├── MainWindow.cpp/h
│   ├── PopupWindow.cpp/h
│   ├── ServerPinger.cpp/h
│   ├── ToolWindow.cpp/h
│   ├── Utils.cpp/h
│   └── main.cpp
├── include/                 # 第三方库头文件
│   └── nlohmann/
│       └── json.hpp         # JSON 解析库
├── config.json              # 配置文件示例
├── prompt_template.json     # 提示词模板示例（可选）
├── start_game.bat           # 游戏启动脚本（需用户自行适配）
├── 编译.bat                 # 编译脚本
└── README.md                # 本文档
```

🔧 依赖库

· nlohmann/json v3.7.2（单头文件，已包含在 include/ 中）
· Windows API（windows.h, gdiplus.h, winsock2.h 等）
· GDI+（用于加载服务器图标）
· Crypt32（用于 Base64 解码）
· Comctl32（用于通用控件）

编译时需链接以下库：

· ws2_32 – 网络通信
· wininet – 可选（未直接使用，但保留）
· gdi32, gdiplus – 图形绘制
· comctl32 – 通用控件
· crypt32 – Base64 解码
· ole32 – 流操作（GDI+ 需要）
· stdc++fs – C++17 文件系统（MinGW 需要）

🚧 已知问题与未来计划

· 弹窗的自动隐藏定时器可能在某些情况下不够灵敏，已预留调试接口。
· 计划增加更多功能：更多 DeepSeek 模板、项目结构生成的预览等。

📄 许可证

本项目代码未明确指定许可证，如需使用请联系作者。
第三方库 json.hpp 采用 MIT 许可证。

---

作者：yuanh4148
欢迎提 Issue 或 Pull Request 参与改进！




