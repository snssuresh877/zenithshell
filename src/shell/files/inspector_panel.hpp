#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <functional>

namespace zenith {

class InspectorPanel {
public:
    using CloseCallback = std::function<void()>;

    static GtkWidget* create(CloseCallback on_close = nullptr);
    static void update_selection(GtkWidget* panel, const std::vector<std::string>& selected_paths);
    static void set_current_directory(GtkWidget* panel, const std::string& directory_path);
};

} // namespace zenith
