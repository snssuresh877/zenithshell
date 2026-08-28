#include "shell/task/active_apps_drawer.hpp"
#include "gtk3_compat.hpp"
#include "compositors/hyprland_ipc.hpp"
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace zenith {

GtkWidget* ActiveAppsDrawer::window = nullptr;
GtkWidget* ActiveAppsDrawer::list_box = nullptr;
GtkWidget* ActiveAppsDrawer::count_badge = nullptr;
GtkWidget* ActiveAppsDrawer::empty_box = nullptr;
GtkWidget* ActiveAppsDrawer::scroll_window = nullptr;
GtkWidget* ActiveAppsDrawer::topbar_btn = nullptr;
GtkWidget* ActiveAppsDrawer::topbar_label = nullptr;
GtkWidget* ActiveAppsDrawer::topbar_arrow = nullptr;
guint ActiveAppsDrawer::live_timer_id = 0;

std::unordered_map<std::string, DesktopAppMeta> ActiveAppsDrawer::desktop_apps_cache;

struct ProcDetail {
    pid_t pid = 0;
    pid_t ppid = 0;
    std::string comm;
    int mem_kb = 0;
};

static std::string format_memory(int mem_mb) {
    if (mem_mb >= 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f GB", mem_mb / 1024.0);
        return buf;
    }
    return std::to_string(mem_mb) + " MB";
}

static std::unordered_map<pid_t, ProcDetail> scan_proc_table() {
    std::unordered_map<pid_t, ProcDetail> procs;
    uid_t user_uid = getuid();

    try {
        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!entry.is_directory()) continue;
            std::string fname = entry.path().filename().string();
            if (fname.empty() || !isdigit(fname[0])) continue;

            pid_t pid = std::atoi(fname.c_str());
            if (pid <= 1) continue;

            struct stat st;
            if (stat(entry.path().c_str(), &st) != 0 || st.st_uid != user_uid) continue;

            ProcDetail p;
            p.pid = pid;

            std::string comm_path = entry.path().string() + "/comm";
            std::ifstream comm_f(comm_path);
            if (!std::getline(comm_f, p.comm)) continue;

            std::string stat_path = entry.path().string() + "/stat";
            std::ifstream stat_f(stat_path);
            std::string stat_line;
            if (std::getline(stat_f, stat_line)) {
                size_t rparen = stat_line.rfind(')');
                if (rparen != std::string::npos && rparen + 2 < stat_line.size()) {
                    std::istringstream iss(stat_line.substr(rparen + 2));
                    char state;
                    iss >> state >> p.ppid;
                }
            }

            std::string status_path = entry.path().string() + "/status";
            std::ifstream status_f(status_path);
            std::string sline;
            while (std::getline(status_f, sline)) {
                if (sline.rfind("VmRSS:", 0) == 0) {
                    std::istringstream iss(sline.substr(6));
                    iss >> p.mem_kb;
                    break;
                }
            }

            procs[pid] = p;
        }
    } catch (...) {}

    return procs;
}

static int calculate_tree_mem_mb(pid_t root_pid, const std::unordered_map<pid_t, ProcDetail>& procs) {
    if (root_pid <= 0) return 0;
    int total_kb = 0;
    for (const auto& [pid, p] : procs) {
        if (pid == root_pid || p.ppid == root_pid) {
            total_kb += p.mem_kb;
        }
    }
    return total_kb / 1024;
}

