#include "app/app.hpp"
#include "cli/zenithctl.hpp"
#include "shell/files/file_manager_window.hpp"
#include "theme/theme_engine.hpp"
#include "theme/css_manager.hpp"
#include <gtk/gtk.h>
#include <string>

int main(int argc, char** argv) {
    std::string prog = (argc > 0 && argv[0]) ? argv[0] : "";
    bool is_files_cmd = false;
    std::string target_path = "";

    if (prog.find("zenith-files") != std::string::npos) {
        is_files_cmd = true;
        if (argc > 1) target_path = argv[1];
    } else if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--files" || arg1 == "files" || arg1 == "fm") {
            is_files_cmd = true;
            if (argc > 2) target_path = argv[2];
        }
    }

    if (is_files_cmd) {
        gtk_init(&argc, &argv);

        GtkSettings* settings = gtk_settings_get_default();
        if (settings) {
            g_object_set(settings,
                "gtk-application-prefer-dark-theme", TRUE,
                "gtk-theme-name", "Orchis-Dark",
                NULL
            );
        }

        zenith::CssManager::init("");
        zenith::ThemeEngine::init();

        GtkWidget* win = zenith::FileManagerWindow::create(target_path);
        g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
        gtk_main();
        return 0;
    }

    if (zenith::ZenithCtl::should_handle(argc, argv)) {
        return zenith::ZenithCtl::run(argc, argv);
    }

    zenith::App app(argc, argv);
    return app.run();
}

