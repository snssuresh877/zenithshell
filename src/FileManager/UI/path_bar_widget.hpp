#pragma once

#include <gtk/gtk.h>
#include <string>
#include <functional>

namespace zenith {

class PathBarWidget {
public:
    using NavigateCallback = std::function<void(const std::string& target_path)>;

    static GtkWidget* create(NavigateCallback on_navigate);
    static void set_path(GtkWidget* widget, const std::string& path);
    static std::string get_path(GtkWidget* widget);
    static void enter_edit_mode(GtkWidget* widget);
    static void exit_edit_mode(GtkWidget* widget);
};

} // namespace zenith
