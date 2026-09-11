#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>

namespace zenith {

class InspectorPanel {
public:
    static GtkWidget* create();
    static void update_selection(GtkWidget* panel, const std::vector<std::string>& selected_paths);
    static void set_current_directory(GtkWidget* panel, const std::string& directory_path);
};

} // namespace zenith
