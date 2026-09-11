#include "shell/files/places_sidebar.hpp"
#include "gtk3_compat.hpp"
#include <gio/gio.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace zenith {

struct PlaceItem {
    std::string name;
    std::string path;
    std::string icon_name;
    std::string glyph_icon;
    bool is_header{false};
    bool is_favorite{false};
};

struct SidebarData {
    PlacesSidebar::NavigateCallback on_navigate;
    std::string active_path;
    GtkWidget* listbox{nullptr};
    std::vector<GtkWidget*> place_rows;
    GVolumeMonitor* volume_monitor{nullptr};
    gulong sid_volume_added{0};
    gulong sid_volume_removed{0};
    gulong sid_mount_added{0};
    gulong sid_mount_removed{0};
};

static std::string get_favorites_file() {
    const char* cfg = g_get_user_config_dir();
    std::string dir = std::string(cfg ? cfg : "~/.config") + "/zenithshell";
    fs::create_directories(dir);
    return dir + "/files_favorites.json";
}

static std::vector<std::pair<std::string, std::string>> load_favorites() {
    std::vector<std::pair<std::string, std::string>> favs;
    std::string file = get_favorites_file();
    if (!fs::exists(file)) {
        // Defaults if user has standard dev / projects folder
        std::string home = g_get_home_dir() ? g_get_home_dir() : "/home";
        if (fs::exists(home + "/Projects")) favs.push_back({"Projects", home + "/Projects"});
        if (fs::exists(home + "/Development")) favs.push_back({"Development", home + "/Development"});
        return favs;
    }

    try {
        std::ifstream in(file);
        json j;
        in >> j;
        if (j.is_array()) {
            for (const auto& item : j) {
                std::string name = item.value("name", "");
                std::string path = item.value("path", "");
                if (!path.empty() && fs::exists(path)) {
                    favs.push_back({name.empty() ? fs::path(path).filename().string() : name, path});
                }
            }
        }
    } catch (...) {}
    return favs;
}

static void save_favorites(const std::vector<std::pair<std::string, std::string>>& favs) {
    std::string file = get_favorites_file();
    json j = json::array();
    for (const auto& f : favs) {
        j.push_back({{"name", f.first}, {"path", f.second}});
    }
    std::ofstream out(file);
    if (out.is_open()) out << j.dump(4);
}

void PlacesSidebar::add_favorite(const std::string& path, const std::string& name) {
    if (path.empty()) return;
    auto favs = load_favorites();
    for (const auto& f : favs) {
        if (f.second == path) return;
    }
    std::string label = !name.empty() ? name : fs::path(path).filename().string();
    if (label.empty()) label = path;
    favs.push_back({label, path});
    save_favorites(favs);
}

void PlacesSidebar::remove_favorite(const std::string& path) {
    if (path.empty()) return;
    auto favs = load_favorites();
    std::vector<std::pair<std::string, std::string>> updated;
    for (const auto& f : favs) {
        if (f.second != path) updated.push_back(f);
    }
    save_favorites(updated);
}

bool PlacesSidebar::is_favorite(const std::string& path) {
    if (path.empty()) return false;
    auto favs = load_favorites();
    for (const auto& f : favs) {
        if (f.second == path) return true;
    }
    return false;
}

