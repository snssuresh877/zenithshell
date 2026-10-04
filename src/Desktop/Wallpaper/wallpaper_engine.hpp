#pragma once

#include <gtk/gtk.h>
#include <string>

namespace zenith {

class WallpaperEngine {
public:
    // Initialize the Wayland layer-shell wallpaper engine
    static void init(GtkApplication* app);

    // Set and transition to a new wallpaper
    static void set_wallpaper(const std::string& path);

    // Configure transition duration in milliseconds (default: 350ms)
    static void set_transition_duration(int ms);

    // Rebuild windows for all active monitors
    static void reload_monitors();

    // Get current wallpaper path
    static std::string get_current_wallpaper();

    // Kill competing external daemons (awww, swww)
    static void terminate_external_daemons();
};

} // namespace zenith
