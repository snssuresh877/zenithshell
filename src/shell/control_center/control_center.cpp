#include "shell/control_center/control_center.hpp"
#include "shell/control_center/wifi_manager.hpp"
#include "pipewire/audio_manager.hpp"
#include "theme/theme_engine.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <cairo.h>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <memory>
#include <array>
#include <algorithm>
#include <glob.h>

namespace zenith {

GtkWidget* ControlCenter::popup_window = nullptr;
GtkWidget* ControlCenter::btn_widget = nullptr;
GtkWidget* ControlCenter::battery_status_lbl = nullptr;
GtkWidget* ControlCenter::stack = nullptr;

GtkWidget* ControlCenter::wifi_ssid_lbl = nullptr;
GtkWidget* ControlCenter::wifi_state_pill = nullptr;
GtkWidget* ControlCenter::bt_status_lbl = nullptr;
GtkWidget* ControlCenter::bt_state_pill = nullptr;
GtkWidget* ControlCenter::audio_sub_lbl = nullptr;
GtkWidget* ControlCenter::audio_pill_lbl = nullptr;
GtkWidget* ControlCenter::display_sub_lbl = nullptr;

GtkWidget* ControlCenter::night_btn = nullptr;
GtkWidget* ControlCenter::night_sub_lbl = nullptr;
GtkWidget* ControlCenter::focus_btn = nullptr;
GtkWidget* ControlCenter::focus_sub_lbl = nullptr;
GtkWidget* ControlCenter::dark_btn = nullptr;
GtkWidget* ControlCenter::dark_sub_lbl = nullptr;

GtkWidget* ControlCenter::theme_card_btn = nullptr;
GtkWidget* ControlCenter::theme_card_sub_lbl = nullptr;

GtkWidget* ControlCenter::brightness_slider = nullptr;
GtkWidget* ControlCenter::brightness_val_lbl = nullptr;
GtkWidget* ControlCenter::volume_slider = nullptr;
GtkWidget* ControlCenter::volume_val_lbl = nullptr;

// Audio Drill-Down Detail Widgets
GtkWidget* ControlCenter::sinks_container = nullptr;
GtkWidget* ControlCenter::audio_detail_vol_slider = nullptr;
GtkWidget* ControlCenter::audio_detail_vol_lbl = nullptr;
GtkWidget* ControlCenter::mic_btn = nullptr;
GtkWidget* ControlCenter::mic_state_lbl = nullptr;
GtkWidget* ControlCenter::mic_gain_slider = nullptr;
GtkWidget* ControlCenter::mic_gain_lbl = nullptr;
GtkWidget* ControlCenter::nc_btn = nullptr;
GtkWidget* ControlCenter::nc_state_lbl = nullptr;

// Theme Drill-Down Detail Widgets
GtkWidget* ControlCenter::theme_grid_container = nullptr;

static std::string exec_cmd_read(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

static bool s_syncing_ui = false;

std::string ControlCenter::get_battery_info() {
    std::string bat_path = "/sys/class/power_supply/BAT0/";
    std::ifstream test(bat_path + "capacity");
    if (!test.is_open()) bat_path = "/sys/class/power_supply/BAT1/";
    test.close();

    std::ifstream f_cap(bat_path + "capacity");
    std::ifstream f_status(bat_path + "status");
    std::string cap = "100", status = "Discharging";
    if (f_cap.is_open()) f_cap >> cap;
    if (f_status.is_open()) f_status >> status;

    if (status == "Charging") {
        return "󰂄 " + cap + "%";
    }
    return "󰂂 " + cap + "%";
}

std::string ControlCenter::get_active_wifi_ssid() {
    std::string ssid = exec_cmd_read("nmcli -t -f ACTIVE,SSID dev wifi 2>/dev/null | grep '^yes:' | cut -d: -f2");
    if (ssid.empty()) return "Disconnected";
    return ssid;
}

bool ControlCenter::is_wifi_enabled() {
    std::string res = exec_cmd_read("nmcli radio wifi 2>/dev/null");
    return res == "enabled";
}

std::string ControlCenter::get_bluetooth_status() {
    std::string p = exec_cmd_read("bluetoothctl show 2>/dev/null | grep 'Powered: yes'");
    if (p.empty()) return "Disabled";
    std::string dev = exec_cmd_read("bluetoothctl info 2>/dev/null | grep 'Name:' | cut -d' ' -f2-");
    if (!dev.empty()) return dev;
    return "Ready";
}

bool ControlCenter::is_bluetooth_enabled() {
    std::string p = exec_cmd_read("bluetoothctl show 2>/dev/null | grep 'Powered: yes'");
    return !p.empty();
}

int ControlCenter::get_current_brightness() {
    // 1. Read target brightness sysfs entry (NOT actual_brightness which fluctuates with ABM/hardware dimming)
    glob_t glob_result;
    if (glob("/sys/class/backlight/*/brightness", 0, nullptr, &glob_result) == 0 && glob_result.gl_pathc > 0) {
        std::ifstream bri_f(glob_result.gl_pathv[0]);
        std::string max_p = glob_result.gl_pathv[0];
        size_t last_slash = max_p.find_last_of('/');
        if (last_slash != std::string::npos) {
            max_p = max_p.substr(0, last_slash) + "/max_brightness";
        }
        std::ifstream max_f(max_p);
        long bri = 0, max = 1;
        if (bri_f.is_open() && max_f.is_open()) {
            bri_f >> bri;
            max_f >> max;
            globfree(&glob_result);
            if (max > 0) return std::clamp(static_cast<int>((bri * 100) / max), 1, 100);
        }
        globfree(&glob_result);
    }

    // 2. Fallback to brightnessctl query
    std::string br = exec_cmd_read("brightnessctl -m 2>/dev/null | cut -d, -f4 | tr -d '%'");
    if (!br.empty()) {
        try { return std::clamp(std::stoi(br), 1, 100); } catch (...) {}
    }
    return 80;
}

std::string ControlCenter::get_display_info() {
    return "1920×1080 @ 60Hz";
}

bool ControlCenter::is_night_light_active() {
    std::string cur = exec_cmd_read("hyprshade current 2>/dev/null");
    return cur.find("blue-light-filter") != std::string::npos;
}

bool ControlCenter::is_focus_dnd_active() {
    return false;
}

void ControlCenter::switch_to_view(const char* view_name) {
    if (stack) {
        gtk_stack_set_visible_child_name(GTK_STACK(stack), view_name);
        if (std::string(view_name) == "audio") {
            refresh_audio_page();
        } else if (std::string(view_name) == "theme") {
            refresh_theme_page();
        }
    }
}

GtkWidget* ControlCenter::create_button() {
    btn_widget = gtk_button_new();
    gtk_widget_add_css_class(btn_widget, "control-btn");
    gtk_widget_set_size_request(btn_widget, 28, 24);

    GtkWidget* icon = gtk_label_new("󰍜");
    gtk_widget_add_css_class(icon, "control-icon");
    gtk_container_add(GTK_CONTAINER(btn_widget), icon);

    g_signal_connect(btn_widget, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        toggle_popup();
    }), nullptr);

