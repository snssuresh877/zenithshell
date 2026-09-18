#include "shell/files/theme_settings.hpp"
#include <fstream>
#include <filesystem>
#include <glib.h>

namespace zenith {

ThemeConfig ThemeSettings::config;
GtkCssProvider* ThemeSettings::override_provider = nullptr;

static std::string get_cfg_path() {
    return std::string(g_get_user_config_dir()) + "/zenithshell/file_theme.conf";
}

void ThemeSettings::init() {
    std::ifstream in(get_cfg_path());
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            if (line.starts_with("accent=")) config.accent_color = line.substr(7);
            else if (line.starts_with("sidebar_width=")) config.sidebar_width = std::stoi(line.substr(14));
            else if (line.starts_with("transparent=")) config.transparent = (line.substr(12) == "1");
        }
    }
    
    override_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(override_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    
    apply();
}

void ThemeSettings::save() {
    std::filesystem::create_directories(std::string(g_get_user_config_dir()) + "/zenithshell");
    std::ofstream out(get_cfg_path());
    out << "accent=" << config.accent_color << "\n";
    out << "sidebar_width=" << config.sidebar_width << "\n";
    out << "transparent=" << (config.transparent ? "1" : "0") << "\n";
    
    apply();
}

void ThemeSettings::apply() {
    std::string css = R"(
        @define-color theme_selected_bg_color )" + config.accent_color + R"(;
        @define-color zenith_accent )" + config.accent_color + R"(;
        
        .files-sidebar {
            min-width: )" + std::to_string(config.sidebar_width) + R"(px;
        }
    )";
    
    if (config.transparent) {
        css += R"(
            window.background { background: rgba(30, 30, 32, 0.85); }
            .files-sidebar { background: rgba(20, 20, 22, 0.85); }
            .files-content-pane { background: rgba(25, 25, 27, 0.85); }
        )";
    } else {
        css += R"(
            window.background { background: #1e1e20; }
            .files-sidebar { background: #141416; }
            .files-content-pane { background: #19191b; }
        )";
    }
    
    gtk_css_provider_load_from_data(override_provider, css.c_str(), -1, nullptr);
}

} // namespace zenith
