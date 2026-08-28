#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string>
#include <vector>
#include <functional>

namespace zenith {

struct NotificationItem {
    uint32_t id = 0;
    std::string app_name;
    std::string app_icon;
    std::string summary;
    std::string body;
    std::string timestamp;
};

class NotificationManager {
public:
    static void init(GtkApplication* app);
    static void cleanup();
    static const std::vector<NotificationItem>& get_history();
    static void clear_history();
    static void remove_notification(uint32_t id);
    static int get_count();
    static const NotificationItem* get_latest();
    static void set_history_changed_callback(std::function<void()> cb);
    static void show_toast(const NotificationItem& item, int timeout_ms = 5000);

private:
    static GtkApplication* gtk_app;
    static GDBusNodeInfo* introspection_data;
    static guint owner_id;
    static uint32_t next_id;
    static std::vector<NotificationItem> history;
    static std::function<void()> history_cb;

    static void on_bus_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_lost(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void handle_method_call(GDBusConnection* connection, const gchar* sender, const gchar* object_path,
                                   const gchar* interface_name, const gchar* method_name, GVariant* parameters,
                                   GDBusMethodInvocation* invocation, gpointer user_data);
};

} // namespace zenith
