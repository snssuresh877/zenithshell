#include "cli/zenithctl.hpp"
#include <gio/gio.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace zenith {

namespace {

const char* C_RESET  = "\033[0m";
const char* C_BOLD   = "\033[1m";
const char* C_DIM    = "\033[2m";
const char* C_RED    = "\033[1;31m";
const char* C_GREEN  = "\033[1;32m";
const char* C_YELLOW = "\033[1;33m";
const char* C_CYAN   = "\033[1;36m";

void print_help() {
    std::cout << C_BOLD << C_CYAN << "ZenithShell Universal Controller (zenithctl)" << C_RESET << "\n";
    std::cout << C_DIM << "High-performance native command-line control for ZenithShell\n\n" << C_RESET;

    std::cout << C_BOLD << "USAGE:\n" << C_RESET;
    std::cout << "  zenithctl <command> [subcommand] [arguments...]\n";
    std::cout << "  zenithshell <command> [subcommand] [arguments...]\n\n";

    std::cout << C_BOLD << "WALLPAPER COMMANDS:\n" << C_RESET;
    std::cout << "  " << C_GREEN << "wallpaper cycle" << C_RESET << "          Cycle to next wallpaper (smooth cross-fade & theme extraction)\n";
    std::cout << "  " << C_GREEN << "wallpaper set <path>" << C_RESET << "    Set active desktop wallpaper\n";
    std::cout << "  " << C_GREEN << "wallpaper current" << C_RESET << "       Print path of current wallpaper\n";
    std::cout << "  " << C_GREEN << "wallpaper dir <path>" << C_RESET << "    Set custom directory for wallpaper cycling\n\n";

    std::cout << C_BOLD << "THEME COMMANDS:\n" << C_RESET;
    std::cout << "  " << C_GREEN << "theme set <name>" << C_RESET << "        Apply a theme (e.g. dynamic, kanagawa, zenith-dark)\n";
    std::cout << "  " << C_GREEN << "theme next" << C_RESET << "              Cycle to next available theme\n";
    std::cout << "  " << C_GREEN << "theme current" << C_RESET << "           Display current active theme\n";
    std::cout << "  " << C_GREEN << "theme list" << C_RESET << "              List all installed and built-in themes\n\n";

    std::cout << C_BOLD << "CLIPBOARD COMMANDS:\n" << C_RESET;
    std::cout << "  " << C_GREEN << "clipboard" << C_RESET << "               Toggle clipboard history overlay (SUPER + V)\n";
    std::cout << "  " << C_GREEN << "clipboard clear" << C_RESET << "         Clear all persistent clipboard history\n";
    std::cout << "  " << C_GREEN << "clipboard store" << C_RESET << "         Read text from stdin and save to clipboard history\n\n";

    std::cout << C_BOLD << "SHELL & OVERLAY TOGGLES:\n" << C_RESET;
    std::cout << "  " << C_GREEN << "bar" << C_RESET << "                     Toggle Top Bar visibility\n";
    std::cout << "  " << C_GREEN << "launcher" << C_RESET << " | " << C_GREEN << "spotlight" << C_RESET << "      Toggle Spotlight Search & Inline Calculator\n";
    std::cout << "  " << C_GREEN << "control-center" << C_RESET << " | " << C_GREEN << "cc" << C_RESET << "       Toggle Quick Settings / Control Center\n";
    std::cout << "  " << C_GREEN << "notifications" << C_RESET << " | " << C_GREEN << "nc" << C_RESET << "        Toggle Notification History Center\n";
    std::cout << "  " << C_GREEN << "reminders" << C_RESET << "               Toggle Reminders Manager\n";
    std::cout << "  " << C_GREEN << "active-apps" << C_RESET << "             Toggle Active Applications Drawer\n";
    std::cout << "  " << C_GREEN << "keybinds" << C_RESET << "                Toggle Hyprland Cheatsheet Overlay\n";
    std::cout << "  " << C_GREEN << "network" << C_RESET << "                 Open Network / Wi-Fi modal\n";
    std::cout << "  " << C_GREEN << "audio" << C_RESET << "                   Open Audio control modal\n";
    std::cout << "  " << C_GREEN << "power" << C_RESET << "                   Open Power & Session menu\n\n";

    std::cout << C_BOLD << "SYSTEM & DAEMON:\n" << C_RESET;
    std::cout << "  " << C_GREEN << "stats" << C_RESET << "                   Print live CPU, RAM, Battery, and Network metrics\n";
    std::cout << "  " << C_GREEN << "stats --json" << C_RESET << "            Output live metrics in raw JSON\n";
    std::cout << "  " << C_GREEN << "reload" << C_RESET << "                  Reload stylesheet and configuration\n";
    std::cout << "  " << C_GREEN << "help" << C_RESET << "                    Display this help manual\n\n";
}

GDBusConnection* get_dbus_connection() {
    GError* error = nullptr;
    GDBusConnection* conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!conn) {
        std::cerr << C_RED << "[zenithctl] Error connecting to session D-Bus: " << C_RESET
                  << (error ? error->message : "unknown") << std::endl;
        if (error) g_error_free(error);
        return nullptr;
    }
    return conn;
}

