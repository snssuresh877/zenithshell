#include "shell/bar/bar_window.hpp"
#include "gtk3_compat.hpp"
#include "shell/bar/workspace_widget.hpp"
#include "shell/bar/sys_info_widget.hpp"
#include "shell/bar/clock_widget.hpp"
#include "shell/control_center/control_center.hpp"
#include "shell/control_center/wifi_manager.hpp"
#include "shell/launcher/launcher_widget.hpp"
#include "shell/launcher/spotlight_search.hpp"
#include "shell/clipboard/clipboard_manager.hpp"
#include "shell/reminder/reminder_manager.hpp"
#include "shell/notification/notification_panel.hpp"
#include "shell/task/active_apps_drawer.hpp"
#include "shell/tray/system_tray_manager.hpp"
#include "compositors/hyprland_ipc.hpp"
#include "system/sys_monitor.hpp"
#include "pipewire/audio_manager.hpp"
#include "dbus/mpris_player.hpp"
#include "dbus/notification_manager.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <iostream>

namespace zenith {

GtkWidget* BarWindow::window = nullptr;
GtkWidget* BarWindow::active_app_icon = nullptr;
GtkWidget* BarWindow::active_title_label = nullptr;

GtkWidget* BarWindow::create(GtkApplication* app, const Config& config) {
    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "zenith-bar");