void ActiveAppsDrawer::load_desktop_apps_cache() {
    desktop_apps_cache.clear();
    std::vector<std::string> dirs = {
        "/usr/share/applications",
        std::string(g_get_user_data_dir()) + "/applications",
        std::string(g_get_home_dir()) + "/.local/share/applications",
        "/var/lib/flatpak/exports/share/applications",
        std::string(g_get_home_dir()) + "/.local/share/flatpak/exports/share/applications"
    };

    for (const auto& dir : dirs) {
        if (!std::filesystem::exists(dir)) continue;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                    std::ifstream f(entry.path());
                    std::string line, name, icon, exec;
                    while (std::getline(f, line)) {
                        if (line.rfind("Name=", 0) == 0 && name.empty()) {
                            name = line.substr(5);
                        } else if (line.rfind("Icon=", 0) == 0 && icon.empty()) {
                            icon = line.substr(5);
                        } else if (line.rfind("Exec=", 0) == 0 && exec.empty()) {
                            std::string raw = line.substr(5);
                            std::istringstream iss(raw);
                            iss >> exec;
                            size_t slash = exec.rfind('/');
                            if (slash != std::string::npos) exec = exec.substr(slash + 1);
                        }
                    }
                    if (!name.empty() && !exec.empty()) {
                        std::string key = exec;
                        for (char& c : key) c = tolower(c);
                        desktop_apps_cache[key] = {name, icon.empty() ? exec : icon, exec};
                    }
                }
            }
        } catch (...) {}
    }
}

std::vector<AppClientInfo> ActiveAppsDrawer::fetch_clients() {
    std::vector<AppClientInfo> clients;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("hyprctl clients -j 2>/dev/null", "r"), pclose);
    if (!pipe) return clients;

    std::string json_str;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        json_str += buffer;
    }

    size_t pos = 0;
    while ((pos = json_str.find("\"address\":", pos)) != std::string::npos) {
        size_t block_end = json_str.find("},{", pos);
        if (block_end == std::string::npos) block_end = json_str.find("}]", pos);
        if (block_end == std::string::npos) block_end = json_str.size();

        std::string block = json_str.substr(pos, block_end - pos);

        auto extract_str = [&](const std::string& key) -> std::string {
            std::string pat = "\"" + key + "\": \"";
            size_t kpos = block.find(pat);
            if (kpos == std::string::npos) {
                pat = "\"" + key + "\":\"";
                kpos = block.find(pat);
                if (kpos == std::string::npos) return "";
            }
            size_t start = kpos + pat.size();
            size_t end = block.find("\"", start);
            if (end == std::string::npos) return "";
            return block.substr(start, end - start);
        };

        auto extract_int = [&](const std::string& key) -> int {
            std::string pat = "\"" + key + "\": ";
            size_t kpos = block.find(pat);
            if (kpos == std::string::npos) {
                pat = "\"" + key + "\":";
                kpos = block.find(pat);
                if (kpos == std::string::npos) return 0;
            }
            size_t start = kpos + pat.size();
            return std::atoi(block.c_str() + start);
        };

        bool is_mapped = block.find("\"mapped\": true") != std::string::npos;
        bool is_hidden = block.find("\"hidden\": true") != std::string::npos;

        if (is_mapped && !is_hidden) {
            AppClientInfo c;
            c.address = extract_str("address");
            c.app_class = extract_str("class");
            c.title = extract_str("title");
            c.workspace_id = extract_int("id");
            c.pid = extract_int("pid");
            c.is_focused = (extract_int("focusHistoryID") == 0);

            if (!c.address.empty() && !c.app_class.empty()) {
                clients.push_back(c);
            }
        }

        pos = block_end + 1;
    }

    return clients;
}

