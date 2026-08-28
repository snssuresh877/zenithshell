#pragma once

#include "theme/theme.hpp"
#include "theme/theme_loader.hpp"
#include "theme/pywal_importer.hpp"
#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <map>

namespace zenith {

struct ThemeInfo {
    std::string id;
    std::string name;
    std::string desc;
    std::string c_accent;
    std::string c_secondary;
    std::string c_surface;
    std::vector<std::string> wallpapers;
};

class ThemeEngine {
public:
    static void init(const std::string& default_theme = "zenith-dark", const std::string& custom_wallpaper_dir = "");
    static void set_theme(const std::string& theme_name);
    static std::string cycle_next_theme();
    static std::string get_current_theme_name();
    static const Theme& get_current_theme();
    static std::vector<std::string> get_available_themes();
    static std::vector<ThemeInfo> get_theme_infos();
    static void reload_current_theme();

    // Wallpaper Management & Synchronization
    static void cycle_wallpaper();
    static std::string get_current_wallpaper_name();
    static std::string get_current_wallpaper_path();
    static void set_wallpaper(const std::string& path);
    static void set_wallpaper_directory(const std::string& dir_path);
    static std::vector<std::string> get_custom_wallpapers();
    static void scan_custom_wallpapers(const std::string& custom_dir = "");

    // Global Appearance / Dark Mode
    static bool is_dark_mode();
    static void set_dark_mode(bool dark);
    static bool toggle_dark_mode();

private:
    static Theme current_theme;
    static std::map<std::string, Theme> builtin_themes;
    static std::map<std::string, std::vector<std::string>> theme_wallpapers;
    static std::vector<std::string> custom_wallpapers;
    static std::string active_wallpaper_dir;
    static std::map<std::string, std::string> theme_descriptions;
    static std::map<std::string, std::string> theme_display_names;
    static std::vector<std::string> theme_order;
    static GtkCssProvider* theme_provider;
    static bool cached_dark_mode;
    static size_t current_wallpaper_idx;

    static void register_builtins();
    static void apply_theme(const Theme& theme);
    static void scan_custom_themes();
    static void scan_packaged_themes();
    static std::string format_theme_title(const std::string& raw_name);
    static std::string get_curated_desc(const std::string& raw_name);
};

} // namespace zenith
