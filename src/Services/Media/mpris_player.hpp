#pragma once

#include "Core/EventBus/event_bus.hpp"
#include "Core/Events/media_events.hpp"
#include <vector>
#include <memory>
#include <gio/gio.h>

namespace zenith {

class MprisPlayer {
public:
    static void init(std::shared_ptr<EventBus> event_bus = nullptr);
    static void cleanup();
    static void set_event_bus(std::shared_ptr<EventBus> bus) { event_bus_ = std::move(bus); }
    static std::shared_ptr<EventBus> get_event_bus() { return event_bus_; }

    // Query media status
    static MediaInfo get_info();
    static std::vector<std::string> get_players();
    static std::string get_active_player_bus();

    // Native playback controls (Zero subprocesses!)
    static bool play_pause(const std::string& player = "");
    static bool next(const std::string& player = "");
    static bool previous(const std::string& player = "");
    static bool play(const std::string& player = "");
    static bool pause(const std::string& player = "");
    static bool stop(const std::string& player = "");

private:
    static MediaInfo cached_info;
    static GDBusConnection* dbus_conn;
    static guint prop_changed_sub_id;
    static guint name_owner_sub_id;
    static std::shared_ptr<EventBus> event_bus_;

    static void query_player(const std::string& bus_name, MediaInfo& info);
    static void refresh_active_player();
    static void on_properties_changed(GDBusConnection* conn,
                                     const gchar* sender_name,
                                     const gchar* object_path,
                                     const gchar* interface_name,
                                     const gchar* signal_name,
                                     GVariant* parameters,
                                     gpointer user_data);
    static void on_name_owner_changed(GDBusConnection* conn,
                                     const gchar* sender_name,
                                     const gchar* object_path,
                                     const gchar* interface_name,
                                     const gchar* signal_name,
                                     GVariant* parameters,
                                     gpointer user_data);
    static bool call_player_method(const std::string& method, const std::string& target_player = "");
};

} // namespace zenith
