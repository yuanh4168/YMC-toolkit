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

struct Config {
    std::vector<ServerInfo> servers;        // 服务器列表
    int currentServer;                      // 当前选中的服务器索引
    std::string newsURL;
    std::vector<Shortcut> shortcuts;
    int edgeThreshold;
    int popupWidth;
    int popupHeight;

    // 以下保留用于兼容旧版（如果旧配置中还有 game 节点，可选）
    std::string gameCommand;
    std::vector<std::string> gameArgs;

    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath); // 新增保存方法，用于保存当前索引
};