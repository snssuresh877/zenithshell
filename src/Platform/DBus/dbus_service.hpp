#pragma once

#include <gio/gio.h>
#include <string>
#include <memory>
#include "Core/EventBus/event_bus.hpp"

namespace zenith {

class DBusService {
public:
    static DBusService& instance();
    void init(std::shared_ptr<EventBus> event_bus = nullptr);
    void cleanup();

private:
    DBusService() = default;
    ~DBusService() = default;

    guint owner_id = 0;
    GDBusNodeInfo* introspection_data = nullptr;
    std::shared_ptr<EventBus> event_bus_ = nullptr;

    static void on_bus_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_acquired(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void on_name_lost(GDBusConnection* connection, const gchar* name, gpointer user_data);
    static void handle_method_call(GDBusConnection* connection,
                                  const gchar* sender,
                                  const gchar* object_path,
                                  const gchar* interface_name,
                                  const gchar* method_name,
                                  GVariant* parameters,
                                  GDBusMethodInvocation* invocation,
                                  gpointer user_data);
};

} // namespace zenith
