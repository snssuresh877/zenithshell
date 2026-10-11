#pragma once

#include <gtk/gtk.h>
#include <string>

namespace zenith {

class ClockWidget {
public:
    static void init(GtkApplication* app);
    static GtkWidget* create(const std::string& format = "");
    static void toggle_calendar();
    static void show_calendar();
    static void hide_calendar();

private:
    static GtkWidget* date_label;
    static GtkWidget* time_label;
    static GtkWidget* cal_window;
    static GtkWidget* cal_header_date;
    static GtkWidget* cal_header_time;
    static GtkWidget* cal_widget;

    static void create_calendar_window(GtkApplication* app);
    static gboolean update_time(gpointer user_data);
};

} // namespace zenith
