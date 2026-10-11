#pragma once

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string>
#include <vector>
#include <functional>

#include "Core/EventBus/event_bus.hpp"
#include "Core/Events/notification_events.hpp"
#include <memory>

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
    static void init(GtkApplication* app, std::shared_ptr<EventBus> event_bus = nullptr);
    static void cleanup();
    static void set_event_bus(std::shared_ptr<EventBus> bus) { event_bus_ = std::move(bus); }
    static std::shared_ptr<EventBus> get_event_bus() { return event_bus_; }
    static const std::vector<NotificationItem>& get_history();
    static void clear_history();
    static void remove_notification(uint32_t id);
    static int get_count();
    static const NotificationItem* get_latest();
    static void show_toast(const NotificationItem& item, int timeout_ms = 5000);
    static uint32_t send_notification(const std::string& app_name,
                                      const std::string& app_icon,
                                      const std::string& summary,
                                      const std::string& body,
                                      int timeout_ms = 5000);

    // Native Do Not Disturb (DND) Control
    static bool is_dnd_enabled();
    static void set_dnd_enabled(bool enabled);
    static bool toggle_dnd();

private:
    static GtkApplication* gtk_app;
    static GDBusNodeInfo* introspection_data;
    static guint owner_id;
    static uint32_t next_id;
    static std::vector<NotificationItem> history;
    static std::shared_ptr<EventBus> event_bus_;
    static bool dnd_enabled;

    static void notify_history_changed();
    static void notify_dnd_changed();

    static void on_bus_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_lost(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void handle_method_call(GDBusConnection* connection, const gchar* sender, const gchar* object_path,
                                   const gchar* interface_name, const gchar* method_name, GVariant* parameters,
                                   GDBusMethodInvocation* invocation, gpointer user_data);
};

} // namespace zenith
