#include "theme/pywal_importer.hpp"
#include "theme/color_utils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <glib.h>
#include <map>

namespace fs = std::filesystem;

namespace zenith {

static std::string get_default_wal_path() {
    const char* home = g_get_home_dir();
    return std::string(home) + "/.cache/wal/colors.json";
}

bool PywalImporter::is_available() {
    return fs::exists(get_default_wal_path());
}

std::optional<Theme> PywalImporter::import_from_cache(const std::string& custom_path) {
    std::string path = custom_path.empty() ? get_default_wal_path() : custom_path;
    if (!fs::exists(path)) {
        std::cerr << "[PywalImporter] Colors file not found: " << path << std::endl;
        return std::nullopt;
    }

    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    auto extract_color = [&](const std::string& key) -> std::string {
        std::string pattern = "\"" + key + "\": \"";
        size_t pos = content.find(pattern);
        if (pos == std::string::npos) {
            pattern = "\"" + key + "\":\"";
            pos = content.find(pattern);
            if (pos == std::string::npos) return "";
        }
        size_t start = pos + pattern.length();
        size_t end = content.find("\"", start);
        if (end == std::string::npos) return "";
        return content.substr(start, end - start);
    };

    std::string bg = extract_color("background");
    std::string fg = extract_color("foreground");
    std::string c0 = extract_color("color0");
    std::string c1 = extract_color("color1");
    std::string c2 = extract_color("color2");
    std::string c3 = extract_color("color3");
    std::string c4 = extract_color("color4");
    std::string c5 = extract_color("color5");
    std::string c6 = extract_color("color6");
    std::string c7 = extract_color("color7");
    std::string c8 = extract_color("color8");

    if (bg.empty() || fg.empty()) {
        std::cerr << "[PywalImporter] Failed to parse background or foreground from " << path << std::endl;
        return std::nullopt;
    }

    std::string raw_accent = !c4.empty() ? c4 : (!c6.empty() ? c6 : (!c1.empty() ? c1 : fg));
    std::string raw_sec = !c6.empty() ? c6 : (!c2.empty() ? c2 : raw_accent);
    std::string seed_bg = !c0.empty() ? c0 : bg;
    std::string seed_fg = !fg.empty() ? fg : (!c7.empty() ? c7 : "#FFFFFF");

    Theme theme;
    theme.name = "dynamic";

    // 1. Surfaces: Keep deep dark with subtle wallpaper mood
    theme.background = ColorUtils::derive_dark_background(seed_bg);
    theme.surface = ColorUtils::derive_dark_surface(seed_bg);
    theme.surface_variant = ColorUtils::derive_dark_surface_variant(seed_bg);
    theme.border = "rgba(255, 255, 255, 0.08)";

    // 2. Text: Wallpaper-derived + contrast-corrected
    theme.text_primary = ColorUtils::derive_text_primary(seed_fg);
    theme.text_secondary = ColorUtils::derive_text_secondary(seed_fg);
    theme.text_disabled = ColorUtils::derive_text_disabled(seed_fg);
    theme.text = theme.text_primary;
    theme.text_muted = theme.text_secondary;

    // 3. Accent: Wallpaper-derived with constrained saturation
    theme.accent = ColorUtils::sanitize_accent(raw_accent);
    theme.accent_secondary = ColorUtils::sanitize_accent(raw_sec);

    // 4. Fixed Semantic Colors (Constant & Recognizable)
    theme.semantic_success = "#3DDC84";
    theme.semantic_warning = "#FFB454";
    theme.semantic_error = "#FF5C6C";
    theme.semantic_info = "#55B9FF";
    theme.success = "#3DDC84";
    theme.warning = "#FFB454";
    theme.error = "#FF5C6C";

    std::cout << "[PywalImporter] Derived dynamic theme: bg=" << theme.background 
              << ", text=" << theme.text_primary 
              << ", accent=" << theme.accent << std::endl;

    return theme;
}

} // namespace zenith
