#include "dbus/mpris_player.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace zenith {

MediaInfo MprisPlayer::cached_info{};
GDBusConnection* MprisPlayer::dbus_conn = nullptr;
guint MprisPlayer::prop_changed_sub_id = 0;
guint MprisPlayer::name_owner_sub_id = 0;
MprisPlayer::ChangeCallback MprisPlayer::change_cb = nullptr;

void MprisPlayer::init() {
    if (dbus_conn) return;

    GError* error = nullptr;
    dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!dbus_conn) {
        std::cerr << "[MprisPlayer] Failed to connect to session bus: "
                  << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return;
    }

    // Subscribe to PropertiesChanged on org.mpris.MediaPlayer2.Player
    prop_changed_sub_id = g_dbus_connection_signal_subscribe(
        dbus_conn,
        nullptr, // from any sender
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_properties_changed,
        nullptr,
        nullptr
    );

    // Subscribe to NameOwnerChanged to detect player launch or exit
    name_owner_sub_id = g_dbus_connection_signal_subscribe(
        dbus_conn,
        "org.freedesktop.DBus",
        "org.freedesktop.DBus",
        "NameOwnerChanged",
        "/org/freedesktop/DBus",
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        on_name_owner_changed,
        nullptr,
        nullptr
    );

    refresh_active_player();
}

void MprisPlayer::cleanup() {
    if (dbus_conn) {
        if (prop_changed_sub_id > 0) {
            g_dbus_connection_signal_unsubscribe(dbus_conn, prop_changed_sub_id);
            prop_changed_sub_id = 0;
        }
        if (name_owner_sub_id > 0) {
            g_dbus_connection_signal_unsubscribe(dbus_conn, name_owner_sub_id);
            name_owner_sub_id = 0;
        }
        g_object_unref(dbus_conn);
        dbus_conn = nullptr;
    }
}

std::vector<std::string> MprisPlayer::get_players() {
    std::vector<std::string> players;
    GDBusConnection* conn = dbus_conn;
    bool local_conn = false;
    if (!conn) {
        conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        local_conn = true;
    }
    if (!conn) return players;

    GError* error = nullptr;
    GVariant* res = g_dbus_connection_call_sync(
        conn,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListNames",
        nullptr,
        G_VARIANT_TYPE("(as)"),
        G_DBUS_CALL_FLAGS_NONE,
        1500,
        nullptr,
        &error
    );

    if (res) {
        GVariantIter* iter = nullptr;
        g_variant_get(res, "(as)", &iter);
        const char* name = nullptr;
        while (g_variant_iter_next(iter, "&s", &name)) {
            if (strncmp(name, "org.mpris.MediaPlayer2.", 23) == 0) {
                players.emplace_back(name);
            }
        }
        g_variant_iter_free(iter);
        g_variant_unref(res);
    } else {
        if (error) g_error_free(error);
    }

    if (local_conn) g_object_unref(conn);
    return players;
}

void MprisPlayer::query_player(const std::string& bus_name, MediaInfo& info) {
    info = MediaInfo{};
    info.bus_name = bus_name;

    // Derive friendly name (e.g. org.mpris.MediaPlayer2.spotify.instance_1 -> spotify)
    if (bus_name.size() > 23) {
        std::string friendly = bus_name.substr(23);
        auto dot_pos = friendly.find('.');
        if (dot_pos != std::string::npos) {
            friendly = friendly.substr(0, dot_pos);
        }
        info.player_name = friendly;
    }

    GDBusConnection* conn = dbus_conn;
    bool local_conn = false;
    if (!conn) {
        conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        local_conn = true;
    }
    if (!conn) return;

    GError* error = nullptr;
    GVariant* res = g_dbus_connection_call_sync(
        conn,
        bus_name.c_str(),
        "/org/mpris/MediaPlayer2",
        "org.freedesktop.DBus.Properties",
        "GetAll",
        g_variant_new("(s)", "org.mpris.MediaPlayer2.Player"),
        G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NONE,
        1500,
        nullptr,
        &error
    );

    if (res) {
        GVariantIter* iter = nullptr;
        g_variant_get(res, "(a{sv})", &iter);
        const char* key = nullptr;
        GVariant* val = nullptr;
        while (g_variant_iter_next(iter, "{&sv}", &key, &val)) {
            if (strcmp(key, "PlaybackStatus") == 0 && g_variant_is_of_type(val, G_VARIANT_TYPE_STRING)) {
                info.status = g_variant_get_string(val, nullptr);
                info.is_playing = (info.status == "Playing");
            } else if (strcmp(key, "CanGoNext") == 0 && g_variant_is_of_type(val, G_VARIANT_TYPE_BOOLEAN)) {
                info.can_go_next = g_variant_get_boolean(val);
            } else if (strcmp(key, "CanGoPrevious") == 0 && g_variant_is_of_type(val, G_VARIANT_TYPE_BOOLEAN)) {
                info.can_go_prev = g_variant_get_boolean(val);
            } else if (strcmp(key, "CanPlay") == 0 && g_variant_is_of_type(val, G_VARIANT_TYPE_BOOLEAN)) {
                info.can_play = g_variant_get_boolean(val);
            } else if (strcmp(key, "CanPause") == 0 && g_variant_is_of_type(val, G_VARIANT_TYPE_BOOLEAN)) {
                info.can_pause = g_variant_get_boolean(val);
            } else if (strcmp(key, "Metadata") == 0 && g_variant_is_of_type(val, G_VARIANT_TYPE("a{sv}"))) {
                GVariantIter* miter = nullptr;
                g_variant_get(val, "a{sv}", &miter);
                const char* mkey = nullptr;
                GVariant* mval = nullptr;
                while (g_variant_iter_next(miter, "{&sv}", &mkey, &mval)) {
                    if (strcmp(mkey, "xesam:title") == 0 && g_variant_is_of_type(mval, G_VARIANT_TYPE_STRING)) {
                        info.title = g_variant_get_string(mval, nullptr);
                    } else if (strcmp(mkey, "xesam:album") == 0 && g_variant_is_of_type(mval, G_VARIANT_TYPE_STRING)) {
                        info.album = g_variant_get_string(mval, nullptr);
                    } else if (strcmp(mkey, "mpris:artUrl") == 0 && g_variant_is_of_type(mval, G_VARIANT_TYPE_STRING)) {
                        info.art_url = g_variant_get_string(mval, nullptr);
                    } else if (strcmp(mkey, "xesam:artist") == 0) {
                        if (g_variant_is_of_type(mval, G_VARIANT_TYPE_STRING_ARRAY)) {
                            GVariantIter* aiter = nullptr;
                            g_variant_get(mval, "as", &aiter);
                            const char* art = nullptr;
                            std::string artists;
                            while (g_variant_iter_next(aiter, "&s", &art)) {
                                if (!artists.empty()) artists += ", ";
                                artists += art;
                            }
                            g_variant_iter_free(aiter);
                            info.artist = artists;
                        } else if (g_variant_is_of_type(mval, G_VARIANT_TYPE_STRING)) {
                            info.artist = g_variant_get_string(mval, nullptr);
                        }
                    }
                    g_variant_unref(mval);
                }
                g_variant_iter_free(miter);
            }
            g_variant_unref(val);
        }
        g_variant_iter_free(iter);
        g_variant_unref(res);
    } else {
        if (error) g_error_free(error);
    }

    if (local_conn) g_object_unref(conn);
}

