#include "shell/reminder/reminder_manager.hpp"
#include "dbus/notification_manager.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <iostream>
#include <ctime>
#include <algorithm>

namespace zenith {

GtkWidget* ReminderManager::window = nullptr;
GtkWidget* ReminderManager::entry_text = nullptr;
GtkWidget* ReminderManager::entry_minutes = nullptr;
GtkWidget* ReminderManager::listbox = nullptr;
std::vector<ReminderItem> ReminderManager::reminders;

void ReminderManager::init(GtkApplication* app) {
    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "reminder-window");

    // Enable true RGBA transparency so rounded corners don't render grey/black box artifacts
    GdkScreen* screen = gtk_widget_get_screen(window);
    GdkVisual* visual = gdk_screen_get_rgba_visual(screen);
    if (visual) {
        gtk_widget_set_visual(window, visual);
    }
    gtk_widget_set_app_paintable(window, TRUE);

    g_signal_connect(window, "draw", G_CALLBACK(+[](GtkWidget*, cairo_t* cr, gpointer) -> gboolean {
        cairo_save(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_restore(cr);
        return FALSE;
    }), nullptr);

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

    // Centered overlay card
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 100);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "reminder-card");
    gtk_widget_set_size_request(card, 450, 400);

    GtkWidget* header = gtk_label_new("󰔟 Timers & Reminders");
    gtk_widget_add_css_class(header, "reminder-title");
    gtk_box_pack_start(GTK_BOX(card), header, FALSE, FALSE, 0);

    // Input row: Text + Minutes + Add Button
    GtkWidget* input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    entry_text = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_text), "Reminder title...");
    gtk_widget_set_hexpand(entry_text, TRUE);

    entry_minutes = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_minutes), "Mins");
    gtk_widget_set_size_request(entry_minutes, 60, -1);

    GtkWidget* add_btn = gtk_button_new_with_label("󰐕 Add");
    gtk_widget_add_css_class(add_btn, "btn-accent");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        const char* txt = gtk_entry_get_text(GTK_ENTRY(entry_text));
        const char* min_str = gtk_entry_get_text(GTK_ENTRY(entry_minutes));
        int mins = (min_str && *min_str) ? std::atoi(min_str) : 10;
        if (txt && *txt) {
            ReminderManager::add_reminder(txt, mins);
            gtk_entry_set_text(GTK_ENTRY(entry_text), "");
            gtk_entry_set_text(GTK_ENTRY(entry_minutes), "");
        }
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(input_row), entry_text, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_row), entry_minutes, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(input_row), add_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), input_row, FALSE, FALSE, 0);

    // List of active reminders
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_vexpand(scroll, TRUE);
    listbox = gtk_list_box_new();
    gtk_widget_add_css_class(listbox, "reminder-list");
    gtk_container_add(GTK_CONTAINER(scroll), listbox);
    gtk_box_pack_start(GTK_BOX(card), scroll, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(window), card);

    // Escape to hide
    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            ReminderManager::hide();
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    // 1-second check loop
    g_timeout_add_seconds(1, check_reminders, nullptr);
}

void ReminderManager::toggle_overlay() {
    if (!window) return;
    if (gtk_widget_get_visible(window)) hide();
    else show();
}

void ReminderManager::show() {
    if (!window) return;
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
    render_list();
    gtk_widget_show_all(window);
    gtk_widget_grab_focus(entry_text);
}

void ReminderManager::hide() {
    if (!window) return;
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    gtk_widget_hide(window);
}

void ReminderManager::add_reminder(const std::string& title, int minutes) {
    auto now = std::chrono::system_clock::now();
    auto target = now + std::chrono::minutes(minutes);

    reminders.push_back({title, target, false});
    render_list();
}

void ReminderManager::render_list() {
    if (!listbox) return;

    GList* children = gtk_container_get_children(GTK_CONTAINER(listbox));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    auto now = std::chrono::system_clock::now();

    for (size_t i = 0; i < reminders.size(); ++i) {
        const auto& r = reminders[i];
        if (r.triggered) continue;

        auto rem_secs = std::chrono::duration_cast<std::chrono::seconds>(r.target_time - now).count();
        int rem_mins = static_cast<int>(std::max(0L, static_cast<long>(rem_secs / 60)));

        GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_add_css_class(row_box, "reminder-item");

        GtkWidget* icon = gtk_label_new("󰔟");
        gtk_widget_add_css_class(icon, "rem-icon");

        GtkWidget* text_lbl = gtk_label_new(r.title.c_str());
        gtk_widget_add_css_class(text_lbl, "rem-text");
        gtk_widget_set_halign(text_lbl, GTK_ALIGN_START);

        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "in %d min", rem_mins);
        GtkWidget* time_lbl = gtk_label_new(tbuf);
        gtk_widget_add_css_class(time_lbl, "rem-time");

        GtkWidget* del_btn = gtk_button_new_with_label("");
        gtk_widget_add_css_class(del_btn, "btn-del");
        g_signal_connect(del_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer idx_ptr) {
            size_t idx = reinterpret_cast<size_t>(idx_ptr);
            if (idx < reminders.size()) {
                reminders.erase(reminders.begin() + idx);
                ReminderManager::render_list();
            }
        }), reinterpret_cast<gpointer>(i));

        gtk_box_pack_start(GTK_BOX(row_box), icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), text_lbl, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row_box), time_lbl, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(row_box), del_btn, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(listbox), row_box);
    }

    gtk_widget_show_all(listbox);
}

gboolean ReminderManager::check_reminders(gpointer) {
    auto now = std::chrono::system_clock::now();
    bool updated = false;

    for (auto& r : reminders) {
        if (!r.triggered && now >= r.target_time) {
            r.triggered = true;
            updated = true;

            // Trigger notification
            std::string body = "Time's up for: " + r.title;
            system(("notify-send '󰔟 Reminder Alert' '" + r.title + "' 2>/dev/null &").c_str());
        }
    }

    if (updated && window && gtk_widget_get_visible(window)) {
        render_list();
    }

    return TRUE;
}

} // namespace zenith
