#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>

namespace zenith {

struct KeybindEntry {
    std::string keys;
    std::string description;
    std::string category;
};

class KeybindsOverlay {
public:
    static void init(GtkApplication* app);
    static void toggle();
    static void show();
    static void hide();

private:
    static GtkWidget* window;
    static GtkWidget* search_entry;
    static GtkWidget* list_box;
    static std::vector<KeybindEntry> keybinds;

    static void create_window(GtkApplication* app);
    static void load_default_keybinds();
    static void filter_keybinds(const std::string& query);
    static void build_entry(const KeybindEntry& entry);
};

} // namespace zenith
