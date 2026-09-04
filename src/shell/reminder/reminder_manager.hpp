#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <chrono>

namespace zenith {

struct ReminderItem {
    std::string title;
    std::chrono::system_clock::time_point target_time;
    bool triggered = false;
};

class ReminderManager {
public:
    static void init(GtkApplication* app);
    static void toggle_overlay();
    static void show();
    static void hide();
    static void add_reminder(const std::string& title, int minutes);
    static void render_list();
    static void save_to_disk();
    static void load_from_disk();

private:
    static GtkWidget* window;
    static GtkWidget* entry_text;
    static GtkWidget* entry_minutes;
    static GtkWidget* listbox;
    static std::vector<ReminderItem> reminders;

    static gboolean check_reminders(gpointer user_data);
};

} // namespace zenith

