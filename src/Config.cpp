#include "Config.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

bool Config::Load(const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    try {
        nlohmann::json j;
        f >> j;

        // ---- 服务器列表（优先）----
        if (j.contains("servers") && j["servers"].is_array()) {
            for (const auto& item : j["servers"]) {
                ServerInfo si;
                si.host = item.value("host", "");
                si.port = item.value("port", 25565);
                if (!si.host.empty()) {
                    servers.push_back(si);
                }
            }
        }
        // 兼容旧版单服务器配置
        if (servers.empty() && j.contains("server") && j["server"].is_object()) {
            ServerInfo si;
            si.host = j["server"].value("host", "");
            si.port = j["server"].value("port", 25565);
            if (!si.host.empty()) {
                servers.push_back(si);
            }
        }
        // 如果还是没有服务器，报错
        if (servers.empty()) {
            std::cerr << "No valid server configuration found." << std::endl;
            return false;
        }

        // 当前选中索引
        currentServer = j.value("current_server", 0);
        if (currentServer < 0 || currentServer >= (int)servers.size()) {
            currentServer = 0;
        }

        // ---- game 配置（可选，保留用于启动，但此处已不再使用）----
        if (j.contains("game") && j["game"].is_object()) {
            if (j["game"].contains("command") && j["game"]["command"].is_string()) {
                gameCommand = j["game"]["command"];
            } else {
                gameCommand = "";
            }
            if (j["game"].contains("args") && j["game"]["args"].is_array()) {
                gameArgs = j["game"]["args"].get<std::vector<std::string>>();
            } else {
                gameArgs.clear();
            }
        } else {
            gameCommand = "";
            gameArgs.clear();
        }

        // ---- news 配置（可选）----
        newsURL = j.value("news", nlohmann::json::object()).value("url", "");

        // ---- shortcuts 配置（可选）----
        if (j.contains("shortcuts") && j["shortcuts"].is_array()) {
            for (const auto& item : j["shortcuts"]) {
                Shortcut sc;
                sc.name = item.value("name", "");
                sc.url  = item.value("url", "");
                if (!sc.name.empty() && !sc.url.empty()) {
                    shortcuts.push_back(sc);
                }
            }
        }

        // ---- ui 配置（可选）----
        if (j.contains("ui") && j["ui"].is_object()) {
            edgeThreshold = j["ui"].value("edge_threshold", 10);
            popupWidth    = j["ui"].value("popup_width", 400);
            popupHeight   = j["ui"].value("popup_height", 300);
        } else {
            edgeThreshold = 10;
            popupWidth    = 400;
            popupHeight   = 300;
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return false;
    }
}

bool Config::Save(const std::string& filePath) {
    nlohmann::json j;
    // 构造服务器数组
    for (const auto& sv : servers) {
        nlohmann::json js;
        js["host"] = sv.host;
        js["port"] = sv.port;
        j["servers"].push_back(js);
    }
    j["current_server"] = currentServer;

    // 保留其它配置（但为了简单，只写回必要的，也可全部重新构造）
    // 注意：如果要保留所有原配置，最好先读取原配置，修改后再写回。这里简化，只写 servers 和 current_server，
    // 其它配置保持不变（不覆盖）。但为了安全，我们可以读取原 json，修改后写回。
    // 更好的做法：读取原 json，修改 "current_server" 字段，然后写回。
    try {
        // 读取原文件内容
        std::ifstream ifs(filePath);
        nlohmann::json full;
        if (ifs.is_open()) {
            ifs >> full;
        }
        // 更新 current_server 字段
        full["current_server"] = currentServer;
        // 写回
        std::ofstream ofs(filePath);
        ofs << full.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save config: " << e.what() << std::endl;
        return false;
    }
}