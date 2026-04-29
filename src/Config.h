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

struct ButtonRect {
    std::string id;
    int left;
    int top;
    int right;
    int bottom;
    int radius;
};

struct TimeDisplay {
    bool enabled = true;
    std::string format = "HH:mm:ss";
};

struct Reminder {
    bool enabled = false;
    int intervalMinutes = 15;
    std::string message = "该休息一会儿了，站起来活动一下吧";
};

struct ServerMonitor {
    bool backgroundEnabled = false;
    int intervalSeconds = 60;
    int maxDataPoints = 30;
};

struct Config {
    std::vector<ServerInfo> servers;
    int currentServer = 0;
    std::string newsURL;
    std::vector<Shortcut> shortcuts;
    int edgeThreshold = 10;
    int popupWidth = 400;
    int popupHeight = 300;
    std::vector<ButtonRect> buttonRects;
    std::string gameCommand;
    std::vector<std::string> gameArgs;

    TimeDisplay timeDisplay;
    Reminder reminder;
    ServerMonitor serverMonitor;

    // 停靠相关
    int dockedEdge = 0;      // 0: top, 1: right, 2: bottom, 3: left
    int edgeOffset = 0;      // 沿边方向的偏移（对于 top/bottom 是 left 坐标，对于 left/right 是 top 坐标）

    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath) const;
    static nlohmann::json ToJson(const Config& cfg);
    static Config FromJson(const nlohmann::json& j);
};