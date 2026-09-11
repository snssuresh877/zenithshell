#include "file_manager_window.hpp"
#include "path_bar_widget.hpp"
#include "places_sidebar.hpp"
#include "file_view_widget.hpp"
#include "file_item.hpp"
#include "file_operations.hpp"
#include "../../gtk3_compat.hpp"

#include <sys/statvfs.h>
#include <gdk/gdkkeysyms.h>
#include <vector>
#include <iostream>
#include <cstring>

namespace zenith {

struct FileManagerState {
    GtkWidget* window{nullptr};
    GtkWidget* btn_back{nullptr};
    GtkWidget* btn_forward{nullptr};
    GtkWidget* btn_up{nullptr};
    GtkWidget* btn_home{nullptr};
    GtkWidget* path_bar{nullptr};
    GtkWidget* btn_search{nullptr};
    GtkWidget* search_revealer{nullptr};
    GtkWidget* search_entry{nullptr};
    GtkWidget* btn_new_folder{nullptr};
    GtkWidget* btn_term{nullptr};
    GtkWidget* btn_hidden{nullptr};
    GtkWidget* btn_grid_view{nullptr};
    GtkWidget* btn_list_view{nullptr};

    GtkWidget* paned{nullptr};
    GtkWidget* sidebar{nullptr};
    GtkWidget* file_view{nullptr};

    GtkWidget* status_label{nullptr};
    GtkWidget* disk_label{nullptr};

    std::string current_path;
    std::vector<std::string> back_history;
    std::vector<std::string> forward_history;