std::vector<BackgroundProcessInfo> ActiveAppsDrawer::fetch_background_processes(std::vector<AppClientInfo>& active_clients) {
    if (desktop_apps_cache.empty()) {
        load_desktop_apps_cache();
    }

    auto procs = scan_proc_table();

    // 1. Calculate live tree memory for active window clients
    for (auto& c : active_clients) {
        if (c.pid > 0) {
            c.mem_mb = calculate_tree_mem_mb(c.pid, procs);
        }
    }

    std::unordered_set<pid_t> active_pids;
    std::unordered_set<std::string> active_classes;
    for (const auto& c : active_clients) {
        if (c.pid > 0) active_pids.insert(c.pid);
        std::string cls_lower = c.app_class;
        for (char& ch : cls_lower) ch = tolower(ch);
        active_classes.insert(cls_lower);
    }

    std::vector<BackgroundProcessInfo> result;

    static const std::unordered_set<std::string> cli_tools = {
        "nvim", "vim", "btop", "htop", "wiremix", "bluetui",
        "python3", "node", "ollama", "ffmpeg", "mpv"
    };

    for (const auto& [pid, p] : procs) {
        std::string comm_lower = p.comm;
        for (char& c : comm_lower) c = tolower(c);

        if (comm_lower == "zenithshell" || comm_lower == "xwayland" || comm_lower == "hyprland" ||
            comm_lower == "bash" || comm_lower == "zsh" || comm_lower == "fish" ||
            comm_lower == "systemd" || comm_lower == "pipewire" || comm_lower == "wireplumber" ||
            comm_lower == "dbus-daemon" || comm_lower == "dbus-broker" || comm_lower == "ninja" ||
            comm_lower == "sshd" || comm_lower == "gnome-keyring-d" || comm_lower == "polkit-gnome-au" ||
            comm_lower == "gdbus" || comm_lower == "hyprctl" || comm_lower == "grim") {
            continue;
        }

        // Check if root process
        if (p.ppid > 1) {
            auto it = procs.find(p.ppid);
            if (it != procs.end()) {
                std::string pcomm_lower = it->second.comm;
                for (char& c : pcomm_lower) c = tolower(c);
                if (pcomm_lower == comm_lower) {
                    continue; // Skip child process
                }
            }
        }

        // Check if already mapped as active window
        if (active_pids.find(pid) != active_pids.end() || active_classes.find(comm_lower) != active_classes.end()) {
            continue;
        }

        // Check desktop apps first
        auto it = desktop_apps_cache.find(comm_lower);
        if (it != desktop_apps_cache.end()) {
            BackgroundProcessInfo bg;
            bg.pid = pid;
            bg.name = it->second.name;
            bg.binary = comm_lower;
            bg.exec_cmd = it->second.exec_cmd;
            bg.icon_name = it->second.icon;
            bg.tag = "Background";
            bg.mem_mb = calculate_tree_mem_mb(pid, procs);
            result.push_back(bg);
        } else if (cli_tools.find(comm_lower) != cli_tools.end()) {
            std::string disp_name = comm_lower;
            if (!disp_name.empty()) disp_name[0] = toupper(disp_name[0]);
            BackgroundProcessInfo bg;
            bg.pid = pid;
            bg.name = disp_name;
            bg.binary = comm_lower;
            bg.exec_cmd = comm_lower;
            bg.icon_name = "utilities-terminal";
            bg.tag = "CLI Task";
            bg.mem_mb = calculate_tree_mem_mb(pid, procs);
            result.push_back(bg);
        }
    }

    return result;
}

void ActiveAppsDrawer::init(GtkApplication* app) {
    if (!window) {
        load_desktop_apps_cache();
        create_window(app);
    }
}

