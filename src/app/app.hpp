#pragma once

#include <gtk/gtk.h>
#include <string>
#include <memory>
#include "config/config.hpp"

namespace zenith {

class App {
public:
    App(int argc, char** argv);
    int run();

private:
    GtkApplication* gtk_app = nullptr;
    Config config;
    std::string config_path = "config.json";
    std::string style_path = "style.css";
    std::string theme_name = "zenith-dark";

    static void on_activate(GtkApplication* app, gpointer user_data);
};

} // namespace zenith