GVariant* call_method(const char* method_name, GVariant* parameters = nullptr) {
    GDBusConnection* conn = get_dbus_connection();
    if (!conn) return nullptr;

    GError* error = nullptr;
    GVariant* res = g_dbus_connection_call_sync(
        conn,
        "dev.zenith.Shell",
        "/dev/zenith/Shell",
        "dev.zenith.Shell",
        method_name,
        parameters,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        3000,
        nullptr,
        &error
    );

    if (error) {
        std::cerr << C_RED << "[zenithctl] Error: " << C_RESET << error->message << "\n";
        std::cerr << C_YELLOW << "Tip: Ensure ZenithShell daemon is running ('zenithshell &')\n" << C_RESET;
        g_error_free(error);
    }

    g_object_unref(conn);
    return res;
}

} // namespace

bool ZenithCtl::should_handle(int argc, char** argv) {
    if (argc < 1) return false;

    std::string prog = argv[0];
    if (prog.rfind("zenithctl") != std::string::npos) {
        return true;
    }

    if (argc > 1) {
        std::string cmd = argv[1];
        if (cmd == "--clip-store") return true;
        if (cmd == "-h" || cmd == "--help" || cmd == "help") return true;
        if (cmd == "-v" || cmd == "--version" || cmd == "version") return true;
        if (cmd == "wallpaper" || cmd == "wp") return true;
        if (cmd == "theme") return true;
        if (cmd == "clipboard" || cmd == "clip") return true;
        if (cmd == "toggle") return true;
        if (cmd == "bar" || cmd == "topbar") return true;
        if (cmd == "launcher" || cmd == "spotlight") return true;
        if (cmd == "control-center" || cmd == "cc") return true;
        if (cmd == "notifications" || cmd == "nc") return true;
        if (cmd == "reminders") return true;
        if (cmd == "active-apps") return true;
        if (cmd == "keybinds") return true;
        if (cmd == "network") return true;
        if (cmd == "audio") return true;
        if (cmd == "power") return true;
        if (cmd == "stats") return true;
        if (cmd == "reload") return true;
    }

    return false;
}