void ActiveAppsDrawer::create_window(GtkApplication* app) {
    window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "active-apps-window");

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

    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    GtkWidget* backdrop = gtk_event_box_new();
    gtk_widget_add_css_class(backdrop, "active-apps-backdrop");

    GtkWidget* outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(outer_box, GTK_ALIGN_START);
    gtk_widget_set_valign(outer_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(outer_box, 48);
    gtk_widget_set_margin_start(outer_box, 180);

    GtkWidget* main_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(main_card, "active-apps-card");
    gtk_widget_set_size_request(main_card, 400, 440);

    auto on_backdrop_clicked = +[](GtkWidget* widget, GdkEventButton* event, gpointer user_data) -> gboolean {
        GtkWidget* card = static_cast<GtkWidget*>(user_data);
        if (!card) return FALSE;
        GtkAllocation alloc;
        gtk_widget_get_allocation(card, &alloc);
        int wx = 0, wy = 0;
        gtk_widget_translate_coordinates(widget, card, static_cast<int>(event->x), static_cast<int>(event->y), &wx, &wy);
        if (wx >= 0 && wx < alloc.width && wy >= 0 && wy < alloc.height) {
            return FALSE;
        }
        ActiveAppsDrawer::hide();
        return TRUE;
    };
    g_signal_connect(backdrop, "button-press-event", G_CALLBACK(on_backdrop_clicked), main_card);

    // Header: [󱊖 Apps & Tasks  (N • RAM)] ------ [󰅖]
    GtkWidget* header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(header_box, "active-apps-header");

    GtkWidget* title_icon = gtk_label_new("󱊖");
    gtk_widget_add_css_class(title_icon, "active-apps-title-icon");

    GtkWidget* title_lbl = gtk_label_new("Apps & Tasks");
    gtk_widget_add_css_class(title_lbl, "active-apps-title");

    count_badge = gtk_label_new("0");
    gtk_widget_add_css_class(count_badge, "active-apps-badge");

    gtk_box_pack_start(GTK_BOX(header_box), title_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), title_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_box), count_badge, FALSE, FALSE, 0);

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "active-apps-close-btn");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ActiveAppsDrawer::hide();
    }), nullptr);
    gtk_box_pack_end(GTK_BOX(header_box), close_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_card), header_box, FALSE, FALSE, 0);

    scroll_window = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll_window, TRUE);

    list_box = gtk_list_box_new();
    gtk_widget_add_css_class(list_box, "active-apps-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_NONE);
    gtk_container_add(GTK_CONTAINER(scroll_window), list_box);

    empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(empty_box, "active-apps-empty-box");
    gtk_widget_set_valign(empty_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(empty_box, TRUE);

    GtkWidget* empty_icon = gtk_label_new("󰄬");
    gtk_widget_add_css_class(empty_icon, "active-apps-empty-icon");

    GtkWidget* empty_title = gtk_label_new("No Active Tasks");
    gtk_widget_add_css_class(empty_title, "active-apps-empty-title");

    GtkWidget* empty_sub = gtk_label_new("All applications and background tasks are idle");
    gtk_widget_add_css_class(empty_sub, "active-apps-empty-sub");

    gtk_box_pack_start(GTK_BOX(empty_box), empty_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(empty_box), empty_title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(empty_box), empty_sub, FALSE, FALSE, 0);

    GtkWidget* content_stack = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(content_stack, TRUE);
    gtk_box_pack_start(GTK_BOX(content_stack), scroll_window, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content_stack), empty_box, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(main_card), content_stack, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(outer_box), main_card, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(backdrop), outer_box);
    gtk_container_add(GTK_CONTAINER(window), backdrop);

    g_signal_connect(window, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer) -> gboolean {
        if (event->keyval == GDK_KEY_Escape) {
            ActiveAppsDrawer::hide();
            return TRUE;
        }
        return FALSE;
    }), nullptr);

    gtk_widget_show_all(window);
    gtk_widget_hide(window);
}

GtkWidget* ActiveAppsDrawer::create_topbar_button() {
    topbar_btn = gtk_button_new();
    gtk_widget_add_css_class(topbar_btn, "pill-widget");
    gtk_widget_add_css_class(topbar_btn, "active-apps-pill");
    gtk_widget_set_size_request(topbar_btn, -1, 24);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    topbar_arrow = gtk_label_new(">");
    gtk_widget_add_css_class(topbar_arrow, "active-apps-arrow");

    topbar_label = gtk_label_new("0 Apps");
    gtk_widget_add_css_class(topbar_label, "active-apps-text");

    gtk_box_pack_start(GTK_BOX(box), topbar_arrow, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), topbar_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(topbar_btn), box);

    g_signal_connect(topbar_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer) {
        ActiveAppsDrawer::toggle();
    }), nullptr);

    auto update_state = []() {
        if (!topbar_label || !topbar_btn) return;
        auto clients = fetch_clients();
        auto bg_procs = fetch_background_processes(clients);
        auto tray_items = SystemTrayManager::get_tray_items();
        int count = static_cast<int>(clients.size() + bg_procs.size() + tray_items.size());

        char buf[32];
        if (count == 1) {
            snprintf(buf, sizeof(buf), "1 App");
        } else {
            snprintf(buf, sizeof(buf), "%d Apps", count);
        }
        gtk_label_set_text(GTK_LABEL(topbar_label), buf);

        if (count == 0) {
            gtk_widget_set_visible(topbar_btn, FALSE);
        } else {
            gtk_widget_set_visible(topbar_btn, TRUE);
        }

        if (window && gtk_widget_get_visible(window)) {
            refresh();
        }
    };

    update_state();

    // 1. Waybar-like instant event-driven sync from Hyprland IPC socket2
    HyprlandIPC::instance().add_window_event_callback([update_state]() {
        update_state();
    });

    // 2. btop-like fallback ticker for memory & background process changes
    g_timeout_add(1500, [](gpointer data) -> gboolean {
        auto* fn = static_cast<std::function<void()>*>(data);
        (*fn)();
        return TRUE;
    }, new std::function<void()>(update_state));

    return topbar_btn;
}