    int last_total_items{0};
    int last_selected_items{0};
    uint64_t last_selected_bytes{0};
};

static void update_nav_buttons(FileManagerState* state) {
    if (!state) return;
    gtk_widget_set_sensitive(state->btn_back, !state->back_history.empty());
    gtk_widget_set_sensitive(state->btn_forward, !state->forward_history.empty());
    gtk_widget_set_sensitive(state->btn_up, state->current_path != "/");
}

static void update_disk_space(FileManagerState* state, const std::string& path) {
    if (!state || !state->disk_label) return;

    struct statvfs vfs;
    if (statvfs(path.c_str(), &vfs) == 0) {
        uint64_t free_bytes = static_cast<uint64_t>(vfs.f_bavail) * vfs.f_frsize;
        std::string s = FileItem::format_size(free_bytes) + " available";
        gtk_label_set_text(GTK_LABEL(state->disk_label), s.c_str());
    } else {
        gtk_label_set_text(GTK_LABEL(state->disk_label), "");
    }
}

static void update_status_text(FileManagerState* state) {
    if (!state || !state->status_label) return;

    std::string text;
    if (state->last_selected_items > 0) {
        text = std::to_string(state->last_selected_items) + " of " +
               std::to_string(state->last_total_items) + " selected";
        if (state->last_selected_bytes > 0) {
            text += " (" + FileItem::format_size(state->last_selected_bytes) + ")";
        }
    } else {
        text = std::to_string(state->last_total_items) + (state->last_total_items == 1 ? " item" : " items");
    }

    gtk_label_set_text(GTK_LABEL(state->status_label), text.c_str());
}

static void navigate_to(FileManagerState* state, const std::string& path, bool record_history = true) {
    if (!state || path.empty()) return;

    if (record_history && !state->current_path.empty() && state->current_path != path) {
        state->back_history.push_back(state->current_path);
        state->forward_history.clear();
    }

    state->current_path = path;

    // Update window title
    std::string title = path;
    const char* home = g_get_home_dir();
    if (home && title.rfind(home, 0) == 0) {
        title = "~" + title.substr(strlen(home));
    }
    gtk_window_set_title(GTK_WINDOW(state->window), (title + " — Zenith Files").c_str());

    update_nav_buttons(state);
    PathBarWidget::set_path(state->path_bar, path);
    PlacesSidebar::set_active_path(state->sidebar, path);
    FileViewWidget::load_directory(state->file_view, path);
    update_disk_space(state, path);
}

static void go_back(FileManagerState* state) {
    if (!state || state->back_history.empty()) return;

    std::string prev = state->back_history.back();
    state->back_history.pop_back();

    if (!state->current_path.empty()) {
        state->forward_history.push_back(state->current_path);
    }

    navigate_to(state, prev, false);
}

static void go_forward(FileManagerState* state) {
    if (!state || state->forward_history.empty()) return;

    std::string next = state->forward_history.back();
    state->forward_history.pop_back();

    if (!state->current_path.empty()) {
        state->back_history.push_back(state->current_path);
    }

    navigate_to(state, next, false);
}

static void go_up(FileManagerState* state) {
    if (!state || state->current_path.empty() || state->current_path == "/") return;

    GFile* cur = g_file_new_for_path(state->current_path.c_str());
    GFile* parent = g_file_get_parent(cur);
    if (parent) {
        char* parent_path = g_file_get_path(parent);
        if (parent_path) {
            navigate_to(state, parent_path, true);
            g_free(parent_path);
        }
        g_object_unref(parent);
    }
    g_object_unref(cur);
}

static void go_home(FileManagerState* state) {
    const char* home = g_get_home_dir();
    if (home) {
        navigate_to(state, home, true);
    }
}

static gboolean on_window_key_press(GtkWidget*, GdkEventKey* event, gpointer user_data) {
    auto* state = static_cast<FileManagerState*>(user_data);
    if (!state) return FALSE;

    guint key = event->keyval;
    guint state_mask = event->state & gtk_accelerator_get_default_mod_mask();

    if (state_mask == GDK_MOD1_MASK) { // Alt key
        if (key == GDK_KEY_Left) {
            go_back(state);
            return TRUE;
        } else if (key == GDK_KEY_Right) {
            go_forward(state);
            return TRUE;
        } else if (key == GDK_KEY_Up) {
            go_up(state);
            return TRUE;
        } else if (key == GDK_KEY_Home) {
            go_home(state);
            return TRUE;
        }
    } else if (state_mask == GDK_CONTROL_MASK) {
        if (key == GDK_KEY_l || key == GDK_KEY_L) {
            PathBarWidget::enter_edit_mode(state->path_bar);
            return TRUE;
        } else if (key == GDK_KEY_f || key == GDK_KEY_F) {
            gboolean active = gtk_revealer_get_reveal_child(GTK_REVEALER(state->search_revealer));
            gtk_revealer_set_reveal_child(GTK_REVEALER(state->search_revealer), !active);
            if (!active) {
                gtk_widget_grab_focus(state->search_entry);
            } else {
                FileViewWidget::set_search_query(state->file_view, "");
            }
            return TRUE;
        } else if (key == GDK_KEY_t || key == GDK_KEY_T) {
            FileOperations::open_terminal(state->current_path);
            return TRUE;
        } else if (key == GDK_KEY_1) {
            FileViewWidget::set_view_mode(state->file_view, ViewMode::GRID);
            gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
            gtk_widget_remove_css_class(state->btn_list_view, "files-btn-view-active");
            return TRUE;
        } else if (key == GDK_KEY_2) {
            FileViewWidget::set_view_mode(state->file_view, ViewMode::LIST);
            gtk_widget_add_css_class(state->btn_list_view, "files-btn-view-active");
            gtk_widget_remove_css_class(state->btn_grid_view, "files-btn-view-active");
            return TRUE;
        }
    } else if (state_mask == 0) {
        if (key == GDK_KEY_Escape) {
            if (gtk_revealer_get_reveal_child(GTK_REVEALER(state->search_revealer))) {
                gtk_entry_set_text(GTK_ENTRY(state->search_entry), "");
                gtk_revealer_set_reveal_child(GTK_REVEALER(state->search_revealer), FALSE);
                FileViewWidget::set_search_query(state->file_view, "");
                gtk_widget_grab_focus(state->file_view);
                return TRUE;
            }
        }
    }

    return FALSE;
}

GtkWidget* FileManagerWindow::create(const std::string& initial_path) {
    auto* state = new FileManagerState();

    state->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(state->window), 980, 640);
    gtk_window_set_icon_name(GTK_WINDOW(state->window), "system-file-manager");
    gtk_widget_add_css_class(state->window, "zenith-files-window");

