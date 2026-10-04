#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string>
#include <vector>
#include <functional>

namespace zenith {

struct TrayItem {
    std::string service;
    std::string path;
    std::string id;
    std::string title;
    std::string icon_name;
    std::string tooltip;
};

class SystemTrayManager {
public:
    static void init();
    static std::vector<TrayItem> get_tray_items();
    static void activate_item(const std::string& service, const std::string& path);
    static void set_changed_callback(std::function<void()> cb);
    static GtkWidget* create_tray_box();

private:
    static GDBusConnection* dbus_conn;
    static std::vector<TrayItem> current_items;
    static std::function<void()> change_cb;
    static guint reg_sub_id;
    static guint unreg_sub_id;

    static void fetch_all_items();
    static TrayItem query_item_properties(const std::string& service, const std::string& path);
    static void on_item_registered(GDBusConnection* conn, const gchar* sender, const gchar* path,
                                   const gchar* iface, const gchar* signal, GVariant* params, gpointer user_data);
    static void on_item_unregistered(GDBusConnection* conn, const gchar* sender, const gchar* path,
                                     const gchar* iface, const gchar* signal, GVariant* params, gpointer user_data);
};

} // namespace zenith