static int64_t last_drawer_hide_time_ms = 0;

void ActiveAppsDrawer::toggle() {
    if (!window) return;
    int64_t now = g_get_monotonic_time() / 1000;
    if (now - last_drawer_hide_time_ms < 250) {
        return;
    }
    if (gtk_widget_get_visible(window)) {
        hide();
    } else {
        show();
    }
}

void ActiveAppsDrawer::show() {
    if (!window) return;
    refresh();
    gtk_widget_show(window);
    gtk_window_present(GTK_WINDOW(window));

    // Start 1.0s live stats ticker like btop while drawer is open
    if (live_timer_id == 0) {
        live_timer_id = g_timeout_add(1000, [](gpointer) -> gboolean {
            if (window && gtk_widget_get_visible(window)) {
                ActiveAppsDrawer::refresh();
                return TRUE;
            }
            live_timer_id = 0;
            return FALSE;
        }, nullptr);
    }
}

void ActiveAppsDrawer::hide() {
    if (!window) return;
    last_drawer_hide_time_ms = g_get_monotonic_time() / 1000;
    if (live_timer_id != 0) {
        g_source_remove(live_timer_id);
        live_timer_id = 0;
    }
    gtk_widget_hide(window);
}

void ActiveAppsDrawer::refresh() {
    if (!list_box || !count_badge) return;

    GList* children = gtk_container_get_children(GTK_CONTAINER(list_box));
    for (GList* l = children; l != nullptr; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);

    auto clients = fetch_clients();
    auto bg_procs = fetch_background_processes(clients);
    auto tray_items = SystemTrayManager::get_tray_items();
    int count = static_cast<int>(clients.size() + bg_procs.size() + tray_items.size());

    int total_mem_mb = 0;
    for (const auto& c : clients) total_mem_mb += c.mem_mb;
    for (const auto& bg : bg_procs) total_mem_mb += bg.mem_mb;

    char count_str[64];
    if (total_mem_mb > 0) {
        snprintf(count_str, sizeof(count_str), "%d • %s", count, format_memory(total_mem_mb).c_str());
    } else {
        snprintf(count_str, sizeof(count_str), "%d", count);
    }
    gtk_label_set_text(GTK_LABEL(count_badge), count_str);

    if (count == 0) {
        gtk_widget_hide(scroll_window);
        gtk_widget_show_all(empty_box);
    } else {
        gtk_widget_hide(empty_box);
        gtk_widget_show(scroll_window);

        // 1. Render Active Window Clients
        for (const auto& c : clients) {
            build_client_item(c);
        }

        // 2. Render Lingering Background Processes (No Window) & CLI Tasks
        for (const auto& bg : bg_procs) {
            build_background_process_item(bg);
        }

        // 3. Render Background SNI Tray Services (Proton VPN, WeChat, etc.)
        for (const auto& t : tray_items) {
            build_tray_item(t);
        }

        gtk_widget_show_all(list_box);
    }
}

