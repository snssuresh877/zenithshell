#include "dbus/dbus_service.hpp"
#include "shell/bar/bar_window.hpp"
#include "shell/launcher/spotlight_search.hpp"
#include "shell/control_center/control_center.hpp"
#include "shell/clipboard/clipboard_manager.hpp"
#include "shell/reminder/reminder_manager.hpp"
#include "shell/notification/notification_panel.hpp"
#include "shell/task/active_apps_drawer.hpp"
#include "shell/keybinds/keybinds_overlay.hpp"
#include "shell/control_center/wifi_manager.hpp"
#include "shell/power/power_menu.hpp"
#include "theme/theme_engine.hpp"
#include "theme/css_manager.hpp"
#include "system/sys_monitor.hpp"
#include "system/backlight_manager.hpp"
#include "pipewire/audio_manager.hpp"
#include "shell/osd/osd_window.hpp"
#include "dbus/mpris_player.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <cstring>

using json = nlohmann::json;

namespace zenith {

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='dev.zenith.Shell'>"
    "    <method name='ToggleBar'/>"
    "    <method name='ToggleTopBar'/>"
    "    <method name='ToggleSpotlight'/>"
    "    <method name='ToggleControlCenter'/>"
    "    <method name='ToggleClipboard'/>"
    "    <method name='StoreClipboard'>"
    "      <arg type='s' name='text' direction='in'/>"
    "    </method>"
    "    <method name='ClearClipboard'/>"
    "    <method name='ToggleReminders'/>"
    "    <method name='ToggleNotifications'/>"
    "    <method name='ToggleNotificationCenter'/>"
    "    <method name='ToggleActiveApps'/>"
    "    <method name='ToggleKeybinds'/>"
    "    <method name='ToggleNetwork'/>"
    "    <method name='ToggleAudio'/>"
    "    <method name='ToggleTheme'/>"
    "    <method name='TogglePowerMenu'/>"
    "    <method name='TogglePower'/>"
    "    <method name='SwitchTheme'>"
    "      <arg type='s' name='theme_name' direction='in'/>"
    "    </method>"
    "    <method name='SetTheme'>"
    "      <arg type='s' name='theme_name' direction='in'/>"
    "    </method>"
    "    <method name='NextTheme'/>"
    "    <method name='NextWallpaper'/>"
    "    <method name='CycleWallpaper'/>"
    "    <method name='SetWallpaper'>"
    "      <arg type='s' name='wallpaper_path' direction='in'/>"
    "    </method>"
    "    <method name='SetWallpaperDir'>"
    "      <arg type='s' name='dir_path' direction='in'/>"
    "    </method>"
    "    <method name='GetTheme'>"
    "      <arg type='s' name='theme_name' direction='out'/>"
    "    </method>"
    "    <method name='ListThemes'>"
    "      <arg type='as' name='themes' direction='out'/>"
    "    </method>"
    "    <method name='GetWallpaper'>"
    "      <arg type='s' name='wallpaper_path' direction='out'/>"
    "    </method>"
    "    <method name='GetBrightness'>"
    "      <arg type='i' name='percent' direction='out'/>"
    "    </method>"
    "    <method name='SetBrightness'>"
    "      <arg type='i' name='percent' direction='in'/>"
    "      <arg type='i' name='new_brightness' direction='out'/>"
    "    </method>"
    "    <method name='IncreaseBrightness'>"
    "      <arg type='i' name='delta' direction='in'/>"
    "      <arg type='i' name='new_brightness' direction='out'/>"
    "    </method>"
    "    <method name='DecreaseBrightness'>"
    "      <arg type='i' name='delta' direction='in'/>"
    "      <arg type='i' name='new_brightness' direction='out'/>"
    "    </method>"
    "    <method name='GetVolume'>"
    "      <arg type='i' name='percent' direction='out'/>"
    "    </method>"
    "    <method name='SetVolume'>"
    "      <arg type='i' name='percent' direction='in'/>"
    "      <arg type='i' name='new_volume' direction='out'/>"
    "    </method>"
    "    <method name='IncreaseVolume'>"
    "      <arg type='i' name='delta' direction='in'/>"
    "      <arg type='i' name='new_volume' direction='out'/>"
    "    </method>"
    "    <method name='DecreaseVolume'>"
    "      <arg type='i' name='delta' direction='in'/>"
    "      <arg type='i' name='new_volume' direction='out'/>"
    "    </method>"
    "    <method name='ToggleVolumeMute'>"
    "      <arg type='b' name='muted' direction='out'/>"
    "    </method>"
    "    <method name='ToggleMicMute'>"
    "      <arg type='b' name='muted' direction='out'/>"
    "    </method>"
    "    <method name='ShowOSD'>"
    "      <arg type='s' name='type' direction='in'/>"
    "      <arg type='i' name='val' direction='in'/>"
    "    </method>"
    "    <method name='AddReminder'>"
    "      <arg type='s' name='title' direction='in'/>"
    "      <arg type='i' name='minutes' direction='in'/>"
    "    </method>"
    "    <method name='ListReminders'>"
    "      <arg type='s' name='json_reminders' direction='out'/>"
    "    </method>"
    "    <method name='ClearReminders'/>"
    "    <method name='MediaPlayPause'/>"
    "    <method name='MediaNext'/>"
    "    <method name='MediaPrevious'/>"
    "    <method name='MediaPlay'/>"
    "    <method name='MediaPause'/>"
    "    <method name='MediaStop'/>"
    "    <method name='GetMediaInfo'>"
    "      <arg type='s' name='json_info' direction='out'/>"
    "    </method>"
    "    <method name='GetStats'>"
    "      <arg type='s' name='json_stats' direction='out'/>"
    "    </method>"
    "    <method name='ReloadConfig'/>"
    "  </interface>"
    "</node>";

DBusService& DBusService::instance() {
    static DBusService inst;
    return inst;
}

void DBusService::init() {
    GError* error = nullptr;
    introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, &error);
    if (!introspection_data) {
        std::cerr << "[DBusService] Introspection parsing error: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return;
    }

    owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
                              "dev.zenith.Shell",
                              G_BUS_NAME_OWNER_FLAGS_NONE,
                              on_bus_acquired,
                              on_name_acquired,
                              on_name_lost,
                              this,
                              nullptr);
}

void DBusService::cleanup() {
    if (owner_id > 0) {
        g_bus_unown_name(owner_id);
        owner_id = 0;
    }
    if (introspection_data) {
        g_dbus_node_info_unref(introspection_data);
        introspection_data = nullptr;
    }
}

void DBusService::on_bus_acquired(GDBusConnection* connection, const gchar*, gpointer user_data) {
    auto* self = static_cast<DBusService*>(user_data);
    static const GDBusInterfaceVTable vtable = {
        handle_method_call,
        nullptr,
        nullptr,
        { 0 }
    };

    GError* error = nullptr;
    guint registration_id = g_dbus_connection_register_object(connection,
                                                                "/dev/zenith/Shell",
                                                                self->introspection_data->interfaces[0],
                                                                &vtable,
                                                                self,
                                                                nullptr,
                                                                &error);
    if (registration_id == 0) {
        std::cerr << "[DBusService] Object registration failed: " << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
    } else {
        std::cout << "[DBusService] Registered object at /dev/zenith/Shell\n";
    }
}

void DBusService::on_name_acquired(GDBusConnection*, const gchar* name, gpointer) {
    std::cout << "[DBusService] Claimed session bus name: " << name << std::endl;
}

void DBusService::on_name_lost(GDBusConnection*, const gchar* name, gpointer) {
    std::cout << "[DBusService] Lost session bus name: " << name << std::endl;
}