    GtkWidget* main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(state->window), main_vbox);

    // 1. Toolbar / HeaderBar
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(toolbar, "files-toolbar");
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);
    gtk_box_pack_start(GTK_BOX(main_vbox), toolbar, FALSE, FALSE, 0);

    // Nav buttons: Back, Forward, Up, Home
    GtkWidget* nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_add_css_class(nav_box, "files-nav-group");

    state->btn_back = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_back, "Back (Alt+Left)");
    gtk_widget_add_css_class(state->btn_back, "files-btn-nav");
    g_signal_connect_swapped(state->btn_back, "clicked", G_CALLBACK(go_back), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_back, FALSE, FALSE, 0);

    state->btn_forward = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_forward, "Forward (Alt+Right)");
    gtk_widget_add_css_class(state->btn_forward, "files-btn-nav");
    g_signal_connect_swapped(state->btn_forward, "clicked", G_CALLBACK(go_forward), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_forward, FALSE, FALSE, 0);

    state->btn_up = gtk_button_new_from_icon_name("go-up-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_up, "Parent Directory (Alt+Up)");
    gtk_widget_add_css_class(state->btn_up, "files-btn-nav");
    g_signal_connect_swapped(state->btn_up, "clicked", G_CALLBACK(go_up), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_up, FALSE, FALSE, 0);

    state->btn_home = gtk_button_new_from_icon_name("user-home-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_home, "Home Folder (Alt+Home)");
    gtk_widget_add_css_class(state->btn_home, "files-btn-nav");
    g_signal_connect_swapped(state->btn_home, "clicked", G_CALLBACK(go_home), state);
    gtk_box_pack_start(GTK_BOX(nav_box), state->btn_home, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(toolbar), nav_box, FALSE, FALSE, 0);

    // PathBar (Breadcrumbs)
    state->path_bar = PathBarWidget::create([state](const std::string& target) {
        navigate_to(state, target, true);
    });
    gtk_box_pack_start(GTK_BOX(toolbar), state->path_bar, TRUE, TRUE, 0);

    // Actions Box: Search, New Folder, Terminal, Hidden Toggle, View Switcher
    GtkWidget* act_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(act_box, "files-act-group");

    state->btn_search = gtk_button_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_search, "Search Files (Ctrl+F)");
    gtk_widget_add_css_class(state->btn_search, "files-btn-tool");
    g_signal_connect_swapped(state->btn_search, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        gboolean active = gtk_revealer_get_reveal_child(GTK_REVEALER(s->search_revealer));
        gtk_revealer_set_reveal_child(GTK_REVEALER(s->search_revealer), !active);
        if (!active) {
            gtk_widget_grab_focus(s->search_entry);
        } else {
            FileViewWidget::set_search_query(s->file_view, "");
        }
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_search, FALSE, FALSE, 0);

    state->btn_new_folder = gtk_button_new_from_icon_name("folder-new-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_new_folder, "New Folder (Ctrl+Shift+N)");
    gtk_widget_add_css_class(state->btn_new_folder, "files-btn-tool");
    g_signal_connect_swapped(state->btn_new_folder, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        FileViewWidget::action_new_folder(s->file_view);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_new_folder, FALSE, FALSE, 0);

    state->btn_term = gtk_button_new_from_icon_name("utilities-terminal-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_term, "Open Terminal (Ctrl+T)");
    gtk_widget_add_css_class(state->btn_term, "files-btn-tool");
    g_signal_connect_swapped(state->btn_term, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        FileOperations::open_terminal(s->current_path);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_term, FALSE, FALSE, 0);

    state->btn_hidden = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(state->btn_hidden), gtk_image_new_from_icon_name("view-conceal-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_widget_set_tooltip_text(state->btn_hidden, "Toggle Hidden Files (Ctrl+H)");
    gtk_widget_add_css_class(state->btn_hidden, "files-btn-tool");
    g_signal_connect_swapped(state->btn_hidden, "toggled", G_CALLBACK(+[](FileManagerState* s) {
        gboolean act = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(s->btn_hidden));
        FileViewWidget::set_show_hidden(s->file_view, act);
    }), state);
    gtk_box_pack_start(GTK_BOX(act_box), state->btn_hidden, FALSE, FALSE, 0);

    // View Switcher (Grid / List)
    GtkWidget* view_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(view_box, "files-view-switcher");

    state->btn_grid_view = gtk_button_new_from_icon_name("view-grid-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_grid_view, "Grid View (Ctrl+1)");
    gtk_widget_add_css_class(state->btn_grid_view, "files-btn-view-active");
    g_signal_connect_swapped(state->btn_grid_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        FileViewWidget::set_view_mode(s->file_view, ViewMode::GRID);
        gtk_widget_add_css_class(s->btn_grid_view, "files-btn-view-active");
        gtk_widget_remove_css_class(s->btn_list_view, "files-btn-view-active");
    }), state);
    gtk_box_pack_start(GTK_BOX(view_box), state->btn_grid_view, FALSE, FALSE, 0);

    state->btn_list_view = gtk_button_new_from_icon_name("view-list-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(state->btn_list_view, "List View (Ctrl+2)");
    g_signal_connect_swapped(state->btn_list_view, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        FileViewWidget::set_view_mode(s->file_view, ViewMode::LIST);
        gtk_widget_add_css_class(s->btn_list_view, "files-btn-view-active");
        gtk_widget_remove_css_class(s->btn_grid_view, "files-btn-view-active");
    }), state);
    gtk_box_pack_start(GTK_BOX(view_box), state->btn_list_view, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(act_box), view_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), act_box, FALSE, FALSE, 0);

    // 2. Filter / Search Bar (Revealer)
    state->search_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(state->search_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);

    GtkWidget* search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(search_box, "files-search-bar");
    gtk_container_set_border_width(GTK_CONTAINER(search_box), 6);

    state->search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(state->search_entry, "files-search-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->search_entry), "Search in current folder...");
    g_signal_connect_swapped(state->search_entry, "search-changed", G_CALLBACK(+[](FileManagerState* s) {
        const char* q = gtk_entry_get_text(GTK_ENTRY(s->search_entry));
        FileViewWidget::set_search_query(s->file_view, q ? q : "");
    }), state);
    gtk_box_pack_start(GTK_BOX(search_box), state->search_entry, TRUE, TRUE, 0);

    GtkWidget* btn_close_search = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_add_css_class(btn_close_search, "files-btn-close-search");
    g_signal_connect_swapped(btn_close_search, "clicked", G_CALLBACK(+[](FileManagerState* s) {
        gtk_entry_set_text(GTK_ENTRY(s->search_entry), "");
        gtk_revealer_set_reveal_child(GTK_REVEALER(s->search_revealer), FALSE);
        FileViewWidget::set_search_query(s->file_view, "");
    }), state);
    gtk_box_pack_start(GTK_BOX(search_box), btn_close_search, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(state->search_revealer), search_box);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->search_revealer, FALSE, FALSE, 0);

    // 3. Middle Paned: Sidebar + FileView
    state->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(state->paned), 210);
    gtk_box_pack_start(GTK_BOX(main_vbox), state->paned, TRUE, TRUE, 0);

    state->sidebar = PlacesSidebar::create([state](const std::string& target) {
        navigate_to(state, target, true);
    });
    gtk_paned_pack1(GTK_PANED(state->paned), state->sidebar, FALSE, FALSE);

    state->file_view = FileViewWidget::create(
        [state](const std::string& target) {
            navigate_to(state, target, true);
        },
        [state](int total, int sel_count, uint64_t sel_bytes) {
            state->last_total_items = total;
            state->last_selected_items = sel_count;
            state->last_selected_bytes = sel_bytes;
            update_status_text(state);
        }
    );
    gtk_paned_pack2(GTK_PANED(state->paned), state->file_view, TRUE, FALSE);

    // 4. Status Bar
    GtkWidget* statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(statusbar, "files-statusbar");
    gtk_container_set_border_width(GTK_CONTAINER(statusbar), 6);

    state->status_label = gtk_label_new("0 items");
    gtk_label_set_xalign(GTK_LABEL(state->status_label), 0.0f);
    gtk_widget_add_css_class(state->status_label, "files-status-text");
    gtk_box_pack_start(GTK_BOX(statusbar), state->status_label, FALSE, FALSE, 0);

    state->disk_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(state->disk_label), 1.0f);
    gtk_widget_add_css_class(state->disk_label, "files-disk-text");
    gtk_box_pack_end(GTK_BOX(statusbar), state->disk_label, FALSE, FALSE, 0);

    gtk_box_pack_end(GTK_BOX(main_vbox), statusbar, FALSE, FALSE, 0);

    // Connect window key press
    g_signal_connect(state->window, "key-press-event", G_CALLBACK(on_window_key_press), state);

    // Save state pointer on window
    g_object_set_data(G_OBJECT(state->window), "file_manager_state", state);

    // Clean up state on window destroy
    g_signal_connect(state->window, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer u) {
        delete static_cast<FileManagerState*>(u);
    }), state);

    // Determine target initial path
    std::string start_path = initial_path;
    if (start_path.empty()) {
        const char* h = g_get_home_dir();
        start_path = h ? h : "/";
    }

    navigate_to(state, start_path, false);

    gtk_widget_show_all(state->window);
    return state->window;
}

void FileManagerWindow::open_path(GtkWidget* window, const std::string& path) {
    if (!window) return;
    auto* state = static_cast<FileManagerState*>(g_object_get_data(G_OBJECT(window), "file_manager_state"));
    if (state) {
        navigate_to(state, path, true);
        gtk_window_present(GTK_WINDOW(window));
    }
}

} // namespace zenith
