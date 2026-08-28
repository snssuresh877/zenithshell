#include "theme/theme_engine.hpp"
#include "theme/theme_loader.hpp"
#include "theme/pywal_importer.hpp"
#include "theme/color_utils.hpp"
#include "theme/css_manager.hpp"
#include "gtk3_compat.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <array>
#include <unordered_set>

namespace fs = std::filesystem;

namespace zenith {

GtkCssProvider* ThemeEngine::theme_provider = nullptr;
Theme ThemeEngine::current_theme;
std::map<std::string, Theme> ThemeEngine::builtin_themes;
std::map<std::string, std::vector<std::string>> ThemeEngine::theme_wallpapers;
std::vector<std::string> ThemeEngine::custom_wallpapers;
std::string ThemeEngine::active_wallpaper_dir;
std::map<std::string, std::string> ThemeEngine::theme_descriptions;
std::map<std::string, std::string> ThemeEngine::theme_display_names;
std::vector<std::string> ThemeEngine::theme_order;
bool ThemeEngine::cached_dark_mode = true;
size_t ThemeEngine::current_wallpaper_idx = 0;

static std::string exec_cmd_read(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

static std::string hex_to_rgb(const std::string& hex) {
    return ColorUtils::get_rgb_csv(hex);
}

std::string ThemeEngine::format_theme_title(const std::string& raw) {
    if (raw == "dynamic") return "Dynamic (Wallpaper)";
    if (raw == "rose-pine") return "Rosé Pine";
    if (raw == "tokyo-night") return "Tokyo Night";
    if (raw == "catppuccin") return "Catppuccin Mocha";
    if (raw == "catppuccin-latte") return "Catppuccin Latte";
    if (raw == "flexoki-light") return "Flexoki Light";
    if (raw == "last-horizon") return "Last Horizon";
    if (raw == "matte-black") return "Matte Black";
    if (raw == "osaka-jade") return "Osaka Jade";
    if (raw == "retro-82") return "Retro '82";
    if (raw == "zenith-dark") return "Zenith Obsidian";

    std::string s = raw;
    std::replace(s.begin(), s.end(), '-', ' ');
    bool cap = true;
    for (char& c : s) {
        if (cap && std::isalpha(c)) {
            c = std::toupper(c);
            cap = false;
        } else if (c == ' ') {
            cap = true;
        }
    }
    return s;
}

std::string ThemeEngine::get_curated_desc(const std::string& raw) {
    if (raw == "dynamic") return "Wallpaper Adaptive Palette";
    if (raw == "kanagawa") return "Sumi Ink & Great Wave";
    if (raw == "rose-pine") return "Soho Dusk & Coral Glow";
    if (raw == "hackerman") return "Matrix Terminal Phosphor";
    if (raw == "everforest") return "Natural Earthy Pine";
    if (raw == "tokyo-night") return "Cyber Neon & Violet";
    if (raw == "catppuccin") return "Modern Pastel Slate";
    if (raw == "catppuccin-latte") return "Clean Daytime Pastel";
    if (raw == "gruvbox") return "Warm Retro Gold";
    if (raw == "nord") return "Cool Arctic Frost";
    if (raw == "dracula") return "Vibrant Purple & Pink";
    if (raw == "matte-black") return "True OLED Pitch Black";
    if (raw == "vantablack") return "Deep Void Obsidian";
    if (raw == "miasma") return "Atmospheric Toxic Swamp";
    if (raw == "osaka-jade") return "Japanese Shrine Jade";
    if (raw == "retro-82") return "Vintage 80s Synthwave";
    if (raw == "ristretto") return "Dark Espresso Roast";
    if (raw == "solitude") return "Quiet Velvet Ash";
    if (raw == "last-horizon") return "Twilight Sunset Horizon";
    if (raw == "lumon") return "Severance Cold Slate";
    if (raw == "lupine") return "Midnight Forest Blue";
    if (raw == "ethereal") return "Dreamy Pastel Velvet";
    if (raw == "flexoki-light") return "Inky Warm Editorial Paper";
    if (raw == "white") return "Pure Studio Paper";
    if (raw == "zenith-dark") return "Cyberpunk Obsidian Glass";
    return "Custom Palette";
}

void ThemeEngine::register_builtins() {
    // 1. Dynamic Wallpaper Adaptive Mode (Top of list)
    Theme dynamic_th;
    dynamic_th.name = "dynamic";
    dynamic_th.background = "#0e0f14";
    dynamic_th.surface = "#161720";
    dynamic_th.surface_variant = "#1e202c";
    dynamic_th.text_primary = "#F2F2F5";
    dynamic_th.text_secondary = "#9A9AAF";
    dynamic_th.text_disabled = "#5F6070";
    dynamic_th.text = "#F2F2F5";
    dynamic_th.text_muted = "#9A9AAF";
    dynamic_th.accent = "#A875FF";
    dynamic_th.accent_secondary = "#38bdf8";
    dynamic_th.border = "rgba(255, 255, 255, 0.08)";
    dynamic_th.semantic_success = "#3DDC84";
    dynamic_th.semantic_warning = "#FFB454";
    dynamic_th.semantic_error = "#FF5C6C";
    dynamic_th.semantic_info = "#55B9FF";
    builtin_themes["dynamic"] = dynamic_th;
    theme_display_names["dynamic"] = "Dynamic (Wallpaper)";
    theme_descriptions["dynamic"] = "Wallpaper Adaptive Palette";
    theme_order.push_back("dynamic");

    // 2. Zenith Obsidian
    Theme zdark;
    zdark.name = "zenith-dark";
    zdark.background = "#0e0f14";
    zdark.surface = "#161720";
    zdark.surface_variant = "#1e202c";
    zdark.text_primary = "#F2EEFF";
    zdark.text_secondary = "#A6A2C2";
    zdark.text_disabled = "#5F5C72";
    zdark.text = "#F2EEFF";
    zdark.text_muted = "#A6A2C2";
    zdark.accent = "#A875FF";
    zdark.accent_secondary = "#C084FC";
    zdark.border = "rgba(255, 255, 255, 0.08)";
    zdark.semantic_success = "#3DDC84";
    zdark.semantic_warning = "#FFB454";
    zdark.semantic_error = "#FF5C6C";
    zdark.semantic_info = "#55B9FF";
    builtin_themes[zdark.name] = zdark;
    theme_display_names[zdark.name] = "Zenith Obsidian";
    theme_descriptions[zdark.name] = "Cyberpunk Obsidian Glass";

    theme_order.push_back("zenith-dark");
}

void ThemeEngine::scan_packaged_themes() {
    std::vector<std::string> search_dirs = {
        std::string(g_get_user_config_dir()) + "/zenithshell/themes",
        std::string(g_get_home_dir()) + "/.local/share/zenithshell/themes",
        "/usr/share/zenithshell/themes",
        std::string(g_get_home_dir()) + "/themes"
    };

    for (const auto& base_dir : search_dirs) {
        if (!fs::exists(base_dir) || !fs::is_directory(base_dir)) continue;

        try {
            for (const auto& entry : fs::directory_iterator(base_dir)) {
                if (entry.is_directory()) {
                    std::string theme_path = entry.path().string();
                    std::vector<std::string> wallpapers;
                    auto loaded = ThemeLoader::load_theme_package(theme_path, wallpapers);
                    if (loaded) {
                        std::string tid = loaded->name;
                        builtin_themes[tid] = *loaded;
                        theme_wallpapers[tid] = wallpapers;
                        theme_display_names[tid] = format_theme_title(tid);
                        theme_descriptions[tid] = get_curated_desc(tid);

                        if (std::find(theme_order.begin(), theme_order.end(), tid) == theme_order.end()) {
                            theme_order.push_back(tid);
                        }
                    }
                }
            }
        } catch (...) {}
    }
}

void ThemeEngine::scan_custom_themes() {
    std::string config_home = g_get_user_config_dir();
    std::string themes_dir = config_home + "/zenithshell/themes";

    if (fs::exists(themes_dir)) {
        try {
            for (const auto& entry : fs::directory_iterator(themes_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".toml") {
                    auto loaded = ThemeLoader::load_from_file(entry.path().string());
                    if (loaded) {
                        builtin_themes[loaded->name] = *loaded;
                        theme_display_names[loaded->name] = format_theme_title(loaded->name);
                        theme_descriptions[loaded->name] = "Custom TOML Theme";
                        if (std::find(theme_order.begin(), theme_order.end(), loaded->name) == theme_order.end()) {
                            theme_order.push_back(loaded->name);
                        }
                    }
                }
            }
        } catch (...) {}
    }
}

void ThemeEngine::scan_custom_wallpapers(const std::string& custom_dir) {
    custom_wallpapers.clear();

    std::vector<std::string> search_dirs;
    if (!custom_dir.empty()) {
        std::string expanded = custom_dir;
        if (!expanded.empty() && expanded[0] == '~') {
            expanded = std::string(g_get_home_dir()) + expanded.substr(1);
        }
        search_dirs.push_back(expanded);
        active_wallpaper_dir = expanded;
    }

    search_dirs.push_back(std::string(g_get_home_dir()) + "/Pictures/wallpapers");
    search_dirs.push_back(std::string(g_get_home_dir()) + "/Pictures/Wallpapers");
    search_dirs.push_back(std::string(g_get_user_config_dir()) + "/zenithshell/wallpapers");

    std::unordered_set<std::string> seen;
    const std::vector<std::string> valid_exts = {".jpg", ".jpeg", ".png", ".webp", ".avif"};

    for (const auto& dir : search_dirs) {
        std::error_code ec;
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;

        if (active_wallpaper_dir.empty()) active_wallpaper_dir = dir;

        try {
            for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
                if (entry.is_regular_file(ec)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (std::find(valid_exts.begin(), valid_exts.end(), ext) != valid_exts.end()) {
                        std::string p = entry.path().string();
                        if (seen.find(p) == seen.end()) {
                            seen.insert(p);
                            custom_wallpapers.push_back(p);
                        }
                    }
                }
            }
        } catch (...) {}
    }

    std::sort(custom_wallpapers.begin(), custom_wallpapers.end());
    std::cout << "[ZenithTheme] Discovered " << custom_wallpapers.size() << " custom wallpapers from " << active_wallpaper_dir << std::endl;

    if (!custom_wallpapers.empty()) {
        theme_wallpapers["dynamic"] = custom_wallpapers;
        if (theme_wallpapers["zenith-dark"].empty()) {
            theme_wallpapers["zenith-dark"] = custom_wallpapers;
        }
    }
}

void ThemeEngine::set_wallpaper_directory(const std::string& dir_path) {
    scan_custom_wallpapers(dir_path);
}

std::vector<std::string> ThemeEngine::get_custom_wallpapers() {
    return custom_wallpapers;
}

void ThemeEngine::init(const std::string& default_theme, const std::string& custom_wallpaper_dir) {
    register_builtins();
    scan_packaged_themes();
    scan_custom_themes();
    scan_custom_wallpapers(custom_wallpaper_dir);

    std::string cs = exec_cmd_read("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null");
    cached_dark_mode = (cs.find("dark") != std::string::npos || cs.find("prefer-dark") != std::string::npos);

    set_theme(default_theme);
}

bool ThemeEngine::is_dark_mode() {
    return cached_dark_mode;
}

void ThemeEngine::set_dark_mode(bool dark) {
    cached_dark_mode = dark;
    if (dark) {
        system("gsettings set org.gnome.desktop.interface color-scheme 'prefer-dark' 2>/dev/null; gsettings set org.gnome.desktop.interface gtk-theme 'Orchis-Dark' 2>/dev/null &");
    } else {
        system("gsettings set org.gnome.desktop.interface color-scheme 'prefer-light' 2>/dev/null; gsettings set org.gnome.desktop.interface gtk-theme 'Orchis-Light' 2>/dev/null &");
    }
}

bool ThemeEngine::toggle_dark_mode() {
    set_dark_mode(!cached_dark_mode);
    return cached_dark_mode;
}

std::vector<ThemeInfo> ThemeEngine::get_theme_infos() {
    std::vector<ThemeInfo> infos;
    for (const auto& id : theme_order) {
        auto it = builtin_themes.find(id);
        if (it != builtin_themes.end()) {
            const auto& t = it->second;
            std::string dname = theme_display_names.count(id) ? theme_display_names[id] : format_theme_title(id);
            std::string desc = theme_descriptions.count(id) ? theme_descriptions[id] : "Color Palette";
            auto wps = theme_wallpapers.count(id) ? theme_wallpapers[id] : std::vector<std::string>{};
            infos.push_back({id, dname, desc, t.accent, t.accent_secondary, t.surface, wps});
        }
    }
    return infos;
}

std::vector<std::string> ThemeEngine::get_available_themes() {
    return theme_order;
}

std::string ThemeEngine::get_current_theme_name() {
    if (theme_display_names.count(current_theme.name)) {
        return theme_display_names[current_theme.name];
    }
    return format_theme_title(current_theme.name);
}

const Theme& ThemeEngine::get_current_theme() {
    return current_theme;
}

std::string ThemeEngine::get_current_wallpaper_path() {
    auto it = theme_wallpapers.find(current_theme.name);
    if (it != theme_wallpapers.end() && !it->second.empty()) {
        if (current_wallpaper_idx >= it->second.size()) current_wallpaper_idx = 0;
        return it->second[current_wallpaper_idx];
    }
    std::error_code ec;
    std::string default_bg = std::string(g_get_home_dir()) + "/.local/state/zenithshell/current/background";
    if (fs::exists(default_bg, ec)) {
        return default_bg;
    }
    return "";
}

std::string ThemeEngine::get_current_wallpaper_name() {
    std::string p = get_current_wallpaper_path();
    if (!p.empty()) {
        fs::path f(p);
        return f.stem().string();
    }
    return "Default Wallpaper";
}

void ThemeEngine::set_wallpaper(const std::string& path) {
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) return;