void DBusService::handle_method_call(GDBusConnection*,
                                     const gchar*,
                                     const gchar*,
                                     const gchar*,
                                     const gchar* method_name,
                                     GVariant* parameters,
                                     GDBusMethodInvocation* invocation,
                                     gpointer) {
    std::string method(method_name);
    std::cout << "[DBusService] Received method call: " << method << std::endl;

    if (method == "ToggleBar" || method == "ToggleTopBar") {
        g_idle_add([](gpointer) -> gboolean {
            BarWindow::toggle();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleSpotlight") {
        g_idle_add([](gpointer) -> gboolean {
            SpotlightSearch::toggle();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleControlCenter") {
        g_idle_add([](gpointer) -> gboolean {
            ControlCenter::toggle_popup();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleClipboard") {
        g_idle_add([](gpointer) -> gboolean {
            ClipboardManager::toggle();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "StoreClipboard") {
        const gchar* text = nullptr;
        g_variant_get(parameters, "(&s)", &text);
        if (text && *text) {
            std::string t = text;
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, [](gpointer data) -> gboolean {
                auto* str = static_cast<std::string*>(data);
                ClipboardManager::add_item(*str);
                delete str;
                return FALSE;
            }, new std::string(t), nullptr);
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ClearClipboard") {
        g_idle_add([](gpointer) -> gboolean {
            ClipboardManager::clear_history();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleReminders") {
        g_idle_add([](gpointer) -> gboolean {
            ReminderManager::toggle_overlay();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleNotifications" || method == "ToggleNotificationCenter") {
        g_idle_add([](gpointer) -> gboolean {
            NotificationPanel::toggle();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleActiveApps") {
        g_idle_add([](gpointer) -> gboolean {
            ActiveAppsDrawer::toggle();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleKeybinds") {
        g_idle_add([](gpointer) -> gboolean {
            KeybindsOverlay::toggle();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleNetwork") {
        g_idle_add([](gpointer) -> gboolean {
            WifiManager::toggle_panel();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleAudio") {
        g_idle_add([](gpointer) -> gboolean {
            ControlCenter::toggle_audio_view();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ToggleTheme") {
        g_idle_add([](gpointer) -> gboolean {
            ControlCenter::toggle_theme_view();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "TogglePowerMenu" || method == "TogglePower") {
        g_idle_add([](gpointer) -> gboolean {
            PowerMenu::toggle();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "SwitchTheme" || method == "SetTheme") {
        const char* theme_name = nullptr;
        g_variant_get(parameters, "(&s)", &theme_name);
        if (theme_name) {
            std::string tname = theme_name;
            g_idle_add([](gpointer data) -> gboolean {
                char* tn = static_cast<char*>(data);
                ThemeEngine::set_theme(tn);
                free(tn);
                return FALSE;
            }, strdup(tname.c_str()));
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "NextTheme" || method == "CycleTheme") {
        g_idle_add([](gpointer) -> gboolean {
            ThemeEngine::cycle_next_theme();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "NextWallpaper" || method == "CycleWallpaper") {
        g_idle_add([](gpointer) -> gboolean {
            ThemeEngine::cycle_wallpaper();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "SetWallpaper") {
        const char* wp_path = nullptr;
        g_variant_get(parameters, "(&s)", &wp_path);
        if (wp_path) {
            std::string path = wp_path;
            g_idle_add([](gpointer data) -> gboolean {
                char* p = static_cast<char*>(data);
                ThemeEngine::set_wallpaper(p);
                free(p);
                return FALSE;
            }, strdup(path.c_str()));
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "SetWallpaperDir") {
        const char* dir_path = nullptr;
        g_variant_get(parameters, "(&s)", &dir_path);
        if (dir_path) {
            std::string path = dir_path;
            g_idle_add([](gpointer data) -> gboolean {
                char* p = static_cast<char*>(data);
                ThemeEngine::set_wallpaper_directory(p);
                free(p);
                return FALSE;
            }, strdup(path.c_str()));
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "GetTheme") {
        std::string theme = ThemeEngine::get_current_theme().name;
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", theme.c_str()));
    } else if (method == "ListThemes") {
        auto themes = ThemeEngine::get_available_themes();
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        for (const auto& t : themes) {
            g_variant_builder_add(&builder, "s", t.c_str());
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(as)", &builder));
    } else if (method == "GetWallpaper") {
        std::string wp = ThemeEngine::get_current_wallpaper_path();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", wp.c_str()));
    } else if (method == "GetBrightness") {
        int b = BacklightManager::get_brightness_percent();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", b));
    } else if (method == "SetBrightness") {
        gint val = 0;
        g_variant_get(parameters, "(i)", &val);
        BacklightManager::set_brightness_percent(val);
        int cur = BacklightManager::get_brightness_percent();
        OSDWindow::show_brightness(cur);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", cur));
    } else if (method == "IncreaseBrightness") {
        gint delta = 5;
        g_variant_get(parameters, "(i)", &delta);
        BacklightManager::increase_brightness(delta);
        int cur = BacklightManager::get_brightness_percent();
        OSDWindow::show_brightness(cur);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", cur));
    } else if (method == "DecreaseBrightness") {
        gint delta = 5;
        g_variant_get(parameters, "(i)", &delta);
        BacklightManager::decrease_brightness(delta);
        int cur = BacklightManager::get_brightness_percent();
        OSDWindow::show_brightness(cur);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", cur));
    } else if (method == "GetVolume") {
        int v = AudioManager::get_volume();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", v));
    } else if (method == "SetVolume") {
        gint val = 0;
        g_variant_get(parameters, "(i)", &val);
        int new_vol = std::clamp(val, 0, 150);
        AudioManager::set_volume(new_vol);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", new_vol));
    } else if (method == "IncreaseVolume") {
        gint delta = 5;
        g_variant_get(parameters, "(i)", &delta);
        int cur = AudioManager::get_volume();
        int new_vol = std::clamp(cur + delta, 0, 150);
        AudioManager::set_volume(new_vol);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", new_vol));
    } else if (method == "DecreaseVolume") {
        gint delta = 5;
        g_variant_get(parameters, "(i)", &delta);
        int cur = AudioManager::get_volume();
        int new_vol = std::clamp(cur - delta, 0, 150);
        AudioManager::set_volume(new_vol);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(i)", new_vol));
    } else if (method == "ToggleVolumeMute") {
        AudioManager::toggle_mute();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", AudioManager::is_muted()));
    } else if (method == "ToggleMicMute") {
        AudioManager::toggle_mic_mute();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", AudioManager::is_mic_muted()));
    } else if (method == "ShowOSD") {
        const char* type_str = nullptr;
        gint val = 0;
        g_variant_get(parameters, "(&si)", &type_str, &val);
        std::string t = type_str ? type_str : "";
        if (t == "volume" || t == "vol") {
            OSDWindow::show_volume(val, false);
        } else if (t == "brightness" || t == "bri") {
            OSDWindow::show_brightness(val);
        } else if (t == "mic") {
            OSDWindow::show_mic(val, false);
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "AddReminder") {
        const char* title = nullptr;
        gint mins = 1;
        g_variant_get(parameters, "(&si)", &title, &mins);
        if (title && *title && mins > 0) {
            std::string t = title;
            g_idle_add([](gpointer data) -> gboolean {
                auto* p = static_cast<std::pair<std::string, int>*>(data);
                ReminderManager::add_reminder(p->first, p->second);
                delete p;
                return FALSE;
            }, new std::pair<std::string, int>(t, mins));
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ClearReminders") {
        g_idle_add([](gpointer) -> gboolean {
            ReminderManager::clear_all();
            return FALSE;
        }, nullptr);
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "ListReminders") {
        auto active = ReminderManager::get_active_reminders();
        auto now = std::chrono::system_clock::now();
        json j = json::array();
        for (const auto& r : active) {
            auto secs_left = std::chrono::duration_cast<std::chrono::seconds>(r.target_time - now).count();
            j.push_back({
                {"title", r.title},
                {"seconds_left", std::max<int64_t>(0, secs_left)}
            });
        }
        std::string json_str = j.dump();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", json_str.c_str()));
    } else if (method == "MediaPlayPause") {
        MprisPlayer::play_pause();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "MediaNext") {
        MprisPlayer::next();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "MediaPrevious") {
        MprisPlayer::previous();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "MediaPlay") {
        MprisPlayer::play();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "MediaPause") {
        MprisPlayer::pause();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "MediaStop") {
        MprisPlayer::stop();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else if (method == "GetMediaInfo") {
        MediaInfo inf = MprisPlayer::get_info();
        json j = {
            {"player", inf.player_name},
            {"bus", inf.bus_name},
            {"title", inf.title},
            {"artist", inf.artist},
            {"album", inf.album},
            {"art_url", inf.art_url},
            {"status", inf.status},
            {"is_playing", inf.is_playing},
            {"can_go_next", inf.can_go_next},
            {"can_go_prev", inf.can_go_prev},
            {"can_play", inf.can_play},
            {"can_pause", inf.can_pause}
        };
        std::string json_str = j.dump();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", json_str.c_str()));
    } else if (method == "GetStats") {
        SysStats stats = SysMonitor::get_stats();
        int vol = AudioManager::get_volume();
        int bri = BacklightManager::get_brightness_percent();
        std::string theme = ThemeEngine::get_current_theme_name();

        std::ostringstream ss;
        ss << "{"
           << "\"cpu_usage\":" << stats.cpu_usage << ","
           << "\"ram_usage\":" << stats.ram_usage << ","
           << "\"net_speed\":\"" << stats.net_speed_str << "\","
           << "\"battery_percent\":" << stats.battery_percent << ","
           << "\"volume\":" << vol << ","
           << "\"brightness\":" << bri << ","
           << "\"theme\":\"" << theme << "\""
           << "}";

        std::string json = ss.str();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", json.c_str()));
    } else if (method == "ReloadConfig") {
        CssManager::reload();
        g_dbus_method_invocation_return_value(invocation, g_variant_new("()"));
    } else {
        g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD, "Unknown method");
    }
}

} // namespace zenith
