#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct ServerInfo {
    std::string host;
    int port;
};

struct Shortcut {
    std::string name;
    std::string url;
};

// 按钮布局配置
struct ButtonRect {
    std::string id;      // 按钮标识符: "launch", "switch", "tool", "exit", "shortcut1"..."shortcut4"
    int left;
    int top;
    int right;
    int bottom;
    int radius;          // 圆角半径（像素），默认 8
};

struct Config {
    std::vector<ServerInfo> servers;
    int currentServer;
    std::string newsURL;
    std::vector<Shortcut> shortcuts;
    int edgeThreshold;
    int popupWidth;
    int popupHeight;

    // 新增：按钮布局列表
    std::vector<ButtonRect> buttonRects;

    // 以下保留用于兼容旧版
    std::string gameCommand;
    std::vector<std::string> gameArgs;

    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath);
};