static void populate_sidebar(SidebarData* data) {
    if (!data || !data->listbox) return;

    // Clear existing
    GList* children = gtk_container_get_children(GTK_CONTAINER(data->listbox));
    for (GList* it = children; it != nullptr; it = g_list_next(it)) {
        gtk_widget_destroy(GTK_WIDGET(it->data));
    }
    g_list_free(children);
    data->place_rows.clear();

    std::string home = g_get_home_dir() ? g_get_home_dir() : "/home";
    std::vector<PlaceItem> items;

    // --- Section 1: Places ---
    items.push_back({"Places", "", "", "", true, false});
    items.push_back({"Home", home, "user-home-symbolic", "", false, false});

    auto add_xdg_dir = [&](const char* name, GUserDirectory dir_type, const char* icon, const char* glyph) {
        const char* p = g_get_user_special_dir(dir_type);
        if (p && fs::exists(p)) {
            items.push_back({name, p, icon, glyph, false, false});
        }
    };

    add_xdg_dir("Desktop", G_USER_DIRECTORY_DESKTOP, "user-desktop-symbolic", "󰇄");
    add_xdg_dir("Downloads", G_USER_DIRECTORY_DOWNLOAD, "folder-download-symbolic", "");
    add_xdg_dir("Documents", G_USER_DIRECTORY_DOCUMENTS, "folder-documents-symbolic", "");
    add_xdg_dir("Pictures", G_USER_DIRECTORY_PICTURES, "folder-pictures-symbolic", "");
    add_xdg_dir("Music", G_USER_DIRECTORY_MUSIC, "folder-music-symbolic", "");
    add_xdg_dir("Videos", G_USER_DIRECTORY_VIDEOS, "folder-videos-symbolic", "");

    // Trash & Network
    items.push_back({"Trash", "trash:///", "user-trash-symbolic", "", false, false});
    items.push_back({"Network", "network:///", "network-workgroup-symbolic", "󰤨", false, false});

    // --- Section 2: Locations & Devices ---
    items.push_back({"Locations", "", "", "", true, false});
    items.push_back({"File System", "/", "drive-harddisk-symbolic", "󰋊", false, false});

    GVolumeMonitor* monitor = g_volume_monitor_get();
    if (monitor) {
        GList* mounts = g_volume_monitor_get_mounts(monitor);
        for (GList* m = mounts; m != nullptr; m = g_list_next(m)) {
            GMount* mount = G_MOUNT(m->data);
            char* name = g_mount_get_name(mount);
            GFile* root = g_mount_get_root(mount);
            char* p = root ? g_file_get_parse_name(root) : nullptr;
            if (p && name && std::string(p) != "/") {
                items.push_back({name, p, "drive-removable-media-symbolic", "󰋊", false, false});
            }
            if (p) g_free(p);
            if (root) g_object_unref(root);
            if (name) g_free(name);
            g_object_unref(mount);
        }
        g_list_free(mounts);
        g_object_unref(monitor);
    }

    // --- Section 3: Favorites ---
    auto favs = load_favorites();
    if (!favs.empty()) {
        items.push_back({"Favorites", "", "", "", true, false});
        for (const auto& fav : favs) {
            items.push_back({fav.first, fav.second, "starred-symbolic", "★", false, true});
        }
    }

    // --- Section 4: Bookmarks ---
    std::string bookmarks_file = home + "/.config/gtk-3.0/bookmarks";
    if (fs::exists(bookmarks_file)) {
        std::ifstream in(bookmarks_file);
        std::string line;
        bool header_added = false;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line.empty()) continue;

            std::string uri, custom_name;
            size_t space_pos = line.find(' ');
            if (space_pos != std::string::npos) {
                uri = line.substr(0, space_pos);
                custom_name = line.substr(space_pos + 1);
            } else {
                uri = line;
            }

            GFile* bf = g_file_new_for_uri(uri.c_str());
            if (bf) {
                char* bp = g_file_get_parse_name(bf);
                if (bp && fs::exists(bp)) {
                    if (!header_added) {
                        items.push_back({"Bookmarks", "", "", "", true, false});
                        header_added = true;
                    }
                    std::string label = !custom_name.empty() ? custom_name : fs::path(bp).filename().string();
                    items.push_back({label, bp, "folder-symbolic", "󰉋", false, false});
                }
                if (bp) g_free(bp);
                g_object_unref(bf);
            }
        }
    }

    // Render list
    for (const auto& item : items) {
        if (item.is_header) {
            GtkWidget* hdr = gtk_label_new(item.name.c_str());
            gtk_widget_add_css_class(hdr, "files-sidebar-header");
            gtk_label_set_xalign(GTK_LABEL(hdr), 0.0f);
            gtk_container_add(GTK_CONTAINER(data->listbox), hdr);
            continue;
        }

        GtkWidget* row = gtk_list_box_row_new();
        gtk_widget_add_css_class(row, "files-sidebar-row");

        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_end(box, 8);
        gtk_widget_set_margin_top(box, 6);
        gtk_widget_set_margin_bottom(box, 6);

        // Icon
        GtkWidget* icon_widget = nullptr;
        GtkIconTheme* theme = gtk_icon_theme_get_default();
        if (theme && gtk_icon_theme_has_icon(theme, item.icon_name.c_str())) {
            icon_widget = gtk_image_new_from_icon_name(item.icon_name.c_str(), GTK_ICON_SIZE_MENU);
        } else {
            icon_widget = gtk_label_new(item.glyph_icon.c_str());
            gtk_widget_add_css_class(icon_widget, "files-sidebar-glyph");
        }
        gtk_box_pack_start(GTK_BOX(box), icon_widget, FALSE, FALSE, 0);

        GtkWidget* lbl = gtk_label_new(item.name.c_str());
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_widget_add_css_class(lbl, "files-sidebar-text");
        gtk_box_pack_start(GTK_BOX(box), lbl, TRUE, TRUE, 0);

        gtk_container_add(GTK_CONTAINER(row), box);

        std::string* dest = new std::string(item.path);
        g_object_set_data_full(G_OBJECT(row), "place_path", dest, +[](gpointer d) {
            delete static_cast<std::string*>(d);
        });

        // Context menu for Favorites & Devices
        if (item.is_favorite) {
            struct FavoriteRowCtx {
                std::string path;
                SidebarData* data;
            };
            auto* fctx = new FavoriteRowCtx{item.path, data};
            auto fav_press_cb = +[](GtkWidget*, GdkEventButton* event, gpointer user_data) -> gboolean {
                if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
                    auto* fc = static_cast<FavoriteRowCtx*>(user_data);
                    GtkWidget* menu = gtk_menu_new();
                    gtk_widget_add_css_class(menu, "files-context-menu");

                    GtkWidget* mi_remove = gtk_menu_item_new_with_label("Remove from Favorites");
                    auto* act_ctx = new FavoriteRowCtx{fc->path, fc->data};
                    auto remove_cb = +[](GtkMenuItem*, gpointer ud) {
                        auto* rc = static_cast<FavoriteRowCtx*>(ud);
                        PlacesSidebar::remove_favorite(rc->path);
                        populate_sidebar(rc->data);
                    };
                    g_signal_connect_data(mi_remove, "activate", G_CALLBACK(remove_cb), act_ctx, [](gpointer d, GClosure*) { delete static_cast<FavoriteRowCtx*>(d); }, static_cast<GConnectFlags>(0));

                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi_remove);
                    gtk_widget_show_all(menu);
                    gtk_menu_popup_at_pointer(GTK_MENU(menu), reinterpret_cast<GdkEvent*>(event));
                    return TRUE;
                }
                return FALSE;
            };
            g_signal_connect_data(row, "button-press-event", G_CALLBACK(fav_press_cb), fctx, [](gpointer d, GClosure*) { delete static_cast<FavoriteRowCtx*>(d); }, static_cast<GConnectFlags>(0));
        }

        // Check if currently active
        if (!data->active_path.empty() && data->active_path == item.path) {
            gtk_widget_add_css_class(row, "active");
        }

        data->place_rows.push_back(row);
        gtk_container_add(GTK_CONTAINER(data->listbox), row);
    }

    gtk_widget_show_all(data->listbox);
}

