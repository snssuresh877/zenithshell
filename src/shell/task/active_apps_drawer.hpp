#pragma once

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "shell/tray/system_tray_manager.hpp"

namespace zenith {

struct DesktopAppMeta {
    std::string name;
    std::string icon;
    std::string exec_cmd;
};

struct AppClientInfo {
    std::string address;
    std::string app_class;
    std::string title;
    int workspace_id = 1;
    pid_t pid = 0;
    int mem_mb = 0;
    bool is_focused = false;
};

struct BackgroundProcessInfo {
    pid_t pid = 0;
    std::string name;
    std::string binary;
    std::string exec_cmd;
    std::string icon_name;
    std::string tag;
    int mem_mb = 0;
};

class ActiveAppsDrawer {
public:
    static void init(GtkApplication* app);
    static void toggle();
    static void show();
    static void hide();
    static void refresh();

    static GtkWidget* create_topbar_button();
    static std::vector<AppClientInfo> fetch_clients();
    static std::vector<BackgroundProcessInfo> fetch_background_processes(std::vector<AppClientInfo>& active_clients);
    static void kill_process(pid_t pid);
    static void relaunch_app(pid_t pid, const std::string& exec_cmd);

private:
    static GtkWidget* window;
    static GtkWidget* list_box;
    static GtkWidget* count_badge;
    static GtkWidget* empty_box;
    static GtkWidget* scroll_window;
    static GtkWidget* topbar_btn;
    static GtkWidget* topbar_label;
    static GtkWidget* topbar_arrow;
    static guint live_timer_id;

    static std::unordered_map<std::string, DesktopAppMeta> desktop_apps_cache;
    static void load_desktop_apps_cache();

    static void create_window(GtkApplication* app);
    static void build_client_item(const AppClientInfo& client);
    static void build_background_process_item(const BackgroundProcessInfo& proc);
    static void build_tray_item(const TrayItem& item);
    static void focus_client(const std::string& address);
    static void close_client(const std::string& address);
};

} // namespace zenith