    std::string state_dir = std::string(g_get_home_dir()) + "/.local/state/zenithshell/current";
    std::string bg_link = state_dir + "/background";
    if (path == bg_link) return;

    // Apply wallpaper with awww transition
    std::string cmd = "awww img \"" + path + "\" --transition-type grow --transition-duration 0.4 --transition-fps 30 2>/dev/null &";
    system(cmd.c_str());

    // Update current background symlink
    try {
        fs::create_directories(state_dir, ec);
        fs::remove(bg_link, ec);
        fs::create_symlink(path, bg_link, ec);
    } catch (...) {}

    // Run Pywal quietly
    std::string wal_cmd = "wal -n -q -i \"" + path + "\" 2>/dev/null";
    system(wal_cmd.c_str());

    // If current theme is dynamic, re-import after wallpaper change
    if (current_theme.name == "dynamic") {
        auto wal_theme = PywalImporter::import_from_cache();
        if (wal_theme) {
            current_theme = *wal_theme;
            builtin_themes["dynamic"] = *wal_theme;
            apply_theme(current_theme);
        }
    }
}

void ThemeEngine::cycle_wallpaper() {
    auto it = theme_wallpapers.find(current_theme.name);
    if (it != theme_wallpapers.end() && !it->second.empty()) {
        current_wallpaper_idx = (current_wallpaper_idx + 1) % it->second.size();
        set_wallpaper(it->second[current_wallpaper_idx]);
    } else if (!custom_wallpapers.empty()) {
        current_wallpaper_idx = (current_wallpaper_idx + 1) % custom_wallpapers.size();
        set_wallpaper(custom_wallpapers[current_wallpaper_idx]);
    } else {
        std::vector<std::string> all_wps;
        for (const auto& pair : theme_wallpapers) {
            all_wps.insert(all_wps.end(), pair.second.begin(), pair.second.end());
        }
        if (!all_wps.empty()) {
            current_wallpaper_idx = (current_wallpaper_idx + 1) % all_wps.size();
            set_wallpaper(all_wps[current_wallpaper_idx]);
        }
    }
}

