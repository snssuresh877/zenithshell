#include "shell/osd/osd_window.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <cairo.h>
#include <algorithm>
#include <iostream>

namespace zenith {

GtkApplication* OSDWindow::s_app = nullptr;
GtkWidget* OSDWindow::s_window = nullptr;
GtkWidget* OSDWindow::s_card = nullptr;
GtkWidget* OSDWindow::s_icon_lbl = nullptr;
GtkWidget* OSDWindow::s_title_lbl = nullptr;
GtkWidget* OSDWindow::s_val_lbl = nullptr;
GtkWidget* OSDWindow::s_progress_bar = nullptr;
guint OSDWindow::s_hide_timer_id = 0;

void OSDWindow::init(GtkApplication* app) {
    s_app = app;
}

void OSDWindow::ensure_window() {
    if (s_window) return;
    if (!s_app) return;

    s_window = gtk_application_window_new(s_app);
    gtk_widget_add_css_class(s_window, "zenith-osd");

    gtk_layer_init_for_window(GTK_WINDOW(s_window));
    gtk_layer_set_layer(GTK_WINDOW(s_window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(s_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(s_window), GTK_LAYER_SHELL_EDGE_BOTTOM, 80);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(s_window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_layer_set_namespace(GTK_WINDOW(s_window), "zenith-osd");

    // Click-through input region (so clicks pass through to underlying windows)
    g_signal_connect(s_window, "realize", G_CALLBACK(+[](GtkWidget* w, gpointer) {
        cairo_region_t* empty = cairo_region_create();
        gtk_widget_input_shape_combine_region(w, empty);
        cairo_region_destroy(empty);
    }), nullptr);

    // Card Container
    s_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(s_card, "zenith-osd-card");
    gtk_container_add(GTK_CONTAINER(s_window), s_card);

    // Top Row: Icon + Title + Value
    GtkWidget* top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(s_card), top_row, FALSE, FALSE, 0);

    s_icon_lbl = gtk_label_new("󰕾");
    gtk_widget_add_css_class(s_icon_lbl, "zenith-osd-icon");
    gtk_box_pack_start(GTK_BOX(top_row), s_icon_lbl, FALSE, FALSE, 0);

    s_title_lbl = gtk_label_new("Volume");
    gtk_widget_add_css_class(s_title_lbl, "zenith-osd-title");
    gtk_box_pack_start(GTK_BOX(top_row), s_title_lbl, FALSE, FALSE, 0);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(top_row), spacer, TRUE, TRUE, 0);

    s_val_lbl = gtk_label_new("100%");
    gtk_widget_add_css_class(s_val_lbl, "zenith-osd-val");
    gtk_box_pack_end(GTK_BOX(top_row), s_val_lbl, FALSE, FALSE, 0);

    // Bottom Row: Rounded Level Progress Bar
    s_progress_bar = gtk_progress_bar_new();
    gtk_widget_add_css_class(s_progress_bar, "zenith-osd-progress");
    gtk_box_pack_start(GTK_BOX(s_card), s_progress_bar, FALSE, FALSE, 0);
}

void OSDWindow::reset_hide_timer() {
    if (s_hide_timer_id > 0) {
        g_source_remove(s_hide_timer_id);
        s_hide_timer_id = 0;
    }
    s_hide_timer_id = g_timeout_add(1500, G_SOURCE_FUNC(+[](gpointer) -> gboolean {
        hide();
        return G_SOURCE_REMOVE;
    }), nullptr);
}

void OSDWindow::hide() {
    if (s_window && gtk_widget_get_visible(s_window)) {
        gtk_widget_hide(s_window);
    }
    s_hide_timer_id = 0;
}

namespace {
struct OSDPayload {
    std::string icon;
    std::string title;
    int percent;
    bool muted;
};
} // namespace

void OSDWindow::show_custom(const std::string& icon, const std::string& title, int percent, bool muted) {
    auto* payload = new OSDPayload{icon, title, percent, muted};
    g_idle_add([](gpointer data) -> gboolean {
        auto* d = static_cast<OSDPayload*>(data);

        ensure_window();
        if (!s_window) {
            delete d;
            return FALSE;
        }

        gtk_label_set_text(GTK_LABEL(s_icon_lbl), d->icon.c_str());
        gtk_label_set_text(GTK_LABEL(s_title_lbl), d->title.c_str());

        if (d->muted) {
            gtk_label_set_text(GTK_LABEL(s_val_lbl), "Muted");
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s_progress_bar), 0.0);
        } else {
            gtk_label_set_text(GTK_LABEL(s_val_lbl), (std::to_string(d->percent) + "%").c_str());
            double frac = std::clamp(d->percent / 100.0, 0.0, 1.5);
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(s_progress_bar), frac);
        }

        gtk_widget_show_all(s_window);
        reset_hide_timer();

        delete d;
        return FALSE;
    }, payload);
}

void OSDWindow::show_volume(int volume_percent, bool muted) {
    std::string icon;
    if (muted) {
        icon = "󰝟";
    } else if (volume_percent > 66) {
        icon = "󰕾";
    } else if (volume_percent > 33) {
        icon = "󰖀";
    } else {
        icon = "󰕿";
    }
    show_custom(icon, "Volume", volume_percent, muted);
}

void OSDWindow::show_brightness(int brightness_percent) {
    std::string icon;
    if (brightness_percent > 66) {
        icon = "󰃠";
    } else if (brightness_percent > 33) {
        icon = "󰃟";
    } else {
        icon = "󰃞";
    }
    show_custom(icon, "Brightness", brightness_percent, false);
}

void OSDWindow::show_mic(int volume_percent, bool muted) {
    std::string icon = muted ? "󰍭" : "󰍬";
    show_custom(icon, "Microphone", volume_percent, muted);
}

} // namespace zenith