    return btn_widget;
}

// ─────────────────────────────────────────────────────────────────────────────
// Page 1: Main Control Center Overview
// ─────────────────────────────────────────────────────────────────────────────
GtkWidget* ControlCenter::create_main_page() {
    GtkWidget* main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    // 1. Header Bar: [ 󰍜 Control Center ] ────── [ Battery 󰂄 96% ] [ Close 󰅖 ]
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header_box, "cc-header-row");

    GtkWidget* header_left = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* cc_icon = gtk_label_new("󰍜");
    gtk_widget_add_css_class(cc_icon, "cc-header-icon");
    GtkWidget* cc_title = gtk_label_new("Control Center");
    gtk_widget_add_css_class(cc_title, "cc-header-title");
    gtk_box_pack_start(GTK_BOX(header_left), cc_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_left), cc_title, FALSE, FALSE, 0);

    GtkWidget* header_right = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    battery_status_lbl = gtk_label_new(get_battery_info().c_str());
    gtk_widget_add_css_class(battery_status_lbl, "cc-battery-badge");

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "cc-close-circle-btn");
    gtk_widget_set_tooltip_text(close_btn, "Close (Esc)");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ControlCenter::hide_popup();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(header_right), battery_status_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_right), close_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(header_box), header_left, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), header_right, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), header_box, FALSE, FALSE, 0);

    // 2. Two-Column Hero Cards Grid (2x2)
    GtkWidget* hero_grid = gtk_grid_new();
    gtk_widget_add_css_class(hero_grid, "cc-hero-grid");
    gtk_grid_set_row_spacing(GTK_GRID(hero_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(hero_grid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(hero_grid), TRUE);

    // ── Hero Card 1: Wi-Fi ──
    GtkWidget* wifi_btn = gtk_button_new();
    gtk_widget_add_css_class(wifi_btn, "cc-hero-card");
    gtk_widget_add_css_class(wifi_btn, "card-wifi");

    GtkWidget* wifi_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* wifi_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* w_icon = gtk_label_new("");
    gtk_widget_add_css_class(w_icon, "cc-card-icon");
    GtkWidget* w_title = gtk_label_new("Wi-Fi");
    gtk_widget_add_css_class(w_title, "cc-card-title");
    gtk_box_pack_start(GTK_BOX(wifi_top), w_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wifi_top), w_title, FALSE, FALSE, 0);

    wifi_ssid_lbl = gtk_label_new(get_active_wifi_ssid().c_str());
    gtk_widget_add_css_class(wifi_ssid_lbl, "cc-card-subtitle");
    gtk_label_set_ellipsize(GTK_LABEL(wifi_ssid_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(wifi_ssid_lbl, GTK_ALIGN_START);

    GtkWidget* wifi_bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    wifi_state_pill = gtk_label_new(is_wifi_enabled() ? "ON  ›" : "OFF  ›");
    gtk_widget_add_css_class(wifi_state_pill, "cc-card-pill");
    gtk_box_pack_start(GTK_BOX(wifi_bot), wifi_state_pill, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(wifi_content), wifi_top, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wifi_content), wifi_ssid_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wifi_content), wifi_bot, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(wifi_btn), wifi_content);
    g_signal_connect(wifi_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        WifiManager::show_panel();
        ControlCenter::hide_popup();
    }), nullptr);

    // ── Hero Card 2: Bluetooth ──
    GtkWidget* bt_btn = gtk_button_new();
    gtk_widget_add_css_class(bt_btn, "cc-hero-card");
    gtk_widget_add_css_class(bt_btn, "card-bluetooth");

    GtkWidget* bt_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* bt_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* b_icon = gtk_label_new("󰂯");
    gtk_widget_add_css_class(b_icon, "cc-card-icon");
    GtkWidget* b_title = gtk_label_new("Bluetooth");
    gtk_widget_add_css_class(b_title, "cc-card-title");
    gtk_box_pack_start(GTK_BOX(bt_top), b_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bt_top), b_title, FALSE, FALSE, 0);

    bt_status_lbl = gtk_label_new(get_bluetooth_status().c_str());
    gtk_widget_add_css_class(bt_status_lbl, "cc-card-subtitle");
    gtk_label_set_ellipsize(GTK_LABEL(bt_status_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(bt_status_lbl, GTK_ALIGN_START);

    GtkWidget* bt_bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    bt_state_pill = gtk_label_new(is_bluetooth_enabled() ? "ON  ›" : "OFF  ›");
    gtk_widget_add_css_class(bt_state_pill, "cc-card-pill");
    gtk_box_pack_start(GTK_BOX(bt_bot), bt_state_pill, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(bt_content), bt_top, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bt_content), bt_status_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bt_content), bt_bot, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(bt_btn), bt_content);
    g_signal_connect(bt_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        system("blueman-manager 2>/dev/null &");
        ControlCenter::hide_popup();
    }), nullptr);

    // ── Hero Card 3: Audio (Drill-Down Trigger) ──
    GtkWidget* audio_btn = gtk_button_new();
    gtk_widget_add_css_class(audio_btn, "cc-hero-card");
    gtk_widget_add_css_class(audio_btn, "card-audio");

    GtkWidget* audio_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* audio_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* a_icon = gtk_label_new("");
    gtk_widget_add_css_class(a_icon, "cc-card-icon");
    GtkWidget* a_title = gtk_label_new("Audio");
    gtk_widget_add_css_class(a_title, "cc-card-title");
    gtk_box_pack_start(GTK_BOX(audio_top), a_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(audio_top), a_title, FALSE, FALSE, 0);

    audio_sub_lbl = gtk_label_new(AudioManager::get_default_sink_name().c_str());
    gtk_widget_add_css_class(audio_sub_lbl, "cc-card-subtitle");
    gtk_label_set_ellipsize(GTK_LABEL(audio_sub_lbl), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(audio_sub_lbl, GTK_ALIGN_START);

    GtkWidget* audio_bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    int cur_vol = AudioManager::get_volume();
    audio_pill_lbl = gtk_label_new((std::to_string(cur_vol) + "%  ›").c_str());
    gtk_widget_add_css_class(audio_pill_lbl, "cc-card-pill");
    gtk_box_pack_start(GTK_BOX(audio_bot), audio_pill_lbl, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(audio_content), audio_top, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(audio_content), audio_sub_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(audio_content), audio_bot, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(audio_btn), audio_content);
    g_signal_connect(audio_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ControlCenter::switch_to_view("audio");
    }), nullptr);

    // ── Hero Card 4: Display ──
    GtkWidget* display_btn = gtk_button_new();
    gtk_widget_add_css_class(display_btn, "cc-hero-card");
    gtk_widget_add_css_class(display_btn, "card-display");

    GtkWidget* display_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* display_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* d_icon = gtk_label_new("󰍹");
    gtk_widget_add_css_class(d_icon, "cc-card-icon");
    GtkWidget* d_title = gtk_label_new("Display");
    gtk_widget_add_css_class(d_title, "cc-card-title");
    gtk_box_pack_start(GTK_BOX(display_top), d_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(display_top), d_title, FALSE, FALSE, 0);

    display_sub_lbl = gtk_label_new(get_display_info().c_str());
    gtk_widget_add_css_class(display_sub_lbl, "cc-card-subtitle");
    gtk_widget_set_halign(display_sub_lbl, GTK_ALIGN_START);

    GtkWidget* display_bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* display_pill = gtk_label_new("100% Scale  ›");
    gtk_widget_add_css_class(display_pill, "cc-card-pill");
    gtk_box_pack_start(GTK_BOX(display_bot), display_pill, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(display_content), display_top, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(display_content), display_sub_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(display_content), display_bot, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(display_btn), display_content);
    g_signal_connect(display_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        system("nwg-displays 2>/dev/null || wdisplays 2>/dev/null &");
        ControlCenter::hide_popup();
    }), nullptr);

    gtk_grid_attach(GTK_GRID(hero_grid), wifi_btn, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hero_grid), bt_btn, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hero_grid), audio_btn, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(hero_grid), display_btn, 1, 1, 1, 1);

    gtk_box_pack_start(GTK_BOX(main_vbox), hero_grid, FALSE, FALSE, 0);

    // 3. Quick Modes Strip (1x3 Grid: Night Light, Focus, Global Dark Mode)
    GtkWidget* modes_grid = gtk_grid_new();
    gtk_widget_add_css_class(modes_grid, "cc-modes-grid");
    gtk_grid_set_column_spacing(GTK_GRID(modes_grid), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(modes_grid), TRUE);

    // Night Light
    night_btn = gtk_button_new();
    gtk_widget_add_css_class(night_btn, "cc-mode-btn");
    GtkWidget* n_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* n_title = gtk_label_new("☾ Night");
    gtk_widget_add_css_class(n_title, "cc-mode-title");
    night_sub_lbl = gtk_label_new(is_night_light_active() ? "ON" : "OFF");
    gtk_widget_add_css_class(night_sub_lbl, "cc-mode-sub");
    if (is_night_light_active()) gtk_widget_add_css_class(night_btn, "active");
    gtk_box_pack_start(GTK_BOX(n_box), n_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(n_box), night_sub_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(night_btn), n_box);
    g_signal_connect(night_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        system("hyprshade toggle blue-light-filter 2>/dev/null &");
        if (gtk_widget_has_css_class(GTK_WIDGET(btn), "active")) {
            gtk_widget_remove_css_class(GTK_WIDGET(btn), "active");
            gtk_label_set_text(GTK_LABEL(night_sub_lbl), "OFF");
        } else {
            gtk_widget_add_css_class(GTK_WIDGET(btn), "active");
            gtk_label_set_text(GTK_LABEL(night_sub_lbl), "ON");
        }
    }), nullptr);

    // Focus / DND
    focus_btn = gtk_button_new();
    gtk_widget_add_css_class(focus_btn, "cc-mode-btn");
    GtkWidget* f_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* f_title = gtk_label_new("󰌵 Focus");
    gtk_widget_add_css_class(f_title, "cc-mode-title");
    focus_sub_lbl = gtk_label_new("OFF");
    gtk_widget_add_css_class(focus_sub_lbl, "cc-mode-sub");
    gtk_box_pack_start(GTK_BOX(f_box), f_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(f_box), focus_sub_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(focus_btn), f_box);
    g_signal_connect(focus_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        system("$HOME/.config/hypr/scripts/ui/toggle_dnd.sh 2>/dev/null &");
        if (gtk_widget_has_css_class(GTK_WIDGET(btn), "active")) {
            gtk_widget_remove_css_class(GTK_WIDGET(btn), "active");
            gtk_label_set_text(GTK_LABEL(focus_sub_lbl), "OFF");
        } else {
            gtk_widget_add_css_class(GTK_WIDGET(btn), "active");
            gtk_label_set_text(GTK_LABEL(focus_sub_lbl), "ON");
        }
    }), nullptr);

    // Global Dark Appearance (Color Scheme Prefer-Dark)
    dark_btn = gtk_button_new();
    gtk_widget_add_css_class(dark_btn, "cc-mode-btn");
    bool is_dark = ThemeEngine::is_dark_mode();
    if (is_dark) gtk_widget_add_css_class(dark_btn, "active");
    GtkWidget* dk_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* dk_title = gtk_label_new("◐ Dark");
    gtk_widget_add_css_class(dk_title, "cc-mode-title");
    dark_sub_lbl = gtk_label_new(is_dark ? "ON" : "OFF");
    gtk_widget_add_css_class(dark_sub_lbl, "cc-mode-sub");
    gtk_box_pack_start(GTK_BOX(dk_box), dk_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dk_box), dark_sub_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(dark_btn), dk_box);
    g_signal_connect(dark_btn, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer) {
        bool now_dark = ThemeEngine::toggle_dark_mode();
        if (now_dark) {
            gtk_widget_add_css_class(GTK_WIDGET(btn), "active");
            gtk_label_set_text(GTK_LABEL(dark_sub_lbl), "ON");
        } else {
            gtk_widget_remove_css_class(GTK_WIDGET(btn), "active");
            gtk_label_set_text(GTK_LABEL(dark_sub_lbl), "OFF");
        }
    }), nullptr);

    gtk_grid_attach(GTK_GRID(modes_grid), night_btn, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(modes_grid), focus_btn, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(modes_grid), dark_btn, 2, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(main_vbox), modes_grid, FALSE, FALSE, 0);

    // 4. Dedicated Theme Selection Card (Drill-Down Trigger)
    theme_card_btn = gtk_button_new();
    gtk_widget_add_css_class(theme_card_btn, "cc-theme-card-btn");
    GtkWidget* th_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    GtkWidget* th_icon = gtk_label_new("🎨");
    gtk_widget_add_css_class(th_icon, "cc-theme-icon");

    GtkWidget* th_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget* th_title = gtk_label_new("Theme Palette");
    gtk_widget_add_css_class(th_title, "cc-theme-title");
    gtk_widget_set_halign(th_title, GTK_ALIGN_START);

    std::string cur_th = ThemeEngine::get_current_theme_name();
    if (!cur_th.empty()) cur_th[0] = std::toupper(cur_th[0]);
    theme_card_sub_lbl = gtk_label_new(cur_th.c_str());
    gtk_widget_add_css_class(theme_card_sub_lbl, "cc-theme-sub");
    gtk_widget_set_halign(theme_card_sub_lbl, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(th_vbox), th_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(th_vbox), theme_card_sub_lbl, FALSE, FALSE, 0);

    GtkWidget* th_pill = gtk_label_new("Palettes  ›");
    gtk_widget_add_css_class(th_pill, "cc-card-pill");

    gtk_box_pack_start(GTK_BOX(th_row), th_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(th_row), th_vbox, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(th_row), th_pill, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(theme_card_btn), th_row);
    g_signal_connect(theme_card_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ControlCenter::switch_to_view("theme");
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(main_vbox), theme_card_btn, FALSE, FALSE, 0);

    // 5. Sliders Card (Brightness & Volume with Percentage Badges)
    GtkWidget* sliders_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(sliders_card, "cc-sliders-card");

    // Brightness Slider Row
    GtkWidget* bri_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* bri_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* bri_icon = gtk_label_new("󰃠");
    gtk_widget_add_css_class(bri_icon, "cc-slider-icon");
    GtkWidget* bri_lbl = gtk_label_new("Brightness");
    gtk_widget_add_css_class(bri_lbl, "cc-slider-lbl");
    int cur_bri = get_current_brightness();
    brightness_val_lbl = gtk_label_new((std::to_string(cur_bri) + "%").c_str());
    gtk_widget_add_css_class(brightness_val_lbl, "cc-slider-val");

    gtk_box_pack_start(GTK_BOX(bri_header), bri_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bri_header), bri_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(bri_header), brightness_val_lbl, FALSE, FALSE, 0);

    brightness_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 5, 100, 1);
    gtk_range_set_value(GTK_RANGE(brightness_slider), cur_bri);
    gtk_widget_set_hexpand(brightness_slider, TRUE);
    g_signal_connect(brightness_slider, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer) {
        if (s_syncing_ui) return;
        int val = static_cast<int>(gtk_range_get_value(range));
        if (brightness_val_lbl) {
            gtk_label_set_text(GTK_LABEL(brightness_val_lbl), (std::to_string(val) + "%").c_str());
        }
        char cmd[64];
        snprintf(cmd, sizeof(cmd), "brightnessctl set %d%% 2>/dev/null &", val);
        system(cmd);
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(bri_vbox), bri_header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bri_vbox), brightness_slider, FALSE, FALSE, 0);

    // Volume Slider Row
    GtkWidget* vol_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* vol_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* vol_icon = gtk_label_new("");
    gtk_widget_add_css_class(vol_icon, "cc-slider-icon");
    GtkWidget* vol_lbl = gtk_label_new("Volume");
    gtk_widget_add_css_class(vol_lbl, "cc-slider-lbl");
    volume_val_lbl = gtk_label_new((std::to_string(cur_vol) + "%").c_str());
    gtk_widget_add_css_class(volume_val_lbl, "cc-slider-val");

    gtk_box_pack_start(GTK_BOX(vol_header), vol_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vol_header), vol_lbl, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(vol_header), volume_val_lbl, FALSE, FALSE, 0);

    volume_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value(GTK_RANGE(volume_slider), cur_vol);
    gtk_widget_set_hexpand(volume_slider, TRUE);
    g_signal_connect(volume_slider, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer) {
        if (s_syncing_ui) return;
        int val = static_cast<int>(gtk_range_get_value(range));
        if (volume_val_lbl) {
            gtk_label_set_text(GTK_LABEL(volume_val_lbl), (std::to_string(val) + "%").c_str());
        }
        if (audio_pill_lbl) {
            gtk_label_set_text(GTK_LABEL(audio_pill_lbl), (std::to_string(val) + "%  ›").c_str());
        }
        AudioManager::set_volume(val);
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(vol_vbox), vol_header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vol_vbox), volume_slider, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(sliders_card), bri_vbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sliders_card), vol_vbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), sliders_card, FALSE, FALSE, 0);

    // 6. Bottom Power Surface Strip (Lock, Logout, Reboot, Power)
    GtkWidget* power_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(power_bar, "cc-power-bar");
    gtk_widget_set_halign(power_bar, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(power_bar, TRUE);

    auto create_pwr_btn = [](const char* icon_str, const char* label_str, const char* css_class, const char* tooltip, const char* cmd) -> GtkWidget* {
        GtkWidget* btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "cc-power-btn");
        gtk_widget_add_css_class(btn, css_class);
        gtk_widget_set_tooltip_text(btn, tooltip);

        GtkWidget* b_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign(b_box, GTK_ALIGN_CENTER);

        GtkWidget* ic = gtk_label_new(icon_str);
        gtk_widget_add_css_class(ic, "cc-power-btn-icon");
        GtkWidget* lb = gtk_label_new(label_str);
        gtk_widget_add_css_class(lb, "cc-power-btn-lbl");

        gtk_box_pack_start(GTK_BOX(b_box), ic, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(b_box), lb, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(btn), b_box);

        std::string command = cmd;
        g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            char* c = static_cast<char*>(data);
            if (c) system(c);
            ControlCenter::hide_popup();
        }), strdup(command.c_str()));

        return btn;
    };

    GtkWidget* p_lock = create_pwr_btn("", "Lock", "pwr-lock", "Lock Screen (hyprlock)", "loginctl lock-session 2>/dev/null || hyprlock 2>/dev/null &");
    GtkWidget* p_logout = create_pwr_btn("󰍃", "Logout", "pwr-logout", "Exit Hyprland", "hyprctl dispatch exit 2>/dev/null &");
    GtkWidget* p_reboot = create_pwr_btn("󰜉", "Reboot", "pwr-reboot", "Restart System", "systemctl reboot 2>/dev/null &");
    GtkWidget* p_power = create_pwr_btn("", "Power", "pwr-shutdown", "Power Off PC", "systemctl poweroff 2>/dev/null &");

    gtk_box_pack_start(GTK_BOX(power_bar), p_lock, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(power_bar), p_logout, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(power_bar), p_reboot, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(power_bar), p_power, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), power_bar, FALSE, FALSE, 0);

    return main_vbox;
}