void ThemeEngine::set_theme(const std::string& theme_name) {
    if (theme_name == "dynamic" || theme_name == "pywal") {
        std::string cur_wp = get_current_wallpaper_path();
        if (cur_wp.empty()) {
            cur_wp = std::string(g_get_home_dir()) + "/.local/state/zenithshell/current/background";
        }
        if (fs::exists(cur_wp)) {
            std::string wal_cmd = "wal -n -q -i \"" + cur_wp + "\" 2>/dev/null";
            system(wal_cmd.c_str());
        }
        auto wal_theme = PywalImporter::import_from_cache();
        if (wal_theme) {
            current_theme = *wal_theme;
            builtin_themes["dynamic"] = *wal_theme;
            apply_theme(current_theme);
            return;
        }
    }

    auto it = builtin_themes.find(theme_name);
    if (it != builtin_themes.end()) {
        current_theme = it->second;
    } else {
        std::string config_home = g_get_user_config_dir();
        std::string theme_path = config_home + "/zenithshell/themes/" + theme_name + ".toml";
        auto loaded = ThemeLoader::load_from_file(theme_path);
        if (loaded) {
            current_theme = *loaded;
            builtin_themes[theme_name] = *loaded;
        } else {
            current_theme = builtin_themes["zenith-dark"];
        }
    }

    apply_theme(current_theme);

    // Apply matching theme wallpaper
    current_wallpaper_idx = 0;
    std::string wp = get_current_wallpaper_path();
    if (!wp.empty()) {
        set_wallpaper(wp);
    }

    // Trigger user hook script if present
    std::string hook = std::string(g_get_user_config_dir()) + "/zenithshell/hooks/on_theme_change.sh";
    if (fs::exists(hook)) {
        std::string hook_cmd = hook + " \"" + current_theme.name + "\" 2>/dev/null &";
        system(hook_cmd.c_str());
    }
}

