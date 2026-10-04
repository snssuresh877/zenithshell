#pragma once

#include <gtk/gtk.h>

namespace zenith {

class SysInfoWidget {
public:
    static GtkWidget* create(int update_interval_ms);

private:
    static GtkWidget* cpu_label;
    static GtkWidget* ram_label;
    static GtkWidget* bat_label;
    static gboolean update_stats(gpointer user_data);
};

} // namespace zenith
