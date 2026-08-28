#pragma once

#include <gtk/gtk.h>
#include <string>

namespace zenith {

class ControlCenter {
public:
    static GtkWidget* create_button();
    static void toggle_popup();
    static void toggle_audio_view();
    static void toggle_theme_view();
    static void show_popup();
    static void hide_popup();
    static void refresh_data();
    static void switch_to_view(const char* view_name);

private:
    static GtkWidget* popup_window;
    static GtkWidget* btn_widget;
    static GtkWidget* battery_status_lbl;
    static GtkWidget* stack;

    // 2x2 Hero Cards
    static GtkWidget* wifi_ssid_lbl;
    static GtkWidget* wifi_state_pill;
    static GtkWidget* bt_status_lbl;
    static GtkWidget* bt_state_pill;
    static GtkWidget* audio_sub_lbl;
    static GtkWidget* audio_pill_lbl;
    static GtkWidget* display_sub_lbl;

    // 3 Quick Modes
    static GtkWidget* night_btn;
    static GtkWidget* night_sub_lbl;
    static GtkWidget* focus_btn;
    static GtkWidget* focus_sub_lbl;
    static GtkWidget* dark_btn;
    static GtkWidget* dark_sub_lbl;

    // Theme Card on Main Overview
    static GtkWidget* theme_card_btn;
    static GtkWidget* theme_card_sub_lbl;

    // Main Overview Sliders
    static GtkWidget* brightness_slider;
    static GtkWidget* brightness_val_lbl;
    static GtkWidget* volume_slider;
    static GtkWidget* volume_val_lbl;

    // Audio Detail Drill-Down Widgets
    static GtkWidget* sinks_container;
    static GtkWidget* audio_detail_vol_slider;
    static GtkWidget* audio_detail_vol_lbl;
    static GtkWidget* mic_btn;
    static GtkWidget* mic_state_lbl;
    static GtkWidget* mic_gain_slider;
    static GtkWidget* mic_gain_lbl;
    static GtkWidget* nc_btn;
    static GtkWidget* nc_state_lbl;

    // Theme Detail Drill-Down Widgets
    static GtkWidget* theme_grid_container;

    static void init_popup();
    static GtkWidget* create_main_page();
    static GtkWidget* create_audio_page();
    static GtkWidget* create_theme_page();
    static void refresh_audio_page();
    static void refresh_theme_page();

    static std::string get_battery_info();
    static std::string get_active_wifi_ssid();
    static bool is_wifi_enabled();
    static std::string get_bluetooth_status();
    static bool is_bluetooth_enabled();
    static int get_current_brightness();
    static std::string get_display_info();
    static bool is_night_light_active();
    static bool is_focus_dnd_active();
};

} // namespace zenith