// ─────────────────────────────────────────────────────────────────────────────
// Page 2: Audio Drill-Down Detail Page
// ─────────────────────────────────────────────────────────────────────────────
GtkWidget* ControlCenter::create_audio_page() {
    GtkWidget* audio_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    // 1. Header with Back Button and Pavucontrol Shortcut
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header_box, "cc-header-row");

    GtkWidget* back_btn = gtk_button_new();
    gtk_widget_add_css_class(back_btn, "cc-back-btn");
    GtkWidget* back_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* back_icon = gtk_label_new("󰅁");
    gtk_widget_add_css_class(back_icon, "cc-back-icon");
    GtkWidget* back_lbl = gtk_label_new("Back");
    gtk_widget_add_css_class(back_lbl, "cc-back-lbl");
    gtk_box_pack_start(GTK_BOX(back_box), back_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(back_box), back_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(back_btn), back_box);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ControlCenter::switch_to_view("main");
    }), nullptr);

    GtkWidget* title = gtk_label_new("Audio Routing");
    gtk_widget_add_css_class(title, "cc-header-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    GtkWidget* pavu_btn = gtk_button_new_with_label(" Mix");
    gtk_widget_add_css_class(pavu_btn, "cc-action-pill-btn");
    gtk_widget_set_tooltip_text(pavu_btn, "Open PulseAudio Volume Control (pavucontrol)");
    g_signal_connect(pavu_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        system("pavucontrol 2>/dev/null &");
        ControlCenter::hide_popup();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(header_box), back_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), pavu_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(audio_vbox), header_box, FALSE, FALSE, 0);

    // 2. Output Devices Section
    GtkWidget* out_hdr = gtk_label_new("OUTPUT DEVICES");
    gtk_widget_add_css_class(out_hdr, "cc-section-lbl");
    gtk_widget_set_halign(out_hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(audio_vbox), out_hdr, FALSE, FALSE, 0);

    sinks_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(sinks_container, "cc-device-group-card");
    gtk_box_pack_start(GTK_BOX(audio_vbox), sinks_container, FALSE, FALSE, 0);

    // 3. Output Volume Slider Card
    GtkWidget* vol_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(vol_card, "cc-sliders-card");

    GtkWidget* vol_hdr_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* v_icon = gtk_label_new("");
    gtk_widget_add_css_class(v_icon, "cc-slider-icon");
    GtkWidget* v_title = gtk_label_new("Output Volume");
    gtk_widget_add_css_class(v_title, "cc-slider-lbl");
    int cur_v = AudioManager::get_volume();
    audio_detail_vol_lbl = gtk_label_new((std::to_string(cur_v) + "%").c_str());
    gtk_widget_add_css_class(audio_detail_vol_lbl, "cc-slider-val");

    gtk_box_pack_start(GTK_BOX(vol_hdr_box), v_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vol_hdr_box), v_title, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(vol_hdr_box), audio_detail_vol_lbl, FALSE, FALSE, 0);

    audio_detail_vol_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value(GTK_RANGE(audio_detail_vol_slider), cur_v);
    gtk_widget_set_hexpand(audio_detail_vol_slider, TRUE);
    g_signal_connect(audio_detail_vol_slider, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer) {
        if (s_syncing_ui) return;
        int val = static_cast<int>(gtk_range_get_value(range));
        if (audio_detail_vol_lbl) {
            gtk_label_set_text(GTK_LABEL(audio_detail_vol_lbl), (std::to_string(val) + "%").c_str());
        }
        if (volume_val_lbl) {
            gtk_label_set_text(GTK_LABEL(volume_val_lbl), (std::to_string(val) + "%").c_str());
        }
        if (audio_pill_lbl) {
            gtk_label_set_text(GTK_LABEL(audio_pill_lbl), (std::to_string(val) + "%  ›").c_str());
        }
        AudioManager::set_volume(val);
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(vol_card), vol_hdr_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vol_card), audio_detail_vol_slider, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(audio_vbox), vol_card, FALSE, FALSE, 0);

    // 4. Microphone Section
    GtkWidget* mic_hdr = gtk_label_new("MICROPHONE INPUT");
    gtk_widget_add_css_class(mic_hdr, "cc-section-lbl");
    gtk_widget_set_halign(mic_hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(audio_vbox), mic_hdr, FALSE, FALSE, 0);

    mic_btn = gtk_button_new();
    gtk_widget_add_css_class(mic_btn, "cc-device-row-btn");
    GtkWidget* mic_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* m_ic = gtk_label_new("󰍬");
    gtk_widget_add_css_class(m_ic, "cc-device-icon");
    GtkWidget* m_title = gtk_label_new(AudioManager::get_default_source_name().c_str());
    gtk_widget_add_css_class(m_title, "cc-device-title");
    gtk_widget_set_halign(m_title, GTK_ALIGN_START);

    mic_state_lbl = gtk_label_new(AudioManager::is_mic_muted() ? "MUTED" : "ON");
    gtk_widget_add_css_class(mic_state_lbl, "cc-state-tag");
    if (!AudioManager::is_mic_muted()) gtk_widget_add_css_class(mic_state_lbl, "active");

    gtk_box_pack_start(GTK_BOX(mic_row), m_ic, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mic_row), m_title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(mic_row), mic_state_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mic_btn), mic_row);
    g_signal_connect(mic_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        AudioManager::toggle_mic_mute();
        ControlCenter::refresh_audio_page();
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(audio_vbox), mic_btn, FALSE, FALSE, 0);

    // 5. Microphone Gain Slider Card
    GtkWidget* gain_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(gain_card, "cc-sliders-card");

    GtkWidget* gain_hdr_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* g_icon = gtk_label_new("󰍬");
    gtk_widget_add_css_class(g_icon, "cc-slider-icon");
    GtkWidget* g_title = gtk_label_new("Microphone Gain");
    gtk_widget_add_css_class(g_title, "cc-slider-lbl");
    int cur_g = AudioManager::get_mic_volume();
    mic_gain_lbl = gtk_label_new((std::to_string(cur_g) + "%").c_str());
    gtk_widget_add_css_class(mic_gain_lbl, "cc-slider-val");

    gtk_box_pack_start(GTK_BOX(gain_hdr_box), g_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gain_hdr_box), g_title, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(gain_hdr_box), mic_gain_lbl, FALSE, FALSE, 0);

    mic_gain_slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_range_set_value(GTK_RANGE(mic_gain_slider), cur_g);
    gtk_widget_set_hexpand(mic_gain_slider, TRUE);
    g_signal_connect(mic_gain_slider, "value-changed", G_CALLBACK(+[](GtkRange* range, gpointer) {
        if (s_syncing_ui) return;
        int val = static_cast<int>(gtk_range_get_value(range));
        if (mic_gain_lbl) {
            gtk_label_set_text(GTK_LABEL(mic_gain_lbl), (std::to_string(val) + "%").c_str());
        }
        AudioManager::set_mic_volume(val);
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(gain_card), gain_hdr_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gain_card), mic_gain_slider, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(audio_vbox), gain_card, FALSE, FALSE, 0);

    // 6. Enhancements Section (Noise Cancellation)
    GtkWidget* enh_hdr = gtk_label_new("AUDIO ENHANCEMENTS");
    gtk_widget_add_css_class(enh_hdr, "cc-section-lbl");
    gtk_widget_set_halign(enh_hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(audio_vbox), enh_hdr, FALSE, FALSE, 0);

    nc_btn = gtk_button_new();
    gtk_widget_add_css_class(nc_btn, "cc-device-row-btn");
    GtkWidget* nc_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* nc_ic = gtk_label_new("󰔏");
    gtk_widget_add_css_class(nc_ic, "cc-device-icon");
    GtkWidget* nc_tbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* nc_title = gtk_label_new("Noise Suppression");
    gtk_widget_add_css_class(nc_title, "cc-device-title");
    gtk_widget_set_halign(nc_title, GTK_ALIGN_START);
    GtkWidget* nc_sub = gtk_label_new("RNNoise Neural Voice Filter");
    gtk_widget_add_css_class(nc_sub, "cc-device-sub");
    gtk_widget_set_halign(nc_sub, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(nc_tbox), nc_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nc_tbox), nc_sub, FALSE, FALSE, 0);

    bool nc_act = AudioManager::is_noise_cancelling_active();
    nc_state_lbl = gtk_label_new(nc_act ? "ON" : "OFF");
    gtk_widget_add_css_class(nc_state_lbl, "cc-state-tag");
    if (nc_act) gtk_widget_add_css_class(nc_state_lbl, "active");

    gtk_box_pack_start(GTK_BOX(nc_row), nc_ic, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(nc_row), nc_tbox, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(nc_row), nc_state_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(nc_btn), nc_row);
    g_signal_connect(nc_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        AudioManager::toggle_noise_cancelling();
        ControlCenter::refresh_audio_page();
    }), nullptr);
    gtk_box_pack_start(GTK_BOX(audio_vbox), nc_btn, FALSE, FALSE, 0);

    return audio_vbox;
}