std::string ThemeEngine::cycle_next_theme() {
    if (theme_order.empty()) return current_theme.name;

    auto it = std::find(theme_order.begin(), theme_order.end(), current_theme.name);
    if (it == theme_order.end() || it + 1 == theme_order.end()) {
        set_theme(theme_order.front());
        return theme_order.front();
    } else {
        set_theme(*(it + 1));
        return *(it + 1);
    }
}

void ThemeEngine::reload_current_theme() {
    set_theme(current_theme.name);
}

void ThemeEngine::apply_theme(const Theme& t) {
    if (!theme_provider) {
        theme_provider = gtk_css_provider_new();
        GdkScreen* screen = gdk_screen_get_default();
        if (screen) {
            gtk_style_context_add_provider_for_screen(
                screen,
                GTK_STYLE_PROVIDER(theme_provider),
                GTK_STYLE_PROVIDER_PRIORITY_USER + 100
            );
        }
    }

    std::stringstream css;
    css << "/* Zenith Master Design System Dynamic Tokens: " << t.name << " */\n";
    css << "@define-color zenith_bg " << t.background << ";\n";
    css << "@define-color zenith_surface " << t.surface << ";\n";
    css << "@define-color zenith_surface_variant " << t.surface_variant << ";\n";
    css << "@define-color zenith_border " << t.border << ";\n";
    css << "@define-color zenith_border_hover rgba(" << hex_to_rgb(t.accent) << ", 0.40);\n";
    css << "@define-color zenith_border_focus " << t.accent << ";\n";
    css << "@define-color zenith_text_primary " << t.text_primary << ";\n";
    css << "@define-color zenith_text_secondary " << t.text_secondary << ";\n";
    css << "@define-color zenith_text_disabled " << t.text_disabled << ";\n";
    css << "@define-color zenith_accent " << t.accent << ";\n";
    css << "@define-color zenith_accent_sec " << t.accent_secondary << ";\n";
    css << "@define-color zenith_accent_soft rgba(" << hex_to_rgb(t.accent) << ", 0.16);\n";
    css << "@define-color zenith_accent_glow rgba(" << hex_to_rgb(t.accent) << ", 0.30);\n";
    css << "@define-color zenith_success #3DDC84;\n";
    css << "@define-color zenith_warning #FFB454;\n";
    css << "@define-color zenith_error #FF5C6C;\n";
    css << "@define-color zenith_info #55B9FF;\n\n";

    css << "* {\n"
        << "  -zenith-bg: " << t.background << ";\n"
        << "  -zenith-surface: " << t.surface << ";\n"
        << "  -zenith-surface-variant: " << t.surface_variant << ";\n"
        << "  -zenith-surface-hover: rgba(" << hex_to_rgb(t.accent) << ", 0.12);\n"
        << "  -zenith-border: " << t.border << ";\n"
        << "  -zenith-border-hover: rgba(" << hex_to_rgb(t.accent) << ", 0.40);\n"
        << "  -zenith-border-focus: " << t.accent << ";\n"
        << "  -zenith-text-primary: " << t.text_primary << ";\n"
        << "  -zenith-text-secondary: " << t.text_secondary << ";\n"
        << "  -zenith-text-disabled: " << t.text_disabled << ";\n"
        << "  -zenith-text: " << t.text_primary << ";\n"
        << "  -zenith-text-muted: " << t.text_secondary << ";\n"
        << "  -zenith-accent: " << t.accent << ";\n"
        << "  -zenith-accent-rgb: " << hex_to_rgb(t.accent) << ";\n"
        << "  -zenith-accent-sec: " << t.accent_secondary << ";\n"
        << "  -zenith-accent-soft: rgba(" << hex_to_rgb(t.accent) << ", 0.16);\n"
        << "  -zenith-success: #3DDC84;\n"
        << "  -zenith-warning: #FFB454;\n"
        << "  -zenith-error: #FF5C6C;\n"
        << "  -zenith-info: #55B9FF;\n"
        << "}\n\n";

    std::string css_str = css.str();
    GError* error = nullptr;
    gtk_css_provider_load_from_data(theme_provider, css_str.c_str(), -1, &error);
    if (error) {
        g_error_free(error);
    }

    CssManager::reload();
}

} // namespace zenith
