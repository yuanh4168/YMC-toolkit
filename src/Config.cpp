#include "Config.h"
#include "DPIHelper.h"
#include <fstream>
#include <iostream>

nlohmann::json Config::ToJson(const Config& cfg) {
    nlohmann::json j;

    // 服务器列表
    j["servers"] = nlohmann::json::array();
    for (const auto& sv : cfg.servers) {
        nlohmann::json js;
        js["host"] = sv.host;
        js["port"] = sv.port;
        j["servers"].push_back(js);
    }
    j["current_server"] = cfg.currentServer;

    // 快捷方式
    j["shortcuts"] = nlohmann::json::array();
    for (const auto& sc : cfg.shortcuts) {
        nlohmann::json jsc;
        jsc["name"] = sc.name;
        jsc["url"] = sc.url;
        j["shortcuts"].push_back(jsc);
    }

    // UI
    j["ui"]["edge_threshold"] = cfg.edgeThreshold;
    j["ui"]["popup_width"] = cfg.popupWidth;
    j["ui"]["popup_height"] = cfg.popupHeight;

    // 按钮布局
    j["buttons"] = nlohmann::json::array();
    for (const auto& btn : cfg.buttonRects) {
        nlohmann::json jbtn;
        jbtn["id"] = btn.id;
        jbtn["left"] = btn.left;
        jbtn["top"] = btn.top;
        jbtn["right"] = btn.right;
        jbtn["bottom"] = btn.bottom;
        jbtn["radius"] = btn.radius;
        j["buttons"].push_back(jbtn);
    }

    // 时间显示
    j["time_display"]["enabled"] = cfg.timeDisplay.enabled;
    j["time_display"]["format"] = cfg.timeDisplay.format;

    // 休息提醒
    j["reminder"]["enabled"] = cfg.reminder.enabled;
    j["reminder"]["interval_minutes"] = cfg.reminder.intervalMinutes;
    j["reminder"]["message"] = cfg.reminder.message;

    // 后台监控
    j["server_monitor"]["background_enabled"] = cfg.serverMonitor.backgroundEnabled;
    j["server_monitor"]["interval_seconds"] = cfg.serverMonitor.intervalSeconds;
    j["server_monitor"]["max_data_points"] = cfg.serverMonitor.maxDataPoints;

    // 游戏启动（可选）
    if (!cfg.gameCommand.empty()) {
        j["game"]["command"] = cfg.gameCommand;
        if (!cfg.gameArgs.empty()) {
            j["game"]["args"] = cfg.gameArgs;
        }
    }
    if (!cfg.newsURL.empty()) {
        j["news"]["url"] = cfg.newsURL;
    }

    return j;
}

Config Config::FromJson(const nlohmann::json& j) {
    Config cfg;

    // 服务器列表
    if (j.contains("servers") && j["servers"].is_array()) {
        for (const auto& item : j["servers"]) {
            ServerInfo si;
            si.host = item.value("host", "");
            si.port = item.value("port", 25565);
            if (!si.host.empty()) cfg.servers.push_back(si);
        }
    }
    if (cfg.servers.empty()) {
        cfg.servers.push_back({"localhost", 25565});
    }
    cfg.currentServer = j.value("current_server", 0);
    if (cfg.currentServer < 0 || cfg.currentServer >= (int)cfg.servers.size()) {
        cfg.currentServer = 0;
    }

    // 快捷方式
    if (j.contains("shortcuts") && j["shortcuts"].is_array()) {
        for (const auto& item : j["shortcuts"]) {
            Shortcut sc;
            sc.name = item.value("name", "");
            sc.url = item.value("url", "");
            if (!sc.name.empty() && !sc.url.empty()) cfg.shortcuts.push_back(sc);
        }
    }
    if (cfg.shortcuts.empty()) {
        cfg.shortcuts.push_back({"DeepSeek", "https://chat.deepseek.com"});
        cfg.shortcuts.push_back({"GitHub", "https://github.com"});
    }

    // UI
    if (j.contains("ui") && j["ui"].is_object()) {
        cfg.edgeThreshold = j["ui"].value("edge_threshold", 10);
        cfg.popupWidth = j["ui"].value("popup_width", 400);
        cfg.popupHeight = j["ui"].value("popup_height", 300);
    } else {
        cfg.edgeThreshold = 10;
        cfg.popupWidth = 400;
        cfg.popupHeight = 300;
    }

    // 按钮布局
    cfg.buttonRects.clear();
    if (j.contains("buttons") && j["buttons"].is_array()) {
        for (const auto& btn : j["buttons"]) {
            ButtonRect br;
            br.id = btn.value("id", "");
            br.left = btn.value("left", 0);
            br.top = btn.value("top", 0);
            br.right = btn.value("right", 0);
            br.bottom = btn.value("bottom", 0);
            br.radius = btn.value("radius", 8);
            if (!br.id.empty()) cfg.buttonRects.push_back(br);
        }
    }

    // 时间显示
    if (j.contains("time_display") && j["time_display"].is_object()) {
        cfg.timeDisplay.enabled = j["time_display"].value("enabled", true);
        cfg.timeDisplay.format = j["time_display"].value("format", "HH:mm:ss");
    }

    // 休息提醒
    if (j.contains("reminder") && j["reminder"].is_object()) {
        cfg.reminder.enabled = j["reminder"].value("enabled", false);
        cfg.reminder.intervalMinutes = j["reminder"].value("interval_minutes", 15);
        cfg.reminder.message = j["reminder"].value("message", "该休息一会儿了，站起来活动一下吧");
    }

    // 后台监控
    if (j.contains("server_monitor") && j["server_monitor"].is_object()) {
        cfg.serverMonitor.backgroundEnabled = j["server_monitor"].value("background_enabled", false);
        cfg.serverMonitor.intervalSeconds = j["server_monitor"].value("interval_seconds", 60);
        cfg.serverMonitor.maxDataPoints = j["server_monitor"].value("max_data_points", 30);
    }

    // 游戏命令
    if (j.contains("game") && j["game"].is_object()) {
        cfg.gameCommand = j["game"].value("command", "");
        if (j["game"].contains("args") && j["game"]["args"].is_array()) {
            cfg.gameArgs = j["game"]["args"].get<std::vector<std::string>>();
        }
    }

    cfg.newsURL = j.value("news", nlohmann::json::object()).value("url", "");

    // 应用 DPI 缩放（注意：popupWidth/Height 等已经是物理像素，不需要再缩放）
    // 但是按钮坐标等需要根据当前 DPI 重新计算，这里留到使用时处理

    return cfg;
}

bool Config::Load(const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f.is_open()) {
        // 文件不存在，使用默认配置
        *this = FromJson(nlohmann::json::object());
        Save(filePath);
        return true;
    }
    try {
        nlohmann::json j;
        f >> j;
        *this = FromJson(j);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config parse error: " << e.what() << ", using defaults." << std::endl;
        *this = FromJson(nlohmann::json::object());
        Save(filePath);
        return false;
    }
}

bool Config::Save(const std::string& filePath) const {
    nlohmann::json j = ToJson(*this);
    std::ofstream ofs(filePath);
    if (!ofs) return false;
    ofs << j.dump(4);
    return true;
}