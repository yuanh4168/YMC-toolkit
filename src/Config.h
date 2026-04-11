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

    bool Load(const std::string& filePath);
    bool Save(const std::string& filePath) const;   // 改为 const，不修改对象
    static nlohmann::json ToJson(const Config& cfg);
    static Config FromJson(const nlohmann::json& j);
};