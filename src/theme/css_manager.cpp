#include "theme/css_manager.hpp"
#include <iostream>
#include <filesystem>
#include <gdk/gdk.h>
#include <glib.h>

namespace fs = std::filesystem;

namespace zenith {

GtkCssProvider* CssManager::provider = nullptr;
std::string CssManager::current_css_path = "style.css";

void CssManager::init(const std::string& css_path) {
    current_css_path = css_path;

    const char* home = g_get_home_dir();
    std::string user_cfg_path = std::string(home) + "/.config/zenithshell/style.css";

    if (!fs::exists(current_css_path)) {
        if (fs::exists(user_cfg_path)) {
            current_css_path = user_cfg_path;
        } else if (fs::exists("style.css")) {
            current_css_path = "style.css";
        } else if (fs::exists("/usr/share/zenithshell/style.css")) {
            current_css_path = "/usr/share/zenithshell/style.css";
        }
    }

    provider = gtk_css_provider_new();

    GdkScreen* screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    reload();
}

void CssManager::reload() {
    if (!provider) return;

    GError* error = nullptr;
    if (!gtk_css_provider_load_from_path(provider, current_css_path.c_str(), &error)) {
        std::cerr << "[ZenithCSS] Failed to load CSS from " << current_css_path << ": "
                  << (error ? error->message : "unknown error") << std::endl;
        if (error) g_error_free(error);
    } else {
        std::cout << "[ZenithCSS] Applied GTK3 styles from " << current_css_path << std::endl;
    }
}

void CssManager::on_file_changed(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent, gpointer) {
    reload();
}

} // namespace zenith