GtkWidget* PlacesSidebar::create(NavigateCallback on_navigate) {
    auto* data = new SidebarData();
    data->on_navigate = std::move(on_navigate);

    GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_add_css_class(scroll, "files-sidebar");
    gtk_widget_set_size_request(scroll, 180, -1);

    GtkWidget* listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(listbox, "files-sidebar-list");
    data->listbox = listbox;

    gtk_container_add(GTK_CONTAINER(scroll), listbox);

    g_signal_connect(listbox, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* row, gpointer user_data) {
        auto* d = static_cast<SidebarData*>(user_data);
        auto* target_path = static_cast<std::string*>(g_object_get_data(G_OBJECT(row), "place_path"));
        if (d && d->on_navigate && target_path) {
            d->on_navigate(*target_path);
        }
    }), data);

    // Volume / Mount signals
    data->volume_monitor = g_volume_monitor_get();
    if (data->volume_monitor) {
        auto on_vol_changed = +[](GVolumeMonitor*, gpointer, gpointer user_data) {
            populate_sidebar(static_cast<SidebarData*>(user_data));
        };
        data->sid_volume_added = g_signal_connect(data->volume_monitor, "volume-added", G_CALLBACK(on_vol_changed), data);
        data->sid_volume_removed = g_signal_connect(data->volume_monitor, "volume-removed", G_CALLBACK(on_vol_changed), data);
        data->sid_mount_added = g_signal_connect(data->volume_monitor, "mount-added", G_CALLBACK(on_vol_changed), data);
        data->sid_mount_removed = g_signal_connect(data->volume_monitor, "mount-removed", G_CALLBACK(on_vol_changed), data);
    }

    g_object_set_data_full(G_OBJECT(scroll), "sidebar_data", data, +[](gpointer d) {
        auto* sd = static_cast<SidebarData*>(d);
        if (sd->volume_monitor) {
            if (sd->sid_volume_added) g_signal_handler_disconnect(sd->volume_monitor, sd->sid_volume_added);
            if (sd->sid_volume_removed) g_signal_handler_disconnect(sd->volume_monitor, sd->sid_volume_removed);
            if (sd->sid_mount_added) g_signal_handler_disconnect(sd->volume_monitor, sd->sid_mount_added);
            if (sd->sid_mount_removed) g_signal_handler_disconnect(sd->volume_monitor, sd->sid_mount_removed);
            g_object_unref(sd->volume_monitor);
        }
        delete sd;
    });

    populate_sidebar(data);
    return scroll;
}

void PlacesSidebar::set_active_path(GtkWidget* sidebar, const std::string& path) {
    if (!sidebar) return;
    auto* data = static_cast<SidebarData*>(g_object_get_data(G_OBJECT(sidebar), "sidebar_data"));
    if (!data) return;

    data->active_path = path;
    for (auto* row : data->place_rows) {
        auto* place_path = static_cast<std::string*>(g_object_get_data(G_OBJECT(row), "place_path"));
        if (place_path && *place_path == path) {
            gtk_widget_add_css_class(row, "active");
        } else {
            gtk_widget_remove_css_class(row, "active");
        }
    }
}

void PlacesSidebar::refresh(GtkWidget* sidebar) {
    if (!sidebar) return;
    auto* data = static_cast<SidebarData*>(g_object_get_data(G_OBJECT(sidebar), "sidebar_data"));
    if (!data) return;
    populate_sidebar(data);
}

} // namespace zenith
