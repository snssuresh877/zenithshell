#include "Engine/Config/config.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <glib.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace zenith {

void Config::ensure_default_config() {
    std::string user_dir = std::string(g_get_user_config_dir()) + "/zenithshell";
    std::error_code ec;
    fs::create_directories(user_dir, ec);
    fs::create_directories(user_dir + "/themes", ec);

    std::string cfg_file = user_dir + "/config.json";
    if (!fs::exists(cfg_file)) {
        std::ofstream out(cfg_file);
        if (out.is_open()) {
            out << "{\n"
                << "    \"position\": \"top\",\n"
                << "    \"height\": 28,\n"
                << "    \"margin_top\": 4,\n"
                << "    \"margin_bottom\": 0,\n"
                << "    \"margin_left\": 12,\n"
                << "    \"margin_right\": 12,\n"
                << "    \"exclusive_zone\": true,\n"
                << "    \"workspaces\": {\n"
                << "        \"count\": 10,\n"
                << "        \"show_icons\": true\n"
                << "    },\n"
                << "    \"clock\": {\n"
                << "        \"format\": \"📅 %a %b %d  🕒 %H:%M\"\n"
                << "    },\n"
                << "    \"sys_info\": {\n"
                << "        \"update_interval_ms\": 1500,\n"
                << "        \"show_cpu\": true,\n"
                << "        \"show_ram\": true,\n"
                << "        \"show_battery\": true\n"
                << "    },\n"
                << "    \"wallpaper_dir\": \"~/Pictures/wallpapers\"\n"
                << "}\n";
            std::cout << "[ZenithConfig] Initialized default configuration at " << cfg_file << std::endl;
        }
    }
}

Config Config::load(const std::string& path) {
    ensure_default_config();

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
