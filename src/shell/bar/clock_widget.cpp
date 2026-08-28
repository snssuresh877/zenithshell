#include "shell/bar/clock_widget.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <cairo.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace zenith {

GtkWidget* ClockWidget::date_label = nullptr;
GtkWidget* ClockWidget::time_label = nullptr;
GtkWidget* ClockWidget::cal_window = nullptr;
GtkWidget* ClockWidget::cal_header_date = nullptr;
GtkWidget* ClockWidget::cal_header_time = nullptr;
GtkWidget* ClockWidget::cal_widget = nullptr;

void ClockWidget::init(GtkApplication* app) {
    create_calendar_window(app);
}

void ClockWidget::create_calendar_window(GtkApplication* app) {
    if (cal_window) return;

    cal_window = gtk_application_window_new(app);
    gtk_widget_add_css_class(cal_window, "calendar-window");
    gtk_window_set_default_size(GTK_WINDOW(cal_window), 280, 290);

    // Enable true RGBA transparency so rounded corners don't render grey/black box artifacts
    GdkScreen* screen = gtk_widget_get_screen(cal_window);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
        gtk_widget_set_visual(cal_window, visual);
    }
    gtk_widget_set_app_paintable(cal_window, TRUE);

    g_signal_connect(cal_window, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer) -> gboolean {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_restore(cr);
        return FALSE;
    }), nullptr);

    gtk_layer_init_for_window(GTK_WINDOW(cal_window));
    gtk_layer_set_layer(GTK_WINDOW(cal_window), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(cal_window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    // Center under the TopBar clock
    gtk_layer_set_anchor(GTK_WINDOW(cal_window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(cal_window), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(cal_window), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(cal_window), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
    gtk_layer_set_margin(GTK_WINDOW(cal_window), GTK_LAYER_SHELL_EDGE_TOP, 34);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(card, "calendar-card");

    // Header with full date & time
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(header_box, "calendar-header-box");

    GtkWidget* icon_lbl = gtk_label_new("󰃰");
    gtk_widget_add_css_class(icon_lbl, "calendar-header-icon");

    GtkWidget* txt_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    cal_header_date = gtk_label_new("");
    gtk_widget_add_css_class(cal_header_date, "calendar-header-date");
    gtk_widget_set_halign(cal_header_date, GTK_ALIGN_START);

    cal_header_time = gtk_label_new("");
    gtk_widget_add_css_class(cal_header_time, "calendar-header-time");
    gtk_widget_set_halign(cal_header_time, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(txt_col), cal_header_date, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(txt_col), cal_header_time, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(header_box), icon_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), txt_col, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), header_box, FALSE, FALSE, 0);

    // Separator line
    GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(sep, "calendar-sep");
    gtk_box_pack_start(GTK_BOX(card), sep, FALSE, FALSE, 0);

    // GtkCalendar widget
    cal_widget = gtk_calendar_new();
    gtk_widget_add_css_class(cal_widget, "zenith-calendar");
    gtk_calendar_set_display_options(GTK_CALENDAR(cal_widget), (GtkCalendarDisplayOptions)(
        GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES
    ));
    gtk_box_pack_start(GTK_BOX(card), cal_widget, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(cal_window), card);

    // Dismiss on Escape
    g_signal_connect(cal_window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            ClockWidget::hide_calendar();
            return TRUE;
        }
        return FALSE;
    }), nullptr);
}

GtkWidget* ClockWidget::create(const std::string&) {
    GtkWidget* btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "pill-widget");
    gtk_widget_add_css_class(btn, "clock-pill");
    gtk_widget_set_size_request(btn, -1, 24);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    date_label = gtk_label_new("󰃰 ...");
    gtk_widget_add_css_class(date_label, "clock-date");

    time_label = gtk_label_new(" 00:00");
    gtk_widget_add_css_class(time_label, "clock-time");

    gtk_box_pack_start(GTK_BOX(box), date_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), time_label, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(btn), box);

    g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ClockWidget::toggle_calendar();
    }), nullptr);

    g_timeout_add_seconds(1, update_time, nullptr);
    update_time(nullptr);

    return btn;
}

void ClockWidget::toggle_calendar() {
    if (!cal_window) return;
    if (gtk_widget_get_visible(cal_window)) {
        hide_calendar();
    } else {
        show_calendar();
    }
}

void ClockWidget::show_calendar() {
    if (!cal_window) return;

    auto t = std::time(nullptr);
    auto* tm = std::localtime(&t);

    char full_date_buf[128];
    std::strftime(full_date_buf, sizeof(full_date_buf), "%A, %d %B %Y", tm);
    if (cal_header_date) {
        gtk_label_set_text(GTK_LABEL(cal_header_date), full_date_buf);
    }

    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%I:%M %p", tm);
    if (cal_header_time) {
        gtk_label_set_text(GTK_LABEL(cal_header_time), time_buf);
    }

    // Select today on calendar
    if (cal_widget) {
        gtk_calendar_select_month(GTK_CALENDAR(cal_widget), tm->tm_mon, tm->tm_year + 1900);
        gtk_calendar_select_day(GTK_CALENDAR(cal_widget), tm->tm_mday);
    }

    gtk_widget_show_all(cal_window);
}

void ClockWidget::hide_calendar() {
    if (!cal_window) return;
    gtk_widget_hide(cal_window);
}

gboolean ClockWidget::update_time(gpointer) {
    auto t = std::time(nullptr);
    auto* tm = std::localtime(&t);

    char date_buf[64];
    std::strftime(date_buf, sizeof(date_buf), "󰃰 %a %d %b", tm);
    if (date_label) {
        gtk_label_set_text(GTK_LABEL(date_label), date_buf);
    }

    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), " %H:%M", tm);
    if (time_label) {
        gtk_label_set_text(GTK_LABEL(time_label), time_buf);
    }

    return TRUE;
}

} // namespace zenith
