#include "config/config.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glib.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace zenith {

Config Config::load(const std::string& path) {
    Config cfg;
    std::string resolved_path = path;

    std::string user_cfg = std::string(g_get_user_config_dir()) + "/zenithshell/config.json";
    if (path.empty() || !fs::exists(resolved_path)) {
        if (fs::exists(user_cfg)) {
            resolved_path = user_cfg;
        } else if (fs::exists("config.json")) {
            resolved_path = "config.json";
        } else if (fs::exists("/usr/share/zenithshell/config.json")) {
            resolved_path = "/usr/share/zenithshell/config.json";
        }
    }

    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        std::cerr << "[ZenithConfig] Could not open config file: " << path << ". Using defaults.\n";
        return cfg;
    }

    try {
        json j;
        file >> j;

        if (j.contains("height")) cfg.height = j["height"];
        if (j.contains("sys_update_interval_ms")) cfg.sys_update_interval_ms = j["sys_update_interval_ms"];
        if (j.contains("exclusive_zone")) cfg.exclusive_zone = j["exclusive_zone"];
        if (j.contains("wallpaper_dir")) cfg.wallpaper_dir = j["wallpaper_dir"];

        std::cout << "[ZenithConfig] Loaded configuration from " << resolved_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ZenithConfig] JSON Parse error in " << resolved_path << ": " << e.what() << std::endl;
    }

    return cfg;
}

} // namespace zenith
