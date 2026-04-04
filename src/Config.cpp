#include "Config.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

bool Config::Load(const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    try {
        nlohmann::json j;
        f >> j;

        // ---- 服务器列表 ----
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
        if (servers.empty() && j.contains("server") && j["server"].is_object()) {
            ServerInfo si;
            si.host = j["server"].value("host", "");
            si.port = j["server"].value("port", 25565);
            if (!si.host.empty()) {
                servers.push_back(si);
            }
        }
        if (servers.empty()) {
            std::cerr << "No valid server configuration found." << std::endl;
            return false;
        }

        currentServer = j.value("current_server", 0);
        if (currentServer < 0 || currentServer >= (int)servers.size()) {
            currentServer = 0;
        }

        // ---- game 配置 ----
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

        newsURL = j.value("news", nlohmann::json::object()).value("url", "");

        // ---- shortcuts ----
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

        // ---- ui 配置（原始逻辑像素）----
        int origWidth = 400, origHeight = 300;
        if (j.contains("ui") && j["ui"].is_object()) {
            edgeThreshold = j["ui"].value("edge_threshold", 10);
            origWidth     = j["ui"].value("popup_width", 400);
            origHeight    = j["ui"].value("popup_height", 300);
        } else {
            edgeThreshold = 10;
            origWidth     = 400;
            origHeight    = 300;
        }

        // ---- 按钮布局配置（新增）----
        buttonRects.clear();
        if (j.contains("buttons") && j["buttons"].is_array()) {
            for (const auto& btn : j["buttons"]) {
                ButtonRect br;
                br.id = btn.value("id", "");
                br.left = btn.value("left", 0);
                br.top = btn.value("top", 0);
                br.right = btn.value("right", 0);
                br.bottom = btn.value("bottom", 0);
                br.radius = btn.value("radius", 8);
                if (!br.id.empty()) {
                    buttonRects.push_back(br);
                }
            }
        }

        // 根据系统 DPI 缩放窗口尺寸和边缘阈值
        HDC hdc = GetDC(NULL);
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
        double scale = dpiX / 96.0;

        popupWidth  = (int)(origWidth * scale);
        popupHeight = (int)(origHeight * scale);
        edgeThreshold = (int)(edgeThreshold * scale);

        // 缩放按钮矩形（如果配置中的坐标是基于 96 DPI 的逻辑像素）
        // 注意：这里假设配置中的坐标是逻辑像素，需要缩放
        for (auto& br : buttonRects) {
            br.left   = (int)(br.left * scale);
            br.top    = (int)(br.top * scale);
            br.right  = (int)(br.right * scale);
            br.bottom = (int)(br.bottom * scale);
            br.radius = (int)(br.radius * scale);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config parse error: " << e.what() << std::endl;
        return false;
    }
}

bool Config::Save(const std::string& filePath) {
    nlohmann::json j;
    for (const auto& sv : servers) {
        nlohmann::json js;
        js["host"] = sv.host;
        js["port"] = sv.port;
        j["servers"].push_back(js);
    }
    j["current_server"] = currentServer;

    try {
        std::ifstream ifs(filePath);
        nlohmann::json full;
        if (ifs.is_open()) {
            ifs >> full;
        }
        full["current_server"] = currentServer;
        std::ofstream ofs(filePath);
        ofs << full.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save config: " << e.what() << std::endl;
        return false;
    }
}