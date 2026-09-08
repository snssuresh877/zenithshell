#pragma once

#include <string>
#include <vector>
#include <functional>
#include <gio/gio.h>

namespace zenith {

struct MediaInfo {
    std::string player_name = "";
    std::string bus_name = "";
    std::string title = "";
    std::string artist = "";
    std::string album = "";
    std::string art_url = "";
    std::string status = "Stopped"; // "Playing", "Paused", "Stopped"
    bool is_playing = false;
    bool can_go_next = false;
    bool can_go_prev = false;
    bool can_play = false;
    bool can_pause = false;
};

class MprisPlayer {
public:
    static void init();
    static void cleanup();

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

    // Reactive callback on track/playback changes
    using ChangeCallback = std::function<void(const MediaInfo&)>;
    static void set_on_change(ChangeCallback cb);

private:
    static MediaInfo cached_info;
    static GDBusConnection* dbus_conn;
    static guint prop_changed_sub_id;
    static guint name_owner_sub_id;
    static ChangeCallback change_cb;

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
