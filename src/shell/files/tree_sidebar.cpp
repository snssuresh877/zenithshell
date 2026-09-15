#include "shell/files/tree_sidebar.hpp"
#include <gio/gio.h>

namespace zenith {

// Stub implementation for now
GtkWidget* TreeSidebar::create(NavigateCallback on_navigate, const std::string& start_path) {
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    GtkWidget* label = gtk_label_new("Tree View Coming Soon");
    gtk_container_add(GTK_CONTAINER(scrolled), label);
    
    return scrolled;
}

} // namespace zenith