// ─────────────────────────────────────────────────────────────────────────────
// Page 3: Theme Drill-Down Detail Page
// ─────────────────────────────────────────────────────────────────────────────
GtkWidget* ControlCenter::create_theme_page() {
    GtkWidget* theme_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    // 1. Header with Back Button
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header_box, "cc-header-row");

    GtkWidget* back_btn = gtk_button_new();
    gtk_widget_add_css_class(back_btn, "cc-back-btn");
    GtkWidget* back_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* back_icon = gtk_label_new("󰅁");
    gtk_widget_add_css_class(back_icon, "cc-back-icon");
    GtkWidget* back_lbl = gtk_label_new("Back");
    gtk_widget_add_css_class(back_lbl, "cc-back-lbl");
    gtk_box_pack_start(GTK_BOX(back_box), back_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(back_box), back_lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(back_btn), back_box);
    g_signal_connect(back_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ControlCenter::switch_to_view("main");
    }), nullptr);

    GtkWidget* title = gtk_label_new("Theme Palettes");
    gtk_widget_add_css_class(title, "cc-header-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(header_box), back_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(theme_vbox), header_box, FALSE, FALSE, 0);

    // 2. Wallpaper Quick Switcher Card
    GtkWidget* wp_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(wp_bar, "cc-wp-bar-card");

    GtkWidget* wp_left = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* wp_ic = gtk_label_new("󰸉");
    gtk_widget_add_css_class(wp_ic, "cc-wp-icon");
    GtkWidget* wp_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget* wp_title = gtk_label_new("Wallpaper Sync");
    gtk_widget_add_css_class(wp_title, "cc-wp-title");
    gtk_widget_set_halign(wp_title, GTK_ALIGN_START);
    GtkWidget* wp_sub = gtk_label_new("Matches active theme pack");
    gtk_widget_add_css_class(wp_sub, "cc-wp-sub");
    gtk_widget_set_halign(wp_sub, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(wp_vbox), wp_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wp_vbox), wp_sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wp_left), wp_ic, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wp_left), wp_vbox, TRUE, TRUE, 0);

    GtkWidget* next_wp_btn = gtk_button_new_with_label("󰒭 Next Wallpaper");
    gtk_widget_add_css_class(next_wp_btn, "cc-action-pill-btn");
    g_signal_connect(next_wp_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ThemeEngine::cycle_wallpaper();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(wp_bar), wp_left, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(wp_bar), next_wp_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(theme_vbox), wp_bar, FALSE, FALSE, 0);

    // 3. Section Header
    GtkWidget* th_hdr = gtk_label_new("AVAILABLE THEMES");
    gtk_widget_add_css_class(th_hdr, "cc-section-lbl");
    gtk_widget_set_halign(th_hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(theme_vbox), th_hdr, FALSE, FALSE, 0);

    // 4. 2-Column Grid Container for Themes inside ScrolledWindow
    theme_grid_container = gtk_grid_new();
    gtk_widget_add_css_class(theme_grid_container, "cc-theme-grid");
    gtk_grid_set_row_spacing(GTK_GRID(theme_grid_container), 8);
    gtk_grid_set_column_spacing(GTK_GRID(theme_grid_container), 8);
    gtk_grid_set_column_homogeneous(GTK_GRID(theme_grid_container), TRUE);

    GtkWidget* scroll_win = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_win), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll_win, -1, 360);
    gtk_widget_add_css_class(scroll_win, "cc-theme-scrolled-win");
    gtk_container_add(GTK_CONTAINER(scroll_win), theme_grid_container);

    gtk_box_pack_start(GTK_BOX(theme_vbox), scroll_win, TRUE, TRUE, 0);

    return theme_vbox;
}

