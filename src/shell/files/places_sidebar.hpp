#pragma once

#include <gtk/gtk.h>
#include <string>
#include <functional>

namespace zenith {

class PlacesSidebar {
public:
    using NavigateCallback = std::function<void(const std::string& target_path)>;

    static GtkWidget* create(NavigateCallback on_navigate);
    static void set_active_path(GtkWidget* sidebar, const std::string& path);
    static void refresh(GtkWidget* sidebar);
};

} // namespace zenith