std::string MprisPlayer::get_active_player_bus() {
    auto players = get_players();
    if (players.empty()) return "";

    std::string first_paused;
    for (const auto& p : players) {
        MediaInfo inf;
        query_player(p, inf);
        if (inf.status == "Playing") {
            return p;
        }
        if (inf.status == "Paused" && first_paused.empty()) {
            first_paused = p;
        }
    }

    if (!first_paused.empty()) return first_paused;
    return players[0];
}

void MprisPlayer::refresh_active_player() {
    std::string active_bus = get_active_player_bus();
    if (active_bus.empty()) {
        cached_info = MediaInfo{};
    } else {
        query_player(active_bus, cached_info);
    }

    if (change_cb) {
        change_cb(cached_info);
    }
}

MediaInfo MprisPlayer::get_info() {
    refresh_active_player();
    return cached_info;
}

bool MprisPlayer::call_player_method(const std::string& method, const std::string& target_player) {
    std::string bus_name = target_player;
    if (bus_name.empty()) {
        bus_name = get_active_player_bus();
    } else if (bus_name.rfind("org.mpris.MediaPlayer2.", 0) != 0) {
        auto players = get_players();
        for (const auto& p : players) {
            if (p.find(target_player) != std::string::npos) {
                bus_name = p;
                break;
            }
        }
    }

    if (bus_name.empty()) {
        return false;
    }

    GDBusConnection* conn = dbus_conn;
    bool local_conn = false;
    if (!conn) {
        conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
        local_conn = true;
    }
    if (!conn) return false;

    GError* error = nullptr;
    GVariant* res = g_dbus_connection_call_sync(
        conn,
        bus_name.c_str(),
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        method.c_str(),
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        nullptr,
        &error
    );

    bool ok = true;
    if (error) {
        std::cerr << "[MprisPlayer] Method call " << method << " failed on " << bus_name
                  << ": " << error->message << std::endl;
        g_error_free(error);
        ok = false;
    }
    if (res) g_variant_unref(res);
    if (local_conn) g_object_unref(conn);

    refresh_active_player();
    return ok;
}

bool MprisPlayer::play_pause(const std::string& player) {
    return call_player_method("PlayPause", player);
}

bool MprisPlayer::next(const std::string& player) {
    return call_player_method("Next", player);
}

bool MprisPlayer::previous(const std::string& player) {
    return call_player_method("Previous", player);
}

bool MprisPlayer::play(const std::string& player) {
    return call_player_method("Play", player);
}

bool MprisPlayer::pause(const std::string& player) {
    return call_player_method("Pause", player);
}

bool MprisPlayer::stop(const std::string& player) {
    return call_player_method("Stop", player);
}

void MprisPlayer::set_on_change(ChangeCallback cb) {
    change_cb = std::move(cb);
}

void MprisPlayer::on_properties_changed(GDBusConnection*,
                                      const gchar*,
                                      const gchar*,
                                      const gchar*,
                                      const gchar*,
                                      GVariant*,
                                      gpointer) {
    refresh_active_player();
}

void MprisPlayer::on_name_owner_changed(GDBusConnection*,
                                      const gchar*,
                                      const gchar*,
                                      const gchar*,
                                      const gchar*,
                                      GVariant* parameters,
                                      gpointer) {
    const char* name = nullptr;
    const char* old_owner = nullptr;
    const char* new_owner = nullptr;
    g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
    if (name && strncmp(name, "org.mpris.MediaPlayer2.", 23) == 0) {
        refresh_active_player();
    }
}

} // namespace zenith