void ControlCenter::refresh_theme_page() {
    if (!theme_grid_container) return;

    // Clear existing children
    GList* children = gtk_container_get_children(GTK_CONTAINER(theme_grid_container));
    for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    }
    g_list_free(children);

    std::string cur_theme_id = ThemeEngine::get_current_theme().name;
    auto theme_list = ThemeEngine::get_theme_infos();

    int row = 0, col = 0;
    for (const auto& t : theme_list) {
        GtkWidget* btn = gtk_button_new();
        gtk_widget_add_css_class(btn, "cc-theme-tile-btn");
        bool is_active = (t.id == cur_theme_id);
        if (is_active) gtk_widget_add_css_class(btn, "active");

        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

        // Top Row: Color Preview Dots + Checkmark
        GtkWidget* top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        
        auto create_dot = [](const std::string& color_hex) -> GtkWidget* {
            GtkWidget* dot = gtk_label_new("●");
            gtk_widget_add_css_class(dot, "cc-theme-swatch-dot");
            // Inject inline color styling through CSS name
            GtkCssProvider* p = gtk_css_provider_new();
            std::string s = "label { color: " + color_hex + "; font-size: 13px; }";
            gtk_css_provider_load_from_data(p, s.c_str(), -1, nullptr);
            gtk_style_context_add_provider(gtk_widget_get_style_context(dot), GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            return dot;
        };

        gtk_box_pack_start(GTK_BOX(top_row), create_dot(t.c_accent), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(top_row), create_dot(t.c_secondary), FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(top_row), create_dot(t.c_surface), FALSE, FALSE, 0);

        if (is_active) {
            GtkWidget* check = gtk_label_new("✓");
            gtk_widget_add_css_class(check, "cc-check-badge");
            gtk_box_pack_end(GTK_BOX(top_row), check, FALSE, FALSE, 0);
        }

        // Title and Subtitle
        GtkWidget* name_lbl = gtk_label_new(t.name.c_str());
        gtk_widget_add_css_class(name_lbl, "cc-theme-tile-title");
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);

        GtkWidget* desc_lbl = gtk_label_new(t.desc.c_str());
        gtk_widget_add_css_class(desc_lbl, "cc-theme-tile-desc");
        gtk_widget_set_halign(desc_lbl, GTK_ALIGN_START);

        gtk_box_pack_start(GTK_BOX(box), top_row, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), name_lbl, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), desc_lbl, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(btn), box);

        std::string theme_id = t.id;
        g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            char* id = static_cast<char*>(data);
            if (id) {
                ThemeEngine::set_theme(id);
                ControlCenter::refresh_theme_page();
                ControlCenter::refresh_data();
            }
        }), strdup(theme_id.c_str()));

        gtk_grid_attach(GTK_GRID(theme_grid_container), btn, col, row, 1, 1);
        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }

    gtk_widget_show_all(theme_grid_container);
}

