#pragma once

#include <gtk/gtk.h>
#include <vector>
#include <string>
#include "dbus/notification_manager.hpp"

namespace zenith {

class NotificationPanel {
public:
    static void init(GtkApplication* app);
    static void toggle();
    static void show();
    static void hide();
    static void refresh();

private:
    static GtkWidget* window;
    static GtkWidget* list_box;
    static GtkWidget* count_badge;
    static GtkWidget* clear_btn;
    static GtkWidget* empty_box;
    static GtkWidget* scroll_window;
    static GtkWidget* content_stack;

    static void create_window(GtkApplication* app);
    static void on_clear_all_clicked(GtkButton* btn, gpointer user_data);
    static void build_notification_item(const NotificationItem& item);
};

} // namespace zenith
