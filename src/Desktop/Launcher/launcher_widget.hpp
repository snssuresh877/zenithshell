#pragma once

#include <gtk/gtk.h>

namespace zenith {

class LauncherWidget {
public:
    static GtkWidget* create();
    static void on_launcher_clicked(GtkButton* btn, gpointer user_data);
};

} // namespace zenith
