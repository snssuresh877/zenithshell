#pragma once
#include <gtk/gtk.h>
#include <string>

namespace zenith {

struct ThemeConfig {
    std::string accent_color = "#5294e2"; // Default Blue
    int sidebar_width = 240;
    bool transparent = false;
};

class ThemeSettings {
public:
    static void init();
    static void apply();
    static void save();
    
    static ThemeConfig config;
    static GtkCssProvider* override_provider;
};

} // namespace zenith
