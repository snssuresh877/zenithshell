#include "theme/theme_loader.hpp"
#include "theme/color_utils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <map>
#include <glib.h>

namespace fs = std::filesystem;

namespace zenith {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string unquote(const std::string& str) {
    std::string s = trim(str);
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string ThemeLoader::get_themes_dir() {
    const char* config_home = g_get_user_config_dir();
    std::string dir = std::string(config_home) + "/zenithshell/themes";
    if (!fs::exists(dir)) {
        try {
            fs::create_directories(dir);
        } catch (...) {}
    }
    return dir;
}

std::vector<std::string> ThemeLoader::list_custom_themes() {
    std::vector<std::string> list;
    std::string dir = get_themes_dir();
    if (!fs::exists(dir)) return list;

    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && (entry.path().extension() == ".toml" || entry.path().extension() == ".conf")) {
                list.push_back(entry.path().stem().string());
            }
        }
    } catch (...) {}
    return list;
}

std::optional<Theme> ThemeLoader::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return std::nullopt;
    }

    Theme theme;
    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = unquote(line.substr(eq + 1));

        if (key == "name") theme.name = val;
        else if (key == "background") theme.background = val;
        else if (key == "surface") theme.surface = val;
        else if (key == "surface_variant") theme.surface_variant = val;
        else if (key == "text") theme.text = val;
        else if (key == "text_muted") theme.text_muted = val;
        else if (key == "accent") theme.accent = val;
        else if (key == "accent_secondary") theme.accent_secondary = val;
        else if (key == "border") theme.border = val;
        else if (key == "success") theme.success = val;
        else if (key == "warning") theme.warning = val;
        else if (key == "error") theme.error = val;
    }

    return theme;
}

std::optional<Theme> ThemeLoader::load_omarchy_theme(const std::string& theme_dir, std::vector<std::string>& out_wallpapers) {
    out_wallpapers.clear();
    std::string colors_file = theme_dir + "/colors.toml";
    std::ifstream file(colors_file);
    if (!file.is_open()) {
        return std::nullopt;
    }

    Theme theme;
    fs::path p(theme_dir);
    theme.name = p.filename().string();

    std::map<std::string, std::string> kv;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = unquote(line.substr(eq + 1));
        kv[key] = val;
    }

    std::string raw_bg = kv.count("background") ? kv["background"] : "#0b0c10";
    std::string raw_fg = kv.count("foreground") ? kv["foreground"] : "#f8f8f2";
    std::string raw_accent = kv.count("accent") ? kv["accent"] : (kv.count("blue") ? kv["blue"] : (kv.count("magenta") ? kv["magenta"] : "#bd93f9"));
    std::string raw_sec = kv.count("magenta") ? kv["magenta"] : (kv.count("cyan") ? kv["cyan"] : (kv.count("orange") ? kv["orange"] : raw_accent));

    // 1. Surfaces: Deep dark with subtle palette mood
    theme.background = ColorUtils::derive_dark_background(raw_bg);
    theme.surface = ColorUtils::derive_dark_surface(raw_bg);
    theme.surface_variant = ColorUtils::derive_dark_surface_variant(raw_bg);

    // 2. Text: Palette-derived + contrast-corrected
    theme.text_primary = ColorUtils::derive_text_primary(raw_fg);
    theme.text_secondary = ColorUtils::derive_text_secondary(raw_fg);
    theme.text_disabled = ColorUtils::derive_text_disabled(raw_fg);
    theme.text = theme.text_primary;
    theme.text_muted = theme.text_secondary;

    // 3. Accent: Palette-derived with constrained saturation
    theme.accent = ColorUtils::sanitize_accent(raw_accent);
    theme.accent_secondary = ColorUtils::sanitize_accent(raw_sec);

    theme.border = "rgba(255, 255, 255, 0.08)";

    // 4. FIXED SEMANTIC COLORS (Never overridden by theme palettes)
    theme.semantic_success = "#3DDC84";
    theme.semantic_warning = "#FFB454";
    theme.semantic_error = "#FF5C6C";
    theme.semantic_info = "#55B9FF";
    theme.success = "#3DDC84";
    theme.warning = "#FFB454";
    theme.error = "#FF5C6C";

    // Scan backgrounds directory
    std::string bg_dir = theme_dir + "/backgrounds";
    if (fs::exists(bg_dir) && fs::is_directory(bg_dir)) {
        try {
            for (const auto& entry : fs::directory_iterator(bg_dir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp") {
                        std::string fname = entry.path().filename().string();
                        // Filter out tiny logo icons
                        if (fname != "omarchy.png") {
                            out_wallpapers.push_back(entry.path().string());
                        }
                    }
                }
            }
            std::sort(out_wallpapers.begin(), out_wallpapers.end());
        } catch (...) {}
    }

    return theme;
}

bool ThemeLoader::save_to_file(const Theme& theme, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "# ZenithShell Theme Definition\n";
    file << "name = \"" << theme.name << "\"\n\n";
    file << "[colors]\n";
    file << "background = \"" << theme.background << "\"\n";
    file << "surface = \"" << theme.surface << "\"\n";
    file << "surface_variant = \"" << theme.surface_variant << "\"\n";
    file << "text = \"" << theme.text << "\"\n";
    file << "text_muted = \"" << theme.text_muted << "\"\n";
    file << "accent = \"" << theme.accent << "\"\n";
    file << "accent_secondary = \"" << theme.accent_secondary << "\"\n";
    file << "border = \"" << theme.border << "\"\n";
    file << "success = \"" << theme.success << "\"\n";
    file << "warning = \"" << theme.warning << "\"\n";
    file << "error = \"" << theme.error << "\"\n";

    return true;
}

} // namespace zenith