void ControlCenter::refresh_audio_page() {
    // 1. Refresh Sinks list
    if (sinks_container) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(sinks_container));
        for (GList* iter = children; iter != nullptr; iter = g_list_next(iter)) {
            gtk_widget_destroy(GTK_WIDGET(iter->data));
        }
        g_list_free(children);

        auto sinks = AudioManager::get_sinks();
        for (const auto& dev : sinks) {
            GtkWidget* btn = gtk_button_new();
            gtk_widget_add_css_class(btn, "cc-sink-item-btn");
            if (dev.is_default) gtk_widget_add_css_class(btn, "active");

            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            std::string ic_str = (dev.name.find("Headphone") != std::string::npos) ? "󰋋" : "";
            GtkWidget* ic = gtk_label_new(ic_str.c_str());
            gtk_widget_add_css_class(ic, "cc-device-icon");

            GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
            GtkWidget* name_lbl = gtk_label_new(dev.name.c_str());
            gtk_widget_add_css_class(name_lbl, "cc-device-title");
            gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);

            GtkWidget* sub_lbl = gtk_label_new(dev.is_default ? "Active Output" : "Available");
            gtk_widget_add_css_class(sub_lbl, "cc-device-sub");
            gtk_widget_set_halign(sub_lbl, GTK_ALIGN_START);

            gtk_box_pack_start(GTK_BOX(vbox), name_lbl, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(vbox), sub_lbl, FALSE, FALSE, 0);

            gtk_box_pack_start(GTK_BOX(row), ic, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(row), vbox, TRUE, TRUE, 0);

            if (dev.is_default) {
                GtkWidget* check = gtk_label_new("✓");
                gtk_widget_add_css_class(check, "cc-check-badge");
                gtk_box_pack_end(GTK_BOX(row), check, FALSE, FALSE, 0);
            }

            gtk_container_add(GTK_CONTAINER(btn), row);

            int d_id = dev.id;
            g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                int id = GPOINTER_TO_INT(data);
                AudioManager::set_default_sink(id);
                ControlCenter::refresh_audio_page();
                ControlCenter::refresh_data();
            }), GINT_TO_POINTER(d_id));

            gtk_box_pack_start(GTK_BOX(sinks_container), btn, FALSE, FALSE, 0);
        }
        gtk_widget_show_all(sinks_container);
    }

    // 2. Refresh Volume Slider
    s_syncing_ui = true;
    int vol = AudioManager::get_volume();
    if (audio_detail_vol_slider) {
        gtk_range_set_value(GTK_RANGE(audio_detail_vol_slider), vol);
    }
    if (audio_detail_vol_lbl) {
        gtk_label_set_text(GTK_LABEL(audio_detail_vol_lbl), (std::to_string(vol) + "%").c_str());
    }

    // 3. Refresh Microphone State & Gain
    bool mic_m = AudioManager::is_mic_muted();
    if (mic_state_lbl) {
        gtk_label_set_text(GTK_LABEL(mic_state_lbl), mic_m ? "MUTED" : "ON");
        if (mic_m) {
            gtk_widget_remove_css_class(mic_state_lbl, "active");
        } else {
            gtk_widget_add_css_class(mic_state_lbl, "active");
        }
    }
    int mg = AudioManager::get_mic_volume();
    if (mic_gain_slider) {
        gtk_range_set_value(GTK_RANGE(mic_gain_slider), mg);
    }
    if (mic_gain_lbl) {
        gtk_label_set_text(GTK_LABEL(mic_gain_lbl), (std::to_string(mg) + "%").c_str());
    }
    s_syncing_ui = false;

    // 4. Refresh Noise Cancelling
    bool nc = AudioManager::is_noise_cancelling_active();
    if (nc_state_lbl) {
        gtk_label_set_text(GTK_LABEL(nc_state_lbl), nc ? "ON" : "OFF");
        if (nc) gtk_widget_add_css_class(nc_state_lbl, "active");
        else gtk_widget_remove_css_class(nc_state_lbl, "active");
    }
}

