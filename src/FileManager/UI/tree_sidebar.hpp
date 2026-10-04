#pragma once
#include <gtk/gtk.h>
#include <string>
#include <functional>

namespace zenith {
class TreeSidebar {
public:
    using NavigateCallback = std::function<void(const std::string&)>;
    static GtkWidget* create(NavigateCallback on_navigate, const std::string& start_path);
};
}
