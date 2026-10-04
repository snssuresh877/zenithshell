#pragma once

#include <gtk/gtk.h>
#include <vector>

namespace zenith {

class WorkspaceWidget {
public:
    static GtkWidget* create(int count);
    static void update_active(int active_id);

private:
    static std::vector<GtkWidget*> buttons;
    static int current_active;
    static void on_workspace_clicked(GtkButton* btn, gpointer user_data);
};

} // namespace zenith
