#include "shell/files/custom_actions.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <fnmatch.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <regex>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace zenith {

std::vector<CustomAction> CustomActionsManager::actions;

std::string CustomActionsManager::get_config_path() {
    const char* config_home = g_get_user_config_dir();
    if (!config_home) return "";
    std::string dir = std::string(config_home) + "/zenithshell";
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
    return dir + "/custom_actions.json";
}

void CustomActionsManager::write_default_config() {
    std::string path = get_config_path();
    if (path.empty() || fs::exists(path)) return;

    json j = json::array();
    
    j.push_back({
        {"id", "set_wallpaper"},
        {"name", "Set as Wallpaper"},
        {"icon", "preferences-desktop-wallpaper-symbolic"},
        {"command", "zenithctl wallpaper set \"%f\""},
        {"patterns", {"*.jpg", "*.png", "*.jpeg", "*.webp"}}
    });

    j.push_back({
        {"id", "edit_as_root"},
        {"name", "Edit as Root"},
        {"icon", "text-editor-symbolic"},
        {"command", "pkexec gedit \"%f\""},
        {"patterns", {"*.txt", "*.conf", "*.json", "*.ini", "*.cpp", "*.hpp", "*.c", "*.h"}}
    });

    std::ofstream out(path);
    if (out.is_open()) {
        out << j.dump(4);
    }
}

void CustomActionsManager::init() {
    write_default_config();
    reload();
}

void CustomActionsManager::reload() {
    actions.clear();
    std::string path = get_config_path();
    if (path.empty() || !fs::exists(path)) return;

    try {
        std::ifstream in(path);
        json j;
        in >> j;

        if (j.is_array()) {
            for (const auto& item : j) {
                CustomAction action;
                if (item.contains("id") && item["id"].is_string()) action.id = item["id"].get<std::string>();
                if (item.contains("name") && item["name"].is_string()) action.name = item["name"].get<std::string>();
                if (item.contains("icon") && item["icon"].is_string()) action.icon = item["icon"].get<std::string>();
                if (item.contains("command") && item["command"].is_string()) action.command = item["command"].get<std::string>();
                if (item.contains("patterns") && item["patterns"].is_array()) {
                    for (const auto& p : item["patterns"]) {
                        if (p.is_string()) action.patterns.push_back(p.get<std::string>());
                    }
                }
                actions.push_back(action);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[CustomActions] Failed to load config: " << e.what() << std::endl;
    }
}

std::vector<CustomAction> CustomActionsManager::get_actions_for(const std::vector<std::string>& selected_paths) {
    std::vector<CustomAction> result;
    if (selected_paths.empty()) return result;

    for (const auto& action : actions) {
        bool all_match = true;
        for (const auto& path : selected_paths) {
            std::string filename = fs::path(path).filename().string();
            bool match = false;
            for (const auto& pattern : action.patterns) {
                if (pattern == "*" || pattern == "*.*") {
                    match = true;
                    break;
                }
                if (fnmatch(pattern.c_str(), filename.c_str(), FNM_CASEFOLD) == 0) {
                    match = true;
                    break;
                }
            }
            if (!match) {
                all_match = false;
                break;
            }
        }
        if (all_match) {
            result.push_back(action);
        }
    }
    return result;
}

void CustomActionsManager::execute_action(const CustomAction& action, const std::vector<std::string>& selected_paths) {
    if (selected_paths.empty() || action.command.empty()) return;

    std::string cmd = action.command;
    
    // Replace %f with the first file (shell escaped)
    std::string first_escaped = g_shell_quote(selected_paths[0].c_str());
    size_t pos = 0;
    while ((pos = cmd.find("%f", pos)) != std::string::npos) {
        cmd.replace(pos, 2, first_escaped);
        pos += first_escaped.length();
    }
    
    // Replace %d with the current directory
    std::string current_dir = fs::path(selected_paths[0]).parent_path().string();
    std::string dir_escaped = g_shell_quote(current_dir.c_str());
    pos = 0;
    while ((pos = cmd.find("%d", pos)) != std::string::npos) {
        cmd.replace(pos, 2, dir_escaped);
        pos += dir_escaped.length();
    }

    // Replace %F with space-separated all files
    std::string all_escaped;
    for (size_t i = 0; i < selected_paths.size(); ++i) {
        if (i > 0) all_escaped += " ";
        all_escaped += g_shell_quote(selected_paths[i].c_str());
    }
    pos = 0;
    while ((pos = cmd.find("%F", pos)) != std::string::npos) {
        cmd.replace(pos, 2, all_escaped);
        pos += all_escaped.length();
    }

    // Execute in background
    std::string full_cmd = cmd + " &";
    system(full_cmd.c_str());
}

} // namespace zenith
