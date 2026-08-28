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
#include "pipewire/audio_manager.hpp"
#include <iostream>
#include <sstream>
#include <cstring>

namespace zenith {

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name='dev.zenith.Shell'>"
    "    <method name='ToggleBar'/>"
    "    <method name='ToggleTopBar'/>"
    "    <method name='ToggleSpotlight'/>"
    "    <method name='ToggleControlCenter'/>"
    "    <method name='ToggleClipboard'/>"
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
        nullptr
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
    } else if (method == "GetStats") {
        SysStats stats = SysMonitor::get_stats();
        int vol = AudioManager::get_volume();
        std::string theme = ThemeEngine::get_current_theme_name();

        std::ostringstream ss;
        ss << "{"
           << "\"cpu_usage\":" << stats.cpu_usage << ","
           << "\"ram_usage\":" << stats.ram_usage << ","
           << "\"net_speed\":\"" << stats.net_speed_str << "\","
           << "\"battery_percent\":" << stats.battery_percent << ","
           << "\"volume\":" << vol << ","
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
