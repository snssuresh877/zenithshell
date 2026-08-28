#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>

namespace zenith {

struct PowerAction {
    std::string id;
    std::string icon;
    std::string title;
    std::string subtitle;
    std::string hotkey;
    std::string css_class;
    std::string command;
};

class PowerMenu {
public:
    static void init(GtkApplication* app);
    static void toggle();
    static void show();
    static void hide();

private:
    static GtkWidget* window;
    static GtkWidget* uptime_lbl;
    static std::vector<GtkWidget*> action_buttons;
    static std::vector<PowerAction> actions;

    static void create_window(GtkApplication* app);
    static void setup_actions();
    static void execute_action(size_t index);
    static std::string get_uptime_string();
};

} // namespace zenith