void ActiveAppsDrawer::build_background_process_item(const BackgroundProcessInfo& proc) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(card, "active-app-item-card");

    // Left Icon Squircle
    GtkWidget* icon_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(icon_container, "active-app-icon-box");
    gtk_widget_set_size_request(icon_container, 34, 34);

    GtkWidget* icon_w = nullptr;
    GtkIconTheme* theme = gtk_icon_theme_get_default();
    if (!proc.icon_name.empty() && gtk_icon_theme_has_icon(theme, proc.icon_name.c_str())) {
        icon_w = gtk_image_new_from_icon_name(proc.icon_name.c_str(), GTK_ICON_SIZE_DND);
    } else if (gtk_icon_theme_has_icon(theme, proc.binary.c_str())) {
        icon_w = gtk_image_new_from_icon_name(proc.binary.c_str(), GTK_ICON_SIZE_DND);
    } else {
        icon_w = gtk_label_new("󱊖");
        gtk_widget_add_css_class(icon_w, "tray-fallback-icon");
    }

    if (GTK_IS_IMAGE(icon_w)) {
        gtk_image_set_pixel_size(GTK_IMAGE(icon_w), 24);
    }
    gtk_box_pack_start(GTK_BOX(icon_container), icon_w, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), icon_container, FALSE, FALSE, 0);

    // Title & Info Box
    GtkWidget* txt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    GtkWidget* title_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* title = gtk_label_new(proc.name.c_str());
    gtk_widget_add_css_class(title, "active-app-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    GtkWidget* ghost_badge = gtk_label_new(proc.tag.empty() ? "Background" : proc.tag.c_str());
    gtk_widget_add_css_class(ghost_badge, "active-app-ghost-tag");

    gtk_box_pack_start(GTK_BOX(title_row), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(title_row), ghost_badge, FALSE, FALSE, 0);

    char sub_buf[128];
    if (proc.mem_mb > 0) {
        snprintf(sub_buf, sizeof(sub_buf), "PID %d • %s • %s",
                 proc.pid,
                 format_memory(proc.mem_mb).c_str(),
                 (proc.tag == "CLI Task") ? "Terminal Task" : "No Active Window");
    } else {
        snprintf(sub_buf, sizeof(sub_buf), "PID %d • Running in Background", proc.pid);
    }
    GtkWidget* sub = gtk_label_new(sub_buf);
    gtk_widget_add_css_class(sub, "active-app-sub");
    gtk_widget_set_halign(sub, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(txt_box), title_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(txt_box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), txt_box, TRUE, TRUE, 0);

    // Action Buttons: [󰘳 Relaunch]  [󰅖 End Task]
    GtkWidget* btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    if (proc.tag != "CLI Task") {
        GtkWidget* relaunch_btn = gtk_button_new_with_label("󰘳 Relaunch");
        gtk_widget_add_css_class(relaunch_btn, "active-app-relaunch-btn");
        auto* rdata = new BackgroundProcessInfo(proc);
        g_signal_connect(relaunch_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
            auto* p = static_cast<BackgroundProcessInfo*>(data);
            ActiveAppsDrawer::relaunch_app(p->pid, p->exec_cmd);
            delete p;
            ActiveAppsDrawer::hide();
        }), rdata);
        gtk_box_pack_start(GTK_BOX(btn_box), relaunch_btn, FALSE, FALSE, 0);
    }

    GtkWidget* end_task_btn = gtk_button_new_with_label("󰅖 End Task");
    gtk_widget_add_css_class(end_task_btn, "active-app-kill-btn");
    auto* kdata = new BackgroundProcessInfo(proc);
    g_signal_connect(end_task_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        auto* p = static_cast<BackgroundProcessInfo*>(data);
        ActiveAppsDrawer::kill_process(p->pid);
        delete p;
        g_timeout_add(100, [](gpointer) -> gboolean {
            ActiveAppsDrawer::refresh();
            return FALSE;
        }, nullptr);
    }), kdata);

    gtk_box_pack_start(GTK_BOX(btn_box), end_task_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(card), btn_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(list_box), card);
}

