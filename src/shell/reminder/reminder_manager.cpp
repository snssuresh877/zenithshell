#include "shell/reminder/reminder_manager.hpp"
#include "dbus/notification_manager.hpp"
#include "gtk3_compat.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ctime>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace zenith {

GtkWidget* ReminderManager::window = nullptr;
GtkWidget* ReminderManager::entry_text = nullptr;
GtkWidget* ReminderManager::entry_minutes = nullptr;
GtkWidget* ReminderManager::listbox = nullptr;
std::vector<ReminderItem> ReminderManager::reminders;

static std::string get_reminders_storage_path() {
    std::string dir = std::string(g_get_user_config_dir()) + "/zenithshell";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir + "/reminders.json";
}

void ReminderManager::save_to_disk() {
    std::string path = get_reminders_storage_path();
    json j = json::array();

    auto now = std::chrono::system_clock::now();
    for (const auto& r : reminders) {
        if (!r.triggered && r.target_time > now) {
            auto epoch_secs = std::chrono::duration_cast<std::chrono::seconds>(r.target_time.time_since_epoch()).count();
            j.push_back({
                {"title", r.title},
                {"target_time", epoch_secs}
            });
        }
    }

    std::ofstream file(path, std::ios::trunc);
    if (file.is_open()) {
        file << j.dump(2);
        file.close();
    }
}

void ReminderManager::load_from_disk() {
    std::string path = get_reminders_storage_path();
    if (!fs::exists(path)) return;

    std::ifstream file(path);
    if (!file.is_open()) return;

    try {
        json j;
        file >> j;
        if (j.is_array()) {
            reminders.clear();
            auto now = std::chrono::system_clock::now();
            for (const auto& elem : j) {
                if (elem.contains("title") && elem.contains("target_time")) {
                    std::string t = elem["title"];
                    int64_t epoch_secs = elem["target_time"];
                    auto target = std::chrono::system_clock::time_point(std::chrono::seconds(epoch_secs));
                    if (target > now) {
                        reminders.push_back({t, target, false});
                    }
                }
            }
        }
    } catch (...) {}
}

