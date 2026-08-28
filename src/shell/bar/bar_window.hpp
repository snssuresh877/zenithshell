#pragma once

#include <gtk/gtk.h>
#include "config/config.hpp"

namespace zenith {

class BarWindow {
public:
    static GtkWidget* create(GtkApplication* app, const Config& config);
    static void toggle();
    static void show();
    static void hide();

private:
    static GtkWidget* window;
    static GtkWidget* active_app_icon;
    static GtkWidget* active_title_label;
};

} // namespace zenith