void ActiveAppsDrawer::build_tray_item(const TrayItem& item) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(card, "active-app-item-card");

    GtkWidget* icon_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(icon_container, "active-app-icon-box");
    gtk_widget_set_size_request(icon_container, 34, 34);

    GtkWidget* icon_w = nullptr;
    if (!item.icon_name.empty() && g_file_test(item.icon_name.c_str(), G_FILE_TEST_EXISTS)) {
        icon_w = gtk_image_new_from_file(item.icon_name.c_str());
    } else {
        std::string icon_key = item.icon_name.empty() ? item.id : item.icon_name;
        if (icon_key == "wechat") icon_key = "wechat";
        GtkIconTheme* theme = gtk_icon_theme_get_default();
        if (gtk_icon_theme_has_icon(theme, icon_key.c_str())) {
            icon_w = gtk_image_new_from_icon_name(icon_key.c_str(), GTK_ICON_SIZE_DND);
        } else if (icon_key == "wechat") {
            icon_w = gtk_label_new("󰘑");
            gtk_widget_add_css_class(icon_w, "tray-fallback-icon");
        } else if (icon_key.find("proton") != std::string::npos || icon_key.find("vpn") != std::string::npos) {
            icon_w = gtk_label_new("󰖟");
            gtk_widget_add_css_class(icon_w, "tray-fallback-icon");
        } else {
            icon_w = gtk_label_new("󰍜");
            gtk_widget_add_css_class(icon_w, "tray-fallback-icon");
        }
    }

    if (GTK_IS_IMAGE(icon_w)) {
        gtk_image_set_pixel_size(GTK_IMAGE(icon_w), 24);
    }
    gtk_box_pack_start(GTK_BOX(icon_container), icon_w, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), icon_container, FALSE, FALSE, 0);

    GtkWidget* txt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* title = gtk_label_new(item.title.c_str());
    gtk_widget_add_css_class(title, "active-app-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    GtkWidget* sub = gtk_label_new("Background Tray Service • Running");
    gtk_widget_add_css_class(sub, "active-app-sub");
    gtk_widget_set_halign(sub, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(txt_box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(txt_box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), txt_box, TRUE, TRUE, 0);

    GtkWidget* open_btn = gtk_button_new_with_label("󰘳 Open");
    gtk_widget_add_css_class(open_btn, "active-app-focus-btn");

    using TrayPair = std::pair<std::string, std::string>;
    auto* tp = new TrayPair(item.service, item.path);
    g_signal_connect(open_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        auto* pair = static_cast<TrayPair*>(data);
        SystemTrayManager::activate_item(pair->first, pair->second);
        delete pair;
        ActiveAppsDrawer::hide();
    }), tp);

    gtk_box_pack_end(GTK_BOX(card), open_btn, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(list_box), card);
}

