#pragma once

#include <gtk/gtk.h>
#include <string>

namespace zenith {

class FileManagerWindow {
public:
    static GtkWidget* create(const std::string& initial_path = "");
    static void open_path(GtkWidget* window, const std::string& path);
};

} // namespace zenith
