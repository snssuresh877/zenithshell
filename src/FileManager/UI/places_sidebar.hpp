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

    static void add_favorite(const std::string& path, const std::string& name = "");
    static void remove_favorite(const std::string& path);
    static bool is_favorite(const std::string& path);
};

} // namespace zenith