int ZenithCtl::run(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.empty() || args[0] == "-h" || args[0] == "--help" || args[0] == "help") {
        print_help();
        return 0;
    }

    if (args[0] == "-v" || args[0] == "--version" || args[0] == "version") {
        std::cout << "ZenithShell v1.0.0 (Native C++ Desktop Suite)\n";
        return 0;
    }

    // --- Clip Store Streamer (stdin -> DBus) ---
    if (args[0] == "--clip-store" || (args[0] == "clipboard" && args.size() > 1 && args[1] == "store")) {
        std::string input((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
        if (input.empty()) return 0;

        GVariant* res = call_method("StoreClipboard", g_variant_new("(s)", input.c_str()));
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }

    // --- Wallpaper Commands ---
    if (args[0] == "wallpaper" || args[0] == "wp") {
        std::string sub = (args.size() > 1) ? args[1] : "cycle";
        if (sub == "cycle" || sub == "next") {
            GVariant* res = call_method("CycleWallpaper");
            if (!res) return 1;
            g_variant_unref(res);
            std::cout << C_GREEN << "✔ " << C_RESET << "Wallpaper cycled successfully\n";
            return 0;
        } else if (sub == "set") {
            if (args.size() < 3) {
                std::cerr << C_RED << "Error: " << C_RESET << "Missing wallpaper path\n";
                std::cerr << "Usage: zenithctl wallpaper set <path>\n";
                return 1;
            }
            std::string path = fs::absolute(args[2]).string();
            if (!fs::exists(path)) {
                std::cerr << C_RED << "Error: " << C_RESET << "File not found: " << path << "\n";
                return 1;
            }
            GVariant* res = call_method("SetWallpaper", g_variant_new("(s)", path.c_str()));
            if (!res) return 1;
            g_variant_unref(res);
            std::cout << C_GREEN << "✔ " << C_RESET << "Wallpaper set: " << C_BOLD << path << C_RESET << "\n";
            return 0;
        } else if (fs::exists(sub)) {
            std::string path = fs::absolute(sub).string();
            GVariant* res = call_method("SetWallpaper", g_variant_new("(s)", path.c_str()));
            if (!res) return 1;
            g_variant_unref(res);
            std::cout << C_GREEN << "✔ " << C_RESET << "Wallpaper set: " << C_BOLD << path << C_RESET << "\n";
            return 0;
        } else if (sub == "current" || sub == "get") {
            GVariant* res = call_method("GetWallpaper");
            if (!res) return 1;
            const char* wp = nullptr;
            g_variant_get(res, "(&s)", &wp);
            std::cout << (wp ? wp : "") << "\n";
            g_variant_unref(res);
            return 0;
        } else if (sub == "dir") {
            if (args.size() < 3) {
                std::cerr << "Usage: zenithctl wallpaper dir <path>\n";
                return 1;
            }
            std::string dir = fs::absolute(args[2]).string();
            GVariant* res = call_method("SetWallpaperDir", g_variant_new("(s)", dir.c_str()));
            if (!res) return 1;
            g_variant_unref(res);
            std::cout << C_GREEN << "✔ " << C_RESET << "Wallpaper directory set: " << dir << "\n";
            return 0;
        } else {
            std::cerr << C_RED << "Unknown wallpaper subcommand: " << C_RESET << sub << "\n";
            return 1;
        }
    }

    // --- Theme Commands ---
    if (args[0] == "theme") {
        if (args.size() == 1) {
            // Default: print current
            GVariant* res = call_method("GetTheme");
            if (!res) return 1;
            const char* t = nullptr;
            g_variant_get(res, "(&s)", &t);
            std::cout << C_BOLD << "Current Theme: " << C_CYAN << (t ? t : "") << C_RESET << "\n";
            g_variant_unref(res);
            return 0;
        }

        std::string sub = args[1];
        if (sub == "next" || sub == "cycle") {
            GVariant* res = call_method("NextTheme");
            if (!res) return 1;
            g_variant_unref(res);
            std::cout << C_GREEN << "✔ " << C_RESET << "Switched to next theme\n";
            return 0;
        } else if (sub == "current" || sub == "get") {
            GVariant* res = call_method("GetTheme");
            if (!res) return 1;
            const char* t = nullptr;
            g_variant_get(res, "(&s)", &t);
            std::cout << (t ? t : "") << "\n";
            g_variant_unref(res);
            return 0;
        } else if (sub == "list") {
            GVariant* cur_res = call_method("GetTheme");
            std::string cur_theme;
            if (cur_res) {
                const char* ct = nullptr;
                g_variant_get(cur_res, "(&s)", &ct);
                if (ct) cur_theme = ct;
                g_variant_unref(cur_res);
            }

            GVariant* res = call_method("ListThemes");
            if (!res) return 1;
            GVariantIter* iter = nullptr;
            g_variant_get(res, "(as)", &iter);
            const char* theme_name = nullptr;
            std::cout << C_BOLD << "Available Themes:\n" << C_RESET;
            while (g_variant_iter_next(iter, "&s", &theme_name)) {
                if (cur_theme == theme_name) {
                    std::cout << C_GREEN << "  * " << C_BOLD << theme_name << C_CYAN << " (active)" << C_RESET << "\n";
                } else {
                    std::cout << "    " << theme_name << "\n";
                }
            }
            g_variant_iter_free(iter);
            g_variant_unref(res);
            return 0;
        } else {
            // Treat as theme name directly (e.g. 'zenithctl theme set kanagawa' or 'zenithctl theme kanagawa')
            std::string tname = (sub == "set" && args.size() > 2) ? args[2] : sub;
            GVariant* res = call_method("SetTheme", g_variant_new("(s)", tname.c_str()));
            if (!res) return 1;
            g_variant_unref(res);
            std::cout << C_GREEN << "✔ " << C_RESET << "Switched theme to: " << C_BOLD << tname << C_RESET << "\n";
            return 0;
        }
    }

    // --- Clipboard Commands ---
    if (args[0] == "clipboard" || args[0] == "clip") {
        if (args.size() > 1 && (args[1] == "clear" || args[1] == "wipe")) {
            GVariant* res = call_method("ClearClipboard");
            if (!res) return 1;
            g_variant_unref(res);
            std::cout << C_GREEN << "✔ " << C_RESET << "Clipboard history cleared\n";
            return 0;
        }
        GVariant* res = call_method("ToggleClipboard");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }

    // --- Module Toggles ---
    std::string target = args[0];
    if (target == "toggle" && args.size() > 1) {
        target = args[1];
    }

    if (target == "bar" || target == "topbar") {
        GVariant* res = call_method("ToggleBar");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "launcher" || target == "spotlight") {
        GVariant* res = call_method("ToggleSpotlight");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "control-center" || target == "cc") {
        GVariant* res = call_method("ToggleControlCenter");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "notifications" || target == "nc") {
        GVariant* res = call_method("ToggleNotifications");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "reminders") {
        GVariant* res = call_method("ToggleReminders");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "active-apps") {
        GVariant* res = call_method("ToggleActiveApps");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "keybinds") {
        GVariant* res = call_method("ToggleKeybinds");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "network") {
        GVariant* res = call_method("ToggleNetwork");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "audio") {
        GVariant* res = call_method("ToggleAudio");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }
    if (target == "power") {
        GVariant* res = call_method("TogglePowerMenu");
        if (!res) return 1;
        g_variant_unref(res);
        return 0;
    }

    // --- System Stats ---
    if (target == "stats") {
        GVariant* res = call_method("GetStats");
        if (!res) return 1;
        const char* json_str = nullptr;
        g_variant_get(res, "(&s)", &json_str);
        if (json_str) {
            if (args.size() > 1 && args[1] == "--json") {
                std::cout << json_str << "\n";
            } else {
                try {
                    auto j = json::parse(json_str);
                    std::cout << C_BOLD << C_CYAN << "ZenithShell Live System Metrics:" << C_RESET << "\n";
                    std::cout << "  " << C_BOLD << "CPU Load:" << C_RESET << "        " << std::fixed << std::setprecision(1) << j.value("cpu_usage", 0.0) << "%\n";
                    std::cout << "  " << C_BOLD << "RAM Usage:" << C_RESET << "       " << std::fixed << std::setprecision(1) << j.value("ram_usage", 0.0) << "%\n";
                    std::cout << "  " << C_BOLD << "Network Speed:" << C_RESET << "   " << j.value("net_speed", "N/A") << "\n";
                    std::cout << "  " << C_BOLD << "Battery Level:" << C_RESET << "   " << j.value("battery_percent", 0) << "%\n";
                    std::cout << "  " << C_BOLD << "Volume:" << C_RESET << "          " << j.value("volume", 0) << "%\n";
                    std::cout << "  " << C_BOLD << "Active Theme:" << C_RESET << "    " << C_GREEN << j.value("theme", "N/A") << C_RESET << "\n";
                } catch (...) {
                    std::cout << json_str << "\n";
                }
            }
        }
        g_variant_unref(res);
        return 0;
    }

    // --- Reload Configuration ---
    if (target == "reload") {
        GVariant* res = call_method("ReloadConfig");
        if (!res) return 1;
        g_variant_unref(res);
        std::cout << C_GREEN << "✔ " << C_RESET << "ZenithShell configuration and styles reloaded\n";
        return 0;
    }

    std::cerr << C_RED << "Unknown command: " << C_RESET << args[0] << "\n";
    std::cerr << "Run 'zenithctl --help' for available commands.\n";
    return 1;
}

} // namespace zenith
