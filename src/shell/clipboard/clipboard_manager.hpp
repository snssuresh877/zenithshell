#pragma once

#include <gtk/gtk.h>
#include <string>
#include <deque>
#include <vector>

namespace zenith {

struct ClipItem {
    std::string text;
    std::string timestamp;
};

class ClipboardManager {
public:
    static void init(GtkApplication* app);
    static void toggle();
    static void show();
    static void hide();
    static void add_item(const std::string& text);
    static void paste_item(int index);
    static void clear_history();

private:
    static GtkWidget* window;
    static GtkWidget* search_entry;
    static GtkWidget* listbox;
    static GtkWidget* badge_lbl;
    static GtkWidget* empty_box;
    static std::deque<ClipItem> history;
    static std::string last_copied;
    static std::vector<int> filtered_indices;

    static void render_list(const std::string& filter);
    static gboolean poll_clipboard(gpointer user_data);
};

} // namespace zenith