void ControlCenter::init_popup() {
    if (popup_window) return;

    popup_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_add_css_class(popup_window, "control-center-window");

    // Enable true RGBA transparency
    GdkScreen* screen = gtk_widget_get_screen(popup_window);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
        gtk_widget_set_visual(popup_window, visual);
    }
    gtk_widget_set_app_paintable(popup_window, TRUE);

    g_signal_connect(popup_window, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer) -> gboolean {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_restore(cr);
        return FALSE;
    }), nullptr);

    gtk_layer_init_for_window(GTK_WINDOW(popup_window));
    gtk_layer_set_layer(GTK_WINDOW(popup_window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(popup_window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    // Full screen overlay for click-anywhere dismissal
    gtk_layer_set_anchor(GTK_WINDOW(popup_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(popup_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(popup_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(popup_window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    GtkWidget* backdrop = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(backdrop), FALSE);
    gtk_widget_add_css_class(backdrop, "control-center-backdrop");

    GtkWidget* outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(outer_box, GTK_ALIGN_END);
    gtk_widget_set_valign(outer_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(outer_box, 48);
    gtk_widget_set_margin_end(outer_box, 12);

    GtkWidget* main_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(main_card, "control-card-flagship");
    gtk_widget_set_size_request(main_card, 400, -1);

    auto on_backdrop_clicked = +[](GtkWidget* widget, GdkEventButton* event, gpointer user_data) -> gboolean {
        GtkWidget* card = static_cast<GtkWidget*>(user_data);
        if (!card) return FALSE;
        GtkAllocation alloc;
        gtk_widget_get_allocation(card, &alloc);
        int wx = 0, wy = 0;
        gtk_widget_translate_coordinates(widget, card, static_cast<int>(event->x), static_cast<int>(event->y), &wx, &wy);
        if (wx >= 0 && wx < alloc.width && wy >= 0 && wy < alloc.height) {
            return FALSE; // Clicked inside Control Center card -> keep open!
        }
        ControlCenter::hide_popup(); // Clicked anywhere outside -> close immediately!
        return TRUE;
    };
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_backdrop_clicked), main_card);

    // GtkStack for Drill-Down Navigation
    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 200);

    GtkWidget* page_main = create_main_page();
    GtkWidget* page_audio = create_audio_page();
    GtkWidget* page_theme = create_theme_page();

    gtk_widget_show_all(page_main);
    gtk_widget_show_all(page_audio);
    gtk_widget_show_all(page_theme);

    gtk_stack_add_named(GTK_STACK(stack), page_main, "main");
    gtk_stack_add_named(GTK_STACK(stack), page_audio, "audio");
    gtk_stack_add_named(GTK_STACK(stack), page_theme, "theme");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "main");

    gtk_box_pack_start(GTK_BOX(main_card), stack, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(outer_box), main_card, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(backdrop), outer_box);
    gtk_container_add(GTK_CONTAINER(popup_window), backdrop);

    gtk_widget_show(stack);
    gtk_widget_show(main_card);
    gtk_widget_show(outer_box);
    gtk_widget_show(backdrop);

    g_signal_connect(popup_window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            if (stack) {
                std::string cur = gtk_stack_get_visible_child_name(GTK_STACK(stack));
                if (cur == "audio" || cur == "theme") {
                    ControlCenter::switch_to_view("main");
                    return TRUE;
                }
            }
            ControlCenter::hide_popup();
            return TRUE;
        }
        return FALSE;
    }), nullptr);
}