void ActiveAppsDrawer::build_client_item(const AppClientInfo& client) {
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(card, "active-app-item-card");

    // Left Icon
    GtkWidget* icon_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(icon_container, "active-app-icon-box");
    gtk_widget_set_size_request(icon_container, 34, 34);

    GtkWidget* icon_w = gtk_image_new_from_icon_name(client.app_class.c_str(), GTK_ICON_SIZE_DND);
    gtk_image_set_pixel_size(GTK_IMAGE(icon_w), 24);
    gtk_box_pack_start(GTK_BOX(icon_container), icon_w, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(card), icon_container, FALSE, FALSE, 0);

    // Friendly App Name
    std::string app_name = client.app_class;
    std::string comm_key = client.app_class;
    for (char& c : comm_key) c = tolower(c);
    auto it = desktop_apps_cache.find(comm_key);
    if (it != desktop_apps_cache.end()) {
        app_name = it->second.name;
    } else if (!app_name.empty()) {
        app_name[0] = toupper(app_name[0]);
    }

    // Title & Info Box
    GtkWidget* txt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget* title = gtk_label_new(app_name.c_str());
    gtk_widget_add_css_class(title, "active-app-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(title), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(title), 26);

    char sub_buf[128];
    std::string title_snippet = client.title.empty() ? client.app_class : client.title;
    if (title_snippet.size() > 24) {
        title_snippet = title_snippet.substr(0, 22) + "..";
    }

    if (client.mem_mb > 0) {
        snprintf(sub_buf, sizeof(sub_buf), "WS %d • %s • %s%s",
                 client.workspace_id,
                 format_memory(client.mem_mb).c_str(),
                 title_snippet.c_str(),
                 client.is_focused ? " • Focused" : "");
    } else {
        snprintf(sub_buf, sizeof(sub_buf), "WS %d • %s%s",
                 client.workspace_id,
                 title_snippet.c_str(),
                 client.is_focused ? " • Focused" : "");
    }

    GtkWidget* sub = gtk_label_new(sub_buf);
    gtk_widget_add_css_class(sub, "active-app-sub");
    gtk_widget_set_halign(sub, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(txt_box), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(txt_box), sub, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(card), txt_box, TRUE, TRUE, 0);

    // Action Buttons: [󰘳 Focus] [󰐥 Kill] [󰅖 Close]
    GtkWidget* btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    GtkWidget* focus_btn = gtk_button_new_with_label("󰘳 Focus");
    gtk_widget_add_css_class(focus_btn, "active-app-focus-btn");
    std::string addr_focus = client.address;
    g_signal_connect(focus_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        char* a = static_cast<char*>(user_data);
        ActiveAppsDrawer::focus_client(a);
        free(a);
        ActiveAppsDrawer::hide();
    }), strdup(addr_focus.c_str()));
    gtk_box_pack_start(GTK_BOX(btn_box), focus_btn, FALSE, FALSE, 0);

    // btop-like Force Kill Process Tree
    if (client.pid > 1) {
        GtkWidget* kill_btn = gtk_button_new_with_label("󰐥");
        gtk_widget_add_css_class(kill_btn, "active-app-kill-icon-btn");
        gtk_widget_set_tooltip_text(kill_btn, "Force Kill Process Tree (btop kill)");
        pid_t p = client.pid;
        g_signal_connect(kill_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
            pid_t pid = GPOINTER_TO_INT(user_data);
            ActiveAppsDrawer::kill_process(pid);
            g_timeout_add(100, [](gpointer) -> gboolean {
                ActiveAppsDrawer::refresh();
                return FALSE;
            }, nullptr);
        }), GINT_TO_POINTER(p));
        gtk_box_pack_start(GTK_BOX(btn_box), kill_btn, FALSE, FALSE, 0);
    }

    GtkWidget* close_btn = gtk_button_new_with_label("󰅖");
    gtk_widget_add_css_class(close_btn, "active-app-close-btn");
    gtk_widget_set_tooltip_text(close_btn, "Close Window");
    std::string addr_close = client.address;
    g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer user_data) {
        char* a = static_cast<char*>(user_data);
        ActiveAppsDrawer::close_client(a);
        free(a);
        g_timeout_add(100, [](gpointer) -> gboolean {
            ActiveAppsDrawer::refresh();
            return FALSE;
        }, nullptr);
    }), strdup(addr_close.c_str()));
    gtk_box_pack_start(GTK_BOX(btn_box), close_btn, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(card), btn_box, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(list_box), card);
}

void ActiveAppsDrawer::focus_client(const std::string& address) {
    if (address.empty()) return;
    std::string cmd = "hyprctl dispatch 'hl.dsp.focus({window=\"address:" + address + "\"})' 2>/dev/null &";
    system(cmd.c_str());
}

void ActiveAppsDrawer::close_client(const std::string& address) {
    if (address.empty()) return;
    std::string cmd = "hyprctl dispatch 'hl.dsp.window.close({address=\"" + address + "\"})' 2>/dev/null &";
    system(cmd.c_str());
}

void ActiveAppsDrawer::kill_process(pid_t pid) {
    if (pid <= 1) return;
    kill(pid, SIGTERM);
    std::string cmd = "pkill -P " + std::to_string(pid) + " 2>/dev/null; kill -15 " + std::to_string(pid) + " 2>/dev/null; sleep 0.1; kill -9 " + std::to_string(pid) + " 2>/dev/null &";
    system(cmd.c_str());
}

void ActiveAppsDrawer::relaunch_app(pid_t pid, const std::string& exec_cmd) {
    kill_process(pid);
    if (!exec_cmd.empty()) {
        std::string launch_cmd = exec_cmd + " &";
        system(launch_cmd.c_str());
    }
}

} // namespace zenith
