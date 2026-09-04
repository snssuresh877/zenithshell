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
#include <fstream>

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
    auto add_builtin = [&](const std::string& name, const std::string& title, const std::string& desc,
                           const std::string& bg, const std::string& surf, const std::string& surf_var,
                           const std::string& txt, const std::string& txt_sec, const std::string& accent,
                           const std::string& sec_accent) {
        Theme t;
        t.name = name;
        t.background = bg;
        t.surface = surf;
        t.surface_variant = surf_var;
        t.text_primary = txt;
        t.text_secondary = txt_sec;
        t.text_disabled = ColorUtils::derive_text_disabled(txt_sec);
        t.text = txt;
        t.text_muted = txt_sec;
        t.accent = accent;
        t.accent_secondary = sec_accent;
        t.border = "rgba(255, 255, 255, 0.08)";
        t.semantic_success = "#3DDC84";
        t.semantic_warning = "#FFB454";
        t.semantic_error = "#FF5C6C";
        t.semantic_info = "#55B9FF";
        t.success = "#3DDC84";
        t.warning = "#FFB454";
        t.error = "#FF5C6C";

        builtin_themes[name] = t;
        theme_display_names[name] = title;
        theme_descriptions[name] = desc;
        if (std::find(theme_order.begin(), theme_order.end(), name) == theme_order.end()) {
            theme_order.push_back(name);
        }
    };

    // 1. Dynamic Wallpaper Mode (Pywal Adaptive)
    add_builtin("dynamic", "Dynamic (Wallpaper)", "Wallpaper Adaptive Palette",
                "#0e0f14", "#161720", "#1e202c", "#F2F2F5", "#9A9AAF", "#A875FF", "#38bdf8");

    // 2. Zenith Obsidian
    add_builtin("zenith-dark", "Zenith Obsidian", "Cyberpunk Obsidian Glass",
                "#0e0f14", "#161720", "#1e202c", "#F2EEFF", "#A6A2C2", "#A875FF", "#C084FC");

    // 3. Kanagawa
    add_builtin("kanagawa", "Kanagawa", "Sumi Ink & Great Wave",
                "#1f1f28", "#2a2a37", "#363646", "#dcd7ba", "#957fb8", "#7e9cd8", "#98bb6c");

    // 4. Rosé Pine
    add_builtin("rose-pine", "Rosé Pine", "Soho Dusk & Coral Glow",
                "#191724", "#1f1d2e", "#26233a", "#e0def4", "#908caa", "#ebbcba", "#31748f");

    // 5. Hackerman (Matrix)
    add_builtin("hackerman", "Hackerman", "Matrix Terminal Phosphor",
                "#0d1117", "#161b22", "#21262d", "#e6edf3", "#8b949e", "#00ff66", "#39d353");

    // 6. Everforest
    add_builtin("everforest", "Everforest", "Natural Earthy Pine",
                "#272e33", "#2e383e", "#374145", "#d3c6aa", "#9da9a0", "#a7c080", "#7fbbb3");

    // 7. Tokyo Night
    add_builtin("tokyo-night", "Tokyo Night", "Cyber Neon & Violet",
                "#1a1b26", "#24283b", "#2f3549", "#c0caf5", "#7982a9", "#7aa2f7", "#bb9af7");

    // 8. Catppuccin Mocha
    add_builtin("catppuccin", "Catppuccin Mocha", "Modern Pastel Slate",
                "#1e1e2e", "#181825", "#313244", "#cdd6f4", "#a6adc8", "#cba6f7", "#89b4fa");

    // 9. Catppuccin Latte
    add_builtin("catppuccin-latte", "Catppuccin Latte", "Clean Daytime Pastel",
                "#eff1f5", "#e6e9ef", "#dce0e8", "#4c4f69", "#6c6f85", "#8839ef", "#1e66f5");

    // 10. Gruvbox
    add_builtin("gruvbox", "Gruvbox", "Warm Retro Gold",
                "#282828", "#3c3836", "#504945", "#ebdbb2", "#a89984", "#fabd2f", "#fe8019");

    // 11. Nord
    add_builtin("nord", "Nord", "Cool Arctic Frost",
                "#2e3440", "#3b4252", "#434c5e", "#eceff4", "#d8dee9", "#88c0d0", "#81a1c1");

    // 12. Dracula
    add_builtin("dracula", "Dracula", "Vibrant Purple & Pink",
                "#282a36", "#44475a", "#6272a4", "#f8f8f2", "#6272a4", "#bd93f9", "#ff79c6");

    // 13. Matte Black
    add_builtin("matte-black", "Matte Black", "True OLED Pitch Black",
                "#000000", "#0c0c0c", "#181818", "#f2f2f2", "#888888", "#ffffff", "#aaaaaa");

    // 14. Vantablack
    add_builtin("vantablack", "Vantablack", "Deep Void Obsidian",
                "#050505", "#0d0d0d", "#1a1a1a", "#eaeaea", "#757575", "#ff0055", "#ff5500");

    // 15. Miasma
    add_builtin("miasma", "Miasma", "Atmospheric Toxic Swamp",
                "#222222", "#2b2b2b", "#363636", "#c2c2b0", "#78824b", "#a28a5b", "#bb7744");

    // 16. Osaka Jade
    add_builtin("osaka-jade", "Osaka Jade", "Japanese Shrine Jade",
                "#141c18", "#1c2621", "#26332d", "#e8f0ec", "#8fa89b", "#52b788", "#74c69d");

    // 17. Retro '82
    add_builtin("retro-82", "Retro '82", "Vintage 80s Synthwave",
                "#1a102f", "#241742", "#32215a", "#fbf1c7", "#928374", "#ff71ce", "#01cdfe");

    // 18. Ristretto
    add_builtin("ristretto", "Ristretto", "Dark Espresso Roast",
                "#2c2523", "#362f2d", "#433b38", "#f0e6df", "#a89984", "#d4a373", "#e09f67");

    // 19. Solitude
    add_builtin("solitude", "Solitude", "Quiet Velvet Ash",
                "#1c1d21", "#25262c", "#30323a", "#e0e2ec", "#9094a6", "#9fa8da", "#b0bec5");

    // 20. Last Horizon
    add_builtin("last-horizon", "Last Horizon", "Twilight Sunset Horizon",
                "#181424", "#221c33", "#2e2645", "#f5e6eb", "#a391a8", "#ff7b00", "#ff0055");

    // 21. Lumon
    add_builtin("lumon", "Lumon", "Severance Cold Slate",
                "#1b2028", "#232a35", "#2d3744", "#e2e8f0", "#94a3b8", "#0ea5e9", "#38bdf8");

    // 22. Lupine
    add_builtin("lupine", "Lupine", "Midnight Forest Blue",
                "#0f172a", "#1e293b", "#334155", "#f8fafc", "#94a3b8", "#38bdf8", "#818cf8");

    // 23. Ethereal
    add_builtin("ethereal", "Ethereal", "Dreamy Pastel Velvet",
                "#1e1b2e", "#28243d", "#353052", "#f3e8ff", "#c084fc", "#d8b4fe", "#f472b6");

    // 24. Flexoki Light
    add_builtin("flexoki-light", "Flexoki Light", "Inky Warm Editorial Paper",
                "#fffcf0", "#f2f0e5", "#e6e4d9", "#100f0f", "#6f6e69", "#205ea6", "#871094");
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

    // 1. Check for previously saved wallpaper across reboots
    std::string saved_wp = get_current_wallpaper_path();

    // 2. Set theme
    set_theme(default_theme);

    // 3. If a saved wallpaper exists, restore it directly on boot
    if (!saved_wp.empty() && fs::exists(saved_wp)) {
        set_wallpaper(saved_wp);
    }
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
    std::error_code ec;
    std::string current_dir = std::string(g_get_home_dir()) + "/.local/state/zenithshell/current";
    std::string state_file = current_dir + "/wallpaper_path.txt";

    // 1. Check persistent wallpaper state file
    if (fs::exists(state_file, ec)) {
        std::ifstream in(state_file);
        std::string saved_path;
        if (in.is_open() && std::getline(in, saved_path)) {
            while (!saved_path.empty() && (saved_path.back() == '\r' || saved_path.back() == '\n' || saved_path.back() == ' ')) {
                saved_path.pop_back();
            }
            if (!saved_path.empty() && fs::exists(saved_path, ec)) {
                return saved_path;
            }
        }
    }

    // 2. Check symlink ~/.local/state/zenithshell/current/background
    std::string default_bg = current_dir + "/background";
    if (fs::exists(default_bg, ec)) {
        std::string target = fs::canonical(default_bg, ec).string();
        if (!target.empty() && fs::exists(target, ec)) {
            return target;
        }
    }

    // 3. Check Pywal saved cache ~/.cache/wal/wal
    std::string wal_file = std::string(g_get_home_dir()) + "/.cache/wal/wal";
    if (fs::exists(wal_file, ec)) {
        std::ifstream in(wal_file);
        std::string wal_path;
        if (in.is_open() && std::getline(in, wal_path)) {
            while (!wal_path.empty() && (wal_path.back() == '\r' || wal_path.back() == '\n' || wal_path.back() == ' ')) {
                wal_path.pop_back();
            }
            if (!wal_path.empty() && fs::exists(wal_path, ec)) {
                return wal_path;
            }
        }
    }

    // 4. Fallback to active theme's wallpaper list
    auto it = theme_wallpapers.find(current_theme.name);
    if (it != theme_wallpapers.end() && !it->second.empty()) {
        if (current_wallpaper_idx >= it->second.size()) current_wallpaper_idx = 0;
        return it->second[current_wallpaper_idx];
    }
    if (!custom_wallpapers.empty()) {
        if (current_wallpaper_idx >= custom_wallpapers.size()) current_wallpaper_idx = 0;
        return custom_wallpapers[current_wallpaper_idx];
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

    std::string state_dir = std::string(g_get_home_dir()) + "/.local/state/zenithshell";
    std::string current_dir = state_dir + "/current";
    fs::create_directories(current_dir, ec);

    // Write persistent wallpaper path to text file
    std::string state_file = current_dir + "/wallpaper_path.txt";
    std::ofstream out(state_file, std::ios::trunc);
    if (out.is_open()) {
        out << path << "\n";
        out.close();
    }

    // Update current background symlink
    std::string bg_link = current_dir + "/background";
    try {
        fs::remove(bg_link, ec);
        fs::create_symlink(path, bg_link, ec);
    } catch (...) {}

    // Update active index in theme and custom lists
    auto it = theme_wallpapers.find(current_theme.name);
    if (it != theme_wallpapers.end()) {
        auto pos = std::find(it->second.begin(), it->second.end(), path);
        if (pos != it->second.end()) {
            current_wallpaper_idx = std::distance(it->second.begin(), pos);
        }
    }
    if (!custom_wallpapers.empty()) {
        auto pos = std::find(custom_wallpapers.begin(), custom_wallpapers.end(), path);
        if (pos != custom_wallpapers.end()) {
            current_wallpaper_idx = std::distance(custom_wallpapers.begin(), pos);
        }
    }

    // Apply wallpaper with awww daemon verification and transition
    std::string cmd = "pgrep -x awww-daemon >/dev/null 2>&1 || awww-daemon & sleep 0.05; awww img \"" + path + "\" --transition-type grow --transition-duration 0.4 --transition-fps 30 2>/dev/null &";
    system(cmd.c_str());

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

    // Apply matching theme wallpaper if theme has distinct packaged wallpapers
    auto wp_it = theme_wallpapers.find(theme_name);
    if (wp_it != theme_wallpapers.end() && !wp_it->second.empty() && wp_it->second != custom_wallpapers) {
        current_wallpaper_idx = 0;
        set_wallpaper(wp_it->second[0]);
    } else {
        std::string wp = get_current_wallpaper_path();
        if (!wp.empty() && fs::exists(wp)) {
            set_wallpaper(wp);
        }
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
