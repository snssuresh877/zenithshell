#include "app/app.hpp"
#include "theme/css_manager.hpp"
#include "theme/theme_engine.hpp"
#include "dbus/notification_manager.hpp"
#include "dbus/dbus_service.hpp"
#include "compositors/hyprland_ipc.hpp"
#include "system/sys_monitor.hpp"
#include "pipewire/audio_manager.hpp"
#include "shell/bar/bar_window.hpp"
#include "shell/launcher/spotlight_search.hpp"
#include "shell/clipboard/clipboard_manager.hpp"
#include "shell/reminder/reminder_manager.hpp"
#include "shell/notification/notification_panel.hpp"
#include "shell/task/active_apps_drawer.hpp"
#include "shell/tray/system_tray_manager.hpp"
#include "shell/bar/clock_widget.hpp"
#include "shell/keybinds/keybinds_overlay.hpp"
#include "shell/control_center/wifi_manager.hpp"
#include "shell/power/power_menu.hpp"
#include <iostream>

namespace zenith {

App::App(int argc, char** argv) {
    gtk_app = gtk_application_new("org.zenithshell.desktop", G_APPLICATION_NON_UNIQUE);

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--style" && i + 1 < argc) {
            style_path = argv[++i];
        } else if (arg == "--theme" && i + 1 < argc) {
            theme_name = argv[++i];
        }
    }

    config = Config::load(config_path);
    g_signal_connect(gtk_app, "startup", G_CALLBACK(+[](GApplication* app, gpointer) {
        g_application_hold(app);
    }), nullptr);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(on_activate), this);
}

int App::run() {
    return g_application_run(G_APPLICATION(gtk_app), 0, nullptr);
}

void App::on_activate(GtkApplication* app, gpointer user_data) {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    g_application_hold(G_APPLICATION(app));

    auto* self = static_cast<App*>(user_data);

    // Force dark theme application preference for ZenithShell itself
    GtkSettings* settings = gtk_settings_get_default();
    if (settings) {
        g_object_set(settings,
            "gtk-application-prefer-dark-theme", TRUE,
            "gtk-theme-name", "Orchis-Dark",
            NULL
        );
    }

    // Initialize CSS Styles & Universal Theme Engine
    CssManager::init(self->style_path);
    ThemeEngine::init(self->theme_name, self->config.wallpaper_dir);

    // Initialize DBus Notification Manager
    NotificationManager::init(app);

    // Initialize System Tray / SNI Watcher
    SystemTrayManager::init();

    // Initialize System Hardware Monitor & Audio
    SysMonitor::init();
    AudioManager::init();

    // Initialize Hyprland Socket Listeners
    HyprlandIPC::instance().init();

    // Initialize Central DBus Service (dev.zenith.Shell)
    DBusService::instance().init();

    // Initialize Spotlight & Overlays
    SpotlightSearch::init(app);
    ClipboardManager::init(app);
    ReminderManager::init(app);
    NotificationPanel::init(app);
    ActiveAppsDrawer::init(app);
    ClockWidget::init(app);
    KeybindsOverlay::init(app);
    WifiManager::init(app);
    PowerMenu::init(app);

    // Create Main Bar Window
    BarWindow::create(app, self->config);
}

} // namespace zenith