    // Init Layer Shell (Top anchor, margin top 6, left 12, right 12, height 40)
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_TOP);

    if (config.exclusive_zone) {
        gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(window));
    }

    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 3);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, 10);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 10);

    // Outer Bar Frame (100% Transparent, compact height 28px)
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(main_box, "bar-box");
    gtk_widget_set_size_request(main_box, -1, 28);

    // ─────────────────────────────────────────────────────────────
    // LEFT SECTION: App Launcher Trigger ("") + Active Window Pill (● Title)
    // ─────────────────────────────────────────────────────────────
    GtkWidget* left_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    
    // Arch Logo Launcher Button (28x24px, icon 󰣇)
    GtkWidget* launcher_btn = gtk_button_new();
    gtk_widget_add_css_class(launcher_btn, "arch-launcher-btn");
    gtk_widget_set_size_request(launcher_btn, 28, 24);
    GtkWidget* arch_icon = gtk_label_new("󰣇");
    gtk_widget_add_css_class(arch_icon, "arch-icon-lbl");
    gtk_container_add(GTK_CONTAINER(launcher_btn), arch_icon);
    g_signal_connect(launcher_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        SpotlightSearch::toggle();
    }), nullptr);

    // Active Window Pill (● Title)
    GtkWidget* active_pill = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(active_pill, "pill-widget");
    gtk_widget_add_css_class(active_pill, "active-window-pill");
    gtk_widget_set_size_request(active_pill, -1, 24);

    GtkWidget* active_dot = gtk_label_new("●");
    gtk_widget_add_css_class(active_dot, "active-dot");

    active_title_label = gtk_label_new("Desktop");
    gtk_widget_add_css_class(active_title_label, "window-title");
    gtk_label_set_ellipsize(GTK_LABEL(active_title_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(active_title_label), 28);

    gtk_box_pack_start(GTK_BOX(active_pill), active_dot, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(active_pill), active_title_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(left_box), launcher_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(left_box), active_pill, FALSE, FALSE, 0);

    GtkWidget* active_apps_btn = ActiveAppsDrawer::create_topbar_button();
    gtk_box_pack_start(GTK_BOX(left_box), active_apps_btn, FALSE, FALSE, 0);

    HyprlandIPC::instance().set_window_title_callback([](const std::string& title) {
        if (active_title_label) {
            gtk_label_set_text(GTK_LABEL(active_title_label), title.empty() ? "Desktop" : title.c_str());
        }
    });

    // ─────────────────────────────────────────────────────────────
    // CENTER SECTION: Workspaces (5 slots) + ClockWidget (Perfect Center)
    // ─────────────────────────────────────────────────────────────
    GtkWidget* center_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* workspaces = WorkspaceWidget::create(5);
    GtkWidget* clock = ClockWidget::create();

    gtk_box_pack_start(GTK_BOX(center_box), workspaces, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(center_box), clock, FALSE, FALSE, 0);

    // ─────────────────────────────────────────────────────────────
    // RIGHT SECTION: Stats + Network Speed + Notification + Control Center + Volume + Battery
    // ─────────────────────────────────────────────────────────────
    GtkWidget* right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    // 1. System Stats ( CPU +  RAM)
    GtkWidget* sys_info = SysInfoWidget::create(config.sys_update_interval_ms);

    // 2. Network Speed Widget
    GtkWidget* net_btn = gtk_button_new();
    gtk_widget_add_css_class(net_btn, "pill-widget");
    gtk_widget_add_css_class(net_btn, "net-pill");
    gtk_widget_set_size_request(net_btn, -1, 24);

    GtkWidget* net_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget* net_icon = gtk_label_new("󰦝");
    gtk_widget_add_css_class(net_icon, "net-icon");
    GtkWidget* net_vpn_lbl = gtk_label_new("Proton VPN  ");
    gtk_widget_add_css_class(net_vpn_lbl, "net-vpn-text");
    GtkWidget* net_speed_lbl = gtk_label_new("0 B/s");
    gtk_widget_add_css_class(net_speed_lbl, "net-speed-text");

    gtk_box_pack_start(GTK_BOX(net_box), net_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(net_box), net_vpn_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(net_box), net_speed_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(net_btn), net_box);

    g_signal_connect(net_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        WifiManager::toggle_panel();
    }), nullptr);

    // 3. Notification History Pill (Hidden when 0 unread)
    GtkWidget* notif_btn = gtk_button_new();
    gtk_widget_add_css_class(notif_btn, "pill-widget");
    gtk_widget_add_css_class(notif_btn, "notif-pill");
    gtk_widget_set_size_request(notif_btn, -1, 24);

    GtkWidget* notif_box_inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* notif_icon = gtk_label_new("󰂚");
    gtk_widget_add_css_class(notif_icon, "notif-pill-icon");
    GtkWidget* notif_badge = gtk_label_new("");
    gtk_widget_add_css_class(notif_badge, "notif-pill-badge");
    gtk_box_pack_start(GTK_BOX(notif_box_inner), notif_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(notif_box_inner), notif_badge, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(notif_btn), notif_box_inner);

    g_signal_connect(notif_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        NotificationPanel::toggle();
    }), nullptr);

    auto update_notif_ui = [notif_btn, notif_icon, notif_badge]() {
        int count = NotificationManager::get_count();
        if (count > 0) {
            gtk_label_set_text(GTK_LABEL(notif_icon), "󰂞");
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", count);
            gtk_label_set_text(GTK_LABEL(notif_badge), buf);
            gtk_widget_add_css_class(notif_btn, "has-notifications");
            gtk_widget_set_visible(notif_btn, TRUE);
        } else {
            gtk_label_set_text(GTK_LABEL(notif_icon), "󰂚");
            gtk_label_set_text(GTK_LABEL(notif_badge), "");
            gtk_widget_remove_css_class(notif_btn, "has-notifications");
            gtk_widget_set_visible(notif_btn, FALSE);
        }
    };

    NotificationManager::set_history_changed_callback(update_notif_ui);
    update_notif_ui();

    // 4. Control Center Trigger Button (32x32px, radius 20, icon 󰍜)
    GtkWidget* control_center = ControlCenter::create_button();

    // 5. Volume Widget (with scroll wheel support and mute toggle)
    GtkWidget* vol_btn = gtk_button_new();
    gtk_widget_add_css_class(vol_btn, "pill-widget");
    gtk_widget_add_css_class(vol_btn, "volume-pill");
    gtk_widget_set_size_request(vol_btn, -1, 24);

    GtkWidget* vol_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget* vol_icon = gtk_label_new("");
    gtk_widget_add_css_class(vol_icon, "volume-icon");
    GtkWidget* vol_text = gtk_label_new("0%");
    gtk_widget_add_css_class(vol_text, "volume-text");
    gtk_box_pack_start(GTK_BOX(vol_box), vol_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vol_box), vol_text, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(vol_btn), vol_box);

    g_signal_connect(vol_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        AudioManager::toggle_mute();
    }), nullptr);

    gtk_widget_add_events(vol_btn, GDK_SCROLL_MASK);
    g_signal_connect(vol_btn, "scroll-event", G_CALLBACK(+[](GtkWidget*, GdkEventScroll* event, gpointer) -> gboolean {
        int cur = AudioManager::get_volume();
        if (event->direction == GDK_SCROLL_UP || event->delta_y < 0) {
            AudioManager::set_volume(std::min(100, cur + 5));
            return TRUE;
        } else if (event->direction == GDK_SCROLL_DOWN || event->delta_y > 0) {
            AudioManager::set_volume(std::max(0, cur - 5));
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    // 6. Battery Widget
    GtkWidget* bat_box_widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_add_css_class(bat_box_widget, "pill-widget");
    gtk_widget_add_css_class(bat_box_widget, "battery-pill");
    gtk_widget_set_size_request(bat_box_widget, -1, 24);

    GtkWidget* bat_icon = gtk_label_new("");
    gtk_widget_add_css_class(bat_icon, "battery-icon");
    GtkWidget* bat_text = gtk_label_new("0%");
    gtk_widget_add_css_class(bat_text, "battery-text");
    gtk_box_pack_start(GTK_BOX(bat_box_widget), bat_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bat_box_widget), bat_text, FALSE, FALSE, 0);

    // Periodic Data Updater
    struct PeriodicData {
        GtkWidget* net_btn;
        GtkWidget* net_icon;
        GtkWidget* net_vpn_lbl;
        GtkWidget* net_speed_lbl;
        GtkWidget* vol_icon;
        GtkWidget* vol_text;
        GtkWidget* bat_icon;
        GtkWidget* bat_text;
    };
    auto* pd = new PeriodicData{ net_btn, net_icon, net_vpn_lbl, net_speed_lbl, vol_icon, vol_text, bat_icon, bat_text };

    auto update_func = [](gpointer user_data) -> gboolean {
        auto* data = static_cast<PeriodicData*>(user_data);

        // Stats & Network
        SysStats stats = SysMonitor::get_stats();
        if (stats.vpn_connected) {
            gtk_label_set_text(GTK_LABEL(data->net_icon), "󰦝");
            gtk_label_set_text(GTK_LABEL(data->net_vpn_lbl), "Proton VPN  ");
            gtk_widget_set_visible(data->net_vpn_lbl, TRUE);
            gtk_label_set_text(GTK_LABEL(data->net_speed_lbl), stats.net_speed_str.c_str());
            gtk_widget_add_css_class(data->net_btn, "vpn-active");
        } else if (!stats.net_connected) {
            gtk_label_set_text(GTK_LABEL(data->net_icon), "󰤭");
            gtk_widget_set_visible(data->net_vpn_lbl, FALSE);
            gtk_label_set_text(GTK_LABEL(data->net_speed_lbl), "Offline");
            gtk_widget_remove_css_class(data->net_btn, "vpn-active");
        } else if (stats.is_wifi) {
            const char* wicon = "";
            if (stats.wifi_quality < 25) wicon = "󰤟";
            else if (stats.wifi_quality < 50) wicon = "󰤢";
            else if (stats.wifi_quality < 75) wicon = "󰤥";
            else wicon = "";
            gtk_label_set_text(GTK_LABEL(data->net_icon), wicon);
            gtk_widget_set_visible(data->net_vpn_lbl, FALSE);
            gtk_label_set_text(GTK_LABEL(data->net_speed_lbl), stats.net_speed_str.c_str());
            gtk_widget_remove_css_class(data->net_btn, "vpn-active");
        } else {
            gtk_label_set_text(GTK_LABEL(data->net_icon), "󰈀");
            gtk_widget_set_visible(data->net_vpn_lbl, FALSE);
            gtk_label_set_text(GTK_LABEL(data->net_speed_lbl), stats.net_speed_str.c_str());
            gtk_widget_remove_css_class(data->net_btn, "vpn-active");
        }

        // Volume
        int vol = AudioManager::get_volume();
        bool muted = AudioManager::is_muted();
        const char* vicon = "";
        if (muted || vol == 0) vicon = "󰖁";
        else if (vol < 30) vicon = "";
        else if (vol < 70) vicon = "";
        else vicon = "";

        gtk_label_set_text(GTK_LABEL(data->vol_icon), vicon);
        if (muted) {
            gtk_label_set_text(GTK_LABEL(data->vol_text), "Muted");
            gtk_widget_add_css_class(data->vol_text, "text-muted");
        } else {
            char vbuf[32];
            snprintf(vbuf, sizeof(vbuf), "%d%%", vol);
            gtk_label_set_text(GTK_LABEL(data->vol_text), vbuf);
            gtk_widget_remove_css_class(data->vol_text, "text-muted");
        }

        // Battery
        const char* bicon = "";
        if (stats.battery_charging) bicon = "";
        else if (stats.battery_percent >= 99) bicon = "";
        else if (stats.battery_percent < 20) bicon = "";
        else if (stats.battery_percent < 40) bicon = "";
        else if (stats.battery_percent < 60) bicon = "";
        else if (stats.battery_percent < 85) bicon = "";
        else bicon = "";

        gtk_label_set_text(GTK_LABEL(data->bat_icon), bicon);
        char bbuf[32];
        snprintf(bbuf, sizeof(bbuf), "%d%%", stats.battery_percent);
        gtk_label_set_text(GTK_LABEL(data->bat_text), bbuf);

        if (stats.battery_charging) {
            gtk_widget_remove_css_class(data->bat_icon, "bat-urgent");
            gtk_widget_add_css_class(data->bat_icon, "bat-charging");
            gtk_widget_remove_css_class(data->bat_text, "bat-urgent");
            gtk_widget_add_css_class(data->bat_text, "bat-charging");
        } else if (stats.battery_percent < 20) {
            gtk_widget_remove_css_class(data->bat_icon, "bat-charging");
            gtk_widget_add_css_class(data->bat_icon, "bat-urgent");
            gtk_widget_remove_css_class(data->bat_text, "bat-charging");
            gtk_widget_add_css_class(data->bat_text, "bat-urgent");
        } else {
            gtk_widget_remove_css_class(data->bat_icon, "bat-charging");
            gtk_widget_remove_css_class(data->bat_icon, "bat-urgent");
            gtk_widget_remove_css_class(data->bat_text, "bat-charging");
            gtk_widget_remove_css_class(data->bat_text, "bat-urgent");
        }

        return TRUE;
    };

    update_func(pd);
    g_timeout_add_seconds(1, update_func, pd);

    gtk_box_pack_start(GTK_BOX(right_box), sys_info, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), net_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), notif_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), control_center, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), vol_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_box), bat_box_widget, FALSE, FALSE, 0);

    // Assemble Layout into Main Box (True Geometric Screen Center)
    gtk_box_pack_start(GTK_BOX(main_box), left_box, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(main_box), right_box, FALSE, FALSE, 0);
    gtk_box_set_center_widget(GTK_BOX(main_box), center_box);

    gtk_container_add(GTK_CONTAINER(window), main_box);
    gtk_widget_show_all(window);

    std::cout << "[ZenithBar] GTK3 Bar window initialized matching QuickShell TopBar.qml\n";
    return window;
}

void BarWindow::toggle() {
    if (!window) return;
    if (gtk_widget_get_visible(window)) {
        hide();
    } else {
        show();
    }
}

void BarWindow::show() {
    if (!window) return;
    gtk_widget_show_all(window);
    std::cout << "[ZenithBar] TopBar shown\n";
}

void BarWindow::hide() {
    if (!window) return;
    gtk_widget_hide(window);
    std::cout << "[ZenithBar] TopBar hidden (reclaiming screen space)\n";
}

} // namespace zenith