void ReminderManager::init(GtkApplication* app) {
    load_from_disk();

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

    // Fullscreen anchor with clickable backdrop to dismiss
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    GtkWidget* backdrop = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(backdrop), FALSE);
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton*, gpointer) -> gboolean {
        ReminderManager::hide();
        return TRUE;
    }), nullptr);

    // Centered outer container
    GtkWidget* outer_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(outer_vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(outer_vbox, GTK_ALIGN_START);
    gtk_widget_set_margin_top(outer_vbox, 90);

    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "reminder-card");
    gtk_widget_set_size_request(card, 480, 420);

    // Prevent clicks inside card from closing backdrop
    g_signal_connect(card, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventButton*, gpointer) -> gboolean {
        return TRUE;
    }), nullptr);

    // Header with title and close button
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* header = gtk_label_new("󰔟 Timers & Reminders");
    gtk_widget_add_css_class(header, "reminder-title");
    gtk_widget_set_halign(header, GTK_ALIGN_START);

    GtkWidget* close_btn = gtk_button_new_with_label("✕");
    gtk_widget_add_css_class(close_btn, "btn-close-sm");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ReminderManager::hide();
    }), nullptr);

    gtk_box_pack_start(GTK_BOX(header_box), header, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header_box), close_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), header_box, FALSE, FALSE, 0);

    // Input row: Text + Minutes + Add Button
    GtkWidget* input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    entry_text = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_text), "Reminder / Task name...");
    gtk_widget_set_hexpand(entry_text, TRUE);

    entry_minutes = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_minutes), "Mins");
    gtk_widget_set_size_request(entry_minutes, 65, -1);

    g_signal_connect(entry_text, "activate", G_CALLBACK(+[](GtkEntry*, gpointer) {
        const char* txt = gtk_entry_get_text(GTK_ENTRY(entry_text));
        const char* min_str = gtk_entry_get_text(GTK_ENTRY(entry_minutes));
        int mins = (min_str && *min_str) ? std::atoi(min_str) : 10;
        if (mins <= 0) mins = 10;
        if (txt && *txt) {
            ReminderManager::add_reminder(txt, mins);
            gtk_entry_set_text(GTK_ENTRY(entry_text), "");
            gtk_entry_set_text(GTK_ENTRY(entry_minutes), "");
        }
    }), nullptr);

    g_signal_connect(entry_minutes, "activate", G_CALLBACK(+[](GtkEntry*, gpointer) {
        const char* txt = gtk_entry_get_text(GTK_ENTRY(entry_text));
        const char* min_str = gtk_entry_get_text(GTK_ENTRY(entry_minutes));
        int mins = (min_str && *min_str) ? std::atoi(min_str) : 10;
        if (mins <= 0) mins = 10;
        if (txt && *txt) {
            ReminderManager::add_reminder(txt, mins);
            gtk_entry_set_text(GTK_ENTRY(entry_text), "");
            gtk_entry_set_text(GTK_ENTRY(entry_minutes), "");
        }
    }), nullptr);

    GtkWidget* add_btn = gtk_button_new_with_label("󰐕 Add");
    gtk_widget_add_css_class(add_btn, "btn-accent");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        const char* txt = gtk_entry_get_text(GTK_ENTRY(entry_text));
        const char* min_str = gtk_entry_get_text(GTK_ENTRY(entry_minutes));
        int mins = (min_str && *min_str) ? std::atoi(min_str) : 10;
        if (mins <= 0) mins = 10;
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

    // Quick Preset Chips (+5m, +15m, +25m Pomodoro, +30m, +1h)
    GtkWidget* presets_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    struct Preset {
        const char* label;
        int mins;
    };
    const std::vector<Preset> presets = {
        {"+5m", 5},
        {"+15m", 15},
        {"🍅 25m Pomodoro", 25},
        {"+30m", 30},
        {"+1h", 60}
    };

    for (const auto& p : presets) {
        GtkWidget* p_btn = gtk_button_new_with_label(p.label);
        gtk_widget_add_css_class(p_btn, "btn-preset-chip");
        int m = p.mins;
        g_signal_connect(p_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            int mins = GPOINTER_TO_INT(data);
            const char* txt = gtk_entry_get_text(GTK_ENTRY(entry_text));
            if (txt && *txt) {
                ReminderManager::add_reminder(txt, mins);
                gtk_entry_set_text(GTK_ENTRY(entry_text), "");
                gtk_entry_set_text(GTK_ENTRY(entry_minutes), "");
            } else {
                gtk_entry_set_text(GTK_ENTRY(entry_minutes), std::to_string(mins).c_str());
                gtk_widget_grab_focus(entry_text);
            }
        }), GINT_TO_POINTER(m));
        gtk_box_pack_start(GTK_BOX(presets_box), p_btn, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(card), presets_box, FALSE, FALSE, 0);

    // List of active reminders
    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_vexpand(scroll, TRUE);
    listbox = gtk_list_box_new();
    gtk_widget_add_css_class(listbox, "reminder-list");
    gtk_container_add(GTK_CONTAINER(scroll), listbox);
    gtk_box_pack_start(GTK_BOX(card), scroll, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(outer_vbox), card, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(backdrop), outer_vbox);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    // Escape key to hide
    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            ReminderManager::hide();
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    // 1-second check timer loop
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
    save_to_disk();
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

    if (reminders.empty()) {
        GtkWidget* empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_top(empty_box, 30);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);

        GtkWidget* empty_ic = gtk_label_new("󰔟");
        gtk_widget_add_css_class(empty_ic, "rem-empty-icon");
        GtkWidget* empty_lbl = gtk_label_new("No active reminders or timers");
        gtk_widget_add_css_class(empty_lbl, "rem-empty-label");

        gtk_box_pack_start(GTK_BOX(empty_box), empty_ic, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(empty_box), empty_lbl, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(listbox), empty_box);
    } else {
        for (size_t i = 0; i < reminders.size(); ++i) {
            const auto& r = reminders[i];
            if (r.triggered) continue;

            auto rem_secs = std::chrono::duration_cast<std::chrono::seconds>(r.target_time - now).count();
            int total_secs = static_cast<int>(std::max(0L, static_cast<long>(rem_secs)));
            int rem_hours = total_secs / 3600;
            int rem_mins = (total_secs % 3600) / 60;
            int rem_s = total_secs % 60;

            GtkWidget* row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_add_css_class(row_box, "reminder-item");

            GtkWidget* icon = gtk_label_new("󰔟");
            gtk_widget_add_css_class(icon, "rem-icon");

            GtkWidget* text_lbl = gtk_label_new(r.title.c_str());
            gtk_widget_add_css_class(text_lbl, "rem-text");
            gtk_widget_set_halign(text_lbl, GTK_ALIGN_START);

            char tbuf[48];
            if (rem_hours > 0) {
                snprintf(tbuf, sizeof(tbuf), "%dh %dm left", rem_hours, rem_mins);
            } else if (rem_mins > 0) {
                snprintf(tbuf, sizeof(tbuf), "%dm %ds left", rem_mins, rem_s);
            } else {
                snprintf(tbuf, sizeof(tbuf), "%ds left", rem_s);
            }

            GtkWidget* time_lbl = gtk_label_new(tbuf);
            gtk_widget_add_css_class(time_lbl, "rem-time");

            GtkWidget* del_btn = gtk_button_new_with_label("");
            gtk_widget_add_css_class(del_btn, "btn-del");
            g_signal_connect(del_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer idx_ptr) {
                size_t idx = reinterpret_cast<size_t>(idx_ptr);
                if (idx < reminders.size()) {
                    reminders.erase(reminders.begin() + idx);
                    ReminderManager::save_to_disk();
                    ReminderManager::render_list();
                }
            }), reinterpret_cast<gpointer>(i));

            gtk_box_pack_start(GTK_BOX(row_box), icon, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(row_box), text_lbl, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(row_box), time_lbl, FALSE, FALSE, 0);
            gtk_box_pack_end(GTK_BOX(row_box), del_btn, FALSE, FALSE, 0);

            gtk_container_add(GTK_CONTAINER(listbox), row_box);
        }
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

            // Trigger visual desktop notification
            system(("notify-send -u critical -i appointment-soon '󰔟 Reminder Alert' '" + r.title + "' 2>/dev/null &").c_str());

            // Play notification sound chime
            system("canberra-gtk-play -i complete 2>/dev/null || paplay /usr/share/sounds/freedesktop/stereo/complete.oga 2>/dev/null || paplay /usr/share/sounds/freedesktop/stereo/alarm-clock-elapsed.oga 2>/dev/null &");
        }
    }

    if (updated) {
        save_to_disk();
    }

    if (window && gtk_widget_get_visible(window)) {
        render_list();
    }

    return TRUE;
}

} // namespace zenith