static int64_t last_cc_hide_time_ms = 0;

void ControlCenter::toggle_popup() {
    init_popup();
    if (!popup_window) return;

    int64_t now = g_get_monotonic_time() / 1000;
    if (now - last_cc_hide_time_ms < 250) {
        return;
    }

    if (gtk_widget_get_visible(popup_window)) {
        hide_popup();
    } else {
        show_popup();
        switch_to_view("main");
    }
}

void ControlCenter::toggle_audio_view() {
    init_popup();
    if (!popup_window) return;

    int64_t now = g_get_monotonic_time() / 1000;
    if (now - last_cc_hide_time_ms < 250) {
        return;
    }

    if (gtk_widget_get_visible(popup_window)) {
        if (stack && std::string(gtk_stack_get_visible_child_name(GTK_STACK(stack))) == "audio") {
            hide_popup();
            return;
        }
    }

    show_popup();
    switch_to_view("audio");
}

void ControlCenter::toggle_theme_view() {
    init_popup();
    if (!popup_window) return;

    int64_t now = g_get_monotonic_time() / 1000;
    if (now - last_cc_hide_time_ms < 250) {
        return;
    }

    if (gtk_widget_get_visible(popup_window)) {
        if (stack && std::string(gtk_stack_get_visible_child_name(GTK_STACK(stack))) == "theme") {
            hide_popup();
            return;
        }
    }

    show_popup();
    switch_to_view("theme");
}

void ControlCenter::refresh_data() {
    AudioManager::update();
    if (battery_status_lbl) {
        gtk_label_set_text(GTK_LABEL(battery_status_lbl), get_battery_info().c_str());
    }
    if (wifi_ssid_lbl) {
        gtk_label_set_text(GTK_LABEL(wifi_ssid_lbl), get_active_wifi_ssid().c_str());
    }
    if (wifi_state_pill) {
        gtk_label_set_text(GTK_LABEL(wifi_state_pill), is_wifi_enabled() ? "ON  ›" : "OFF  ›");
    }
    if (bt_status_lbl) {
        gtk_label_set_text(GTK_LABEL(bt_status_lbl), get_bluetooth_status().c_str());
    }
    if (bt_state_pill) {
        gtk_label_set_text(GTK_LABEL(bt_state_pill), is_bluetooth_enabled() ? "ON  ›" : "OFF  ›");
    }
    if (audio_sub_lbl) {
        gtk_label_set_text(GTK_LABEL(audio_sub_lbl), AudioManager::get_default_sink_name().c_str());
    }
    s_syncing_ui = true;
    int v = AudioManager::get_volume();
    if (audio_pill_lbl) {
        gtk_label_set_text(GTK_LABEL(audio_pill_lbl), (std::to_string(v) + "%  ›").c_str());
    }
    if (volume_slider) {
        gtk_range_set_value(GTK_RANGE(volume_slider), v);
        if (volume_val_lbl) {
            gtk_label_set_text(GTK_LABEL(volume_val_lbl), (std::to_string(v) + "%").c_str());
        }
    }
    if (brightness_slider) {
        int b = get_current_brightness();
        gtk_range_set_value(GTK_RANGE(brightness_slider), b);
        if (brightness_val_lbl) {
            gtk_label_set_text(GTK_LABEL(brightness_val_lbl), (std::to_string(b) + "%").c_str());
        }
    }
    s_syncing_ui = false;

    // Theme Subtitle
    if (theme_card_sub_lbl) {
        std::string cur_th = ThemeEngine::get_current_theme_name();
        if (!cur_th.empty()) cur_th[0] = std::toupper(cur_th[0]);
        gtk_label_set_text(GTK_LABEL(theme_card_sub_lbl), cur_th.c_str());
    }

    // Dark Mode Status
    if (dark_btn && dark_sub_lbl) {
        bool d = ThemeEngine::is_dark_mode();
        gtk_label_set_text(GTK_LABEL(dark_sub_lbl), d ? "ON" : "OFF");
        if (d) gtk_widget_add_css_class(dark_btn, "active");
        else gtk_widget_remove_css_class(dark_btn, "active");
    }
}

void ControlCenter::show_popup() {
    init_popup();
    if (!popup_window) return;

    refresh_data();

    gtk_widget_show_all(popup_window);
    gtk_window_present(GTK_WINDOW(popup_window));
}

void ControlCenter::hide_popup() {
    if (!popup_window) return;
    last_cc_hide_time_ms = g_get_monotonic_time() / 1000;
    gtk_widget_hide(popup_window);
}

} // namespace zenith
