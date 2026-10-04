#pragma once

#include <gtk/gtk.h>
#include <string>

namespace zenith {

class CssManager {
public:
    static void init(const std::string& css_path);
    static void reload();

private:
    static std::string current_css_path;
    static GtkCssProvider* provider;
    static void on_file_changed(GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event_type, gpointer user_data);
};

} // namespace zenith
